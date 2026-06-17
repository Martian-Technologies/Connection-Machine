#include "imageManager.h"
#include "imgui/imgui_impl_vulkan.h"
#include "mainRenderer.h"

std::pair<vk::DescriptorSet, std::vector<std::shared_ptr<void>>> ImageManager::getImage(const std::string& path, WindowId windowId) {
	{
		auto iter = loadedImages.find(path);
		if (iter != loadedImages.end()) {
			auto iter2 = iter->second.find(windowId);
			if (iter2 == iter->second.end()) {
				auto lifetimeObjects = iter2->second->lifetimeObjects;
				lifetimeObjects.push_back(iter2->second);
				return {iter2->second->descriptorSet, lifetimeObjects};
			}
		}
	}

	auto windowIter = MainRenderer::get().windowRenderers.find(windowId);
	if (windowIter == MainRenderer::get().windowRenderers.end()) return { vk::DescriptorSet{}, {} };

	int width, height, channels;
	stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!pixels) return { vk::DescriptorSet{}, {} };
	vk::Extent3D size{};
	size.width = width;
	size.height = height;
	size.depth = 1;

	std::shared_ptr<AllocatedImage> image = std::make_shared<AllocatedImage>(
		MainRenderer::get().getVulkanInstance().getDevice(), pixels, size, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled
	);

	if (sampler == nullptr) {
		vk::SamplerCreateInfo samplerInfo{};
		samplerInfo.magFilter = vk::Filter::eLinear;
		samplerInfo.minFilter = vk::Filter::eLinear;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		auto samplerResult = MainRenderer::get().getVulkanInstance().getDevice().getDevice().createSamplerUnique(samplerInfo);
		if (samplerResult.result != vk::Result::eSuccess) throwFatalError("failed to create sampler");
		sampler = std::make_shared<ViewportRenderer::Sampler>(std::move(samplerResult.value));
	}

	std::shared_ptr<ImGuiRenderer::ImGuiDescriptorSet> ds = loadedImages[path].emplace(windowId,
		std::make_shared<ImGuiRenderer::ImGuiDescriptorSet>(
			vk::DescriptorSet(ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(sampler->sampler.get()), static_cast<VkImageView>(image->imageView.get()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)),
			windowIter->second.getImGuiRenderer(),
			std::vector<std::shared_ptr<void>>{ image, sampler }
		)
	).first->second;
	auto lifetimeObjects = ds->lifetimeObjects;
	lifetimeObjects.push_back(ds);
	return {ds->descriptorSet, lifetimeObjects};
}

void ImageManager::clearWindow(WindowId windowId) {
	for (auto& loadedImage : loadedImages) {
		auto iter = loadedImage.second.find(windowId);
		if (iter != loadedImage.second.end()) {
			loadedImage.second.erase(iter);
		}
	}
}
