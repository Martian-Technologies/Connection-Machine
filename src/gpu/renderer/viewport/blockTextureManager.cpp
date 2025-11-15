#include "blockTextureManager.h"

#include "gpu/vulkanDevice.h"
#include "util/algorithm.h"

#include <stb_image.h>

BlockTextureArray::~BlockTextureArray() {
	destroyImage(image);
	vkDestroySampler(device->getDevice(), sampler, nullptr);
	descriptorAllocator.cleanup();
}

void BlockTextureManager::init(VulkanDevice* device) {
	this->device = device;

	// create layout and descriptor set
	DescriptorLayoutBuilder textureLayoutBuilder;
	textureLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descriptorLayout = textureLayoutBuilder.build(device->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);

	makeTextureArray(1, { BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE, 1 });
	while (layerRectPackers.size() < textureArray->layerCount) layerRectPackers.push_back(RectPacker(Vec2Int(textureArray->textureSize.width, textureArray->textureSize.height)));
}

BlockTextureId BlockTextureManager::addTexture(const std::string& path) {
	// check if its already loaded
	auto iter = loadedTextureFiles.find(path);
	if (iter != loadedTextureFiles.end()) {
		return iter->second;
	}

	// --- Load pixels from file ---
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

	// --- Load pixels from file ---
	int textureWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &textureWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		throwFatalError("Failed to load texture: " + path);
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

BlockTextureCords BlockTextureManager::getBlockTextureCords(BlockTextureId blockTextureId, Vec2Int tileSize, Vec2Int smallestCordTile, Vec2Int blockSize) const {
	return getBlockTextureCords(blockTextureId, tileSize, smallestCordTile, blockSize, Vec2Int(0, tileSize.y * blockSize.y));
}

BlockTextureCords BlockTextureManager::getBlockTextureCords(
	BlockTextureId blockTextureId,
	Vec2Int tileSize,
	Vec2Int smallestCordTile,
	Vec2Int blockSize,
	Vec2Int textureStepSize
) const {
	auto blockTextureIter = blockTextures.find(blockTextureId);
	if (blockTextureIter == blockTextures.end()) {
		logError("Could not find block texture with id {}", "BlockTextureManager", blockTextureId);
		return { glm::vec2(0, 0), glm::vec2(0, 0), 0, glm::vec2(0, 0) };
	}
	unsigned int pixelsWidth = blockSize.x * tileSize.x;
	unsigned int pixelsHeight = blockSize.y * tileSize.y;
	if (pixelsWidth == pixelsHeight) {
		return BlockTextureCords(
			glm::vec2(
				(double)(blockTextureIter->second.textureOrigin.x + smallestCordTile.x * tileSize.x + pixelInset) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureOrigin.y + smallestCordTile.y * tileSize.y + pixelInset) / (double)textureArray->textureSize.height
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
				((double)(blockTextureIter->second.textureOrigin.x + smallestCordTile.x * tileSize.x) + otherPixelInset) / (double)textureArray->textureSize.width,
				(double)(blockTextureIter->second.textureOrigin.y + smallestCordTile.y * tileSize.y + pixelInset) / (double)textureArray->textureSize.height
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
				(double)(blockTextureIter->second.textureOrigin.x + smallestCordTile.x * tileSize.x + pixelInset) / (double)textureArray->textureSize.width,
				((double)(blockTextureIter->second.textureOrigin.y + smallestCordTile.y * tileSize.y) + otherPixelInset) / (double)textureArray->textureSize.height
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
	// destroy image
	vkDestroyDescriptorSetLayout(device->getDevice(), descriptorLayout, nullptr);

	textureArray.reset();
}

void BlockTextureManager::makeTextureArray(uint32_t layerCount, VkExtent3D textureSize) {
	assert(layerCount != 0);

	// logInfo("Making texture array {}, {}, {}", "BlockTextureManager", textureSize.width, textureSize.height, layerCount);
	textureArray = std::make_shared<BlockTextureArray>();
	textureArray->descriptorAllocator.init(device, 1, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.0f } });
	textureArray->textureSize = textureSize;
	textureArray->device = device;
	textureArray->layerCount = layerCount;
	textureArray->image = createImageArray(
		device,
		textureSize,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		false,
		textureArray->layerCount
	);

	textureArray->device->immediateSubmit([&](VkCommandBuffer cmd) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = textureArray->image.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = layerCount;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	});

	textureArray->descriptor = textureArray->descriptorAllocator.allocate(descriptorLayout);

	// create sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &textureArray->sampler);

	// write descriptor
	DescriptorWriter textureWriter;
	textureWriter.writeImage(0, textureArray->image.imageView, textureArray->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	textureWriter.updateSet(device->getDevice(), textureArray->descriptor);
}

void BlockTextureManager::addTextureToArray(const stbi_uc* pixels, Vec2Int textureSize, Vec2Int texPos, unsigned int textureLayer) {
	std::lock_guard<std::mutex> lock(descriptorMutex);

	if (textureLayer >= textureArray->layerCount) {
		resizeTextureArray(textureLayer + 1);
	} else {
		resizeTextureArray(textureArray->layerCount); // dont update textures that are being used
	}

	if (writtenLayers.size() <= textureLayer) writtenLayers.resize(textureLayer+1);

	VkDevice device = textureArray->device->getDevice();

	VkDeviceSize imageSize = static_cast<VkDeviceSize>(textureSize.x) * textureSize.y * 4;

	// --- Create staging buffer ---
	AllocatedBuffer stagingBuffer = createBuffer(textureArray->device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	// Copy pixels into the staging buffer
	vmaCopyMemoryToAllocation(textureArray->device->getAllocator(), pixels, stagingBuffer.allocation, 0, imageSize);

	// --- Upload to the array image layer ---
	textureArray->device->immediateSubmit([&](VkCommandBuffer cmd) {
		// Transition target layer to TRANSFER_DST_OPTIMAL
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = writtenLayers[textureLayer] ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = textureArray->image.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = textureLayer;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		// Copy staging buffer → texture layer
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = textureLayer;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { static_cast<int32_t>(texPos.x), static_cast<int32_t>(texPos.y), 0 };
		region.imageExtent = { static_cast<uint32_t>(textureSize.x), static_cast<uint32_t>(textureSize.y), 1 };

		vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, textureArray->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		// Transition layer to SHADER_READ_ONLY_OPTIMAL
		VkImageMemoryBarrier shaderBarrier = barrier;
		shaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		shaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		shaderBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		shaderBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &shaderBarrier);
	});

	writtenLayers[textureLayer] = true;

	destroyBuffer(stagingBuffer);
}

void BlockTextureManager::resizeTextureArray(uint32_t newLayerCount) {
	// save old img
	std::shared_ptr<BlockTextureArray> oldTextureArray = std::move(textureArray);

	// Create new image
	makeTextureArray(newLayerCount, oldTextureArray->textureSize);
	while (layerRectPackers.size() < textureArray->layerCount) layerRectPackers.push_back(RectPacker(Vec2Int(textureArray->textureSize.width, textureArray->textureSize.height)));

	device->immediateSubmit([&](VkCommandBuffer cmd) {
		// --- 1. Transition old image: SHADER_READ_ONLY_OPTIMAL -> TRANSFER_SRC_OPTIMAL
		VkImageMemoryBarrier barrierOld{};
		barrierOld.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrierOld.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrierOld.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrierOld.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrierOld.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrierOld.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrierOld.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrierOld.image = oldTextureArray->image.image;
		barrierOld.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, oldTextureArray->layerCount };

		// --- 2. Transition new image: UNDEFINED -> TRANSFER_DST_OPTIMAL
		VkImageMemoryBarrier barrierNew{};
		barrierNew.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrierNew.srcAccessMask = 0;
		barrierNew.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrierNew.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrierNew.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrierNew.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrierNew.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrierNew.image = textureArray->image.image;
		barrierNew.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, textureArray->layerCount };

		VkImageMemoryBarrier barriers[] = { barrierOld, barrierNew };
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

		// --- 3. Copy each layer from old to new
		for (uint32_t layer = 0; layer < oldTextureArray->layerCount; layer++) {
			VkImageCopy region{};
			region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1 };
			region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1 };
			region.extent = { oldTextureArray->textureSize.width, oldTextureArray->textureSize.height, 1 };
			vkCmdCopyImage(cmd, oldTextureArray->image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureArray->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		// --- 4. Transition new image: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
		VkImageMemoryBarrier finalBarrier{};
		finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		finalBarrier.image = textureArray->image.image;
		finalBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, newLayerCount };

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &finalBarrier);
	});

	writtenLayers.resize(textureArray->layerCount, true);
	for (unsigned int i = 0; i < textureArray->layerCount; i++) {
		writtenLayers[i] = true;
	}

	// --- 6. Update descriptor
	DescriptorWriter writer;
	writer.writeImage(0, textureArray->image.imageView, textureArray->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.updateSet(device->getDevice(), textureArray->descriptor);
}
