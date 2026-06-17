#ifndef imGuiRenderer_h
#define imGuiRenderer_h

#include "gpu/vulkanCommon.h"

struct SDL_Window;
union SDL_Event;
class ImGuiContext;
class VulkanInstance;
class VulkanDevice;

class ImGuiRenderer {
public:
	struct ImGuiDescriptorSet {
		ImGuiDescriptorSet(vk::DescriptorSet descriptorSet, ImGuiRenderer& imGuiRenderer, const std::vector<std::shared_ptr<void>>& lifetimeObjects = {});
		~ImGuiDescriptorSet();
		vk::DescriptorSet descriptorSet;
		std::vector<std::shared_ptr<void>> lifetimeObjects;
		ImGuiRenderer& imGuiRenderer;
	};

	ImGuiRenderer(SDL_Window& window, vk::RenderPass renderPass, uint32_t framesInFlight);
	~ImGuiRenderer();

	ImGuiRenderer(const ImGuiRenderer&) = delete;
	ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

	void beginFrame();
	void endFrame(vk::CommandBuffer cmd);

	ImGuiContext* getContext() const { return context; }
	std::unique_lock<std::mutex> setActiveContext() const;
	void addPostFrameWork(std::function<void()>);

	static ImGuiRenderer* getImGuiRenderer(SDL_Window& sdlWindow);
	static void allProcessEvent(const SDL_Event& e);

private:
	void initImGui();

	SDL_Window& mainWindow;
	vk::RenderPass renderPass;
	uint32_t framesInFlight;

	std::mutex postFrameWorkLock;
	std::vector<std::function<void()>> postFrameWork;

	ImGuiContext* context = nullptr;
	vk::UniqueDescriptorPool imguiDescriptorPool;
};

#endif /* imGuiRenderer_h */
