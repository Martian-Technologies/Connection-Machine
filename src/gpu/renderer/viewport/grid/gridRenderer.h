#ifndef gridRenderer_h
#define gridRenderer_h

#include "gpu/renderer/frameManager.h"
#include "gpu/abstractions/vulkanPipeline.h"

class Evaluator;

struct GridPushConstants {
	glm::mat4 iMvp;
	alignas(16) glm::vec3 background;
	float gridFade;
	glm::vec4 gradientColor;
};

class GridRenderer {
public:
	void init(VulkanDevice* device, VkRenderPass& renderPass);
	void cleanup();

	void render(Frame& frame, const glm::mat4& viewMatrix, float viewScale, Evaluator* evaluator);

private:
	Pipeline pipeline;

	// push constant data
	size_t fragmentPushOffset = offsetof(GridPushConstants, background);
};

#endif
