#include "gridRenderer.h"

#include "computerAPI/fileLoader.h"
#include "computerAPI/directoryManager.h"
#include "gpu/abstractions/vulkanShader.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"

void GridRenderer::init(VulkanDevice& device, vk::RenderPass renderPass) {
	vk::UniqueShaderModule gridVertShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/grid.vert.spv"));
	vk::UniqueShaderModule gridFragShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/grid.frag.spv"));

	PipelineInformation gridPipelineInfo{};
	gridPipelineInfo.vertShader = gridVertShader.get();
	gridPipelineInfo.fragShader = gridFragShader.get();
	gridPipelineInfo.renderPass = renderPass;
	gridPipelineInfo.frontFace = vk::FrontFace::eClockwise;
	gridPipelineInfo.pushConstants.push_back({vk::ShaderStageFlagBits::eVertex, fragmentPushOffset});
	gridPipelineInfo.pushConstants.push_back({vk::ShaderStageFlagBits::eFragment, sizeof(GridPushConstants) - fragmentPushOffset});
	gridPipelineInfo.sampleCount = device.getMaxUsableSampleCount();
	pipeline.init(device, gridPipelineInfo);
}

void GridRenderer::cleanup() {
	pipeline.cleanup();
}

constexpr float gridFadeOutDistance = 160.0f;
constexpr float gridFadeOutWidth = 60.0f;

void GridRenderer::render(Frame& frame, const glm::mat4& viewMatrix, float viewScale, const EvalLogicSimulator* simulator) {
	// calculate grid fade num
	float gridFade = std::clamp(1.0f - ((viewScale - gridFadeOutDistance) * (1.0f / gridFadeOutWidth)), 0.0f, 1.0f);
	// invert the view matrix to get the right coordinates for the grid in the shader
	constexpr float baseBackground = 0.69f * 1.3478f;
	const float visibilityScale = 0.6f + 0.4f * (simulator ? 1.0f : 0.0f);
	const glm::vec3 backgroundColor(baseBackground * visibilityScale);
	glm::vec4 gradientColor(0.0f, 0.0f, 0.0f, 0.1f);
	// 174, 164, 249
	if (simulator && simulator->isViewingReplay()) {
		gradientColor = glm::vec4(124.0f / 255.0f, 114.0f / 255.0f, 229.0f / 255.0f, 0.2f);
	}

	GridPushConstants pushConstants {
		glm::inverse(viewMatrix),
		backgroundColor,
		gridFade,
		gradientColor
	};

	frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getHandle());

	pipeline.cmdPushConstants(frame.mainCommandBuffer, &pushConstants);

	frame.mainCommandBuffer.draw(6, 1, 0, 0);
}
