#include "rmlRenderer.h"

#include "computerAPI/directoryManager.h"
#include "computerAPI/fileLoader.h"
#include "gpu/abstractions/vulkanShader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

std::vector<VkVertexInputBindingDescription> RmlVertex::getBindingDescriptions() {
	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(RmlVertex);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> RmlVertex::getAttributeDescriptions() {
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(RmlVertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	attributeDescriptions[1].offset = offsetof(RmlVertex, colour);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(RmlVertex, tex_coord);

	return attributeDescriptions;
}

RmlGeometryAllocation::RmlGeometryAllocation(VulkanDevice* device, Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
	: device(device) {

	size_t vertexBufferSize = vertices.size() * sizeof(RmlVertex);
	size_t indexBufferSize = indices.size() * sizeof(int);
	numIndices = indices.size();

	vertexBuffer = createBuffer(device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	indexBuffer = createBuffer(device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	vmaCopyMemoryToAllocation(device->getAllocator(), vertices.data(), vertexBuffer.allocation, 0, vertexBufferSize);
	vmaCopyMemoryToAllocation(device->getAllocator(), indices.data(), indexBuffer.allocation, 0, indexBufferSize);
}

RmlGeometryAllocation::~RmlGeometryAllocation() {
	destroyBuffer(indexBuffer);
	destroyBuffer(vertexBuffer);
}

RmlTexture::RmlTexture(VulkanDevice* device, void* data, VkExtent3D size, VkDescriptorSet myDescriptor)
	: device(device) {
	// create image
	image = createImage(device, data, size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	// create image sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &sampler);

	// update image descriptor
	descriptor = myDescriptor;
	DescriptorWriter writer;
	writer.writeImage(0, image.imageView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.updateSet(device->getDevice(), descriptor);
}

RmlTexture::~RmlTexture() {
	vkDestroySampler(device->getDevice(), sampler, nullptr);
	destroyImage(image);
}

// ================================= RML RENDERER ==================================================

void RmlRenderer::init(VulkanDevice* device, VkRenderPass& renderPass) {
	
	this->device = device;
	descriptorAllocator.init(device, 100, {{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.0f }});
	
	// set up image descriptor
	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	singleImageDescriptorSetLayout = layoutBuilder.build(device->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);
	
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
	rmlPipelineInfo.descriptorSets.push_back(singleImageDescriptorSetLayout); // descriptor set 0 (texture image)
	rmlPipelineInfo.premultipliedAlpha = true;
	texturedPipeline.init(device, rmlPipelineInfo);

	// destroy shaders
	destroyShaderModule(device->getDevice(), rmlVertShader);
	destroyShaderModule(device->getDevice(), rmlFragShaderUntextured);
	destroyShaderModule(device->getDevice(), rmlFragShaderTextured);
}

void RmlRenderer::cleanup() {
	vkDestroyDescriptorSetLayout(device->getDevice(), singleImageDescriptorSetLayout, nullptr);

	texturedPipeline.cleanup();
	untexturedPipeline.cleanup();

	descriptorAllocator.cleanup();
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
Rml::CompiledGeometryHandle RmlRenderer::compileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
	// get and increment handle
	Rml::CompiledGeometryHandle newHandle = ++currentGeometryHandle;
	// alocate new geometry
	geometryAllocations[newHandle] = std::make_shared<RmlGeometryAllocation>(device, vertices, indices);
	
	return newHandle;
}
void RmlRenderer::releaseGeometry(Rml::CompiledGeometryHandle geometry) {
	geometryAllocations.erase(geometry);
}
void RmlRenderer::renderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) {
	// find geometry
	auto geometryItr = geometryAllocations.find(handle);
	if (geometryItr == geometryAllocations.end()) {
		logError("tried to render non-existent RML geometry", "Vulkan");
		return;
	}

	// find texture if specified
	std::shared_ptr<RmlTexture> texturePtr = nullptr;
	if (texture != 0) {
		auto textureItr = textures.find(texture);
		if (textureItr != textures.end()) {
			texturePtr = textureItr->second;
		}
	}
	
	tempRenderInstructions.push_back(RmlDrawInstruction(geometryItr->second, {translation.x, translation.y}, texturePtr));
}

// Textures
Rml::TextureHandle RmlRenderer::loadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {

	// get and increment handle
	Rml::TextureHandle newHandle = ++currentTextureHandle;

	// load texture
	int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(source.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	// allocate new texture
	VkExtent3D size { (uint32_t)texWidth, (uint32_t)texHeight, 1};
    textures[newHandle] = std::make_shared<RmlTexture>(device, pixels, size, descriptorAllocator.allocate(singleImageDescriptorSetLayout));
	
	// free pixels
	stbi_image_free(pixels);
	
	return newHandle;
}
Rml::TextureHandle RmlRenderer::generateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
	// get and increment handle
	Rml::TextureHandle newHandle = ++currentTextureHandle;
	
	// alocate new texture
	VkExtent3D size;
	size.width = source_dimensions.x;
	size.height = source_dimensions.y;
	size.depth = 1;
	textures[newHandle] = std::make_shared<RmlTexture>(device, (void*)source.data(), size, descriptorAllocator.allocate(singleImageDescriptorSetLayout));
	
	return newHandle;
}
void RmlRenderer::releaseTexture(Rml::TextureHandle texture_handle) {
	textures.erase(texture_handle);
}

// Scissor
void RmlRenderer::enableScissorRegion(bool enable) {
	tempRenderInstructions.push_back(RmlEnableScissorInstruction(enable));
}
void RmlRenderer::setScissorRegion(Rml::Rectanglei region) {
	tempRenderInstructions.push_back(RmlSetScissorInstruction({region.Left(), region.Top()}, {region.Width(), region.Height()}));
}
