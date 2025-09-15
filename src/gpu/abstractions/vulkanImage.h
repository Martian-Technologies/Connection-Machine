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

	VulkanDevice* device;
};

AllocatedImage createImage(VulkanDevice* device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
AllocatedImage createImage(VulkanDevice* device, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
void destroyImage(AllocatedImage& image);

bool transitionImageLayout(VkCommandBuffer cmd, AllocatedImage& image, VkImageLayout oldLayout, VkImageLayout newLayout);

#endif
