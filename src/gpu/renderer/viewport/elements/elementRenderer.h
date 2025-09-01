#ifndef elementRenderer_h
#define elementRenderer_h

#include "backend/position/position.h"
#include "gpu/abstractions/vulkanPipeline.h"
#include "gpu/renderer/frameManager.h"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>

struct BlockPreviewPushConstant {
    alignas(16) glm::mat4 mvp;
    alignas(8)  glm::vec2 position;
    alignas(8)  glm::vec2 size;
    alignas(4)  float uvOffsetX;
	alignas(4)  uint32_t orientation;
	alignas(4)  float uvCellSizeX;
	alignas(4)  float uvCellSizeY;
};
struct BlockPreviewRenderData {
	glm::vec2 position;
	glm::vec2 size;
	Orientation orientation;
	unsigned int textureIndex;
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

class ElementRenderer {
public:
	void init(VulkanDevice* device, VkRenderPass& renderPass);
	void cleanup();

	void renderBlockPreviews(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BlockPreviewRenderData>& blockPreviews);

	void renderBoxSelections(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BoxSelectionRenderData>& boxSelections);

	void renderConnectionPreviews(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ConnectionPreviewRenderData>& connectionPreviews);

	void renderArrows(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ArrowRenderData>& arrows);

private:
	Pipeline blockPreviewPipeline;
	Pipeline boxSelectionPipeline;
	Pipeline connectionPreviewPipeline;
	Pipeline arrowCirclePipeline;
	Pipeline arrowPipeline;

	VulkanDevice* device = nullptr;
};

#endif
