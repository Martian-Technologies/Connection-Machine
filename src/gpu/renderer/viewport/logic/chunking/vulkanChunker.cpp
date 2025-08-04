#include "vulkanChunker.h"

#ifdef TRACY_PROFILER
	#include <tracy/Tracy.hpp>
#endif

#include "../sharedLogic/logicRenderingUtils.h"
#include "backend/position/position.h"
#include "logging/logging.h"
#include <mutex>

const int CHUNK_SIZE = 64;
cord_t getChunk(cord_t in) {
	return std::floor((float)in / (float)CHUNK_SIZE) * (int)CHUNK_SIZE;
}
Position getChunk(Position in) {
	in.x = getChunk(in.x);
	in.y = getChunk(in.y);
	return in;
}

#include "gpu/renderer/viewport/blockTextureManager.h"

// VulkanChunkAllocation
// =========================================================================================================

VulkanChunkAllocation::VulkanChunkAllocation(VulkanDevice* device, RenderedBlocks& blocks, RenderedWires& wires) {
	// TODO - should pre-allocate buffers with size and pool them
	// TODO - maybe should use smaller size coordinates with one big offset

	// Maps "state position" to a position in the address list so that multiple objects can index the same array
	std::unordered_map<Position, size_t> posToAddressIdx;

	// Generate block instances
	if (blocks.size() > 0) {
		std::vector<BlockInstance> blockInstances;
		blockInstances.reserve(blocks.size());
		for (const auto& block : blocks) {
			Position blockPosition = block.first;
			Vec2 uvOrigin = device->getBlockTextureManager()->getTileset().getTopLeftUV(block.second.blockType + 1 + (block.second.blockType >= BlockType::CUSTOM), 0);

			BlockInstance instance;
			instance.pos = glm::vec2(blockPosition.x, blockPosition.y);
			instance.sizeX = block.second.size.dx;
			instance.sizeY = block.second.size.dy;
			instance.rotation = block.second.rotation;
			instance.texX = uvOrigin.x;

			blockInstances.push_back(instance);

			// blocks are added to state array
			posToAddressIdx[block.second.statePosition] = statePositions.size();
			statePositions.push_back(block.second.statePosition);
		}

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
			auto itr = posToAddressIdx.find(wire.first.first);
			// check if wire state position is already in the state array
			if (itr != posToAddressIdx.end()) {
				stateIdx = itr->second;
			} else {
				// add address to state buffer
				stateIdx = statePositions.size();
				posToAddressIdx[wire.first.first] = stateIdx;
				statePositions.push_back(wire.first.first);
			}

			WireInstance instance;
			instance.pointA = glm::vec2(wire.second.start.x, wire.second.start.y);
			instance.pointB = glm::vec2(wire.second.end.x, wire.second.end.y);
			instance.stateIndex = stateIdx;

			wireInstances.push_back(instance);
		}

		// upload wire vertices
		numWireInstances = wireInstances.size();
		size_t wireBufferSize = sizeof(WireInstance) * numWireInstances;
		wireBuffer = createBuffer(device, wireBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		vmaCopyMemoryToAllocation(device->getAllocator(), wireInstances.data(), wireBuffer->allocation, 0, wireBufferSize);
	}

	if (!statePositions.empty()) {
		// Create state buffer
		size_t stateBufferSize = statePositions.size() * sizeof(logic_state_t);
		stateBuffer.emplace();
		stateBuffer->init(device, stateBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		std::vector<logic_state_t> defaultStates(statePositions.size(), logic_state_t::HIGH);
	}
}

VulkanChunkAllocation::~VulkanChunkAllocation() {
	if (blockBuffer.has_value()) destroyBuffer(blockBuffer.value());
	if (wireBuffer.has_value()) destroyBuffer(wireBuffer.value());
	if (stateBuffer.has_value()) stateBuffer->cleanup();
}

// ChunkChain
// =========================================================================================================

void Chunk::rebuildAllocation(VulkanDevice* device) {
	if (!blocks.empty() || !wires.empty()) { // if we have data to upload
		// allocate new date
		std::shared_ptr<VulkanChunkAllocation> newAllocation = std::make_unique<VulkanChunkAllocation>(device, blocks, wires);
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

	allocationDirty = false;
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
	for (const Position& chunk : chunksToUpdate) {
		chunks[chunk].rebuildAllocation(device);
	}
	chunksToUpdate.clear();

	mux.unlock();
}

void VulkanChunker::addBlock(BlockType type, Position position, Vector size, Rotation rotation, Position statePosition) {
	Position chunk = getChunk(position);
	chunks[chunk].getRenderedBlocks()[position] = RenderedBlock(type, rotation, size.free(), statePosition);
	chunksToUpdate.insert(chunk);
}

void VulkanChunker::removeBlock(Position position) {
	Position chunk = getChunk(position);
	chunks[chunk].getRenderedBlocks().erase(position);
	chunksToUpdate.insert(chunk);
}

void VulkanChunker::moveBlock(Position curPos, Position newPos, Rotation newRotation, Vector newSize) {
	Position curChunk = getChunk(curPos);
	Position newChunk = getChunk(newPos);

	auto itr = chunks[curChunk].getRenderedBlocks().find(curPos);
	if (itr != chunks[curChunk].getRenderedBlocks().end()) {
		RenderedBlock block = itr->second;
		block.statePosition = newPos + rotateVectorWithArea(block.statePosition - curPos, block.size.snap(), subRotations(newRotation, block.rotation));
		block.rotation = newRotation;
		block.size = newSize.free();
		chunks[curChunk].getRenderedBlocks().erase(itr);
		chunks[newChunk].getRenderedBlocks()[newPos] = block;

		chunksToUpdate.insert(curChunk);
		chunksToUpdate.insert(newChunk);
	} else {
		logError("Could not move block from ({}, {}) to ({}, {}) because it could not be found", "Vulkan", curPos.x, curPos.y, newPos.x, newPos.y);
	}
}

void VulkanChunker::addWire(std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets) {
	FPosition a = points.first.free() + socketOffsets.first;
	FPosition b = points.second.free() + socketOffsets.second;
	for (const ChunkIntersection& intersection : getChunkIntersections(a, b)) {
		chunks[intersection.chunk].getRenderedWires()[points] = { intersection.start, intersection.end };
		chunksUnderWire[points].push_back(intersection.chunk);
		chunksToUpdate.insert(intersection.chunk);
	}
}

void VulkanChunker::removeWire(std::pair<Position, Position> points) {
	auto itr = chunksUnderWire.find(points);
	if (itr == chunksUnderWire.end()) {
		logError("Could not find wire ({}, {}) to remove", "VulkanChunker", points.first, points.second);
		return;
	}
	for (Position p : itr->second) {
		chunks[p].getRenderedWires().erase(points);
		chunksToUpdate.insert(p);
	}
	chunksUnderWire.erase(itr);
}

void VulkanChunker::reset() {
	std::lock_guard<std::mutex> lock(mux);

	chunks.clear();
	chunksUnderWire.clear();
}

std::vector<std::shared_ptr<VulkanChunkAllocation>> VulkanChunker::getAllocations(Position min, Position max) {
	std::lock_guard<std::mutex> lock(mux);

	// get chunk bounds with padding for large blocks (this will technically goof if there are blocks larger than chunk size)
	min = getChunk(min) - Vector(CHUNK_SIZE, CHUNK_SIZE);
	max = getChunk(max) + Vector(CHUNK_SIZE, CHUNK_SIZE);

	// go through each chunk in view and collect it if it exists and has an allocation
	std::vector<std::shared_ptr<VulkanChunkAllocation>> seen;
	for (cord_t chunkX = min.x; chunkX <= max.x; chunkX += CHUNK_SIZE) {
		for (cord_t chunkY = min.y; chunkY <= max.y; chunkY += CHUNK_SIZE) {
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
	FVector rayUnitStepSize = FVector( sqrt(1 + (dir.dy / dir.dx) * (dir.dy / dir.dx)), sqrt(1 + (dir.dx / dir.dy) * (dir.dx / dir.dy)) ) * CHUNK_SIZE;

	// starting conditions
	FVector rayLength1D;
	Vector step;
	if (dir.dx < 0)
	{
		step.dx = -CHUNK_SIZE;
		rayLength1D.dx = ((start.x - float(chunk.x)) / float(CHUNK_SIZE)) * rayUnitStepSize.dx;
	} else {
		step.dx = CHUNK_SIZE;
		rayLength1D.dx = ((float(chunk.x + CHUNK_SIZE) - start.x) / float(CHUNK_SIZE)) * rayUnitStepSize.dx;
	}
	if (dir.dy < 0)
	{
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
		if (rayLength1D.dx < rayLength1D.dy)
		{
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
		intersections.push_back({oldChunk, currentPos, newPos});
		currentPos = newPos;
	}

	return intersections;
}

