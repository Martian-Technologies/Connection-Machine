#include "rmlResourceManager.h"

#include <stb_image.h>

#include "gpu/vulkanDevice.h"

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


// =================================== RML RESOURCE MANAGER ==================================================

void RmlResourceManager::init(VulkanDevice* device) {
	this->device = device;
	descriptorAllocator.init(device, 100, {{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.0f }});

	// set up image descriptor
	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	singleImageDescriptorSetLayout = layoutBuilder.build(device->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);
}

void RmlResourceManager::cleanup() {
	vkDestroyDescriptorSetLayout(device->getDevice(), singleImageDescriptorSetLayout, nullptr);

	if (!geometryAllocations.empty()) {
		logError("RmlUi did not clean up all RmlGeometryAllocation. {} were left alocated.", "RmlResourceManager", geometryAllocations.size());
		geometryAllocations.clear();
	}
	textures.clear();
	descriptorAllocator.cleanup();
}

// =================================== RENDER INTERFACE ==================================================
Rml::CompiledGeometryHandle RmlResourceManager::compileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
	// get and increment handle
	Rml::CompiledGeometryHandle newHandle = ++currentGeometryHandle;
	// alocate new geometry
	geometryAllocations[newHandle] = std::make_shared<RmlGeometryAllocation>(device, vertices, indices);

	return newHandle;
}
void RmlResourceManager::releaseGeometry(Rml::CompiledGeometryHandle geometry) {
	geometryAllocations.erase(geometry);
}

// Textures
Rml::TextureHandle RmlResourceManager::loadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
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
Rml::TextureHandle RmlResourceManager::generateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
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
void RmlResourceManager::releaseTexture(Rml::TextureHandle texture_handle) {
	textures.erase(texture_handle);
}

std::shared_ptr<RmlGeometryAllocation> RmlResourceManager::getGeometry(Rml::CompiledGeometryHandle handle) {
	auto iter = geometryAllocations.find(handle);
	if (iter == geometryAllocations.end()) {
		return nullptr;
	}
	return iter->second;
}

std::shared_ptr<RmlTexture> RmlResourceManager::getTexture(Rml::TextureHandle handle) {
	auto iter = textures.find(handle);
	if (iter == textures.end()) {
		return nullptr;
	}
	return iter->second;
}

