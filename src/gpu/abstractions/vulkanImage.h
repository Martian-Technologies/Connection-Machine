#ifndef vulkanImage_h
#define vulkanImage_h

#include <vk_mem_alloc.h>

class VulkanDevice;

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
	VkImageAspectFlags aspect;
	uint32_t mipLevels;
	uint32_t arrayLayers;

	VulkanDevice* device;
};

AllocatedImage createImage(VulkanDevice* device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, uint32_t arrayLayers = 1);
AllocatedImage createImage(VulkanDevice* device, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
void destroyImage(AllocatedImage& image);

void generateMipmapsCmd(VkCommandBuffer cmd, const AllocatedImage& image, uint32_t baseArrayLayer, uint32_t layerCount);

bool transitionImageLayout(VkCommandBuffer cmd, AllocatedImage& image, VkImageLayout oldLayout, VkImageLayout newLayout);

#endif
