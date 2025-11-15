#ifndef blockTextureManager_h
#define blockTextureManager_h

#include "glm/ext/vector_float2.hpp"
#include "gpu/abstractions/vulkanDescriptor.h"
#include "gpu/abstractions/vulkanImage.h"
#include "util/rectPacker.h"
#include "util/vec2.h"

#define BLOCK_TEXTURE_SIZE 8192

class VulkanDevice;

struct BlockTextureArray {
	DescriptorAllocator descriptorAllocator;
	VulkanDevice* device;
	AllocatedImage image;
	VkSampler sampler;
	VkDescriptorSet descriptor;
	VkExtent3D textureSize;

	uint32_t layerCount;

	BlockTextureArray() : device(nullptr), sampler(VK_NULL_HANDLE), descriptor(VK_NULL_HANDLE), layerCount(0) { }
	~BlockTextureArray();
};

typedef unsigned int BlockTextureId;

struct BlockTexture {
public:
	BlockTexture(Vec2Int textureOrigin, Vec2Int textureSize, unsigned int textureLayer, RectPacker::RectID rectId) :
		textureOrigin(textureOrigin), textureSize(textureSize), textureLayer(textureLayer), rectId((rectId)) { }
	Vec2Int textureOrigin;
	Vec2Int textureSize;
	unsigned int textureLayer;
	RectPacker::RectID rectId;
};

struct BlockTextureCords {
public:
	BlockTextureCords(glm::vec2 textureOriginUV, glm::vec2 textureSizeUV, unsigned int textureLayer, glm::vec2 textureUVStateStep) :
		textureOriginUV(textureOriginUV), textureSizeUV(textureSizeUV), textureLayer(textureLayer), textureUVStateStep(textureUVStateStep) { }
	bool operator==(const BlockTextureCords& other) const {
		return textureOriginUV == other.textureOriginUV && textureSizeUV == other.textureSizeUV && textureLayer == other.textureLayer &&
			   textureUVStateStep == other.textureUVStateStep;
	}
	glm::vec2 textureUVStateStep;
	glm::vec2 textureOriginUV;
	glm::vec2 textureSizeUV;
	unsigned int textureLayer;
};

class BlockTextureManager {
public:
	void init(VulkanDevice* device);
	BlockTextureId addTexture(const std::string& path);
	void refreshBlockTexture(const std::string& path);
	BlockTextureId addTexture(const unsigned char* pixels, int textureWidth, int textureHeight);
	void updateBlockTexture(const unsigned char* pixels, BlockTextureId blockTextureId);
	void removeBlockTexture(const std::string& path);
	void removeBlockTexture(BlockTextureId blockTextureId);
	BlockTextureCords getBlockTextureCords(BlockTextureId blockTextureId) const;
	BlockTextureCords getBlockTextureCords(BlockTextureId blockTextureId, Vec2Int tileSize, Vec2Int smallestCordTile, Vec2Int blockSize) const;
	BlockTextureCords getBlockTextureCords(BlockTextureId blockTextureId, Vec2Int tileSize, Vec2Int smallestCordTile, Vec2Int blockSize, Vec2Int textureStepSize) const;
	void update();
	void cleanup();

	inline VkDescriptorSetLayout getDescriptorLayout() { return descriptorLayout; }
	inline std::shared_ptr<BlockTextureArray> getTextureArray() {
		std::lock_guard<std::mutex> lock(descriptorMutex);
		return textureArray;
	}

private:
	void makeTextureArray(uint32_t newLayerCount, VkExtent3D textureSize);
	void addTextureToArray(const unsigned char* pixels, Vec2Int textureSize, Vec2Int texturePos, unsigned int textureLayer);
	void resizeTextureArray(uint32_t newLayerCount);

	VulkanDevice* device;

	VkDescriptorSetLayout descriptorLayout;

	std::vector<bool> writtenLayers;
	std::vector<RectPacker> layerRectPackers;

	std::mutex descriptorMutex;
	std::shared_ptr<BlockTextureArray> textureArray;
	std::map<BlockTextureId, BlockTexture> blockTextures;
	std::map<std::string, BlockTextureId> loadedTextureFiles;
};

#endif
