#include "elementRenderer.h"

#include <cstddef>
#include <glm/ext/vector_float2.hpp>

#include "computerAPI/directoryManager.h"
#include "computerAPI/fileLoader.h"
#include "gpu/abstractions/vulkanShader.h"
#include "gpu/renderer/viewport/blockTextureManager.h"

void ElementRenderer::init(VulkanDevice& device, vk::RenderPass renderPass) {
	this->device = &device;

	// block preview
	vk::UniqueShaderModule blockPreviewVertShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/blockPreview.vert.spv"));
	vk::UniqueShaderModule blockPreviewFragShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/blockPreview.frag.spv"));

	PipelineInformation blockPreviewPipelineInfo{};
	blockPreviewPipelineInfo.vertShader = blockPreviewVertShader.get();
	blockPreviewPipelineInfo.fragShader = blockPreviewFragShader.get();
	blockPreviewPipelineInfo.renderPass = renderPass;
	blockPreviewPipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eVertex, sizeof(BlockPreviewPushConstant) });
	blockPreviewPipelineInfo.descriptorSets.push_back(device.getBlockTextureManager().getDescriptorLayout());
	blockPreviewPipelineInfo.sampleCount = device.getMaxUsableSampleCount();
	blockPreviewPipeline.init(device, blockPreviewPipelineInfo);

	// box selection
	vk::UniqueShaderModule boxSelectionVertShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/boxSelection.vert.spv"));
	vk::UniqueShaderModule boxSelectionFragShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/boxSelection.frag.spv"));

	PipelineInformation boxSelectionPipelineInfo{};
	boxSelectionPipelineInfo.vertShader = boxSelectionVertShader.get();
	boxSelectionPipelineInfo.fragShader = boxSelectionFragShader.get();
	boxSelectionPipelineInfo.renderPass = renderPass;
	boxSelectionPipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eVertex, offsetof(BoxSelectionPushConstant, state) });
	boxSelectionPipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eFragment, sizeof(uint32_t) });
	boxSelectionPipelineInfo.sampleCount = device.getMaxUsableSampleCount();
	boxSelectionPipeline.init(device, boxSelectionPipelineInfo);

	// connection preview
	vk::UniqueShaderModule connectionPreviewVertShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/connectionPreview.vert.spv"));
	vk::UniqueShaderModule connectionPreviewFragShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/connectionPreview.frag.spv"));

	PipelineInformation connectionPreviewPipelineInfo{};
	connectionPreviewPipelineInfo.vertShader = connectionPreviewVertShader.get();
	connectionPreviewPipelineInfo.fragShader = connectionPreviewFragShader.get();
	connectionPreviewPipelineInfo.renderPass = renderPass;
	connectionPreviewPipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eVertex, sizeof(ConnectionPreviewPushConstant) });
	connectionPreviewPipelineInfo.sampleCount = device.getMaxUsableSampleCount();
	connectionPreviewPipeline.init(device, connectionPreviewPipelineInfo);

	// arrow circle
	vk::UniqueShaderModule arrowCircleVertShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrowCircle.vert.spv"));
	vk::UniqueShaderModule arrowCircleFragShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrowCircle.frag.spv"));

	PipelineInformation arrowCirclePipelineInfo{};
	arrowCirclePipelineInfo.vertShader = arrowCircleVertShader.get();
	arrowCirclePipelineInfo.fragShader = arrowCircleFragShader.get();
	arrowCirclePipelineInfo.renderPass = renderPass;
	arrowCirclePipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eVertex, offsetof(ArrowCirclePushConstant, depth) });
	arrowCirclePipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eFragment, sizeof(uint32_t) });
	arrowCirclePipelineInfo.sampleCount = device.getMaxUsableSampleCount();
	arrowCirclePipeline.init(device, arrowCirclePipelineInfo);

	// arrow
	vk::UniqueShaderModule arrowVertShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrow.vert.spv"));
	vk::UniqueShaderModule arrowFragShader = createShaderModule(device.getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrow.frag.spv"));

	PipelineInformation arrowPipelineInfo{};
	arrowPipelineInfo.vertShader = arrowVertShader.get();
	arrowPipelineInfo.fragShader = arrowFragShader.get();
	arrowPipelineInfo.renderPass = renderPass;
	arrowPipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eVertex, offsetof(ArrowPushConstant, depth) });
	arrowPipelineInfo.pushConstants.push_back({ vk::ShaderStageFlagBits::eFragment, sizeof(uint32_t) });
	arrowPipelineInfo.sampleCount = device.getMaxUsableSampleCount();
	arrowPipeline.init(device, arrowPipelineInfo);
}

