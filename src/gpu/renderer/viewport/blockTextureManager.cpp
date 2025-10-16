#include "blockTextureManager.h"

#include "gpu/vulkanDevice.h"
#include "util/algorithm.h"

namespace {
constexpr uint32_t kSkippedTailMipLevels = 2;
constexpr float kBlockSamplerMipBias = -0.5f;
}

BlockTextureArray::~BlockTextureArray() {
	destroyImage(image);
	vkDestroySampler(device->getDevice(), sampler, nullptr);
}

void BlockTextureManager::init(VulkanDevice* device) {
	this->device = device;

	descriptorAllocator.init(device, 100, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.0f } });

	VkExtent3D textureSize = { 4096, 4096, 1 };
	textureArray = std::make_shared<BlockTextureArray>();
	textureArray->textureSize = textureSize;
	textureArray->device = device;
	textureArray->maxLayers = 1;
	textureArray->nextFreeLayer = 0;
	textureArray->layerInitialized.assign(textureArray->maxLayers, false);
	textureArray->image =
		createImage(device, textureSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true, textureArray->maxLayers);

	// create layout and descriptor set
	DescriptorLayoutBuilder textureLayoutBuilder;
	textureLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descriptorLayout = textureLayoutBuilder.build(device->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);
	textureArray->descriptor = descriptorAllocator.allocate(descriptorLayout);

	// create sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.mipLodBias = kBlockSamplerMipBias;
	samplerInfo.minLod = 0.0f;
	const uint32_t totalMipLevels = std::max(1u, textureArray->image.mipLevels);
	const uint32_t usableMipLevels =
		totalMipLevels > kSkippedTailMipLevels ? totalMipLevels - kSkippedTailMipLevels : totalMipLevels;
	samplerInfo.maxLod = static_cast<float>(usableMipLevels - 1);
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &textureArray->sampler);

	// write descriptor
	DescriptorWriter textureWriter;
	textureWriter
		.writeImage(0, textureArray->image.imageView, textureArray->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	textureWriter.updateSet(device->getDevice(), textureArray->descriptor);
}

BlockTextureId BlockTextureManager::addTexture(const std::string& path) {
	// check if its already loaded
	auto iter = loadedTextureFiles.find(path);
	if (iter != loadedTextureFiles.end()) {
		return iter->second;
	}

	// --- Load pixels from file ---
	int textureWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &textureWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		logError("Failed to load texture: {}", "BlockTextureManager", path);
		return 0;
	}

	addTextureToArray(pixels, { textureWidth, texHeight }, { 0, 0 }, textureArray->nextFreeLayer);
	stbi_image_free(pixels);

	BlockTextureId blockTextureId = findUnusedKey<BlockTextureId>(blockTextures, 1);
	blockTextures.try_emplace(blockTextureId, glm::vec2(0, 0), glm::vec2(textureWidth, texHeight), textureArray->nextFreeLayer);

	loadedTextureFiles.emplace(path, blockTextureId);

	textureArray->nextFreeLayer++;

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

BlockTextureId BlockTextureManager::addTexture(const stbi_uc* pixels, int textureWidth, int textureHeight) {
	addTextureToArray(pixels, { textureWidth, textureHeight }, { 0, 0 }, textureArray->nextFreeLayer);

	BlockTextureId blockTextureId = findUnusedKey<BlockTextureId>(blockTextures, 1);
	blockTextures.try_emplace(blockTextureId, glm::vec2(0, 0), glm::vec2(textureWidth, textureHeight), textureArray->nextFreeLayer);

	textureArray->nextFreeLayer++;

	return blockTextureId;
}

void BlockTextureManager::updateBlockTexture(const stbi_uc* pixels, BlockTextureId blockTextureId) {
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
		logError("Can't remove block texture with id \"{}\" when it doesn't exist. (THIS SHOULD NEVER HAPPEN!!! PLEASE REPORT!!!)", "BlockTextureManager", pathIter->second);
		return;
	}
	blockTextures.erase(iter);
	loadedTextureFiles.erase(pathIter);
}

void BlockTextureManager::removeBlockTexture(BlockTextureId blockTextureId) {
	auto iter = blockTextures.find(blockTextureId);
	if (iter == blockTextures.end()) {
		logError("Can't remove block texture with id \"{}\" when it doesn't exist.", "BlockTextureManager", blockTextureId);
		return;
	}
	blockTextures.erase(iter);

	auto pathIter = std::find_if(loadedTextureFiles.begin(), loadedTextureFiles.end(), [blockTextureId](const std::pair<std::string, BlockTextureId>& pair){
		return pair.second == blockTextureId;
	});
	if (pathIter != loadedTextureFiles.end()) loadedTextureFiles.erase(pathIter);
}

BlockTextureCords BlockTextureManager::getBlockTextureCords(BlockTextureId blockTextureId) const {
	auto blockTextureIter = blockTextures.find(blockTextureId);
	if (blockTextureIter == blockTextures.end()) {
		logError("Could not find block texture with id {}", "BlockTextureManager", blockTextureId);
		return { glm::vec2(0, 0), glm::vec2(0, 0), 0 };
	}
	return BlockTextureCords(
		glm::vec2(
			(double)(blockTextureIter->second.textureOrigin.x) / (double)textureArray->textureSize.width,
			(double)(blockTextureIter->second.textureOrigin.y) / (double)textureArray->textureSize.height
		),
		glm::vec2(
			(double)(blockTextureIter->second.textureSize.x) / (double)textureArray->textureSize.width,
			(double)(blockTextureIter->second.textureSize.y) / (double)textureArray->textureSize.height
		),
		blockTextureIter->second.textureLayer
	);
}

