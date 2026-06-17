#include "blockTextureManager.h"

#include "gpu/abstractions/vulkanBuffer.h"
#include "gpu/vulkanDevice.h"
#include "util/algorithm.h"

#include <stb_image.h>

BlockTextureArray::~BlockTextureArray() {
	sampler.reset();
	descriptorAllocator.cleanup();
}

void BlockTextureManager::init(VulkanDevice& device) {
	this->device = &device;

	DescriptorLayoutBuilder textureLayoutBuilder;
	textureLayoutBuilder.addBinding(0, vk::DescriptorType::eCombinedImageSampler);
	descriptorLayout = textureLayoutBuilder.build(this->device->getDevice(), vk::ShaderStageFlagBits::eFragment);

	makeTextureArray(1, { BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE, 1 });
	while (layerRectPackers.size() < textureArray->layerCount) layerRectPackers.push_back(RectPacker(Vec2Int(textureArray->textureSize.width, textureArray->textureSize.height)));
}

BlockTextureId BlockTextureManager::addTexture(const std::string& path) {
	// check if its already loaded
	auto iter = loadedTextureFiles.find(path);
	if (iter != loadedTextureFiles.end()) {
		return iter->second;
	}

	// Load pixels from file
	int textureWidth, textureHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &textureWidth, &textureHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		logError("Failed to load texture: {}", "BlockTextureManager", path);
		return 0;
	}

	RectPacker::RectID rectId;
	unsigned int layer;
	Vec2Int pos;
	for (layer = 0; layer < layerRectPackers.size(); layer++) {
		rectId = layerRectPackers[layer].tryAddRect({textureWidth, textureHeight});
		if (rectId) {
			pos = layerRectPackers[layer].getRect(rectId).first;
			break;
		}
	}
	if (!rectId) {
		layerRectPackers.push_back(Vec2Int(textureArray->textureSize.width, textureArray->textureSize.height));
		rectId = layerRectPackers[layer].tryAddRect({textureWidth, textureHeight});
		if (!rectId) {
			logError("Could not fit texture with size ({}, {})", "BlockTextureManager", textureWidth, textureHeight);
			stbi_image_free(pixels);
			return 0;
		}
	}
	addTextureToArray(pixels, { textureWidth, textureHeight }, pos, layer);

	stbi_image_free(pixels);

	BlockTextureId blockTextureId = findUnusedKey<BlockTextureId>(blockTextures, 1);
	blockTextures.try_emplace(blockTextureId, pos, Vec2Int(textureWidth, textureHeight), layer, rectId);

	loadedTextureFiles.emplace(path, blockTextureId);

	return blockTextureId;
}

void BlockTextureManager::refreshBlockTexture(const std::string& path) {
	// check if its already loaded
	auto iter = loadedTextureFiles.find(path);
	if (iter == loadedTextureFiles.end()) {
		logError("Can't refresh block texture \"{}\" that is not loaded.", "BlockTextureManager", path);
		return;
	}
	BlockTexture& blockTexture = blockTextures.at(iter->second);

	// Load pixels from file
	int textureWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &textureWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		logError("Failed to load texture: {}", "BlockTextureManager", path);
		return;
	}

	if (blockTexture.textureSize.x != textureWidth || blockTexture.textureSize.y != texHeight) {
		logError(
			"Can't refresh block texture \"{}\" that is not the same size. Before: {}x{}, After: {}x{}",
			"BlockTextureManager",
			path,
			blockTexture.textureSize.x,
			blockTexture.textureSize.y,
			textureWidth,
			texHeight
		);
		stbi_image_free(pixels);
		return;
	}

	addTextureToArray(pixels, { textureWidth, texHeight }, blockTexture.textureOrigin, blockTexture.textureLayer);
	stbi_image_free(pixels);
}

