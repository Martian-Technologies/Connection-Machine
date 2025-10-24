#include "vulkanImage.h"

#include "gpu/abstractions/vulkanBuffer.h"
#include "gpu/vulkanInstance.h"

#include <iostream>

AllocatedImage createImageArray(VulkanDevice* device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, uint32_t arrayLayers) {
    AllocatedImage newImage{};
    newImage.device = device;
    newImage.imageFormat = format;
    newImage.imageExtent = size;
    newImage.arrayLayers = arrayLayers;

    // calculate mipmapping levels
    if (mipmapped) {
        newImage.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    } else {
        newImage.mipLevels = 1;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = size;
    imageInfo.mipLevels = newImage.mipLevels;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    // VMA allocation
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VmaAllocator allocator = device->getAllocator();
    if (allocator == VK_NULL_HANDLE) {
        throwFatalError("VmaAllocator is null in createImage");
    }

    VkResult result = vmaCreateImage(allocator, &imageInfo, &vmaAllocInfo, &newImage.image, &newImage.allocation, nullptr);
    if (result != VK_SUCCESS) {
        std::stringstream ss;
        ss << "vmaCreateImage failed, VkResult = " << result;
        throwFatalError(ss.str());
    }
    if (newImage.image == VK_NULL_HANDLE) {
        throwFatalError("vmaCreateImage succeeded but returned null VkImage");
    }

    // set the aspect flag to depth if image is using a depth format
    if (format == VK_FORMAT_D32_SFLOAT) {
        newImage.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        newImage.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // create image view
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.pNext = nullptr;

    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imageViewInfo.image = newImage.image;
    imageViewInfo.format = format;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = newImage.mipLevels;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = arrayLayers;
    imageViewInfo.subresourceRange.aspectMask = newImage.aspect;

    VkResult r2 = vkCreateImageView(device->getDevice(), &imageViewInfo, nullptr, &newImage.imageView);
    if (r2 != VK_SUCCESS) {
        std::stringstream ss;
        ss << "vkCreateImageView failed, VkResult = " << r2;
        // cleanup image/allocation before throwing
        vmaDestroyImage(allocator, newImage.image, newImage.allocation);
        throwFatalError(ss.str());
    }

    return newImage;
}

AllocatedImage createImage(VulkanDevice* device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped) {
    AllocatedImage newImage{};
    newImage.device = device;
    newImage.imageFormat = format;
    newImage.imageExtent = size;
    newImage.arrayLayers = 1;

    // calculate mipmapping levels
    if (mipmapped) {
        newImage.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    } else {
        newImage.mipLevels = 1;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = size;
    imageInfo.mipLevels = newImage.mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    // VMA allocation
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VmaAllocator allocator = device->getAllocator();
    if (allocator == VK_NULL_HANDLE) {
        throwFatalError("VmaAllocator is null in createImage");
    }

    VkResult result = vmaCreateImage(allocator, &imageInfo, &vmaAllocInfo, &newImage.image, &newImage.allocation, nullptr);
    if (result != VK_SUCCESS) {
        std::stringstream ss;
        ss << "vmaCreateImage failed, VkResult = " << result;
        throwFatalError(ss.str());
    }
    if (newImage.image == VK_NULL_HANDLE) {
        throwFatalError("vmaCreateImage succeeded but returned null VkImage");
    }

    // set the aspect flag to depth if image is using a depth format
    if (format == VK_FORMAT_D32_SFLOAT) {
        newImage.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        newImage.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // create image view
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.pNext = nullptr;

    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.image = newImage.image;
    imageViewInfo.format = format;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = newImage.mipLevels;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 1;
    imageViewInfo.subresourceRange.aspectMask = newImage.aspect;

    VkResult r2 = vkCreateImageView(device->getDevice(), &imageViewInfo, nullptr, &newImage.imageView);
    if (r2 != VK_SUCCESS) {
        std::stringstream ss;
        ss << "vkCreateImageView failed, VkResult = " << r2;
        // cleanup image/allocation before throwing
        vmaDestroyImage(allocator, newImage.image, newImage.allocation);
        throwFatalError(ss.str());
    }

    return newImage;
}


AllocatedImage createImage(VulkanDevice* device, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped) {
	// upload data to staging upload buffer
	size_t dataSize = size.depth * size.height * size.width * 4; // each pixel is 4 bytes (8bit channels RGBA)
	AllocatedBuffer uploadBuffer = createBuffer(device, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	vmaCopyMemoryToAllocation(device->getAllocator(), data, uploadBuffer.allocation, 0, dataSize);

	// create image
	AllocatedImage newImage = createImage(device, size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped); // not exactly sure why we need transfer src but tutorial says so

	// submit copy from staging to real
	device->immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, newImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		transitionImageLayout(cmd, newImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});

	destroyBuffer(uploadBuffer);

	return newImage;
}

void destroyImage(AllocatedImage& image) {
	vkDestroyImageView(image.device->getDevice(), image.imageView, nullptr);
    vmaDestroyImage(image.device->getAllocator(), image.image, image.allocation);
}

bool transitionImageLayout(VkCommandBuffer cmd, AllocatedImage& image, VkImageLayout oldLayout, VkImageLayout newLayout) {
	// set the aspect flag to depth if image is using a depth format
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image.image;
	barrier.subresourceRange.aspectMask = image.aspect;

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	// determine access masks
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		logError("unsupported image layout transition!", "Vulkan");
		return false;
	}

	vkCmdPipelineBarrier(
		cmd,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	return true;
}
