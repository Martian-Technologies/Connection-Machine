#include "rmlRenderer.h"

#include "computerAPI/directoryManager.h"
#include "computerAPI/fileLoader.h"
#include "gpu/abstractions/vulkanShader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

// ================================= RML RENDERER ==================================================

void RmlRenderer::init(VulkanDevice* device, VkRenderPass& renderPass) {
	this->device = device;

	// load shaders
	VkShaderModule rmlVertShader = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/rml.vert.spv"));
	VkShaderModule rmlFragShaderUntextured = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/rml.frag.spv"));
	VkShaderModule rmlFragShaderTextured = createShaderModule(device->getDevice(), readFileAsBytes(DirectoryManager::getResourceDirectory() / "shaders/rmlTextured.frag.spv"));

	// set up pipelines
	// untextured
	PipelineInformation rmlPipelineInfo{};
	rmlPipelineInfo.vertShader = rmlVertShader;
	rmlPipelineInfo.fragShader = rmlFragShaderUntextured;
	rmlPipelineInfo.renderPass = renderPass;
	rmlPipelineInfo.vertexBindingDescriptions = RmlVertex::getBindingDescriptions();
	rmlPipelineInfo.vertexAttributeDescriptions = RmlVertex::getAttributeDescriptions();
	rmlPipelineInfo.pushConstants.push_back({VK_SHADER_STAGE_VERTEX_BIT, sizeof(RmlPushConstants)});
	untexturedPipeline.init(device, rmlPipelineInfo);
	// textured
	rmlPipelineInfo.fragShader = rmlFragShaderTextured;
	rmlPipelineInfo.descriptorSets.push_back(device->getRmlResourceManager().getSingleImageDescriptorSetLayout()); // descriptor set 0 (texture image)
	rmlPipelineInfo.premultipliedAlpha = true;
	texturedPipeline.init(device, rmlPipelineInfo);

	// destroy shaders
	destroyShaderModule(device->getDevice(), rmlVertShader);
	destroyShaderModule(device->getDevice(), rmlFragShaderUntextured);
	destroyShaderModule(device->getDevice(), rmlFragShaderTextured);
}

void RmlRenderer::cleanup() {
	texturedPipeline.cleanup();
	untexturedPipeline.cleanup();
}

void RmlRenderer::prepareForRmlRender() {
	// clear list of temp RML instructions so we can start adding
	tempRenderInstructions.clear();
}

void RmlRenderer::endRmlRender() {
	// swap the real instructions out for the new ones
	std::lock_guard<std::mutex> lock(rmlInstructionMux);
	renderInstructions = std::move(tempRenderInstructions);
}

void RmlRenderer::render(Frame& frame, VkExtent2D windowExtent) {
	VkCommandBuffer cmd = frame.mainCommandBuffer;

	// set viewport state
	VkExtent2D& extent = windowExtent;
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	// set default scissor
	VkRect2D defaultScissor{};
	defaultScissor.offset = {0, 0};
	defaultScissor.extent = extent;
	vkCmdSetScissor(cmd, 0, 1, &defaultScissor);

	// set up data for custom scissor
	VkRect2D customScissor{};
	customScissor.offset = {0, 0};
	customScissor.extent = extent;

	// bind untextured pipeline by default
	bool pipelineRebindNeeded = true;
	Pipeline* currentPipeline = &untexturedPipeline;

	// set up shared push constants data
	RmlPushConstants pushConstants{ glm::ortho(0.0f, (float)extent.width, 0.0f, (float)extent.height), glm::vec2(0.0f, 0.0f) };

	// go through and do render instructions
	std::lock_guard<std::mutex> lock(rmlInstructionMux);
	for (const auto& instruction : renderInstructions) {
		if (std::holds_alternative<RmlDrawInstruction>(instruction)) {
			// DRAW instruction
			const RmlDrawInstruction& renderInstruction = std::get<RmlDrawInstruction>(instruction);

			// Add geometry we are going to use to the frame
			frame.lifetime.push(renderInstruction.geometry);

			// update pipeline if needed
			bool texturedDraw = renderInstruction.texture != nullptr;
			Pipeline* newPipeline = texturedDraw ? &texturedPipeline : &untexturedPipeline;
			if (newPipeline != currentPipeline || pipelineRebindNeeded) {
				currentPipeline = newPipeline;
				pipelineRebindNeeded = false;

				// bind untextured pipeline
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getHandle());
			}

			// bind texture descriptor if needed
			if (texturedDraw) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, texturedPipeline.getLayout(), 0, 1, &renderInstruction.texture->getDescriptor(), 0, nullptr);

				// Add texture we are going to use to the frame
				frame.lifetime.push(renderInstruction.texture);
			}

			// upload push constants
			pushConstants.translation = renderInstruction.translation;
			currentPipeline->cmdPushConstants(cmd, &pushConstants);

			// bind vertex buffer
			VkBuffer vertexBuffers[] = { renderInstruction.geometry->getVertexBuffer().buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

			// bind index buffer
			vkCmdBindIndexBuffer(cmd, renderInstruction.geometry->getIndexBuffer().buffer, offsets[0], VK_INDEX_TYPE_UINT32);

			// draw
			vkCmdDrawIndexed(cmd, renderInstruction.geometry->getNumIndices(), 1, 0, 0, 0);

		}
		else if (std::holds_alternative<RmlEnableScissorInstruction>(instruction)) {
			// ENABLE SCISSOR instruction
			const RmlEnableScissorInstruction& enableScissorInstruction = std::get<RmlEnableScissorInstruction>(instruction);

			if (enableScissorInstruction.state) {
				vkCmdSetScissor(cmd, 0, 1, &customScissor);
			}
			else {
				vkCmdSetScissor(cmd, 0, 1, &defaultScissor);
			}
		}
		else if (std::holds_alternative<RmlSetScissorInstruction>(instruction)) {
			// SET SCISSOR instruction
			const RmlSetScissorInstruction& setScissorInstruction = std::get<RmlSetScissorInstruction>(instruction);

			customScissor.offset = { (int)setScissorInstruction.offset.x, (int)setScissorInstruction.offset.y};
			customScissor.extent = { (unsigned int)setScissorInstruction.size.x, (unsigned int)setScissorInstruction.size.y};
			vkCmdSetScissor(cmd, 0, 1, &customScissor);
		}
	}
}

// =================================== RENDER INTERFACE ==================================================

// Geometry
void RmlRenderer::renderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) {
	// find geometry
	std::shared_ptr<RmlGeometryAllocation> rmlGeometryAllocation = device->getRmlResourceManager().getGeometry(handle);

	// find texture if specified
	std::shared_ptr<RmlTexture> texturePtr = nullptr;
	if (texture != 0) {
		texturePtr = device->getRmlResourceManager().getTexture(texture);
	}

	tempRenderInstructions.push_back(RmlDrawInstruction(rmlGeometryAllocation, {translation.x, translation.y}, texturePtr));
}

// Scissor
void RmlRenderer::enableScissorRegion(bool enable) {
	tempRenderInstructions.push_back(RmlEnableScissorInstruction(enable));
}
void RmlRenderer::setScissorRegion(Rml::Rectanglei region) {
	tempRenderInstructions.push_back(RmlSetScissorInstruction({region.Left(), region.Top()}, {region.Width(), region.Height()}));
}
