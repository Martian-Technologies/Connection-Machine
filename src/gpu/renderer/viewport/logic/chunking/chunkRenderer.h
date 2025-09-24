#ifndef chunkRenderer_h
#define chunkRenderer_h

#include "backend/evaluator/evaluator.h"
#include "gpu/renderer/frameManager.h"
#include "gpu/abstractions/vulkanPipeline.h"
#include "vulkanChunker.h"

struct ChunkPushConstants {
	glm::mat4 mvp;
	float uvCellSizeX;
	float uvCellSizeY;
};

class ChunkRenderer {
public:
	void init(VulkanDevice* device, VkRenderPass& renderPass);
	void cleanup();

	void render(Frame& frame, const glm::mat4& viewMatrix, Evaluator* evaluator, const Address& address, const std::vector<std::shared_ptr<VulkanChunkAllocation>>& chunks);

private:
	Pipeline blockPipeline;
	Pipeline wirePipeline;

	// state buffer descriptor layout
	VkDescriptorSetLayout stateBufferDescriptorSetLayout;

	// refs
	VulkanDevice* device = nullptr;
};

#endif
