#ifndef gridRenderer_h
#define gridRenderer_h

#include "gpu/renderer/frameManager.h"
#include "gpu/abstractions/vulkanPipeline.h"

struct GridPushConstants {
	glm::mat4 iMvp;
	alignas(16) glm::vec3 background;
	float gridFade;
	glm::vec4 gradientColor;
};

class EvalLogicSimulator;

class GridRenderer {
public:
	void init(VulkanDevice& device, vk::RenderPass renderPass);
	void cleanup();

	void render(Frame& frame, const glm::mat4& viewMatrix, float viewScale, const EvalLogicSimulator* simulator);

private:
	Pipeline pipeline;

	size_t fragmentPushOffset = offsetof(GridPushConstants, background);
};

#endif
