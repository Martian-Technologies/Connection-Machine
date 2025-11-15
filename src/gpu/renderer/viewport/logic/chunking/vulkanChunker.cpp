#include "vulkanChunker.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "backend/evaluator/evaluator.h"
#include "backend/position/position.h"
#include "logging/logging.h"

#include "gpu/mainRenderer.h"
#include "gpu/renderer/viewport/blockTextureManager.h"

const int CHUNK_SIZE = 64;
coordinate_t getChunk(coordinate_t in) { return std::floor((float)in / (float)CHUNK_SIZE) * (int)CHUNK_SIZE; }
Position getChunk(Position in) {
	in.x = getChunk(in.x);
	in.y = getChunk(in.y);
	return in;
}

const unsigned int maxLaneCountBeforeWireShrink = 8;
constexpr float WIRE_LINE_WIDTH = 0.07f;

namespace {
	// Match wireConstants.glsl::LINE_WIDTH to keep CPU/GPU visuals aligned.
	constexpr float DIRECTION_EPSILON = 1e-5f;

	glm::vec2 computeBusOffset(const glm::vec2& pointA, const glm::vec2& pointB, uint32_t laneCount, uint32_t laneIndex) {
		// TODO: should handle cases like the colored light where we want the offset to be tangent to the side of the block
		if (laneCount <= 1) return glm::vec2(0.0f);

		glm::vec2 direction = pointB - pointA;
		float length = glm::length(direction);
		if (length < DIRECTION_EPSILON) {
			return glm::vec2(0.0f);
		}

		glm::vec2 normal = glm::vec2(-direction.y, direction.x) / length;
		if (normal.y < 0.f || (normal.y == 0.f && normal.x > 0.f)) {
			normal *= -1.f;
		}
		float centeredIndex = static_cast<float>(laneIndex) - (static_cast<float>(laneCount - 1) * 0.5f);
		return normal * (centeredIndex * WIRE_LINE_WIDTH * std::min((float)maxLaneCountBeforeWireShrink / (float)laneCount, 1.f));
	}
}

// VulkanChunkAllocation
// =========================================================================================================

