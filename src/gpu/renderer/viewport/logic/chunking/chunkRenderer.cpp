#include "chunkRenderer.h"

#include "gpu/renderer/viewport/blockTextureManager.h"
#include "gpu/abstractions/vulkanShader.h"

#include "backend/evaluator/simulator/logicState.h"
#include "computerAPI/fileLoader.h"
#include "computerAPI/directoryManager.h"

#ifdef TRACY_PROFILER
	#include <tracy/Tracy.hpp>
#endif

void ChunkRenderer::init(VulkanDevice& device, vk::RenderPass renderPass) {
	this->device = &device;

	DescriptorLayoutBuilder stateBufferBuilder;
	stateBufferBuilder.addBinding(0, vk::DescriptorType::eStorageBuffer);
	stateBufferDescriptorSetLayout = stateBufferBuilder.build(this->device->getDevice(), vk::ShaderStageFlagBits::eVertex, vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR);

	vk::UniqueShaderModule blockVertShader = createShaderModule(this->device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/block.vert.spv"));
	vk::UniqueShaderModule blockFragShader = createShaderModule(this->device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/block.frag.spv"));
	vk::UniqueShaderModule wireVertShader = createShaderModule(this->device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/wire.vert.spv"));
	vk::UniqueShaderModule wireFragShader = createShaderModule(this->device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/wire.frag.spv"));

	PipelineInformation blockPipelineInfo{};
	blockPipelineInfo.vertShader = blockVertShader.get();
	blockPipelineInfo.fragShader = blockFragShader.get();
	blockPipelineInfo.renderPass = renderPass;
	blockPipelineInfo.vertexBindingDescriptions = BlockInstance::getBindingDescriptions();
	blockPipelineInfo.vertexAttributeDescriptions = BlockInstance::getAttributeDescriptions();
	blockPipelineInfo.pushConstants.push_back({vk::ShaderStageFlagBits::eVertex, sizeof(ChunkPushConstants)});
	blockPipelineInfo.descriptorSets.push_back(stateBufferDescriptorSetLayout.get());
	blockPipelineInfo.descriptorSets.push_back(this->device->getBlockTextureManager().getDescriptorLayout());
	blockPipelineInfo.sampleCount = this->device->getMaxUsableSampleCount();
	blockPipeline.init(device, blockPipelineInfo);

	PipelineInformation wirePipelineInfo{};
	wirePipelineInfo.vertShader = wireVertShader.get();
	wirePipelineInfo.fragShader = wireFragShader.get();
	wirePipelineInfo.renderPass = renderPass;
	wirePipelineInfo.vertexBindingDescriptions = WireInstance::getBindingDescriptions();
	wirePipelineInfo.vertexAttributeDescriptions = WireInstance::getAttributeDescriptions();
	wirePipelineInfo.pushConstants.push_back({vk::ShaderStageFlagBits::eVertex, sizeof(ChunkPushConstants)});
	wirePipelineInfo.descriptorSets.push_back(stateBufferDescriptorSetLayout.get());
	wirePipelineInfo.sampleCount = this->device->getMaxUsableSampleCount();
	wirePipeline.init(device, wirePipelineInfo);
}

void ChunkRenderer::cleanup() {
	stateBufferDescriptorSetLayout.reset();
	blockPipeline.cleanup();
	wirePipeline.cleanup();
}

void ChunkRenderer::render(Frame& frame, const glm::mat4& viewMatrix, const EvalLogicSimulator* simulator, const Address& address, const std::vector<std::shared_ptr<VulkanLogicAllocation>>& chunks) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	for (auto& chunk : chunks) {
		frame.lifetime.push(chunk);
	}

	ChunkPushConstants pushConstants{};
	pushConstants.mvp = viewMatrix;

	for (std::shared_ptr<VulkanLogicAllocation> chunk : chunks) {
		if (chunk->getStateBuffer().has_value()) {
			chunk->getStateBuffer()->incrementBufferFrame();

			std::vector<logic_state_t> states(chunk->getStateSimulatorIds().size());
			if (simulator != nullptr) {
				states = simulator->getStates(chunk->getStateSimulatorIds());
			}

			device->getAllocator().copyMemoryToAllocation(states.data(), chunk->getStateBuffer()->getCurrentBuffer().allocation.get(), 0, states.size());
		}
	}
	if (simulator != nullptr) {
		simulator->requestNewStatesOutputVector();
	}

	// block drawing pass
	{
		#ifdef TRACY_PROFILER
			ZoneScopedN("block drawing");
		#endif
		frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, blockPipeline.getHandle());

		blockPipeline.cmdPushConstants(frame.mainCommandBuffer, &pushConstants);

		std::shared_ptr<BlockTextureArray> blockTexture = device->getBlockTextureManager().getTextureArray();
		frame.lifetime.push(blockTexture);
		frame.mainCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blockPipeline.getLayout(), 1, blockTexture->descriptor, nullptr);

		for (std::shared_ptr<VulkanLogicAllocation> chunk : chunks) {
			if (chunk->getBlockBuffer().has_value()) {
				vk::Buffer vertexBuffers[] = { chunk->getBlockBuffer()->buffer.get() };
				vk::DeviceSize offsets[] = { 0 };
				frame.mainCommandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

				vk::WriteDescriptorSet stateBufferDescriptorWrite{};
				stateBufferDescriptorWrite.dstSet = nullptr;
				stateBufferDescriptorWrite.dstBinding = 0;
				stateBufferDescriptorWrite.descriptorCount = 1;
				stateBufferDescriptorWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
				stateBufferDescriptorWrite.pBufferInfo = &chunk->getStateBuffer()->getCurrentBufferInfo();

				frame.mainCommandBuffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, blockPipeline.getLayout(), 0, stateBufferDescriptorWrite);

				frame.mainCommandBuffer.draw(6, static_cast<uint32_t>(chunk->getNumBlockInstances()), 0, 0);
			}
		}
	}

	// wire drawing pass
	{
		#ifdef TRACY_PROFILER
			ZoneScopedN("wire drawing");
		#endif
		frame.mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, wirePipeline.getHandle());

		wirePipeline.cmdPushConstants(frame.mainCommandBuffer, &pushConstants);

		for (std::shared_ptr<VulkanLogicAllocation> chunk : chunks) {
			if (chunk->getWireBuffer().has_value()) {
				vk::Buffer vertexBuffers[] = { chunk->getWireBuffer()->buffer.get() };
				vk::DeviceSize offsets[] = { 0 };
				frame.mainCommandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

				vk::WriteDescriptorSet stateBufferDescriptorWrite{};
				stateBufferDescriptorWrite.dstSet = nullptr;
				stateBufferDescriptorWrite.dstBinding = 0;
				stateBufferDescriptorWrite.descriptorCount = 1;
				stateBufferDescriptorWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
				stateBufferDescriptorWrite.pBufferInfo = &chunk->getStateBuffer()->getCurrentBufferInfo();

				frame.mainCommandBuffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, wirePipeline.getLayout(), 0, stateBufferDescriptorWrite);

				frame.mainCommandBuffer.draw(6, static_cast<uint32_t>(chunk->getNumWireInstances()), 0, 0);
			}
		}
	}
}