BlockTextureId BlockTextureManager::addTexture(const unsigned char* pixels, int textureWidth, int textureHeight) {
	unsigned int layer;
	Vec2Int pos;
	RectPacker::RectID rectId;
	for (layer = 0; layer < layerRectPackers.size(); layer++) {
		rectId = layerRectPackers[layer].tryAddRect({textureWidth, textureHeight});
		if (rectId) {
			pos = layerRectPackers[layer].getRect(rectId).first;
			break;
		}
	}
	if (!rectId) {
		layerRectPackers.push_back(Vec2Int(textureArray->textureSize.width, textureArray->textureSize.height));
		rectId = layerRectPackers[layer].tryAddRect({textureWidth, textureHeight});
		if (!rectId) {
			logError("Could not fit texture with size ({}, {})", "BlockTextureManager", textureWidth, textureHeight);
			return 0;
		}
	}
	addTextureToArray(pixels, { textureWidth, textureHeight }, pos, layer);

	BlockTextureId blockTextureId = findUnusedKey<BlockTextureId>(blockTextures, 1);
	blockTextures.try_emplace(blockTextureId, pos, Vec2Int(textureWidth, textureHeight), layer, rectId);

	return blockTextureId;
}

void BlockTextureManager::updateBlockTexture(const unsigned char* pixels, BlockTextureId blockTextureId) {
	auto iter = blockTextures.find(blockTextureId);
	if (iter == blockTextures.end()) {
		logError("Can't update block texture with id \"{}\" when it doesn't exist.", "BlockTextureManager", blockTextureId);
		return;
	}
	BlockTexture& blockTexture = iter->second;

	addTextureToArray(pixels, blockTexture.textureSize, blockTexture.textureOrigin, blockTexture.textureLayer);
}

void BlockTextureManager::removeBlockTexture(const std::string& path) {
	auto pathIter = loadedTextureFiles.find(path);
	if (pathIter == loadedTextureFiles.end()) {
		logError("Can't refresh block texture \"{}\" that is not loaded.", "BlockTextureManager", path);
		return;
	}

	auto iter = blockTextures.find(pathIter->second);
	if (iter == blockTextures.end()) {
		logError(
			"Can't remove block texture with id \"{}\" when it doesn't exist. (THIS SHOULD NEVER HAPPEN!!! PLEASE REPORT!!!)",
			"BlockTextureManager",
			pathIter->second
		);
		return;
	}
	layerRectPackers[iter->second.textureLayer].removeRect(iter->second.rectId);
	blockTextures.erase(iter);
	loadedTextureFiles.erase(pathIter);
}

void BlockTextureManager::removeBlockTexture(BlockTextureId blockTextureId) {
	auto iter = blockTextures.find(blockTextureId);
	if (iter == blockTextures.end()) {
		logError("Can't remove block texture with id \"{}\" when it doesn't exist.", "BlockTextureManager", blockTextureId);
		return;
	}
	layerRectPackers[iter->second.textureLayer].removeRect(iter->second.rectId);
	blockTextures.erase(iter);

	auto pathIter = std::find_if(loadedTextureFiles.begin(), loadedTextureFiles.end(), [blockTextureId](const std::pair<std::string, BlockTextureId>& pair) {
		return pair.second == blockTextureId;
	});
	if (pathIter != loadedTextureFiles.end()) loadedTextureFiles.erase(pathIter);
}

const int pixelInset = 1;

