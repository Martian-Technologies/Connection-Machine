#ifndef blockTextureManager_h
#define blockTextureManager_h

#include "glm/ext/vector_float2.hpp"
#include "gpu/abstractions/vulkanDescriptor.h"
#include "gpu/abstractions/vulkanImage.h"
#include "util/vec2.h"

class VulkanDevice;

#include <stb_image.h>

struct BlockTextureArray {
    VulkanDevice* device;
    AllocatedImage image;
    VkSampler sampler;
    VkDescriptorSet descriptor;
	VkExtent3D textureSize;
	std::vector<bool> layerInitialized;

    uint32_t maxLayers;
    uint32_t nextFreeLayer;

    BlockTextureArray() : device(nullptr), sampler(VK_NULL_HANDLE), descriptor(VK_NULL_HANDLE),
                          maxLayers(0), nextFreeLayer(0) {}
	~BlockTextureArray();
};

typedef unsigned int BlockTextureId;

struct BlockTexture {
public:
	BlockTexture(glm::vec2 textureOrigin, glm::vec2 textureSize, unsigned int textureLayer) : textureOrigin(textureOrigin), textureSize(textureSize), textureLayer(textureLayer) {}
	glm::vec2 textureOrigin;
	glm::vec2 textureSize;
	unsigned int textureLayer;
};

struct BlockTextureCords {
public:
	BlockTextureCords(glm::vec2 textureOriginUV, glm::vec2 textureSizeUV, unsigned int textureLayer) : textureOriginUV(textureOriginUV), textureSizeUV(textureSizeUV), textureLayer(textureLayer) {}
	bool operator==(const BlockTextureCords& other) const {
		return textureOriginUV == other.textureOriginUV && textureSizeUV == other.textureSizeUV && textureLayer == other.textureLayer;
	}
	glm::vec2 textureOriginUV;
	glm::vec2 textureSizeUV;
	unsigned int textureLayer;
};

class BlockTextureManager {
public:
	void init(VulkanDevice* device);
	BlockTextureId addTexture(const std::string& path);
	void refreshBlockTexture(const std::string& path);
	BlockTextureId addTexture(const stbi_uc* pixels, int textureWidth, int textureHeight);
	void updateBlockTexture(const stbi_uc* pixels, BlockTextureId blockTextureId);
	void removeBlockTexture(const std::string& path);
	void removeBlockTexture(BlockTextureId blockTextureId);
	BlockTextureCords getBlockTextureCords(BlockTextureId blockTextureId) const;
	BlockTextureCords getBlockTextureCords(BlockTextureId blockTextureId, Vec2Int tileSize, Vec2Int smallestCordTile, Vec2Int blockSize) const;
	void update();
	void cleanup();

	inline VkDescriptorSetLayout getDescriptorLayout() { return descriptorLayout; }
    inline std::shared_ptr<BlockTextureArray> getTextureArray() { return textureArray; }

private:
	// this needs to free pixels
	void addTextureToArray(const stbi_uc* pixels, glm::vec2 textureSize, glm::vec2 texturePos, unsigned int textureLayer);
	void resizeTextureArray(uint32_t newLayerCount);

	VulkanDevice* device;

	DescriptorAllocator descriptorAllocator;
	VkDescriptorSetLayout descriptorLayout;

	std::shared_ptr<BlockTextureArray> textureArray;
	std::map<BlockTextureId, BlockTexture> blockTextures;
	std::map<std::string, BlockTextureId> loadedTextureFiles;
};

#endif
