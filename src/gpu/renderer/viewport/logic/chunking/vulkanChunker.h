#ifndef vulkanChunker_h
#define vulkanChunker_h

#include <freetype/tttables.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <parallel_hashmap/phmap.h>

#include "backend/evaluator/evaluator.h"
#include "gpu/abstractions/vulkanBuffer.h"
#include "gpu/blockRenderDataManager.h"
#include "gpu/helper/nBuffer.h"

class SimulatorMappingUpdate;

// ====================================================================================================================

struct BlockInstance {
	glm::vec2 pos;
	uint32_t sizeX;
	uint32_t sizeY;
	float texX;
	uint32_t orientation;

	inline static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(BlockInstance);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        return bindingDescriptions;
    }

	inline static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(BlockInstance, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_UINT;
		attributeDescriptions[1].offset = offsetof(BlockInstance, sizeX);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(BlockInstance, texX);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
		attributeDescriptions[3].offset = offsetof(BlockInstance, orientation);

		return attributeDescriptions;
	}
};

struct WireInstance {
	glm::vec2 pointA;
	glm::vec2 pointB;
	uint32_t stateIndex;

	inline static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(WireInstance);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        return bindingDescriptions;
    }

	inline static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(WireInstance, pointA);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(WireInstance, pointB);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32_UINT;
		attributeDescriptions[2].offset = offsetof(WireInstance, stateIndex);

		return attributeDescriptions;
	}
};

// ====================================================================================================================
struct RenderedBlock {
	unsigned int textureIndex;
	Orientation orientation;
	FSize size;
	Position statePosition;
};

struct RenderedWire {
	FPosition start;
	FPosition end;
};

typedef phmap::flat_hash_map<Position, RenderedBlock> RenderedBlocks;
typedef phmap::flat_hash_map<std::pair<Position, Position>, RenderedWire> RenderedWires;

// TODO - maybe these should just be split into two different types
class VulkanChunkAllocation {
public:
	VulkanChunkAllocation(VulkanDevice* device, const RenderedBlocks& blocks, const RenderedWires& wires, const Evaluator* evaluator, const Address& address);
	~VulkanChunkAllocation();

	inline const std::optional<AllocatedBuffer>& getBlockBuffer() const { return blockBuffer; }
	inline uint32_t getNumBlockInstances() const { return numBlockInstances; }

	inline const std::optional<AllocatedBuffer>& getWireBuffer() const { return wireBuffer; }
	inline uint32_t getNumWireInstances() const { return numWireInstances; }

	inline std::optional<NBuffer>& getStateBuffer() { return stateBuffer; }

	inline std::vector<simulator_id_t>& getStateSimulatorIds() { return simulatorIds; }
	inline const phmap::flat_hash_map<Position, size_t>& getBlockStateIndex() const { return blockStateIndex; }
	inline const phmap::flat_hash_map<Position, size_t>& getPortStateIndex() const { return portStateIndex; }

	inline bool isAllocationComplete() const { return true; }

private:
	std::optional<AllocatedBuffer> blockBuffer;
	uint32_t numBlockInstances;

	std::optional<AllocatedBuffer> wireBuffer;
	uint32_t numWireInstances;

	std::optional<NBuffer> stateBuffer;
	VkDescriptorBufferInfo stateDescriptorBufferInfo;

	std::vector<simulator_id_t> simulatorIds;
	phmap::flat_hash_map<Position, size_t> blockStateIndex;
	phmap::flat_hash_map<Position, size_t> portStateIndex;
};

// ====================================================================================================================

class Chunk {
public:
	inline RenderedBlocks& getRenderedBlocks() { allocationDirty = true; return blocks; }
	inline RenderedWires& getRenderedWires() { allocationDirty = true; return wires; }
	void rebuildAllocation(VulkanDevice* device, const Evaluator* evaluator, const Address& address);

	std::optional<std::shared_ptr<VulkanChunkAllocation>> getAllocation();

private:
	void annihilateOrphanGBs();

private:
	RenderedBlocks blocks;
	RenderedWires wires;
	bool allocationDirty = false;

	std::optional<std::shared_ptr<VulkanChunkAllocation>> newestAllocation;
	std::optional<std::shared_ptr<VulkanChunkAllocation>> currentlyAllocating;
	std::vector<std::shared_ptr<VulkanChunkAllocation>> gbJail; // deleted chunks mid allocation go here
};

// ====================================================================================================================

struct ChunkIntersection {
	Position chunk;
	FPosition start;
	FPosition end;
};

class VulkanChunker {
public:
	VulkanChunker(VulkanDevice* device);
	~VulkanChunker();

	void startMakingEdits();
	void stopMakingEdits();
	void addBlock(BlockRenderDataId blockRenderDataId, Position position, Orientation orientation, Position statePosition);
	void removeBlock(Position position);
	void moveBlock(Position curPos, Position newPos, Orientation newOrientation);
	void addWire(std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets);
	void removeWire(std::pair<Position, Position> points);
	void reset();

	void updateSimulatorIds(const std::vector<SimulatorMappingUpdate>& simulatorMappingUpdates);
	void setEvaluator(Evaluator* evaluator, const Address& address);

	std::vector<std::shared_ptr<VulkanChunkAllocation>> getAllocations(Position min, Position max);

private:
	std::vector<ChunkIntersection> getChunkIntersections(FPosition start, FPosition end);
	std::vector<ChunkIntersection> getNeededChunkIntersections(FPosition start, FPosition end);

private:
	phmap::flat_hash_map<Position, Chunk> chunks;
	phmap::flat_hash_map<std::pair<Position, Position>, std::vector<Position>> chunksUnderWire;
	std::mutex mux; // sync can be relaxed in the future

	// while edits are being made
	std::unordered_set<Position> chunksToUpdate;

	VulkanDevice* device = nullptr;
	Evaluator* evaluator = nullptr;
	Address address;
};

#endif
