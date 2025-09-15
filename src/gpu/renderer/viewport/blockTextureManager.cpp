#include "blockTextureManager.h"

#include "computerAPI/directoryManager.h"
#include "gpu/vulkanDevice.h"

#include <stb_image.h>

BlockTexture::~BlockTexture() {
	destroyImage(image);
	vkDestroySampler(device->getDevice(), sampler, nullptr);
}

void BlockTextureManager::init(VulkanDevice* device) {
	this->device = device;

	descriptorAllocator.init(device, 100, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.0f } });

	// upload texture
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load((DirectoryManager::getResourceDirectory() / "logicTiles.png").generic_string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkExtent3D size{ (uint32_t)texWidth, (uint32_t)texHeight, 1 };
	mainTexture = std::make_shared<BlockTexture>();
	mainTexture->device = device;
	mainTexture->image = createImage(device, pixels, size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
	stbi_image_free(pixels);

	// create layout and descriptor set
	DescriptorLayoutBuilder textureLayoutBuilder;
	textureLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descriptorLayout = textureLayoutBuilder.build(device->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);
	mainTexture->descriptor = descriptorAllocator.allocate(descriptorLayout);

	// create sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &mainTexture->sampler);

	// write descriptor
	DescriptorWriter textureWriter;
	textureWriter.writeImage(0, mainTexture->image.imageView, mainTexture->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	textureWriter.updateSet(device->getDevice(), mainTexture->descriptor);
}

void BlockTextureManager::cleanup() {

	// destroy image
	vkDestroyDescriptorSetLayout(device->getDevice(), descriptorLayout, nullptr);

	mainTexture.reset();

	// destroy descriptor allocator
	descriptorAllocator.cleanup();
}