void ElementRenderer::cleanup() {
	arrowPipeline.cleanup();
	arrowCirclePipeline.cleanup();
	connectionPreviewPipeline.cleanup();
	boxSelectionPipeline.cleanup();
	blockPreviewPipeline.cleanup();
}

void ElementRenderer::renderBlockPreviews(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BlockPreviewRenderData>& blockPreviews) {
	if (!blockPreviews.empty()) {
		frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, blockPreviewPipeline.getHandle());
		std::shared_ptr<BlockTextureArray> blockTexture = device->getBlockTextureManager().getTextureArray();
		frame.lifetime.push(blockTexture);
		frame.mainCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blockPreviewPipeline.getLayout(), 0, blockTexture->descriptor, nullptr);

		BlockPreviewPushConstant blockPreviewConstant;
		blockPreviewConstant.mvp = viewMatrix;
		for (const BlockPreviewRenderData& preview : blockPreviews) {
			blockPreviewConstant.position = preview.position;
			blockPreviewConstant.size = preview.size;
			blockPreviewConstant.orientation = preview.orientation.rotation + 4 * preview.orientation.flipped;
			blockPreviewConstant.texLayer = preview.textureIndex;
			blockPreviewConstant.texPos = preview.texPos;
			blockPreviewConstant.texSize = preview.texSize;

			blockPreviewPipeline.cmdPushConstants(frame.mainCommandBuffer, &blockPreviewConstant);
			frame.mainCommandBuffer.draw(6, 1, 0, 0);
		}
	}
}

void ElementRenderer::renderBoxSelections(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BoxSelectionRenderData>& boxSelections) {
	if (!boxSelections.empty()) {
		frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, boxSelectionPipeline.getHandle());

		BoxSelectionPushConstant boxSelectionConstant;
		boxSelectionConstant.mvp = viewMatrix;

		for (const BoxSelectionRenderData& boxSelection : boxSelections) {
			boxSelectionConstant.position = boxSelection.topLeft;
			boxSelectionConstant.size = boxSelection.size;
			boxSelectionConstant.state = boxSelection.state;

			boxSelectionPipeline.cmdPushConstants(frame.mainCommandBuffer, &boxSelectionConstant);
			frame.mainCommandBuffer.draw(6, 1, 0, 0);
		}
	}
}

void ElementRenderer::renderConnectionPreviews(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ConnectionPreviewRenderData>& connectionPreviews) {
	if (!connectionPreviews.empty()) {
		frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, connectionPreviewPipeline.getHandle());

		ConnectionPreviewPushConstant connectionPreviewConstant;
		connectionPreviewConstant.mvp = viewMatrix;

		for (const ConnectionPreviewRenderData& connectionPreview : connectionPreviews) {
			connectionPreviewConstant.pointA = connectionPreview.pointA;
			connectionPreviewConstant.pointB = connectionPreview.pointB;

			connectionPreviewPipeline.cmdPushConstants(frame.mainCommandBuffer, &connectionPreviewConstant);
			frame.mainCommandBuffer.draw(6, 1, 0, 0);
		}
	}
}

void ElementRenderer::renderArrows(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ArrowRenderData>& arrows) {
	if (!arrows.empty()) {
		frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, arrowCirclePipeline.getHandle());
		ArrowCirclePushConstant arrowCircleConstant;
		arrowCircleConstant.mvp = viewMatrix;
		for (const ArrowRenderData& arrow : arrows) {
			if (arrow.pointA != arrow.pointB) continue;

			arrowCircleConstant.topLeft = glm::vec2(arrow.pointA.x, arrow.pointA.y);
			arrowCircleConstant.depth = arrow.depth;

			arrowCirclePipeline.cmdPushConstants(frame.mainCommandBuffer, &arrowCircleConstant);
			frame.mainCommandBuffer.draw(6, 1, 0, 0);
		}

		frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, arrowPipeline.getHandle());
		ArrowPushConstant arrowConstant;
		arrowConstant.mvp = viewMatrix;
		for (const ArrowRenderData& arrow : arrows) {
			if (arrow.pointA == arrow.pointB) continue;

			arrowConstant.pointA = glm::vec2(arrow.pointA.x + 0.5f, arrow.pointA.y + 0.5f);
			arrowConstant.pointB = glm::vec2(arrow.pointB.x + 0.5f, arrow.pointB.y + 0.5f);
			arrowConstant.depth = arrow.depth;

			arrowPipeline.cmdPushConstants(frame.mainCommandBuffer, &arrowConstant);
			frame.mainCommandBuffer.draw(9, 1, 0, 0);
		}
	}
}
