#include "chunkRenderer.h"

#include "gpu/renderer/viewport/blockTextureManager.h"
#include "gpu/abstractions/vulkanShader.h"

#include "backend/evaluator/logicState.h"
#include "computerAPI/fileLoader.h"
#include "computerAPI/directoryManager.h"

#include "util/vec2.h"


#ifdef TRACY_PROFILER
	#include <tracy/Tracy.hpp>
#endif

void ChunkRenderer::init(VulkanDevice* device, VkRenderPass& renderPass) {
	this->device = device;

	// ==================== STATE BUFFER setup =============================================
	// create layout and descriptor set
	DescriptorLayoutBuilder stateBufferBuilder;
	stateBufferBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	stateBufferDescriptorSetLayout = stateBufferBuilder.build(device->getDevice(), VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

	// ==================== PIPELINE setup =================================================

	// load shaders
	VkShaderModule blockVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/block.vert.spv"));
	VkShaderModule blockFragShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/block.frag.spv"));
	VkShaderModule wireVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/wire.vert.spv"));
	VkShaderModule wireFragShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/wire.frag.spv"));

	PipelineInformation blockPipelineInfo{};
	blockPipelineInfo.vertShader = blockVertShader;
	blockPipelineInfo.fragShader = blockFragShader;
	blockPipelineInfo.renderPass = renderPass;
	blockPipelineInfo.vertexBindingDescriptions = BlockInstance::getBindingDescriptions();
	blockPipelineInfo.vertexAttributeDescriptions = BlockInstance::getAttributeDescriptions();
	blockPipelineInfo.pushConstants.push_back({VK_SHADER_STAGE_VERTEX_BIT, sizeof(ChunkPushConstants)});
	blockPipelineInfo.descriptorSets.push_back(stateBufferDescriptorSetLayout);
	blockPipelineInfo.descriptorSets.push_back(device->getBlockTextureManager()->getDescriptorLayout());
	blockPipeline.init(device, blockPipelineInfo);

	PipelineInformation wirePipelineInfo{};
	wirePipelineInfo.vertShader = wireVertShader;
	wirePipelineInfo.fragShader = wireFragShader;
	wirePipelineInfo.renderPass = renderPass;
	wirePipelineInfo.vertexBindingDescriptions = WireInstance::getBindingDescriptions();
	wirePipelineInfo.vertexAttributeDescriptions = WireInstance::getAttributeDescriptions();
	wirePipelineInfo.pushConstants.push_back({VK_SHADER_STAGE_VERTEX_BIT, sizeof(ChunkPushConstants)});
	wirePipelineInfo.descriptorSets.push_back(stateBufferDescriptorSetLayout);
	wirePipeline.init(device, wirePipelineInfo);

	// destroy shader modules since we won't be recreating pipelines
	destroyShaderModule(device->getDevice(), blockVertShader);
	destroyShaderModule(device->getDevice(), blockFragShader);
	destroyShaderModule(device->getDevice(), wireVertShader);
	destroyShaderModule(device->getDevice(), wireFragShader);
}

void ChunkRenderer::cleanup() {

	// destroy state buffer layout
	vkDestroyDescriptorSetLayout(device->getDevice(), stateBufferDescriptorSetLayout, nullptr);

	// destroy pipelines
	blockPipeline.cleanup();
	wirePipeline.cleanup();
}

void ChunkRenderer::render(Frame& frame, const glm::mat4& viewMatrix, Evaluator* evaluator, const Address& address, const std::vector<std::shared_ptr<VulkanChunkAllocation>>& chunks) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif

	// save chunk data to frame
	for (auto& chunk : chunks) {
		frame.lifetime.push(chunk);
	}

	// shared push constants
	ChunkPushConstants pushConstants{};
	pushConstants.mvp = viewMatrix;
	Vec2 uvCellSize = device->getBlockTextureManager()->getTileset().getCellUVSize();
	pushConstants.uvCellSizeX = uvCellSize.x;
	pushConstants.uvCellSizeY = uvCellSize.y;

	// fill state buffers
	for (std::shared_ptr<VulkanChunkAllocation> chunk : chunks) {
		if (chunk->getStateBuffer().has_value()) {
			chunk->getStateBuffer()->incrementBufferFrame();

			std::vector<logic_state_t> states(chunk->getStateSimulatorIds().size());
			if (evaluator != nullptr) {
				states = evaluator->getStatesFromSimulatorIds(chunk->getStateSimulatorIds());
			}

			vmaCopyMemoryToAllocation(device->getAllocator(), states.data(), chunk->getStateBuffer()->getCurrentBuffer().allocation, 0, states.size());
		}
	}

	// block drawing pass
	{
		#ifdef TRACY_PROFILER
			ZoneScopedN("block drawing");
		#endif
		// bind render pipeline
		vkCmdBindPipeline(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPipeline.getHandle());

		// bind push constants
		blockPipeline.cmdPushConstants(frame.mainCommandBuffer, &pushConstants);

		// bind texture descriptor
		std::shared_ptr<BlockTexture> blockTexture = device->getBlockTextureManager()->getTexture();
		frame.lifetime.push(blockTexture);
		vkCmdBindDescriptorSets(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPipeline.getLayout(), 1, 1, &blockTexture->descriptor, 0, nullptr);

		for (std::shared_ptr<VulkanChunkAllocation> chunk : chunks) {
			if (chunk->getBlockBuffer().has_value()) {

				// bind vertex buffers
				VkBuffer vertexBuffers[] = { chunk->getBlockBuffer()->buffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(frame.mainCommandBuffer, 0, 1, vertexBuffers, offsets);

				// Write state buffer descriptor
				VkWriteDescriptorSet stateBufferDescriptorWrite = {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = 0,
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &chunk->getStateBuffer()->getCurrentBufferInfo()
				};
				vkCmdPushDescriptorSetKHR(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPipeline.getLayout(), 0, 1, &stateBufferDescriptorWrite);

				// draw
				vkCmdDraw(frame.mainCommandBuffer, 6, static_cast<uint32_t>(chunk->getNumBlockInstances()), 0, 0);
			}
		}
	}

	// wire drawing pass
	{
		#ifdef TRACY_PROFILER
			ZoneScopedN("wire drawing");
		#endif
		// bind render pipeline
		vkCmdBindPipeline(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wirePipeline.getHandle());

		// bind push constants
		wirePipeline.cmdPushConstants(frame.mainCommandBuffer, &pushConstants);

		for (std::shared_ptr<VulkanChunkAllocation> chunk : chunks) {
			if (chunk->getWireBuffer().has_value()) {

				// bind vertex buffers
				VkBuffer vertexBuffers[] = { chunk->getWireBuffer()->buffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(frame.mainCommandBuffer, 0, 1, vertexBuffers, offsets);

				// Write state buffer descriptor
				VkWriteDescriptorSet stateBufferDescriptorWrite = {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = 0,
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &chunk->getStateBuffer()->getCurrentBufferInfo()
				};
				vkCmdPushDescriptorSetKHR(frame.mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wirePipeline.getLayout(), 0, 1, &stateBufferDescriptorWrite);

				// draw
				vkCmdDraw(frame.mainCommandBuffer, 6, static_cast<uint32_t>(chunk->getNumWireInstances()), 0, 0);
			}
		}
	}
}
