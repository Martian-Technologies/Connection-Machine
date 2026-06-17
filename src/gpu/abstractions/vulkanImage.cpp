#include "vulkanImage.h"

#include "gpu/abstractions/vulkanBuffer.h"
#include "gpu/vulkanInstance.h"

#include <iostream>

AllocatedImage::AllocatedImage(VulkanDevice& device, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, uint32_t arrayLayers, vk::SampleCountFlagBits samples)
	: device(device) {
	this->imageFormat = format;
	this->imageExtent = size;
	this->arrayLayers = arrayLayers;

	// calculate mipmapping levels
	if (mipmapped) {
		this->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	} else {
		this->mipLevels = 1;
	}

	vk::ImageCreateInfo imageInfo{};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = format;
	imageInfo.extent = size;
	imageInfo.mipLevels = this->mipLevels;
	imageInfo.arrayLayers = arrayLayers;
	imageInfo.samples = samples;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.usage = usage;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	vma::AllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = vma::MemoryUsage::eAuto;
	vmaAllocInfo.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

	vma::Allocator allocator = device.getAllocator();
	if (!allocator) {
		throwFatalError("VmaAllocator is null in createImage");
	}

	auto imageResult = allocator.createImageUnique(imageInfo, vmaAllocInfo);
	if (imageResult.result != vk::Result::eSuccess) {
		std::stringstream ss;
		ss << "vmaCreateImage failed, VkResult = " << static_cast<int>(imageResult.result);
		throwFatalError(ss.str());
	}
	this->image = std::move(imageResult.value.second);
	this->allocation = std::move(imageResult.value.first);

	// set the aspect flag to depth if image is using a depth format
	if (format == vk::Format::eD32Sfloat) {
		this->aspect = vk::ImageAspectFlagBits::eDepth;
	} else {
		this->aspect = vk::ImageAspectFlagBits::eColor;
	}

	vk::ImageViewCreateInfo imageViewInfo{};
	imageViewInfo.viewType = vk::ImageViewType::e2DArray;
	imageViewInfo.image = this->image.get();
	imageViewInfo.format = format;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = this->mipLevels;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = arrayLayers;
	imageViewInfo.subresourceRange.aspectMask = this->aspect;

	auto viewResult = device.getDevice().createImageViewUnique(imageViewInfo);
	if (viewResult.result != vk::Result::eSuccess) {
		std::stringstream ss;
		ss << "vkCreateImageView failed, VkResult = " << static_cast<int>(viewResult.result);
		throwFatalError(ss.str());
	}
	this->imageView = std::move(viewResult.value);

	for (unsigned int i = 0; i < arrayLayers; i++) {
		vk::ImageViewCreateInfo layerViewInfo{};
		layerViewInfo.viewType = vk::ImageViewType::e2D;
		layerViewInfo.image = this->image.get();
		layerViewInfo.format = format;
		layerViewInfo.subresourceRange.baseMipLevel = 0;
		layerViewInfo.subresourceRange.levelCount = this->mipLevels;
		layerViewInfo.subresourceRange.baseArrayLayer = i;
		layerViewInfo.subresourceRange.layerCount = 1;
		layerViewInfo.subresourceRange.aspectMask = this->aspect;
		auto layerViewResult = device.getDevice().createImageViewUnique(layerViewInfo);
		if (layerViewResult.result != vk::Result::eSuccess) {
			std::stringstream ss;
			ss << "vkCreateImageView failed, VkResult = " << static_cast<int>(layerViewResult.result);
			throwFatalError(ss.str());
		}
		layerViews.push_back(std::move(layerViewResult.value));
	}
}

AllocatedImage::AllocatedImage(VulkanDevice& device, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, vk::SampleCountFlagBits samples)
	: device(device) {
	this->imageFormat = format;
	this->imageExtent = size;
	this->arrayLayers = 1;

	// calculate mipmapping levels
	if (mipmapped) {
		this->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	} else {
		this->mipLevels = 1;
	}

	vk::ImageCreateInfo imageInfo{};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = format;
	imageInfo.extent = size;
	imageInfo.mipLevels = this->mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = samples;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.usage = usage;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	vma::AllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = vma::MemoryUsage::eAuto;
	vmaAllocInfo.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

	vma::Allocator allocator = device.getAllocator();
	if (!allocator) {
		throwFatalError("VmaAllocator is null in createImage");
	}

	auto imageResult = allocator.createImageUnique(imageInfo, vmaAllocInfo);
	if (imageResult.result != vk::Result::eSuccess) {
		std::stringstream ss;
		ss << "vmaCreateImage failed, VkResult = " << static_cast<int>(imageResult.result);
		throwFatalError(ss.str());
	}
	this->image = std::move(imageResult.value.second);
	this->allocation = std::move(imageResult.value.first);

	// set the aspect flag to depth if image is using a depth format
	if (format == vk::Format::eD32Sfloat) {
		this->aspect = vk::ImageAspectFlagBits::eDepth;
	} else {
		this->aspect = vk::ImageAspectFlagBits::eColor;
	}

	vk::ImageViewCreateInfo imageViewInfo{};
	imageViewInfo.viewType = vk::ImageViewType::e2D;
	imageViewInfo.image = this->image.get();
	imageViewInfo.format = format;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = this->mipLevels;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;
	imageViewInfo.subresourceRange.aspectMask = this->aspect;

	auto viewResult = device.getDevice().createImageViewUnique(imageViewInfo);
	if (viewResult.result != vk::Result::eSuccess) {
		std::stringstream ss;
		ss << "vkCreateImageView failed, VkResult = " << static_cast<int>(viewResult.result);
		throwFatalError(ss.str());
	}
	this->imageView = std::move(viewResult.value);
}

AllocatedImage::AllocatedImage(VulkanDevice& device, void* data, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, vk::SampleCountFlagBits samples)
	: AllocatedImage(device, size, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipmapped, samples) {
	// upload data to staging upload buffer
	size_t dataSize = size.depth * size.height * size.width * 4; // each pixel is 4 bytes (8bit channels RGBA)
	AllocatedBuffer uploadBuffer = createBuffer(device, dataSize, vk::BufferUsageFlagBits::eTransferSrc, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
	device.getAllocator().copyMemoryToAllocation(data, uploadBuffer.allocation.get(), 0, dataSize);

	// submit copy from staging to real
	device.immediateSubmit([&](vk::CommandBuffer cmd) {
		transitionImageLayout(cmd, *this, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

		vk::BufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		cmd.copyBufferToImage(uploadBuffer.buffer.get(), this->image.get(), vk::ImageLayout::eTransferDstOptimal, copyRegion);

		transitionImageLayout(cmd, *this, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	});

	destroyBuffer(uploadBuffer);
}

AllocatedImage::~AllocatedImage() {
	layerViews.clear();
	imageView.reset();
	image.reset();
	allocation.reset();
}

bool transitionImageLayout(vk::CommandBuffer cmd, AllocatedImage& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	// set the aspect flag to depth if image is using a depth format
	vk::ImageMemoryBarrier barrier{};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image.image.get();
	barrier.subresourceRange.aspectMask = image.aspect;

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	// determine access masks
	vk::PipelineStageFlags sourceStage;
	vk::PipelineStageFlags destinationStage;
	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	} else {
		logError("unsupported image layout transition!", "Vulkan");
		return false;
	}

	cmd.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);

	return true;
}