VulkanChunkAllocation::VulkanChunkAllocation(
	VulkanDevice* device,
	const RenderedBlocks& blocks,
	const RenderedWires& wires,
	const Evaluator* evaluator,
	const Address& address
) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	// TODO - should pre-allocate buffers with size and pool them
	// TODO - maybe should use smaller size coordinates with one big offset

	std::vector<Position> positions;
	std::vector<size_t> indices;

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
			instance.stateStep = block.second.textureStateStep;

			blockInstances.push_back(instance);

			// blocks are added to state array
			blockStateIndex[block.first] = simulatorIds.size();
			positions.push_back(block.first);
			indices.push_back(simulatorIds.size());
			simulatorIds.push_back(simulator_id_t(0));
		}

		if (evaluator) {
			std::vector<simulator_id_t> simIds = evaluator->getBlockSimulatorIds(address, positions);
			for (size_t i = 0; i < simIds.size(); i++) {
				simulatorIds[indices[i]] = simIds[i];
			}
		}

		positions.clear();
		indices.clear();

		// upload block vertices
		numBlockInstances = blockInstances.size();
		size_t blockBufferSize = sizeof(BlockInstance) * numBlockInstances;
		blockBuffer = createBuffer(device, blockBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		vmaCopyMemoryToAllocation(device->getAllocator(), blockInstances.data(), blockBuffer->allocation, 0, blockBufferSize);
	}

	// Generate wire vertices
	if (wires.size() > 0) {
		struct WireSegment {
			glm::vec2 pointA;
			glm::vec2 pointB;
			Position portPosition;
		};

		std::vector<WireSegment> wireSegments;
		wireSegments.reserve(wires.size());

		positions.clear();
		indices.clear();
		std::vector<uint32_t> laneCounts;
		laneCounts.reserve(wires.size());

		phmap::flat_hash_map<Position, size_t> portRequestLookup;

		for (const auto& wire : wires) {
			Position portPosition = wire.first.first;
			wireSegments.push_back({
				glm::vec2(wire.second.start.x, wire.second.start.y),
				glm::vec2(wire.second.end.x, wire.second.end.y),
				portPosition
			});

			auto [itr, inserted] = portRequestLookup.emplace(portPosition, positions.size());
			if (inserted) {
				positions.push_back(portPosition);
			}
		}

		std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> pinSimIds;
		if (evaluator) {
			pinSimIds = evaluator->getPinSimulatorIds(address, positions);
		}

		laneCounts.resize(positions.size(), 1);
		indices.resize(positions.size());

		for (size_t i = 0; i < positions.size(); i++) {
			uint32_t laneCount = 1;
			if (pinSimIds.size() != 0 && std::holds_alternative<std::vector<simulator_id_t>>(pinSimIds[i])) {
				const std::vector<simulator_id_t>& wireSimIds = std::get<std::vector<simulator_id_t>>(pinSimIds[i]);
				if (wireSimIds.empty()) {
					logError("pin simulator ids vector should not be empty. Pin: {}", "VulkanChunkAllocation", positions[i]);
				} else {
					laneCount = static_cast<uint32_t>(wireSimIds.size());
				}
			}

			size_t baseIndex = simulatorIds.size();
			laneCounts[i] = laneCount;
			indices[i] = baseIndex;
			portStateIndex[positions[i]] = PortStateRange(baseIndex, laneCount);

			for (uint32_t lane = 0; lane < laneCount; lane++) {
				simulatorIds.push_back(simulator_id_t(0));
			}
		}

		if (evaluator) {
			for (size_t i = 0; i < pinSimIds.size() && i < positions.size(); i++) {
				size_t baseIndex = indices[i];
				uint32_t laneCount = laneCounts[i];
				if (std::holds_alternative<std::vector<simulator_id_t>>(pinSimIds[i])) {
					const auto& vec = std::get<std::vector<simulator_id_t>>(pinSimIds[i]);
					for (uint32_t lane = 0; lane < laneCount && lane < vec.size(); lane++) {
						simulatorIds[baseIndex + lane] = vec[lane];
					}
				} else {
					simulatorIds[baseIndex] = std::get<simulator_id_t>(pinSimIds[i]);
				}
			}
		}

		size_t totalWireInstances = 0;
		for (const WireSegment& segment : wireSegments) {
			auto lookup = portRequestLookup.find(segment.portPosition);
			uint32_t laneCount = 1;
			if (lookup != portRequestLookup.end()) {
				laneCount = laneCounts[lookup->second];
			}
			totalWireInstances += laneCount;
		}

		std::vector<WireInstance> wireInstances;
		wireInstances.reserve(totalWireInstances);

		for (const WireSegment& segment : wireSegments) {
			auto lookup = portRequestLookup.find(segment.portPosition);
			uint32_t laneCount = 1;
			size_t baseIndex = 0;
			if (lookup != portRequestLookup.end()) {
				laneCount = laneCounts[lookup->second];
				baseIndex = indices[lookup->second];
			}
			if (laneCount == 0) laneCount = 1;

			for (uint32_t lane = 0; lane < laneCount; lane++) {
				glm::vec2 offset = computeBusOffset(segment.pointA, segment.pointB, laneCount, lane);

				WireInstance instance;
				instance.pointA = segment.pointA + offset;
				instance.pointB = segment.pointB + offset;
				instance.wireWidth = WIRE_LINE_WIDTH * std::min((float)maxLaneCountBeforeWireShrink / (float)laneCount, 1.f);
				instance.stateIndex = static_cast<uint32_t>(baseIndex + lane);

				wireInstances.push_back(instance);
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

VulkanChunker::VulkanChunker(VulkanDevice* device) : device(device) { }

VulkanChunker::~VulkanChunker() { std::lock_guard<std::mutex> lock(mux); }

void VulkanChunker::startMakingEdits() { mux.lock(); }

void VulkanChunker::stopMakingEdits() {
	if (chunkToAlwaysRenderNeedUpdate) {
		chunkToAlwaysRender.rebuildAllocation(device, evaluator, address);
	}
	for (const Position& chunkPos : chunksToUpdate) {
		auto iter = chunks.find(chunkPos);
		if (iter != chunks.end()) iter->second.rebuildAllocation(device, evaluator, address);
	}
	chunksToUpdate.clear();

	mux.unlock();
}

void VulkanChunker::addBlock(BlockRenderDataId blockRenderDataId, Position position, Orientation orientation) {
	const BlockRenderDataManager::BlockRenderData* blockRenderData = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(blockRenderDataId);
	if (blockRenderData->size.w > CHUNK_SIZE || blockRenderData->size.h > CHUNK_SIZE) {
		chunkToAlwaysRender.getRenderedBlocks().emplace(
			position,
			RenderedBlock(
				blockRenderDataId,
				blockRenderData->blockTextureCords.textureLayer,
				blockRenderData->blockTextureCords.textureOriginUV,
				blockRenderData->blockTextureCords.textureSizeUV,
				blockRenderData->blockTextureCords.textureUVStateStep,
				orientation,
				(orientation * blockRenderData->size).free()
			)
		);
		chunkToAlwaysRenderNeedUpdate = true;
	} else {
		Position chunkPos = getChunk(position);
		auto iter = chunks.find(chunkPos);
		chunks[chunkPos].getRenderedBlocks().emplace(
			position,
			RenderedBlock(
				blockRenderDataId,
				blockRenderData->blockTextureCords.textureLayer,
				blockRenderData->blockTextureCords.textureOriginUV,
				blockRenderData->blockTextureCords.textureSizeUV,
				blockRenderData->blockTextureCords.textureUVStateStep,
				orientation,
				(orientation * blockRenderData->size).free()
			)
		);
		chunksToUpdate.insert(chunkPos);
	}
	blockTypesCount[blockRenderDataId] = 1; // for now just make it dirty. Should keep it from remaking everything.
}

void VulkanChunker::removeBlock(Position position) {
	auto blockIter = chunkToAlwaysRender.getRenderedBlocks().find(position);
	if (blockIter != chunkToAlwaysRender.getRenderedBlocks().end()) {
		chunkToAlwaysRender.getRenderedBlocks().erase(blockIter);
		chunkToAlwaysRenderNeedUpdate = true;
		return;
	}
	Position chunkPos = getChunk(position);
	auto iter = chunks.find(chunkPos);
	if (iter == chunks.end()) {
		logError("Could not remove block {} because it's chunk {} could not be found", "Vulkan", position, chunkPos);
		return;
	}
	blockIter = iter->second.getRenderedBlocks().find(position);
	if (blockIter == iter->second.getRenderedBlocks().end()) {
		logError("Could not remove block {} because it could not be found", "Vulkan", position);
		return;
	}
	iter->second.getRenderedBlocks().erase(blockIter);
	chunksToUpdate.insert(chunkPos);
}

void VulkanChunker::moveBlock(Position curPos, Position newPos, Orientation newOrientation) {
	auto blockIter = chunkToAlwaysRender.getRenderedBlocks().find(curPos);
	if (blockIter != chunkToAlwaysRender.getRenderedBlocks().end()) {
		RenderedBlock block = blockIter->second;
		block.orientation = newOrientation;
		block.size = newOrientation.relativeTo(blockIter->second.orientation) * block.size;
		chunkToAlwaysRender.getRenderedBlocks().erase(blockIter);
		chunkToAlwaysRender.getRenderedBlocks().emplace(newPos, block);
		chunkToAlwaysRenderNeedUpdate = true;
	}

	Position curChunkPos = getChunk(curPos);
	Position newChunkPos = getChunk(newPos);

	auto curChunkIter = chunks.find(curChunkPos);
	if (curChunkIter == chunks.end()) {
		logError("Cound not find chunk {} to move block at {}", "VulkanChunker", curChunkPos, curPos);
		return;
	}
	blockIter = curChunkIter->second.getRenderedBlocks().find(curPos);
	if (blockIter == curChunkIter->second.getRenderedBlocks().end()) {
		logError("Could not move block from {} to {} because it could not be found", "VulkanChunker", curPos, newPos);
		return;
	}
	RenderedBlock block = blockIter->second;
	block.orientation = newOrientation;
	block.size = newOrientation.relativeTo(blockIter->second.orientation) * block.size;
	curChunkIter->second.getRenderedBlocks().erase(blockIter);
	chunksToUpdate.insert(curChunkPos);

	if (newChunkPos != curChunkPos) {
		chunks[newChunkPos].getRenderedBlocks().emplace(newPos, block);
		chunksToUpdate.insert(newChunkPos);
	} else {
		curChunkIter->second.getRenderedBlocks().emplace(newPos, block);
	}
}

void VulkanChunker::addWire(std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets) {
	FPosition a = points.first.free() + socketOffsets.first;
	FPosition b = points.second.free() + socketOffsets.second;
	std::vector<ChunkIntersection> intersections = getNeededChunkIntersections(a, b);
	std::vector<Position>& chunksUnderThisWire = chunksUnderWire[points];
	chunksUnderThisWire.reserve(intersections.size());
	for (const ChunkIntersection& intersection : intersections) {
		portStatePosToChunks[points.first][intersection.chunk]++;
		chunks[intersection.chunk].getRenderedWires()[points] = { intersection.start, intersection.end };
		chunksUnderThisWire.push_back(intersection.chunk);
		chunksToUpdate.insert(intersection.chunk);
	}
}

void VulkanChunker::removeWire(std::pair<Position, Position> points) {
	auto itr = chunksUnderWire.find(points);
	if (itr == chunksUnderWire.end()) {
		std::swap(points.first, points.second);
		itr = chunksUnderWire.find(points);
		if (itr == chunksUnderWire.end()) {
			logError("Could not find wire ({}, {}) to remove", "VulkanChunker", points.first, points.second);
			return;
		}
	}
	auto portStateChunksIter = portStatePosToChunks.find(points.first);
	if (portStateChunksIter != portStatePosToChunks.end()) {
		if (portStateChunksIter->second.size() <= itr->second.size()) {
			portStatePosToChunks.erase(portStateChunksIter);
		} else {
			for (Position chunkPos : itr->second) {
				auto portStateChunkIter = portStateChunksIter->second.find(chunkPos);
				if (portStateChunkIter != portStateChunksIter->second.end()) {
					if (portStateChunkIter->second <= 1) {
						portStateChunksIter->second.erase(portStateChunkIter);
					} else {
						portStateChunkIter->second--;
					}
				}
			}
		}
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
				block.second.size = (block.second.orientation * blockRenderData->size).free();
				block.second.textureIndex = blockRenderData->blockTextureCords.textureLayer;
				block.second.textureOrigin = blockRenderData->blockTextureCords.textureOriginUV;
				block.second.textureSize = blockRenderData->blockTextureCords.textureSizeUV;
				block.second.textureStateStep = blockRenderData->blockTextureCords.textureUVStateStep;
			}
		}
		if (foundType) chunk.second.rebuildAllocation(device, evaluator, address);
	}
	bool foundType = false;
	for (std::pair<const Position, RenderedBlock>& block : chunkToAlwaysRender.getRenderedBlocks()) {
		if (block.second.blockRenderDataId == blockRenderDataId) {
			foundType = true;
			block.second.size = (block.second.orientation * blockRenderData->size).free();
			block.second.textureIndex = blockRenderData->blockTextureCords.textureLayer;
			block.second.textureOrigin = blockRenderData->blockTextureCords.textureOriginUV;
			block.second.textureSize = blockRenderData->blockTextureCords.textureSizeUV;
			block.second.textureStateStep = blockRenderData->blockTextureCords.textureUVStateStep;
		}
	}
	if (foundType) chunkToAlwaysRender.rebuildAllocation(device, evaluator, address);
}

void VulkanChunker::updateSimulatorIds(const std::vector<SimulatorMappingUpdate>& simulatorMappingUpdates) {
	for (const SimulatorMappingUpdate& simulatorMappingUpdate : simulatorMappingUpdates) {
		const std::variant<simulator_id_t, std::vector<simulator_id_t>>& simIds = simulatorMappingUpdate.simulatorIds;

		if (simulatorMappingUpdate.type == SimulatorMappingUpdateType::BLOCK) {
			simulator_id_t simulatorId = simulator_id_t(0);
			if (std::holds_alternative<std::vector<simulator_id_t>>(simIds)) {
				const std::vector<simulator_id_t>& vec = std::get<std::vector<simulator_id_t>>(simIds);
				if (!vec.empty()) {
					simulatorId = vec[0];
				}
			} else {
				simulatorId = std::get<simulator_id_t>(simIds);
			}

			std::optional<std::shared_ptr<VulkanChunkAllocation>> vulkanChunkAllocation = chunkToAlwaysRender.getAllocation();
			if (vulkanChunkAllocation) {
				auto iter = vulkanChunkAllocation.value()->getBlockStateIndex().find(simulatorMappingUpdate.portPosition);
				if (iter != vulkanChunkAllocation.value()->getBlockStateIndex().end()) {
					vulkanChunkAllocation.value()->getStateSimulatorIds()[iter->second] = simulatorId;
				}
			}

			Position chunkPos = getChunk(simulatorMappingUpdate.portPosition);
			auto chunkIter = chunks.find(chunkPos);
			if (chunkIter == chunks.end()) continue;
			vulkanChunkAllocation = chunkIter->second.getAllocation();
			if (!vulkanChunkAllocation) continue;
			auto iter = vulkanChunkAllocation.value()->getBlockStateIndex().find(simulatorMappingUpdate.portPosition);
			if (iter == vulkanChunkAllocation.value()->getBlockStateIndex().end()) continue;
			vulkanChunkAllocation.value()->getStateSimulatorIds()[iter->second] = simulatorId;
		} else {
			auto iter = portStatePosToChunks.find(simulatorMappingUpdate.portPosition);
			if (iter == portStatePosToChunks.end()) continue;
			for (std::pair<Position, unsigned int> chunkPos : iter->second) {
				auto chunkIter = chunks.find(chunkPos.first);
				if (chunkIter == chunks.end()) continue;
				std::optional<std::shared_ptr<VulkanChunkAllocation>> vulkanChunkAllocation = chunkIter->second.getAllocation();
				if (!vulkanChunkAllocation) continue;
				auto portStateIter = vulkanChunkAllocation.value()->getPortStateIndex().find(simulatorMappingUpdate.portPosition);
				if (portStateIter == vulkanChunkAllocation.value()->getPortStateIndex().end()) continue;

				const PortStateRange& range = portStateIter->second;
				if (!range.isValid()) continue;

				std::vector<simulator_id_t>& chunkStateSimulatorIds = vulkanChunkAllocation.value()->getStateSimulatorIds();
				if (std::holds_alternative<std::vector<simulator_id_t>>(simIds)) {
					const std::vector<simulator_id_t>& wireSimIds = std::get<std::vector<simulator_id_t>>(simIds);
					uint32_t laneCount = wireSimIds.size();
					if (laneCount != range.laneCount) {
						chunksToUpdate.insert(chunkPos.first);
					} else {
						for (uint32_t lane = 0; lane < laneCount; lane++) {
							chunkStateSimulatorIds[range.baseIndex + lane] = wireSimIds[lane];
						}
					}
				} else {
					if (1 != range.laneCount) {
						chunksToUpdate.insert(chunkPos.first);
					} else {
						chunkStateSimulatorIds[range.baseIndex] = std::get<simulator_id_t>(simIds);
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

	auto allocation = chunkToAlwaysRender.getAllocation();
	if (allocation.has_value()) seen.push_back(allocation.value());

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

	// length of ray from one x or y-side to next x or y-side
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

	// length of ray from one x or y-side to next x or y-side
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
