#ifndef windowRenderer_h
#define windowRenderer_h

#include "gpu/renderer/imgui/imGuiRenderer.h"
#include "gui/sdl/sdlWindow.h"

#include "gpu/abstractions/vulkanSwapchain.h"
#include "gpu/abstractions/vulkanImage.h"
#include "gpu/renderer/frameManager.h"

class WindowRenderer {
public:
	WindowRenderer(WindowId windowId, SdlWindow* sdlWindow);
	~WindowRenderer();

	WindowRenderer(const WindowRenderer&) = delete;
	WindowRenderer& operator=(const WindowRenderer&) = delete;

	void resize(std::pair<uint32_t, uint32_t> windowSize);

	ImGuiRenderer& getImGuiRenderer() { return *imGuiRenderer; }
	const ImGuiRenderer& getImGuiRenderer() const { return *imGuiRenderer; }
	void setImGuiRenderFunc(std::function<void()> imGuiRenderFunc);

	void addSemaphore(vk::Semaphore semaphore, const std::vector<std::shared_ptr<void>>& lifetimeObjects) {
		semaphoreForThisFrame.push_back(semaphore);
		addLifetimeObjects(lifetimeObjects);
	}
	void addLifetimeObjects(const std::vector<std::shared_ptr<void>>& lifetimeObjects) {
		for (const std::shared_ptr<void>& obj : lifetimeObjects) frames.getCurrentFrame()->lifetime.push(obj);
	}
	VulkanDevice& getDevice() { return *device; }

	std::pair<vk::DescriptorSet, std::vector<std::shared_ptr<void>>> getBlockTextureArrayLayer_ImGui(unsigned int layer) {
		updateImGuiBlockTextureArrayLayers();
		std::lock_guard lock(imGuiBlockTextureArrayLayersMux);
		if (imGuiBlockTextureArrayLayers.size() <= layer) {
			logError("layer index of {} out of range for imGuiBlockTextureArrayLayers with {} layers", "WindowRenderer::getBlockTextureArrayLayer_ImGui", layer, imGuiBlockTextureArrayLayers.size());
			return { vk::DescriptorSet{}, {} };
		}
		return std::make_pair(imGuiBlockTextureArrayLayers[layer]->descriptorSet, std::vector<std::shared_ptr<void>>{ imGuiBlockTextureArrayLayers[layer] } );
	}

private:
	void createColorResources();
	void renderToCommandBuffer(Frame& frame, uint32_t imageIndex);
	void createRenderPass();
	void recreateSwapchain();

private:
	WindowId windowId;
	Swapchain swapchain;
	std::atomic<bool> swapchainRecreationNeeded = false;
	std::pair<uint32_t, uint32_t> windowSize;
	std::mutex windowSizeMux;

	vk::SurfaceKHR surface;
	vk::UniqueRenderPass renderPass;
	std::vector<vk::Semaphore> semaphoreForThisFrame;

	std::optional<ImGuiRenderer> imGuiRenderer;
	std::mutex imGuiRenderFuncMux;
	std::function<void()> imGuiRenderFunc = nullptr;

	FrameManager frames;
	std::thread renderThread;
	std::atomic<bool> running = false;
	std::atomic<bool> renderLoopStopped = true;
	void renderLoop();

	void updateImGuiBlockTextureArrayLayers();
	std::mutex imGuiBlockTextureArrayLayersMux;
	std::vector<std::shared_ptr<ImGuiRenderer::ImGuiDescriptorSet>> imGuiBlockTextureArrayLayers;
	std::shared_ptr<BlockTextureArray> blockTextureArrayImage;

	SdlWindow* sdlWindow;
	VulkanDevice* device;

	std::optional<AllocatedImage> colorImage;
};

#endif /* windowRenderer_h */
