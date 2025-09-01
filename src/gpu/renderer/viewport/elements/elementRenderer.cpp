#include "elementRenderer.h"

#include <cstddef>
#include <glm/ext/vector_float2.hpp>
#include <vulkan/vulkan_core.h>

#include "computerAPI/directoryManager.h"
#include "computerAPI/fileLoader.h"
#include "gpu/abstractions/vulkanShader.h"
#include "util/vec2.h"
#include "gpu/renderer/viewport/blockTextureManager.h"

void ElementRenderer::init(VulkanDevice* device, VkRenderPass& renderPass) {
	this->device = device;

	// block preview
	VkShaderModule blockPreviewVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/blockPreview.vert.spv"));
	VkShaderModule blockPreviewFragShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/blockPreview.frag.spv"));

	PipelineInformation blockPreviewPipelineInfo{};
	blockPreviewPipelineInfo.vertShader = blockPreviewVertShader;
	blockPreviewPipelineInfo.fragShader = blockPreviewFragShader;
	blockPreviewPipelineInfo.renderPass = renderPass;
	blockPreviewPipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_VERTEX_BIT, sizeof(BlockPreviewPushConstant) });
	blockPreviewPipelineInfo.descriptorSets.push_back(device->getBlockTextureManager()->getDescriptorLayout());
	blockPreviewPipeline.init(device, blockPreviewPipelineInfo);

	destroyShaderModule(device->getDevice(), blockPreviewVertShader);
	destroyShaderModule(device->getDevice(), blockPreviewFragShader);

	// box selection
	VkShaderModule boxSelectionVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/boxSelection.vert.spv"));
	VkShaderModule boxSelectionFragShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/boxSelection.frag.spv"));

	PipelineInformation boxSelectionPipelineInfo{};
	boxSelectionPipelineInfo.vertShader = boxSelectionVertShader;
	boxSelectionPipelineInfo.fragShader = boxSelectionFragShader;
	boxSelectionPipelineInfo.renderPass = renderPass;
	boxSelectionPipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_VERTEX_BIT, offsetof(BoxSelectionPushConstant, state) });
	boxSelectionPipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t) });
	boxSelectionPipeline.init(device, boxSelectionPipelineInfo);

	destroyShaderModule(device->getDevice(), boxSelectionVertShader);
	destroyShaderModule(device->getDevice(), boxSelectionFragShader);

	// connection preview
	VkShaderModule connectionPreviewVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/connectionPreview.vert.spv"));
	VkShaderModule connectionPreviewFragShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/connectionPreview.frag.spv"));

	PipelineInformation connectionPreviewPipelineInfo{};
	connectionPreviewPipelineInfo.vertShader = connectionPreviewVertShader;
	connectionPreviewPipelineInfo.fragShader = connectionPreviewFragShader;
	connectionPreviewPipelineInfo.renderPass = renderPass;
	connectionPreviewPipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_VERTEX_BIT, sizeof(ConnectionPreviewPushConstant) });
	connectionPreviewPipeline.init(device, connectionPreviewPipelineInfo);

	destroyShaderModule(device->getDevice(), connectionPreviewVertShader);
	destroyShaderModule(device->getDevice(), connectionPreviewFragShader);

	// arrow circle
	VkShaderModule arrowCircleVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrowCircle.vert.spv"));
	VkShaderModule arrowCircleFragShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrowCircle.frag.spv"));

	PipelineInformation arrowCirclePipelineInfo{};
	arrowCirclePipelineInfo.vertShader = arrowCircleVertShader;
	arrowCirclePipelineInfo.fragShader = arrowCircleFragShader;
	arrowCirclePipelineInfo.renderPass = renderPass;
	arrowCirclePipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_VERTEX_BIT, offsetof(ArrowCirclePushConstant, depth) });
	arrowCirclePipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t) });
	arrowCirclePipeline.init(device, arrowCirclePipelineInfo);

	destroyShaderModule(device->getDevice(), arrowCircleVertShader);
	destroyShaderModule(device->getDevice(), arrowCircleFragShader);

	// arrow
	VkShaderModule arrowVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrow.vert.spv"));
	VkShaderModule arrowFragShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/arrow.frag.spv"));

	PipelineInformation arrowPipelineInfo{};
	arrowPipelineInfo.vertShader = arrowVertShader;
	arrowPipelineInfo.fragShader = arrowFragShader;
	arrowPipelineInfo.renderPass = renderPass;
	arrowPipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_VERTEX_BIT, offsetof(ArrowPushConstant, depth) });
	arrowPipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t) });
	arrowPipeline.init(device, arrowPipelineInfo);

	destroyShaderModule(device->getDevice(), arrowVertShader);
	destroyShaderModule(device->getDevice(), arrowFragShader);
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
		vkCmdBindPipeline(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPreviewPipeline.getHandle());
		std::shared_ptr<BlockTexture> blockTexture = device->getBlockTextureManager()->getTexture();
		frame.lifetime.push(blockTexture);
		vkCmdBindDescriptorSets(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPreviewPipeline.getLayout(), 0, 1, &blockTexture->descriptor, 0, nullptr);

		BlockPreviewPushConstant blockPreviewConstant;
		blockPreviewConstant.mvp = viewMatrix;
		Vec2 uvCellSize = device->getBlockTextureManager()->getTileset().getCellUVSize();
		blockPreviewConstant.uvCellSizeX = uvCellSize.x;
		blockPreviewConstant.uvCellSizeY = uvCellSize.y;
		for (const BlockPreviewRenderData& preview : blockPreviews) {
			blockPreviewConstant.position = preview.position;
			blockPreviewConstant.size = preview.size;
			blockPreviewConstant.orientation = preview.orientation.rotation + 4 * preview.orientation.flipped;
			blockPreviewConstant.uvOffsetX = device->getBlockTextureManager()->getTileset().getTopLeftUV(preview.textureIndex, 0).x;

			blockPreviewPipeline.cmdPushConstants(frame.mainCommandBuffer, &blockPreviewConstant);
			vkCmdDraw(frame.mainCommandBuffer, 6, 1, 0, 0);
		}
	}
}

