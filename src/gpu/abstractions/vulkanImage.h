#ifndef vulkanImage_h
#define vulkanImage_h

#include "gpu/vulkanCommon.h"

class VulkanDevice;

struct AllocatedImage {
	AllocatedImage(VulkanDevice& device, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, uint32_t arrayLayers = 1, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
	AllocatedImage(VulkanDevice& device, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
	AllocatedImage(VulkanDevice& device, void* data, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
	~AllocatedImage();

	AllocatedImage(AllocatedImage&&) = default;
	AllocatedImage(const AllocatedImage&) = delete;
	AllocatedImage& operator=(const AllocatedImage&) = delete;

	vma::UniqueImage image;
	vma::UniqueAllocation allocation;
	vk::UniqueImageView imageView;
	std::vector<vk::UniqueImageView> layerViews;

	vk::Extent3D imageExtent;
	vk::Format imageFormat;
	vk::ImageAspectFlags aspect;
	uint32_t mipLevels;
	uint32_t arrayLayers;

	VulkanDevice& device;
};

bool transitionImageLayout(vk::CommandBuffer cmd, AllocatedImage& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

#endif /* vulkanImage_h */
