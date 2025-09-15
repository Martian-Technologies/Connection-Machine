#ifndef rmlResourceManager_h
#define rmlResourceManager_h

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <RmlUi/Core/Vertex.h>

#include "gpu/abstractions/vulkanDescriptor.h"
#include "gpu/abstractions/vulkanBuffer.h"
#include "gpu/abstractions/vulkanImage.h"
#include "gpu/renderer/frameManager.h"

// =========================== RML GEOMETRY =================================

struct RmlVertex : Rml::Vertex {
	static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class RmlGeometryAllocation {
public:
	RmlGeometryAllocation(VulkanDevice* device, Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices);
	~RmlGeometryAllocation();

	inline AllocatedBuffer& getVertexBuffer() { return vertexBuffer; }
	inline AllocatedBuffer& getIndexBuffer() { return indexBuffer; }
	inline unsigned int getNumIndices() { return numIndices; }

private:
	AllocatedBuffer vertexBuffer, indexBuffer;
	unsigned int numIndices;

	VulkanDevice* device;
};

// ============================= RML TEXTURES ===================================
class RmlTexture {
public:
	RmlTexture(VulkanDevice* device, void* data, VkExtent3D size, VkDescriptorSet myDescriptor);
	~RmlTexture();

	inline VkDescriptorSet& getDescriptor() { return descriptor; }

private:
	AllocatedImage image;
	VkSampler sampler;
	VkDescriptorSet descriptor;

	VulkanDevice* device;
};

// ========================= RML RENDERER ====================================

struct RmlPushConstants {
	glm::mat4 pixelViewMat;
	glm::vec2 translation;
};

class RmlResourceManager {
public:
	void init(VulkanDevice* device);
	void cleanup();

	void prepareForRmlRender();
	void endRmlRender();

	void render(Frame& frame, VkExtent2D windowExtent);

public:
	// -- Rml::RenderInterface --
	Rml::CompiledGeometryHandle compileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices);
	void releaseGeometry(Rml::CompiledGeometryHandle geometry);
	void renderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture);

	Rml::TextureHandle loadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source);
	Rml::TextureHandle generateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions);
	void releaseTexture(Rml::TextureHandle texture_handle);

	void enableScissorRegion(bool enable);
	void setScissorRegion(Rml::Rectanglei region);

	VkDescriptorSetLayout& getSingleImageDescriptorSetLayout() { return singleImageDescriptorSetLayout; }
	std::shared_ptr<RmlGeometryAllocation> getGeometry(Rml::CompiledGeometryHandle handle);
	std::shared_ptr<RmlTexture> getTexture(Rml::TextureHandle handle);
private:
	// geometry allocations
	Rml::CompiledGeometryHandle currentGeometryHandle = 0;
	std::unordered_map<Rml::CompiledGeometryHandle, std::shared_ptr<RmlGeometryAllocation>> geometryAllocations;

	// texture allocations
	Rml::TextureHandle currentTextureHandle = 0;
	std::unordered_map<Rml::TextureHandle, std::shared_ptr<RmlTexture>> textures;

	// texture descriptor
	DescriptorAllocator descriptorAllocator;
	VkDescriptorSetLayout singleImageDescriptorSetLayout;

	VulkanDevice* device;
};

#endif /* rmlResourceManager_h */