void ElementRenderer::renderBoxSelections(Frame& frame, const glm::mat4& viewMatrix, const std::vector<BoxSelectionRenderData>& boxSelections) {
	if (!boxSelections.empty()) {
		vkCmdBindPipeline(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, boxSelectionPipeline.getHandle());

		BoxSelectionPushConstant boxSelectionConstant;
		boxSelectionConstant.mvp = viewMatrix;

		for (const BoxSelectionRenderData& boxSelection : boxSelections) {
			boxSelectionConstant.position = boxSelection.topLeft;
			boxSelectionConstant.size = boxSelection.size;
			boxSelectionConstant.state = boxSelection.state;

			boxSelectionPipeline.cmdPushConstants(frame.mainCommandBuffer, &boxSelectionConstant);
			vkCmdDraw(frame.mainCommandBuffer, 6, 1, 0, 0);
		}
	}
}

void ElementRenderer::renderConnectionPreviews(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ConnectionPreviewRenderData>& connectionPreviews) {
	if (!connectionPreviews.empty()) {
		vkCmdBindPipeline(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, connectionPreviewPipeline.getHandle());

		ConnectionPreviewPushConstant connectionPreviewConstant;
		connectionPreviewConstant.mvp = viewMatrix;

		for (const ConnectionPreviewRenderData& connectionPreview : connectionPreviews) {
			connectionPreviewConstant.pointA = connectionPreview.pointA;
			connectionPreviewConstant.pointB = connectionPreview.pointB;

			connectionPreviewPipeline.cmdPushConstants(frame.mainCommandBuffer, &connectionPreviewConstant);
			vkCmdDraw(frame.mainCommandBuffer, 6, 1, 0, 0);
		}
	}
}

void ElementRenderer::renderArrows(Frame& frame, const glm::mat4& viewMatrix, const std::vector<ArrowRenderData>& arrows) {
	if (!arrows.empty()) {
		// arrow circles
		vkCmdBindPipeline(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, arrowCirclePipeline.getHandle());
		ArrowCirclePushConstant arrowCircleConstant;
		arrowCircleConstant.mvp = viewMatrix;
		for (const ArrowRenderData& arrow : arrows) {
			if (arrow.pointA != arrow.pointB) continue;

			arrowCircleConstant.topLeft = glm::vec2(arrow.pointA.x, arrow.pointA.y);
			arrowCircleConstant.depth = arrow.depth;

			arrowCirclePipeline.cmdPushConstants(frame.mainCommandBuffer, &arrowCircleConstant);
			vkCmdDraw(frame.mainCommandBuffer, 6, 1, 0, 0);
		}

		// arrow
		vkCmdBindPipeline(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, arrowPipeline.getHandle());
		ArrowPushConstant arrowConstant;
		arrowConstant.mvp = viewMatrix;
		for (const ArrowRenderData& arrow : arrows) {
			if (arrow.pointA == arrow.pointB) continue;

			arrowConstant.pointA = glm::vec2(arrow.pointA.x + 0.5f, arrow.pointA.y + 0.5f);
			arrowConstant.pointB = glm::vec2(arrow.pointB.x + 0.5f, arrow.pointB.y + 0.5f);
			arrowConstant.depth = arrow.depth;

			arrowPipeline.cmdPushConstants(frame.mainCommandBuffer, &arrowConstant);
			vkCmdDraw(frame.mainCommandBuffer, 9, 1, 0, 0);
		}
	}
}
