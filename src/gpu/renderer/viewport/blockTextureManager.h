#ifndef blockTextureManager_h
#define blockTextureManager_h

#include "gpu/abstractions/vulkanDescriptor.h"
#include "gpu/abstractions/vulkanImage.h"
#include "tileSet.h"

class VulkanDevice;

struct BlockTexture {
	~BlockTexture();
	VkDescriptorSet descriptor;
	VkSampler sampler;
	AllocatedImage image;
	VulkanDevice* device;
};

class BlockTextureManager {
public:
	void init(VulkanDevice* device);
	void update();
	void cleanup();

	inline VkDescriptorSetLayout getDescriptorLayout() { return descriptorLayout; }
	inline std::shared_ptr<BlockTexture> getTexture() { return mainTexture; }
	inline TileSetInfo& getTileset() { return mainTileSet; }

private:
	VulkanDevice* device;

	DescriptorAllocator descriptorAllocator;
	VkDescriptorSetLayout descriptorLayout;

	std::shared_ptr<BlockTexture> mainTexture;
	TileSetInfo mainTileSet = TileSetInfo(256, 15, 4);
};

#endif
