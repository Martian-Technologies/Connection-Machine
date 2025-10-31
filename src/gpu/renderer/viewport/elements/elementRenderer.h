#ifndef elementRenderer_h
#define elementRenderer_h

#include "backend/position/position.h"
#include "gpu/abstractions/vulkanPipeline.h"
#include "gpu/abstractions/vulkanBuffer.h"
#include "gpu/renderer/frameManager.h"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>

struct BlockPreviewPushConstant {
    alignas(16) glm::mat4 mvp;
    alignas(8)  glm::vec2 position;
    alignas(8)  glm::vec2 size;
	alignas(4)  uint32_t orientation;
	alignas(4)  uint32_t texLayer;
	alignas(8)  glm::vec2 texPos;
	alignas(8)  glm::vec2 texSize;
};
struct BlockPreviewRenderData {
	glm::vec2 position;
	glm::vec2 size;
	Orientation orientation;
	uint32_t textureIndex;
	glm::vec2 texPos;
	glm::vec2 texSize;
};
struct BlockPreviewRenderBatch {
    AllocatedBuffer buffer;
    uint32_t instanceCount;
    glm::vec2 offset;
};
struct BoxSelectionPushConstant {
    alignas(16) glm::mat4 mvp;
    alignas(8)  glm::vec2 position;
    alignas(8)  glm::vec2 size;
    alignas(4)  uint32_t state;
};
struct BoxSelectionRenderData {
	enum BoxSelectionState : uint32_t {
		Normal,
		Inverted,
		Special
	};
	glm::vec2 topLeft;
	glm::vec2 size;
	BoxSelectionState state;
};

struct ConnectionPreviewPushConstant {
	alignas(16) glm::mat4 mvp;
    alignas(8)  glm::vec2 pointA;
    alignas(8)  glm::vec2 pointB;
};
struct ConnectionPreviewRenderData {
	glm::vec2 pointA;
	glm::vec2 pointB;
};

struct ArrowPushConstant {
	alignas(16) glm::mat4 mvp;
    alignas(8)  glm::vec2 pointA;
    alignas(8)  glm::vec2 pointB;
	alignas(4)  uint32_t depth;
};
struct ArrowCirclePushConstant {
	alignas(16) glm::mat4 mvp;
    alignas(8)  glm::vec2 topLeft;
	alignas(4)  uint32_t depth;
};
struct ArrowRenderData {
	Position pointA;
	Position pointB;
	uint32_t depth;
};


struct BlockInstance {
	glm::vec2 pos;
	uint32_t sizeX;
	uint32_t sizeY;
	uint32_t texLayer;
	glm::vec2 texPos;
	glm::vec2 texSize;
	uint32_t orientation;
	glm::vec2 stateStep;

	inline static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(BlockInstance);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        return bindingDescriptions;
    }

	inline static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(7);

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
		attributeDescriptions[2].format = VK_FORMAT_R32_UINT;
		attributeDescriptions[2].offset = offsetof(BlockInstance, texLayer);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(BlockInstance, texPos);

		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 4;
		attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[4].offset = offsetof(BlockInstance, texSize);

		attributeDescriptions[5].binding = 0;
		attributeDescriptions[5].location = 5;
		attributeDescriptions[5].format = VK_FORMAT_R32_UINT;
		attributeDescriptions[5].offset = offsetof(BlockInstance, orientation);

		attributeDescriptions[6].binding = 0;
		attributeDescriptions[6].location = 6;
		attributeDescriptions[6].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[6].offset = offsetof(BlockInstance, stateStep);

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

struct ChunkPushConstants {
	glm::mat4 mvp;
};

class ElementRenderer {
public:
	void init(VulkanDevice* device, VkRenderPass& renderPass);
	void cleanup();

	void renderBlockPreviews(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BlockPreviewRenderData>& blockPreviews);
	
	void renderBlockPreviewBatches(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BlockPreviewRenderBatch>& blockPreviewBatches);	

	void renderBoxSelections(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BoxSelectionRenderData>& boxSelections);

	void renderConnectionPreviews(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ConnectionPreviewRenderData>& connectionPreviews);

	void renderArrows(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ArrowRenderData>& arrows);

private:
	Pipeline blockPipeline;
	Pipeline wirePipeline;
	Pipeline blockPreviewPipeline;
	Pipeline boxSelectionPipeline;
	Pipeline connectionPreviewPipeline;
	Pipeline arrowCirclePipeline;
	Pipeline arrowPipeline;
	VkDescriptorSetLayout stateBufferDescriptorSetLayout;

	VulkanDevice* device = nullptr;
};

#endif
