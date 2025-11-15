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

AllocatedImage createImageArray(VulkanDevice* device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, uint32_t arrayLayers = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
AllocatedImage createImage(VulkanDevice* device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
AllocatedImage createImage(VulkanDevice* device, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
void destroyImage(AllocatedImage& image);

bool transitionImageLayout(VkCommandBuffer cmd, AllocatedImage& image, VkImageLayout oldLayout, VkImageLayout newLayout);

#endif