BlockTextureCords BlockTextureManager::getBlockTextureCords(BlockTextureId blockTextureId) const {
	auto blockTextureIter = blockTextures.find(blockTextureId);
	if (blockTextureIter == blockTextures.end()) {
		logError("Could not find block texture with id {}", "BlockTextureManager", blockTextureId);
		return { glm::vec2(0, 0), glm::vec2(0, 0), 0, glm::vec2(0, 0) };
	}
	if (blockTextureIter->second.textureSize.x == blockTextureIter->second.textureSize.y) {
		return BlockTextureCords(
			glm::vec2(
				(double)(blockTextureIter->second.textureOrigin.x + pixelInset) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureOrigin.y + pixelInset) / (double)textureArray->textureSize.height
			),
			glm::vec2(
				(double)(blockTextureIter->second.textureSize.x - pixelInset * 2) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureSize.y - pixelInset * 2) / (double)textureArray->textureSize.height
			),
			blockTextureIter->second.textureLayer,
			glm::vec2(0, 0)
		);
	} else if (blockTextureIter->second.textureSize.x > blockTextureIter->second.textureSize.y) {
		double otherPixelInset = (double)pixelInset * ((double)blockTextureIter->second.textureSize.x / (double)blockTextureIter->second.textureSize.y);
		return BlockTextureCords(
			glm::vec2(
				((double)(blockTextureIter->second.textureOrigin.x) + otherPixelInset) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureOrigin.y + pixelInset) / (double)textureArray->textureSize.height
			),
			glm::vec2(
				((double)(blockTextureIter->second.textureSize.x) - otherPixelInset * 2.) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureSize.y - pixelInset * 2) / (double)textureArray->textureSize.height
			),
			blockTextureIter->second.textureLayer,
			glm::vec2(0, 0)
		);
	} else {
		double otherPixelInset = (double)pixelInset * ((double)blockTextureIter->second.textureSize.y / (double)blockTextureIter->second.textureSize.x);
		return BlockTextureCords(
			glm::vec2(
				(double)(blockTextureIter->second.textureOrigin.x + pixelInset) / (double)textureArray->textureSize.width,
				((double)(blockTextureIter->second.textureOrigin.y) + otherPixelInset) / (double)textureArray->textureSize.height
			),
			glm::vec2(
				(double)(blockTextureIter->second.textureSize.x - pixelInset * 2) / (double)textureArray->textureSize.width,
				((double)(blockTextureIter->second.textureSize.y) - otherPixelInset * 2.) / (double)textureArray->textureSize.height
			),
			blockTextureIter->second.textureLayer,
			glm::vec2(0, 0)
		);
	}
}

BlockTextureCords BlockTextureManager::getBlockTextureCords(BlockTextureId blockTextureId, Vec2Int smallestTextureCord, Vec2Int textureSize) const {
	return getBlockTextureCords(blockTextureId, smallestTextureCord, textureSize, Vec2Int(0, 0));
}

BlockTextureCords BlockTextureManager::getBlockTextureCords(
	BlockTextureId blockTextureId,
	Vec2Int smallestTextureCord,
	Vec2Int textureSize,
	Vec2Int textureStepSize
) const {
	auto blockTextureIter = blockTextures.find(blockTextureId);
	if (blockTextureIter == blockTextures.end()) {
		logError("Could not find block texture with id {}", "BlockTextureManager", blockTextureId);
		return { glm::vec2(0, 0), glm::vec2(0, 0), 0, glm::vec2(0, 0) };
	}
	unsigned int pixelsWidth = textureSize.x;
	unsigned int pixelsHeight = textureSize.y;
	if (pixelsWidth == pixelsHeight) {
		return BlockTextureCords(
			glm::vec2(
				(double)(blockTextureIter->second.textureOrigin.x + smallestTextureCord.x + pixelInset) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureOrigin.y + smallestTextureCord.y + pixelInset) / (double)textureArray->textureSize.height
			),
			glm::vec2(
				(double)(pixelsWidth - pixelInset * 2) / (double)textureArray->textureSize.width,
				(double)(pixelsHeight - pixelInset * 2) / (double)textureArray->textureSize.height
			),
			blockTextureIter->second.textureLayer,
			glm::vec2((double)textureStepSize.x / (double)textureArray->textureSize.width, (double)textureStepSize.y / (double)textureArray->textureSize.height)
		);
	} else if (pixelsWidth > pixelsHeight) {
		double otherPixelInset = (double)pixelInset * ((double)pixelsWidth / (double)pixelsHeight);
		return BlockTextureCords(
			glm::vec2(
				((double)(blockTextureIter->second.textureOrigin.x + smallestTextureCord.x) + otherPixelInset) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureOrigin.y + smallestTextureCord.y + pixelInset) / (double)textureArray->textureSize.height
			),
			glm::vec2(
				((double)pixelsWidth - otherPixelInset * 2.) / (double)textureArray->textureSize.width,
				(double)(pixelsHeight - pixelInset * 2) / (double)textureArray->textureSize.height
			),
			blockTextureIter->second.textureLayer,
			glm::vec2((double)textureStepSize.x / (double)textureArray->textureSize.width, (double)textureStepSize.y / (double)textureArray->textureSize.height)
		);
	} else {
		double otherPixelInset = (double)pixelInset * ((double)pixelsHeight / (double)pixelsWidth);
		return BlockTextureCords(
			glm::vec2(
				(double)(blockTextureIter->second.textureOrigin.x + smallestTextureCord.x + pixelInset) / (double)textureArray->textureSize.width,
				((double)(blockTextureIter->second.textureOrigin.y + smallestTextureCord.y) + otherPixelInset) / (double)textureArray->textureSize.height
			),
			glm::vec2(
				(double)(pixelsWidth - pixelInset * 2) / (double)textureArray->textureSize.width,
				((double)pixelsHeight - otherPixelInset * 2.) / (double)textureArray->textureSize.height
			),
			blockTextureIter->second.textureLayer,
			glm::vec2((double)textureStepSize.x / (double)textureArray->textureSize.width, (double)textureStepSize.y / (double)textureArray->textureSize.height)
		);
	}
}

