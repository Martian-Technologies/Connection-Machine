#include "vulkanChunker.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "backend/position/position.h"
#include "logging/logging.h"

#include "gpu/mainRenderer.h"
#include "gpu/renderer/viewport/blockTextureManager.h"

const int CHUNK_SIZE = 32;
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
} // namespace

// VulkanLogicAllocation
// =========================================================================================================

VulkanLogicAllocation::VulkanLogicAllocation(
	VulkanDevice& device,
	const RenderedBlocks& blocks,
	const RenderedWires& wires,
	const EvalLogicSimulator* simulator,
	const Address& address
) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	// TODO - should pre-allocate buffers with size and pool them
	// TODO - maybe should use smaller size coordinates with one big offset

	simulatorIds = { 0 }; // set first state index to 0 for blocks that dont have any state

	// Generate block instances
	if (blocks.size() > 0) {
		std::vector<size_t> indices = { 0 };
		std::vector<std::pair<Position, virtual_connection_id_t>> virtualConnections;
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

			if (simulator) {
				// blocks are added to state array
				std::optional<virtual_connection_id_t> textureVirtualConnection = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(block.second.blockRenderDataId)->textureVirtualConnection;
				if (textureVirtualConnection) {
					blockStateIndex[block.first] = simulatorIds.size();
					virtualConnections.push_back({block.first, textureVirtualConnection.value()});
					indices.push_back(simulatorIds.size());
					simulatorIds.push_back(simulator_state_reference(0));
				} else {
					blockStateIndex[block.first] = 0;
				}
			} else {
				blockStateIndex[block.first] = 0;
			}
		}

		if (simulator) {
			std::vector<SimulatorStateIndexVecVariant> blockSimulatorIds = simulator->getVirtualConnectionSimulatorIds(address, virtualConnections);
			for (size_t i = 0; i < blockSimulatorIds.size(); i++) {
				if (std::holds_alternative<std::vector<simulator_state_reference>>(blockSimulatorIds[i])) {
					simulatorIds[indices[i]] = 0;
				} else {
					simulatorIds[indices[i]] = std::get<simulator_state_reference>(blockSimulatorIds[i]);
				}
			}
		}

		// upload block vertices
		numBlockInstances = blockInstances.size();
		size_t blockBufferSize = sizeof(BlockInstance) * numBlockInstances;
		blockBuffer.emplace(createBuffer(device, blockBufferSize, vk::BufferUsageFlagBits::eVertexBuffer, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite));
		device.getAllocator().copyMemoryToAllocation(blockInstances.data(), blockBuffer->allocation.get(), 0, blockBufferSize);

		indices.clear();
	}

	// Generate wire vertices
	if (wires.size() > 0) {
		std::vector<size_t> indices;
		std::vector<Position> positions;

		struct WireSegment {
			glm::vec2 pointA;
			glm::vec2 pointB;
			Position portPosition;
		};

		std::vector<WireSegment> wireSegments;
		wireSegments.reserve(wires.size());

		std::vector<uint32_t> laneCounts;
		laneCounts.reserve(wires.size());

		phmap::flat_hash_map<Position, size_t> portRequestLookup;

		for (const auto& wire : wires) {
			Position portPosition = wire.first.first;
			wireSegments.push_back({ glm::vec2(wire.second.start.x, wire.second.start.y), glm::vec2(wire.second.end.x, wire.second.end.y), portPosition });

			auto [itr, inserted] = portRequestLookup.emplace(portPosition, positions.size());
			if (inserted) {
				positions.push_back(portPosition);
			}
		}

		std::vector<SimulatorStateIndexVecVariant> pinSimulatorIds;
		if (simulator) {
			pinSimulatorIds = simulator->getPinSimulatorIds(address, positions);
		}

		laneCounts.resize(positions.size(), 1);
		indices.resize(positions.size());

		for (size_t i = 0; i < positions.size(); i++) {
			uint32_t laneCount = 1;
			if (pinSimulatorIds.size() != 0 && std::holds_alternative<std::vector<simulator_state_reference>>(pinSimulatorIds[i])) {
				const std::vector<simulator_state_reference>& wireSimulatorIds = std::get<std::vector<simulator_state_reference>>(pinSimulatorIds[i]);
				if (wireSimulatorIds.empty()) {
					logError("pin simulator ids vector should not be empty. Pin: {}", "VulkanLogicAllocation", positions[i]);
				} else {
					laneCount = static_cast<uint32_t>(wireSimulatorIds.size());
				}
			}

			size_t baseIndex = simulatorIds.size();
			laneCounts[i] = laneCount;
			indices[i] = baseIndex;
			portStateIndex[positions[i]] = PortStateRange(baseIndex, laneCount);

			for (uint32_t lane = 0; lane < laneCount; lane++) {
				simulatorIds.push_back(simulator_state_reference(0));
			}
		}

		if (simulator) {
			for (size_t i = 0; i < pinSimulatorIds.size() && i < positions.size(); i++) {
				size_t baseIndex = indices[i];
				uint32_t laneCount = laneCounts[i];
				if (std::holds_alternative<std::vector<simulator_state_reference>>(pinSimulatorIds[i])) {
					const auto& vec = std::get<std::vector<simulator_state_reference>>(pinSimulatorIds[i]);
					for (uint32_t lane = 0; lane < laneCount && lane < vec.size(); lane++) {
						simulatorIds[baseIndex + lane] = vec[lane];
					}
				} else {
					simulatorIds[baseIndex] = std::get<simulator_state_reference>(pinSimulatorIds[i]);
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
		wireBuffer.emplace(createBuffer(device, wireBufferSize, vk::BufferUsageFlagBits::eVertexBuffer, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite));
		device.getAllocator().copyMemoryToAllocation(wireInstances.data(), wireBuffer->allocation.get(), 0, wireBufferSize);
	}

	if (!simulatorIds.empty()) {
		// Create state buffer
		size_t stateBufferSize = simulatorIds.size() * sizeof(logic_state_t);
		stateBuffer.emplace();
		stateBuffer->init(device, stateBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
		std::vector<logic_state_t> defaultStates(simulatorIds.size(), logic_state_t::HIGH);
	}
}

VulkanLogicAllocation::~VulkanLogicAllocation() {
	if (stateBuffer.has_value()) stateBuffer->cleanup();
}

// LogicGroup
// =========================================================================================================

void LogicGroup::rebuildAllocation(VulkanDevice& device, const EvalLogicSimulator* simulator, const Address& address) {
	if (!blocks.empty() || !wires.empty()) { // if we have data to upload
		// allocate new date
		std::shared_ptr<VulkanLogicAllocation> newAllocation = std::make_unique<VulkanLogicAllocation>(device, blocks, wires, simulator, address);
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

std::optional<std::shared_ptr<VulkanLogicAllocation>> LogicGroup::getAllocation() {
	// if the buffer has finished allocating, replace the newest with it
	if (currentlyAllocating.has_value() && currentlyAllocating.value()->isAllocationComplete()) {
		newestAllocation = currentlyAllocating;
		currentlyAllocating.reset();
	}

	annihilateOrphanGBs();

	// get the newest allocation if there is one
	return newestAllocation;
}

void LogicGroup::annihilateOrphanGBs() {
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

VulkanChunker::VulkanChunker(VulkanDevice& device) : device(device) { }

VulkanChunker::~VulkanChunker() { std::lock_guard<std::mutex> lock(mux); }

void VulkanChunker::startMakingEdits() { mux.lock(); }

void VulkanChunker::stopMakingEdits() {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
	for (LogicGroup* logicGroup : logicGroupsToUpdate) logicGroup->rebuildAllocation(device, simulator, address);
	logicGroupsToUpdate.clear();
	mux.unlock();
}

void VulkanChunker::addBlock(BlockRenderDataId blockRenderDataId, Position position, Orientation orientation) {
	const BlockRenderDataManager::BlockRenderData* blockRenderData = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(blockRenderDataId);
	std::vector<Position> groupKeyVec;
	for (auto chunkPosIter = getChunk(position).iterTo(getChunk(position + (orientation * blockRenderData->size).getLargestVectorInArea())); chunkPosIter;
		 ++chunkPosIter) {
		groupKeyVec.push_back(*chunkPosIter);
	}
	std::sort(groupKeyVec.begin(), groupKeyVec.end());
	std::string groupKey;
	for (Position pos : groupKeyVec) {
		groupKey += pos.toString();
	}
	LogicGroup& logicGroup = logicGroups[groupKey];
	bool newGroup = logicGroup.getRenderedBlocks().size() == 0 && logicGroup.getRenderedWires().size() == 0;
	logicGroup.getRenderedBlocks().emplace(
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
	if (newGroup) {
		for (auto chunkPosIter = getChunk(position).iterTo(getChunk(position + (orientation * blockRenderData->size).getLargestVectorInArea())); chunkPosIter;
			 ++chunkPosIter) {
			chunkToGroups[*chunkPosIter].insert(&logicGroup);
		}
	}
	logicGroupsToUpdate.insert(&logicGroup);
	blockTypesCount[blockRenderDataId] = 1; // for now just make it dirty. Should keep it from remaking everything.
}

void VulkanChunker::removeBlock(Position position) {
	Position chunkPos = getChunk(position);
	auto groupsAtChunkIter = chunkToGroups.find(chunkPos);
	if (groupsAtChunkIter == chunkToGroups.end()) {
		logError("Failed to remove block. No logic groups at chunk pos {}.", "VulkanChunker", chunkPos);
		return;
	}
	for (LogicGroup* logicGroup : groupsAtChunkIter->second) {
		auto blockIter = logicGroup->getRenderedBlocks().find(position);
		if (blockIter == logicGroup->getRenderedBlocks().end()) continue;
		const BlockRenderDataManager::BlockRenderData* blockRenderData = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(blockIter->second.blockRenderDataId);
		logicGroup->getRenderedBlocks().erase(blockIter);
		logicGroupsToUpdate.insert(logicGroup);
		return;
	}
	logError("Failed to remove block. No block at pos {} found in any logic group.", "VulkanChunker", position);
}

void VulkanChunker::moveBlock(Position curPos, Position newPos, Orientation newOrientation) {
	Position curChunkPos = getChunk(curPos);
	auto groupsAtCurChunkIter = chunkToGroups.find(curChunkPos);
	if (groupsAtCurChunkIter == chunkToGroups.end()) {
		logError("Failed to move block. No logic groups at chunk pos {}.", "VulkanChunker", curChunkPos);
		return;
	}
	for (LogicGroup* logicGroup : groupsAtCurChunkIter->second) {
		auto blockIter = logicGroup->getRenderedBlocks().find(curPos);
		if (blockIter == logicGroup->getRenderedBlocks().end()) continue;
		if (blockIter->second.orientation == newOrientation && curPos == newPos) return;
		BlockRenderDataId blockRenderDataId = blockIter->second.blockRenderDataId;
		removeBlock(curPos);
		addBlock(blockRenderDataId, newPos, newOrientation);
		return;
	}
	logError("Failed to move block. No block at pos {} found in any logic group.", "VulkanChunker", curPos);
}

void VulkanChunker::addWire(std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets) {
	FPosition visualWireStart = points.first.free() + socketOffsets.first;
	FPosition visualWireEnd = points.second.free() + socketOffsets.second;
	std::vector<ChunkIntersection> intersections = getChunkIntersections(visualWireStart, visualWireEnd);
	std::vector<Position> groupKeyVec;
	groupKeyVec.reserve(intersections.size());
	for (ChunkIntersection& intersection : intersections) {
		groupKeyVec.push_back(intersection.chunk);
	}
	std::sort(groupKeyVec.begin(), groupKeyVec.end());
	std::string groupKey;
	for (Position pos : groupKeyVec) {
		groupKey += pos.toString();
	}
	LogicGroup& logicGroup = logicGroups[groupKey];
	bool newGroup = logicGroup.getRenderedBlocks().size() == 0 && logicGroup.getRenderedWires().size() == 0;
	logicGroup.getRenderedWires()[points] = { visualWireStart, visualWireEnd };
	logicGroupsToUpdate.insert(&logicGroup);

	if (newGroup) {
		for (const ChunkIntersection& intersection : intersections) {
			chunkToGroups[intersection.chunk].insert(&logicGroup);
		}
	}
}

void VulkanChunker::removeWire(std::pair<Position, Position> points) {
	Position chunkPos = getChunk(points.first);
	auto groupsAtChunkIter = chunkToGroups.find(chunkPos);
	if (groupsAtChunkIter == chunkToGroups.end()) {
		logError("Failed to remove wire. No logic groups at chunk pos {}.", "VulkanChunker", chunkPos);
		return;
	}
	for (LogicGroup* logicGroup : groupsAtChunkIter->second) {
		auto wireIter = logicGroup->getRenderedWires().find(points);
		if (wireIter == logicGroup->getRenderedWires().end()) {
			std::swap(points.first, points.second);
			wireIter = logicGroup->getRenderedWires().find(points);
			if (wireIter == logicGroup->getRenderedWires().end()) continue;
		};
		logicGroup->getRenderedWires().erase(wireIter);
		logicGroupsToUpdate.insert(logicGroup);
		return;
	}
	logError("Failed to remove wire. No wire with points ({}, {}) found in any logic group.", "VulkanChunker", points.first, points.second);
}

void VulkanChunker::reset() {
	std::lock_guard<std::mutex> lock(mux);

	logicGroups.clear();
	chunkToGroups.clear();
	logicGroupsToUpdate.clear();
	blockTypesCount.clear();
}

void VulkanChunker::regenerateAllChunksWithBlock(BlockRenderDataId blockRenderDataId) {
	std::lock_guard<std::mutex> lock(mux);
	auto iter = blockTypesCount.find(blockRenderDataId);
	if (iter == blockTypesCount.end()) return;
	if (iter->second == 0) return;

	const BlockRenderDataManager::BlockRenderData* blockRenderData = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(blockRenderDataId);

	for (std::pair<const std::string, LogicGroup>& logicGroup : logicGroups) {
		bool foundType = false;
		for (std::pair<const Position, RenderedBlock>& block : logicGroup.second.getRenderedBlocks()) {
			if (block.second.blockRenderDataId == blockRenderDataId) {
				foundType = true;
				block.second.size = (block.second.orientation * blockRenderData->size).free();
				block.second.textureIndex = blockRenderData->blockTextureCords.textureLayer;
				block.second.textureOrigin = blockRenderData->blockTextureCords.textureOriginUV;
				block.second.textureSize = blockRenderData->blockTextureCords.textureSizeUV;
				block.second.textureStateStep = blockRenderData->blockTextureCords.textureUVStateStep;
			}
		}
		if (foundType) logicGroup.second.rebuildAllocation(device, simulator, address);
	}
}

void VulkanChunker::updateSimulatorIds(const std::vector<SimulatorMappingUpdate>& simulatorMappingUpdates) {
	for (const SimulatorMappingUpdate& simulatorMappingUpdate : simulatorMappingUpdates) {
		const SimulatorStateIndexVecVariant& simulatorIds = simulatorMappingUpdate.simulatorIds;
		Position chunkPos = getChunk(simulatorMappingUpdate.position);
		auto groupsAtChunkIter = chunkToGroups.find(chunkPos);
		if (groupsAtChunkIter == chunkToGroups.end()) {
			continue;
		}
		if (simulatorMappingUpdate.virtualConnectionId) {
			for (LogicGroup* logicGroup : groupsAtChunkIter->second) {
				std::optional<std::shared_ptr<VulkanLogicAllocation>> vulkanLogicAllocation = logicGroup->getAllocation();
				if (!vulkanLogicAllocation) continue;
				auto blockIter = logicGroup->getRenderedBlocks().find(simulatorMappingUpdate.position);
				if (blockIter == logicGroup->getRenderedBlocks().end()) {
					// logError("Could not find block pos {} in renderedBlocks.", "VulkanChunker::updateSimulatorIds", simulatorMappingUpdate.position);
					continue;
				}
				if (simulatorMappingUpdate.virtualConnectionId == MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(blockIter->second.blockRenderDataId)->textureVirtualConnection) {
					auto iter = vulkanLogicAllocation.value()->getBlockStateIndex().find(simulatorMappingUpdate.position);
					if (iter == vulkanLogicAllocation.value()->getBlockStateIndex().end()) continue;
					if (std::holds_alternative<std::vector<simulator_state_reference>>(simulatorIds)) {
						vulkanLogicAllocation.value()->getStateSimulatorIds()[iter->second] = 0;
					} else {
						vulkanLogicAllocation.value()->getStateSimulatorIds()[iter->second] = std::get<simulator_state_reference>(simulatorIds);
					}
					logicGroupsToUpdate.insert(logicGroup);
					break;
				}
			}
		} else {
			for (LogicGroup* logicGroup : groupsAtChunkIter->second) {
				std::optional<std::shared_ptr<VulkanLogicAllocation>> vulkanLogicAllocation = logicGroup->getAllocation();
				if (!vulkanLogicAllocation) continue;
				auto portStateIter = vulkanLogicAllocation.value()->getPortStateIndex().find(simulatorMappingUpdate.position);
				if (portStateIter == vulkanLogicAllocation.value()->getPortStateIndex().end()) continue;

				const PortStateRange& range = portStateIter->second;
				if (!range.isValid()) continue;

				std::vector<simulator_state_reference>& chunkStateSimulatorIds = vulkanLogicAllocation.value()->getStateSimulatorIds();
				if (std::holds_alternative<std::vector<simulator_state_reference>>(simulatorIds)) {
					const std::vector<simulator_state_reference>& wireSimulatorIds = std::get<std::vector<simulator_state_reference>>(simulatorIds);
					uint32_t laneCount = wireSimulatorIds.size();
					if (laneCount != range.laneCount) {
						logicGroupsToUpdate.insert(logicGroup);
					} else {
						for (uint32_t lane = 0; lane < laneCount; lane++) {
							chunkStateSimulatorIds[range.baseIndex + lane] = wireSimulatorIds[lane];
						}
					}
				} else {
					if (1 != range.laneCount) {
						logicGroupsToUpdate.insert(logicGroup);
					} else {
						chunkStateSimulatorIds[range.baseIndex] = std::get<simulator_state_reference>(simulatorIds);
					}
				}
			}
		}
	}
}

void VulkanChunker::setSimulator(const EvalLogicSimulator* simulator, const Address& address) {
	if (this->simulator) {
		this->simulator->disconnectListener(this);
	}
	this->address = address;
	this->simulator = simulator;
	if (simulator) {
		simulator->connectListener(this, address, std::bind(&VulkanChunker::updateSimulatorIds, this, std::placeholders::_1));
	}
	for (auto& pair : logicGroups) {
		pair.second.rebuildAllocation(device, simulator, address);
	}
}

std::vector<std::shared_ptr<VulkanLogicAllocation>> VulkanChunker::getAllocations(Position min, Position max) {
	std::lock_guard<std::mutex> lock(mux);

	// get chunk bounds with padding for large blocks (this will technically goof if there are blocks larger than chunk size)
	min = getChunk(min - Vector(1));
	max = getChunk(max + Vector(1));

	// go through each chunk in view and collect it if it exists and has an allocation
	std::set<LogicGroup*> logicGroupsToRender;
	for (coordinate_t chunkX = min.x; chunkX <= max.x; chunkX += CHUNK_SIZE) {
		for (coordinate_t chunkY = min.y; chunkY <= max.y; chunkY += CHUNK_SIZE) {
			auto groupsAtChunkIter = chunkToGroups.find({ chunkX, chunkY });
			if (groupsAtChunkIter == chunkToGroups.end()) continue;
			for (LogicGroup* group : groupsAtChunkIter->second) {
				if (group->getAllocation().has_value()) logicGroupsToRender.insert(group);
			}
		}
	}
	std::vector<std::shared_ptr<VulkanLogicAllocation>> seen;
	for (LogicGroup* group : logicGroupsToRender) {
		seen.push_back(group->getAllocation().value());
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
