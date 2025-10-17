#include "vulkanChunker.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "backend/evaluator/evaluator.h"
#include "backend/position/position.h"
#include "logging/logging.h"

#include "gpu/renderer/viewport/blockTextureManager.h"
#include "gpu/mainRenderer.h"

const int CHUNK_SIZE = 64;
coordinate_t getChunk(coordinate_t in) {
	return std::floor((float)in / (float)CHUNK_SIZE) * (int)CHUNK_SIZE;
}
Position getChunk(Position in) {
	in.x = getChunk(in.x);
	in.y = getChunk(in.y);
	return in;
}

// VulkanChunkAllocation
// =========================================================================================================

VulkanChunkAllocation::VulkanChunkAllocation(VulkanDevice* device,const RenderedBlocks& blocks,const RenderedWires& wires, const Evaluator* evaluator, const Address& address) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	// TODO - should pre-allocate buffers with size and pool them
	// TODO - maybe should use smaller size coordinates with one big offset

	std::vector<Position> positions;
	std::vector<size_t> indexes;

	// Generate block instances
	if (blocks.size() > 0) {
		std::vector<BlockInstance> blockInstances;
		blockInstances.reserve(blocks.size());
		for (const auto& block : blocks) {
			Position blockPosition = block.first;

			BlockInstance instance;
			instance.pos = glm::vec2(blockPosition.x, blockPosition.y);
			instance.sizeX = block.second.size.w;
			instance.sizeY = block.second.size.h;
			instance.orientation = block.second.orientation.rotation + 4 * block.second.orientation.flipped;
			instance.texLayer = block.second.textureIndex;
			instance.texPos = block.second.textureOrigin;
			instance.texSize = block.second.textureSize;

			blockInstances.push_back(instance);

			// blocks are added to state array
			blockStateIndex[block.second.statePosition] = simulatorIds.size();
			positions.push_back(block.second.statePosition);
			indexes.push_back(simulatorIds.size());
			simulatorIds.push_back(0);
		}

		if (evaluator) {
			std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> simIds = evaluator->getBlockSimulatorIds(address, positions);
			for (size_t i = 0; i < simIds.size(); i++) {
				// for now, if we get multiple sim ids, just use the first one
				if (std::holds_alternative<std::vector<simulator_id_t>>(simIds[i])) {
					auto vec = std::get<std::vector<simulator_id_t>>(simIds[i]);
					if (!vec.empty()) {
						simulatorIds[indexes[i]] = vec[0];
					}
				} else {
					simulatorIds[indexes[i]] = std::get<simulator_id_t>(simIds[i]);
				}
			}
		}

		positions.clear();
		indexes.clear();

		// upload block vertices
		numBlockInstances = blockInstances.size();
		size_t blockBufferSize = sizeof(BlockInstance) * numBlockInstances;
		blockBuffer = createBuffer(device, blockBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		vmaCopyMemoryToAllocation(device->getAllocator(), blockInstances.data(), blockBuffer->allocation, 0, blockBufferSize);
	}

	// Generate wire vertices
	if (wires.size() > 0) {
		std::vector<WireInstance> wireInstances;
		wireInstances.reserve(wires.size());
		for (const auto& wire : wires) {

			// get wire's index in state buffer
			size_t stateIdx;
			auto itr = portStateIndex.find(wire.first.first);
			// check if wire state position is already in the state array
			if (itr != portStateIndex.end()) {
				stateIdx = itr->second;
			} else {
				// add address to state buffer
				stateIdx = simulatorIds.size();
				portStateIndex[wire.first.first] = stateIdx;
				positions.push_back(wire.first.first);
				indexes.push_back(stateIdx);
				simulatorIds.push_back(0);
			}

			WireInstance instance;
			instance.pointA = glm::vec2(wire.second.start.x, wire.second.start.y);
			instance.pointB = glm::vec2(wire.second.end.x, wire.second.end.y);
			instance.stateIndex = stateIdx;

			wireInstances.push_back(instance);
		}

		if (evaluator) {
			std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> simIds = evaluator->getPinSimulatorIds(address, positions);
			for (size_t i = 0; i < simIds.size(); i++) {
				// for now, if we get multiple sim ids, just use the first one
				if (std::holds_alternative<std::vector<simulator_id_t>>(simIds[i])) {
					auto vec = std::get<std::vector<simulator_id_t>>(simIds[i]);
					if (!vec.empty()) {
						simulatorIds[indexes[i]] = vec[0];
					}
				} else {
					simulatorIds[indexes[i]] = std::get<simulator_id_t>(simIds[i]);
				}
			}
		}

		// upload wire vertices
		numWireInstances = wireInstances.size();
		size_t wireBufferSize = sizeof(WireInstance) * numWireInstances;
		wireBuffer = createBuffer(device, wireBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		vmaCopyMemoryToAllocation(device->getAllocator(), wireInstances.data(), wireBuffer->allocation, 0, wireBufferSize);
	}
	
	if (!simulatorIds.empty()) {
		// Create state buffer
		size_t stateBufferSize = simulatorIds.size() * sizeof(logic_state_t);
		stateBuffer.emplace();
		stateBuffer->init(device, stateBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		std::vector<logic_state_t> defaultStates(simulatorIds.size(), logic_state_t::HIGH);
	}
}

VulkanChunkAllocation::~VulkanChunkAllocation() {
	if (blockBuffer.has_value()) destroyBuffer(blockBuffer.value());
	if (wireBuffer.has_value()) destroyBuffer(wireBuffer.value());
	if (stateBuffer.has_value()) stateBuffer->cleanup();
}

// ChunkChain
// =========================================================================================================

void Chunk::rebuildAllocation(VulkanDevice* device, const Evaluator* evaluator, const Address& address) {
	if (!blocks.empty() || !wires.empty()) { // if we have data to upload
		// allocate new date
		std::shared_ptr<VulkanChunkAllocation> newAllocation = std::make_unique<VulkanChunkAllocation>(device, blocks, wires, evaluator, address);
		// replace currently allocating data
		if (currentlyAllocating.has_value()) {
			gbJail.push_back(currentlyAllocating.value());
		}
		currentlyAllocating = newAllocation;

	} else { // if we have no data to upload
		// drop currently allocating, send to gay baby jail
		if (currentlyAllocating.has_value()) {
			gbJail.push_back(currentlyAllocating.value());
		}
		currentlyAllocating.reset();

		// drop newest allocation
		newestAllocation.reset();
	}
}

std::optional<std::shared_ptr<VulkanChunkAllocation>> Chunk::getAllocation() {
	// if the buffer has finished allocating, replace the newest with it
	if (currentlyAllocating.has_value() && currentlyAllocating.value()->isAllocationComplete()) {
		newestAllocation = currentlyAllocating;
		currentlyAllocating.reset();
	}

	annihilateOrphanGBs();

	// get the newest allocation if there is one
	return newestAllocation;
}

void Chunk::annihilateOrphanGBs() {
	// erase all GBs that are complete and not pointed to
	auto itr = gbJail.begin();
	while (itr != gbJail.end()) {
		if ((*itr)->isAllocationComplete()) {
			itr = gbJail.erase(itr);
		} else itr++;
	}
}

// VulkanChunker
// =========================================================================================================

VulkanChunker::VulkanChunker(VulkanDevice* device)
	: device(device) {

}

VulkanChunker::~VulkanChunker() {
	std::lock_guard<std::mutex> lock(mux);
}

void VulkanChunker::startMakingEdits() {
	mux.lock();
}

void VulkanChunker::stopMakingEdits() {
	for (const Position& chunkPos : chunksToUpdate) {
		auto iter = chunks.find(chunkPos);
		if (iter != chunks.end()) iter->second.rebuildAllocation(device, evaluator, address);
	}
	chunksToUpdate.clear();

	mux.unlock();
}

void VulkanChunker::addBlock(BlockRenderDataId blockRenderDataId, Position position, Orientation orientation, Position statePosition) {
	Position chunkPos = getChunk(position);
	auto iter = chunks.find(chunkPos);
	const BlockRenderDataManager::BlockRenderData* blockRenderData = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(blockRenderDataId);
	chunks[chunkPos].getRenderedBlocks().emplace(position, RenderedBlock(
		blockRenderDataId,
		blockRenderData->blockTextureCords.textureLayer,
		blockRenderData->blockTextureCords.textureOriginUV,
		blockRenderData->blockTextureCords.textureSizeUV,
		orientation,
		(orientation * blockRenderData->size).free(),
		statePosition
	));
	chunksToUpdate.insert(chunkPos);
	blockTypesCount[blockRenderDataId] = 1; // for now just make it dirty. Should keep it from remaking everything.
}

void VulkanChunker::removeBlock(Position position) {
	Position chunkPos = getChunk(position);
	auto iter = chunks.find(chunkPos);
	if (iter != chunks.end()) {
		iter->second.getRenderedBlocks().erase(position);
		chunksToUpdate.insert(chunkPos);
	}
}

void VulkanChunker::moveBlock(Position curPos, Position newPos, Orientation newOrientation) {
	Position curChunkPos = getChunk(curPos);
	Position newChunkPos = getChunk(newPos);


	auto curChunkIter = chunks.find(curChunkPos);
	if (curChunkIter == chunks.end()) {
		logError("Cound not find chunk {} to move block at {}", "VulkanChunker", curChunkPos, curPos);
		return;
	}
	auto blockIter = curChunkIter->second.getRenderedBlocks().find(curPos);
	if (blockIter != curChunkIter->second.getRenderedBlocks().end()) {
		RenderedBlock block = blockIter->second;
		Orientation transformAmount = newOrientation.relativeTo(block.orientation);
		block.statePosition = newPos + transformAmount.transformVectorWithArea(block.statePosition - curPos, block.size.snap());
		block.orientation = newOrientation;
		block.size = transformAmount * block.size;
		curChunkIter->second.getRenderedBlocks().erase(blockIter);
		chunksToUpdate.insert(curChunkPos);

		if (newChunkPos != curChunkPos) {
			chunks[newChunkPos].getRenderedBlocks().emplace(newPos, block);
			chunksToUpdate.insert(newChunkPos);
		} else {
			curChunkIter->second.getRenderedBlocks().emplace(newPos, block);
		}
	} else {
		logError("Could not move block from {} to {} because it could not be found", "Vulkan", curPos, newPos);
	}
}

void VulkanChunker::addWire(std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets) {
	FPosition a = points.first.free() + socketOffsets.first;
	FPosition b = points.second.free() + socketOffsets.second;
	std::vector<ChunkIntersection> intersections = getNeededChunkIntersections(a, b);
	std::vector<Position>& chunksUnderThisWire = chunksUnderWire[points];
	chunksUnderThisWire.reserve(intersections.size());
	for (const ChunkIntersection& intersection : intersections) {
		chunks[intersection.chunk].getRenderedWires()[points] = { intersection.start, intersection.end };
		chunksUnderThisWire.push_back(intersection.chunk);
		chunksToUpdate.insert(intersection.chunk);
	}
}

void VulkanChunker::removeWire(std::pair<Position, Position> points) {
	auto itr = chunksUnderWire.find(points);
	if (itr == chunksUnderWire.end()) {
		logError("Could not find wire ({}, {}) to remove", "VulkanChunker", points.first, points.second);
		return;
	}
	for (Position chunkPos : itr->second) {
		auto iter = chunks.find(chunkPos);
		if (iter != chunks.end()) {
			iter->second.getRenderedWires().erase(points);
			chunksToUpdate.insert(chunkPos);
		}
	}
	chunksUnderWire.erase(itr);
}

void VulkanChunker::reset() {
	std::lock_guard<std::mutex> lock(mux);

	chunks.clear();
	chunksUnderWire.clear();
	blockTypesCount.clear();
}

void VulkanChunker::regenerateAllChunksWithBlock(BlockRenderDataId blockRenderDataId) {
	std::lock_guard<std::mutex> lock(mux);
	auto iter = blockTypesCount.find(blockRenderDataId);
	if (iter == blockTypesCount.end()) return;
	if (iter->second == 0) return;

	const BlockRenderDataManager::BlockRenderData* blockRenderData = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(blockRenderDataId);

	for (std::pair<const Position, Chunk>& chunk : chunks) {
		bool foundType = false;
		for (std::pair<const Position, RenderedBlock>& block : chunk.second.getRenderedBlocks()) {
			if (block.second.blockRenderDataId == blockRenderDataId) {
				foundType = true;
				block.second.size = blockRenderData->size.free();
				block.second.textureIndex = blockRenderData->blockTextureCords.textureLayer;
				block.second.textureOrigin = blockRenderData->blockTextureCords.textureOriginUV;
				block.second.textureSize = blockRenderData->blockTextureCords.textureSizeUV;
			}
		}
		if (foundType) chunk.second.rebuildAllocation(device, evaluator, address);
	}
}

void VulkanChunker::updateSimulatorIds(const std::vector<SimulatorMappingUpdate>& simulatorMappingUpdates) {
	for (auto& pair : chunks) {
		std::optional<std::shared_ptr<VulkanChunkAllocation>> allocation = pair.second.getAllocation();
		std::shared_ptr<VulkanChunkAllocation> vulkanChunkAllocation = allocation.value_or(nullptr);
		if (vulkanChunkAllocation) {
			for (const SimulatorMappingUpdate& simulatorMappingUpdate : simulatorMappingUpdates) {
				const std::variant<simulator_id_t, std::vector<simulator_id_t>> simIds = simulatorMappingUpdate.simulatorIds;
				// for now, if we get multiple sim ids, just use the first one
				simulator_id_t simulatorId = 0;
				if (std::holds_alternative<std::vector<simulator_id_t>>(simulatorMappingUpdate.simulatorIds)) {
					auto vec = std::get<std::vector<simulator_id_t>>(simulatorMappingUpdate.simulatorIds);
					if (!vec.empty()) {
						simulatorId = vec[0];
					}
				} else {
					simulatorId = std::get<simulator_id_t>(simulatorMappingUpdate.simulatorIds);
				}
				if (simulatorMappingUpdate.type == SimulatorMappingUpdateType::BLOCK) {
					auto iter = vulkanChunkAllocation->getBlockStateIndex().find(simulatorMappingUpdate.portPosition);
					if (iter != vulkanChunkAllocation->getBlockStateIndex().end()) {
						vulkanChunkAllocation->getStateSimulatorIds()[iter->second] = simulatorId;
					}
				} else {
					auto iter = vulkanChunkAllocation->getPortStateIndex().find(simulatorMappingUpdate.portPosition);
					if (iter != vulkanChunkAllocation->getPortStateIndex().end()) {
						vulkanChunkAllocation->getStateSimulatorIds()[iter->second] = simulatorId;
					}
				}

			}
		}
	}
}

void VulkanChunker::setEvaluator(Evaluator* evaluator, const Address& address) {
	if (this->evaluator) {
		this->evaluator->disconnectListener(this);
	}
	this->address = address;
	this->evaluator = evaluator;
	if (evaluator) {
		logInfo("setEvaluator > connectListener");
		evaluator->connectListener(this, address, std::bind(&VulkanChunker::updateSimulatorIds, this, std::placeholders::_1));
	}
	for (auto& pair : chunks) {
		pair.second.rebuildAllocation(device, evaluator, address);
	}
}

std::vector<std::shared_ptr<VulkanChunkAllocation>> VulkanChunker::getAllocations(Position min, Position max) {
	std::lock_guard<std::mutex> lock(mux);

	// get chunk bounds with padding for large blocks (this will technically goof if there are blocks larger than chunk size)
	min = getChunk(min - Vector(CHUNK_SIZE) - Vector(1));
	max = getChunk(max + Vector(CHUNK_SIZE) + Vector(1));

	// go through each chunk in view and collect it if it exists and has an allocation
	std::vector<std::shared_ptr<VulkanChunkAllocation>> seen;
	for (coordinate_t chunkX = min.x; chunkX <= max.x; chunkX += CHUNK_SIZE) {
		for (coordinate_t chunkY = min.y; chunkY <= max.y; chunkY += CHUNK_SIZE) {
			auto chunk = chunks.find({ chunkX, chunkY });
			if (chunk != chunks.end()) {
				auto allocation = chunk->second.getAllocation();
				if (allocation.has_value()) seen.push_back(allocation.value());
			}
		}
	}

	return seen;
}

std::vector<ChunkIntersection> VulkanChunker::getChunkIntersections(FPosition start, FPosition end) {
	// the JACLWANICBSBTIOPICG (copyright 2025, released under MIT license) also known as DDA (or Ben is lying)
	// Thank you One Lone Coder and lodev

	std::vector<ChunkIntersection> intersections;

	FVector diff = end - start;
	float distance = diff.length();
	FVector dir = diff / distance;

	Position chunk = getChunk(start.snap());

	//length of ray from one x or y-side to next x or y-side
	FVector rayUnitStepSize = FVector(sqrt(1 + (dir.dy / dir.dx) * (dir.dy / dir.dx)), sqrt(1 + (dir.dx / dir.dy) * (dir.dx / dir.dy))) * CHUNK_SIZE;

	// starting conditions
	FVector rayLength1D;
	Vector step;
	if (dir.dx < 0) {
		step.dx = -CHUNK_SIZE;
		rayLength1D.dx = ((start.x - float(chunk.x)) / float(CHUNK_SIZE)) * rayUnitStepSize.dx;
	} else {
		step.dx = CHUNK_SIZE;
		rayLength1D.dx = ((float(chunk.x + CHUNK_SIZE) - start.x) / float(CHUNK_SIZE)) * rayUnitStepSize.dx;
	}
	if (dir.dy < 0) {
		step.dy = -CHUNK_SIZE;
		rayLength1D.dy = ((start.y - float(chunk.y)) / float(CHUNK_SIZE)) * rayUnitStepSize.dy;
	} else {
		step.dy = CHUNK_SIZE;
		rayLength1D.dy = ((float(chunk.y + CHUNK_SIZE) - start.y) / float(CHUNK_SIZE)) * rayUnitStepSize.dy;
	}

	FPosition currentPos = start;
	float currentDistance = 0.0f;
	while (currentDistance < distance) {

		Position oldChunk = chunk;

		// decide which direction we walk
		if (rayLength1D.dx < rayLength1D.dy) {
			chunk.x += step.dx;
			currentDistance = rayLength1D.dx;
			rayLength1D.dx += rayUnitStepSize.dx;

		} else if (rayLength1D.dx > rayLength1D.dy) {
			chunk.y += step.dy;
			currentDistance = rayLength1D.dy;
			rayLength1D.dy += rayUnitStepSize.dy;
		} else {
			chunk += step;
			currentDistance = rayLength1D.dx;
			rayLength1D.dx += rayUnitStepSize.dx;
			rayLength1D.dy += rayUnitStepSize.dy;
		}

		// clamp overshoot
		if (currentDistance > distance) {
			currentDistance = distance;
		}

		// add point at current distance
		FPosition newPos = start + dir * currentDistance;
		intersections.push_back({ oldChunk, currentPos, newPos });
		currentPos = newPos;
	}

	return intersections;
}

std::vector<ChunkIntersection> VulkanChunker::getNeededChunkIntersections(FPosition start, FPosition end) {
	// the JACLWANICBSBTIOPICG (copyright 2025, released under MIT license) also known as DDA (or Ben is lying)
	// Thank you One Lone Coder and lodev

	std::vector<ChunkIntersection> intersections;

	FVector diff = end - start;
	float distance = diff.length();
	FVector dir = diff / distance;

	Position chunk = getChunk(start.snap());

	//length of ray from one x or y-side to next x or y-side
	FVector rayUnitStepSize = FVector(sqrt(1 + (dir.dy / dir.dx) * (dir.dy / dir.dx)), sqrt(1 + (dir.dx / dir.dy) * (dir.dx / dir.dy))) * CHUNK_SIZE;

	// starting conditions
	FVector rayLength1D;
	Vector step;
	if (dir.dx < 0) {
		step.dx = -CHUNK_SIZE;
		rayLength1D.dx = ((start.x - float(chunk.x)) / float(CHUNK_SIZE)) * rayUnitStepSize.dx;
	} else {
		step.dx = CHUNK_SIZE;
		rayLength1D.dx = ((float(chunk.x + CHUNK_SIZE) - start.x) / float(CHUNK_SIZE)) * rayUnitStepSize.dx;
	}
	if (dir.dy < 0) {
		step.dy = -CHUNK_SIZE;
		rayLength1D.dy = ((start.y - float(chunk.y)) / float(CHUNK_SIZE)) * rayUnitStepSize.dy;
	} else {
		step.dy = CHUNK_SIZE;
		rayLength1D.dy = ((float(chunk.y + CHUNK_SIZE) - start.y) / float(CHUNK_SIZE)) * rayUnitStepSize.dy;
	}

	FPosition currentPos = start;
	float currentDistance = 0.0f;
	unsigned int doAdd = 0;
	Position oldOldChunk = chunk;
	while (currentDistance < distance) {

		Position oldChunk = chunk;

		// decide which direction we walk
		if (rayLength1D.dx < rayLength1D.dy) {
			chunk.x += step.dx;
			currentDistance = rayLength1D.dx;
			rayLength1D.dx += rayUnitStepSize.dx;

		} else if (rayLength1D.dx > rayLength1D.dy) {
			chunk.y += step.dy;
			currentDistance = rayLength1D.dy;
			rayLength1D.dy += rayUnitStepSize.dy;
		} else {
			chunk += step;
			currentDistance = rayLength1D.dx;
			rayLength1D.dx += rayUnitStepSize.dx;
			rayLength1D.dy += rayUnitStepSize.dy;
		}

		// clamp overshoot
		if (currentDistance > distance) {
			currentDistance = distance;
		}

		// add point at current distance

		doAdd++;
		if (currentDistance >= distance || doAdd == 3) {
			doAdd = 0;
			FPosition newPos = start + dir * currentDistance;
			intersections.push_back({ oldOldChunk, currentPos, newPos });
			currentPos = newPos;
		}
		oldOldChunk = oldChunk;
	}

	return intersections;
}