void BlockTextureManager::cleanup() {
	descriptorLayout.reset();
	textureArray.reset();
}

void BlockTextureManager::makeTextureArray(uint32_t layerCount, vk::Extent3D textureSize) {
	assert(layerCount != 0);

	textureArray = std::make_shared<BlockTextureArray>();
	textureArray->descriptorAllocator.init(*device, 1, { { vk::DescriptorType::eCombinedImageSampler, 1.0f } });
	textureArray->textureSize = textureSize;
	textureArray->device = device;
	textureArray->layerCount = layerCount;
	textureArray->image.emplace(
		*device,
		textureSize,
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
		false,
		textureArray->layerCount
	);

	textureArray->device->immediateSubmit([&](vk::CommandBuffer cmd) {
		vk::ImageMemoryBarrier barrier{};
		barrier.oldLayout = vk::ImageLayout::eUndefined;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = textureArray->image->image.get();
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = layerCount;
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			nullptr,
			nullptr,
			barrier
		);
	});

	textureArray->descriptor = textureArray->descriptorAllocator.allocate(descriptorLayout.get());

	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.minFilter = vk::Filter::eLinear;
	samplerInfo.magFilter = vk::Filter::eLinear;
	auto samplerResult = device->getDevice().createSamplerUnique(samplerInfo);
	if (samplerResult.result != vk::Result::eSuccess) throwFatalError("failed to create sampler");
	textureArray->sampler = std::move(samplerResult.value);

	DescriptorWriter textureWriter;
	textureWriter.writeImage(0, textureArray->image->imageView.get(), textureArray->sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
	textureWriter.updateSet(device->getDevice(), textureArray->descriptor);
}