BlockTextureCords BlockTextureManager::getBlockTextureCords(BlockTextureId blockTextureId, Vec2Int tileSize, Vec2Int smallestCordTile, Vec2Int blockSize) const {
	auto blockTextureIter = blockTextures.find(blockTextureId);
	if (blockTextureIter == blockTextures.end()) {
		logError("Could not find block texture with id {}", "BlockTextureManager", blockTextureId);
		return { glm::vec2(0, 0), glm::vec2(0, 0), 0 };
	}
	return BlockTextureCords(
		glm::vec2(
			(double)(blockTextureIter->second.textureOrigin.x + smallestCordTile.x * tileSize.x) / (double)textureArray->textureSize.width,
			(double)(blockTextureIter->second.textureOrigin.y + smallestCordTile.y * tileSize.y) / (double)textureArray->textureSize.height
		),
		glm::vec2((double)(blockSize.x * tileSize.x) / (double)textureArray->textureSize.width, (double)(blockSize.y * tileSize.y) / (double)(textureArray->textureSize.height)),
		blockTextureIter->second.textureLayer
	);
}

void BlockTextureManager::cleanup() {
	// destroy image
	vkDestroyDescriptorSetLayout(device->getDevice(), descriptorLayout, nullptr);

	textureArray.reset();

	// destroy descriptor allocator
	descriptorAllocator.cleanup();
}

void BlockTextureManager::addTextureToArray(const stbi_uc* pixels, glm::vec2 textureSize, glm::vec2 texPos, unsigned int textureLayer) {
	if (textureLayer >= textureArray->maxLayers) {
		resizeTextureArray(textureLayer + 1);
	}

	const bool layerAlreadyInitialized = textureArray->layerInitialized.size() > textureLayer && textureArray->layerInitialized[textureLayer];

	VkDevice device = textureArray->device->getDevice();

	VkDeviceSize imageSize = static_cast<VkDeviceSize>(textureSize.x) * textureSize.y * 4;

	// --- Create staging buffer ---
	AllocatedBuffer stagingBuffer =
		createBuffer(textureArray->device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	// Copy pixels into the staging buffer
	vmaCopyMemoryToAllocation(textureArray->device->getAllocator(), pixels, stagingBuffer.allocation, 0, imageSize);

	// --- Upload to the array image layer ---
	textureArray->device->immediateSubmit([&](VkCommandBuffer cmd) {
		// Transition target layer to TRANSFER_DST_OPTIMAL
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = layerAlreadyInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = textureArray->image.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = textureArray->image.mipLevels;
		barrier.subresourceRange.baseArrayLayer = textureLayer;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = layerAlreadyInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		VkPipelineStageFlags srcStage = layerAlreadyInitialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

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

		if (textureArray->image.mipLevels > 1) {
			generateMipmapsCmd(cmd, textureArray->image, textureLayer, 1);
		} else {
			VkImageMemoryBarrier shaderBarrier = barrier;
			shaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			shaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			shaderBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			shaderBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &shaderBarrier);
		}
	});

	destroyBuffer(stagingBuffer);
	textureArray->layerInitialized[textureLayer] = true;
}

void BlockTextureManager::resizeTextureArray(uint32_t newLayerCount) {
	VulkanDevice* dev = textureArray->device;
	auto oldImage = textureArray->image;
	uint32_t oldLayerCount = textureArray->maxLayers;

	AllocatedImage newImage = createImage(
		dev,
		textureArray->textureSize,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		true,
		newLayerCount
	);

	dev->immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, newImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		for (uint32_t layer = 0; layer < oldLayerCount; layer++) {
			const bool hadData = textureArray->layerInitialized.size() > layer && textureArray->layerInitialized[layer];
			if (!hadData) {
				continue;
			}

			VkImageMemoryBarrier srcBarrier{};
			srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			srcBarrier.image = oldImage.image;
			srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			srcBarrier.subresourceRange.baseMipLevel = 0;
			srcBarrier.subresourceRange.levelCount = oldImage.mipLevels;
			srcBarrier.subresourceRange.baseArrayLayer = layer;
			srcBarrier.subresourceRange.layerCount = 1;
			srcBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			srcBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &srcBarrier);

			VkImageCopy region{};
			region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1 };
			region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1 };
			region.extent = { textureArray->textureSize.width, textureArray->textureSize.height, 1 };
			vkCmdCopyImage(cmd, oldImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		if (newImage.mipLevels > 1) {
			generateMipmapsCmd(cmd, newImage, 0, newLayerCount);
		} else {
			transitionImageLayout(cmd, newImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	});

	std::vector<bool> updatedInitialization(newLayerCount, false);
	for (uint32_t layer = 0; layer < oldLayerCount && layer < textureArray->layerInitialized.size(); layer++) {
		updatedInitialization[layer] = textureArray->layerInitialized[layer];
	}

	destroyImage(oldImage);
	textureArray->image = newImage;
	textureArray->maxLayers = newLayerCount;
	textureArray->layerInitialized = std::move(updatedInitialization);

	DescriptorWriter writer;
	writer.writeImage(0, newImage.imageView, textureArray->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.updateSet(dev->getDevice(), textureArray->descriptor);
}
