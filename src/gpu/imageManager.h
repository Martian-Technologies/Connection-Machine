#ifndef imageManager_h
#define imageManager_h

#include "renderer/imgui/imGuiRenderer.h"
#include "renderer/viewport/viewportRenderer.h"
#include "stb_image.h"

class ImageManager {
	typedef std::unordered_map<std::string, std::map<WindowId, std::shared_ptr<ImGuiRenderer::ImGuiDescriptorSet>>> ImagesMap;
public:
	std::pair<vk::DescriptorSet, std::vector<std::shared_ptr<void>>> getImage(const std::string& path, WindowId windowId);
	void clearWindow(WindowId windowId);
private:
	std::shared_ptr<ViewportRenderer::Sampler> sampler;
	std::mutex loadedImagesMux;
	ImagesMap loadedImages;
};

#endif /* imageManager_h */