void BlockTextureManager::addTextureToArray(const stbi_uc* pixels, Vec2Int textureSize, Vec2Int texPos, unsigned int textureLayer) {
	std::lock_guard<std::mutex> lock(descriptorMutex);

	if (textureLayer >= textureArray->layerCount) {
		resizeTextureArray(textureLayer + 1);
	} else {
		resizeTextureArray(textureArray->layerCount);
	}

	if (writtenLayers.size() <= textureLayer) writtenLayers.resize(textureLayer+1);

	vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(textureSize.x) * textureSize.y * 4;

	// Create staging buffer
	AllocatedBuffer stagingBuffer = createBuffer(*textureArray->device, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

	// Copy pixels into the staging buffer
	textureArray->device->getAllocator().copyMemoryToAllocation(pixels, stagingBuffer.allocation.get(), 0, imageSize);

	// Upload to the array image layer
	textureArray->device->immediateSubmit([&](vk::CommandBuffer cmd) {
		vk::ImageMemoryBarrier barrier{};
		barrier.oldLayout = writtenLayers[textureLayer] ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eUndefined;
		barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = textureArray->image->image.get();
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = textureLayer;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier);

		// Copy staging buffer -> texture layer
		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = textureLayer;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = vk::Offset3D{ static_cast<int32_t>(texPos.x), static_cast<int32_t>(texPos.y), 0 };
		region.imageExtent = vk::Extent3D{ static_cast<uint32_t>(textureSize.x), static_cast<uint32_t>(textureSize.y), 1 };

		cmd.copyBufferToImage(stagingBuffer.buffer.get(), textureArray->image->image.get(), vk::ImageLayout::eTransferDstOptimal, region);

		vk::ImageMemoryBarrier shaderBarrier = barrier;
		shaderBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		shaderBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		shaderBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		shaderBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, shaderBarrier);
	});

	writtenLayers[textureLayer] = true;

	destroyBuffer(stagingBuffer);
}

void BlockTextureManager::resizeTextureArray(uint32_t newLayerCount) {
	std::shared_ptr<BlockTextureArray> oldTextureArray = std::move(textureArray);

	makeTextureArray(newLayerCount, oldTextureArray->textureSize);
	while (layerRectPackers.size() < textureArray->layerCount) layerRectPackers.push_back(RectPacker(Vec2Int(textureArray->textureSize.width, textureArray->textureSize.height)));

	device->immediateSubmit([&](vk::CommandBuffer cmd) {
		// 1. Transition old image: SHADER_READ_ONLY_OPTIMAL -> TRANSFER_SRC_OPTIMAL
		vk::ImageMemoryBarrier barrierOld{};
		barrierOld.srcAccessMask = vk::AccessFlagBits::eShaderRead;
		barrierOld.dstAccessMask = vk::AccessFlagBits::eTransferRead;
		barrierOld.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrierOld.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrierOld.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrierOld.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		// 2. Transition new image: UNDEFINED -> TRANSFER_DST_OPTIMAL
		barrierOld.image = oldTextureArray->image->image.get();
		barrierOld.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, oldTextureArray->layerCount };

		vk::ImageMemoryBarrier barrierNew{};
		barrierNew.srcAccessMask = {};
		barrierNew.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrierNew.oldLayout = vk::ImageLayout::eUndefined;
		barrierNew.newLayout = vk::ImageLayout::eTransferDstOptimal;
		barrierNew.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrierNew.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrierNew.image = textureArray->image->image.get();
		barrierNew.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, textureArray->layerCount };

		vk::ImageMemoryBarrier barriers[] = { barrierOld, barrierNew };
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 0, nullptr, 2, barriers);

		// 3. Copy each layer from old to new
		for (uint32_t layer = 0; layer < oldTextureArray->layerCount; layer++) {
			vk::ImageCopy region{};
			region.srcSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, layer, 1 };
			region.dstSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, layer, 1 };
			region.extent = vk::Extent3D{ oldTextureArray->textureSize.width, oldTextureArray->textureSize.height, 1 };
			cmd.copyImage(oldTextureArray->image->image.get(), vk::ImageLayout::eTransferSrcOptimal, textureArray->image->image.get(), vk::ImageLayout::eTransferDstOptimal, region);
		}

		// 4. Transition new image: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
		vk::ImageMemoryBarrier finalBarrier{};
		finalBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		finalBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		finalBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		finalBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		finalBarrier.image = textureArray->image->image.get();
		finalBarrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, newLayerCount };

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, finalBarrier);
	});

	writtenLayers.resize(textureArray->layerCount, true);
	for (unsigned int i = 0; i < textureArray->layerCount; i++) {
		writtenLayers[i] = true;
	}

	// 6. Update descriptor
	DescriptorWriter writer;
	writer.writeImage(0, textureArray->image->imageView.get(), textureArray->sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
	writer.updateSet(device->getDevice(), textureArray->descriptor);
}
