#include "imGuiRenderer.h"

#include "app.h"
#include "computerAPI/directoryManager.h"
#include "gpu/mainRenderer.h"
#include "computerAPI/fileLoader.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_notify.h"

std::map<SDL_Window*, ImGuiRenderer&> imGuiRenderers;
std::mutex mutex;

static void check_vk_result(VkResult err) {
	if (err == VK_SUCCESS)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

ImGuiRenderer::ImGuiDescriptorSet::ImGuiDescriptorSet(
	vk::DescriptorSet descriptorSet,
	ImGuiRenderer& imGuiRenderer,
	const std::vector<std::shared_ptr<void>>& lifetimeObjects
) : descriptorSet(descriptorSet), lifetimeObjects(lifetimeObjects), imGuiRenderer(imGuiRenderer) { }

ImGuiRenderer::ImGuiDescriptorSet::~ImGuiDescriptorSet() {
	VkDescriptorSet rawSet = static_cast<VkDescriptorSet>(descriptorSet);
	imGuiRenderer.addPostFrameWork(
		[rawSet]() {
			ImGui_ImplVulkan_RemoveTexture(rawSet);
		}
	);
}

ImGuiRenderer::ImGuiRenderer(SDL_Window& window, vk::RenderPass renderPass, uint32_t framesInFlight) :
	mainWindow(window), renderPass(renderPass), framesInFlight(framesInFlight) {
	std::lock_guard<std::mutex> lock(mutex);
	auto result = imGuiRenderers.emplace(&mainWindow, *this);
	assert(result.second);

	vk::DescriptorPoolSize poolSizes[] = {
		{ vk::DescriptorType::eSampler, 1000 },
		{ vk::DescriptorType::eCombinedImageSampler, 1000 },
		{ vk::DescriptorType::eSampledImage, 1000 },
		{ vk::DescriptorType::eStorageImage, 1000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		{ vk::DescriptorType::eUniformBuffer, 1000 },
		{ vk::DescriptorType::eStorageBuffer, 1000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		{ vk::DescriptorType::eInputAttachment, 1000 }
	};

	vk::DescriptorPoolCreateInfo poolInfo{};
	poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	poolInfo.maxSets = 1000 * std::size(poolSizes);
	poolInfo.poolSizeCount = std::size(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	auto poolResult = MainRenderer::get().getVulkanInstance().getDevice().getDevice().createDescriptorPoolUnique(poolInfo);
	if (poolResult.result != vk::Result::eSuccess) {
		throwFatalError("Failed to create ImGui descriptor pool");
	}
	imguiDescriptorPool = std::move(poolResult.value);

	context = ImGui::CreateContext();

	ImGui::SetCurrentContext(context);
	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	if (!ImGui_ImplSDL3_InitForVulkan(&mainWindow)) {
		throwFatalError("Failed to initialize ImGui SDL3 backend");
	}

	static bool vulkanFunctionsLoaded = false;
	if (!vulkanFunctionsLoaded) {
		ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* name, void*) {
			return vkGetInstanceProcAddr(MainRenderer::get().getVulkanInstance().getVkbInstance(), name);
		});
		vulkanFunctionsLoaded = true;
	}

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = MainRenderer::get().getVulkanInstance().getVkbInstance();
	init_info.PhysicalDevice = MainRenderer::get().getVulkanInstance().getDevice().getVkbDevice().physical_device;
	init_info.Device = MainRenderer::get().getVulkanInstance().getDevice().getVkbDevice().device;
	init_info.QueueFamily = MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueueIndex();
	init_info.Queue = static_cast<VkQueue>(MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueue().queue);
	init_info.DescriptorPool = static_cast<VkDescriptorPool>(imguiDescriptorPool.get());
	init_info.MinImageCount = framesInFlight;
	init_info.ImageCount = framesInFlight;
	init_info.PipelineInfoMain.RenderPass = static_cast<VkRenderPass>(renderPass);
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = static_cast<VkSampleCountFlagBits>(MainRenderer::get().getVulkanInstance().getDevice().getMaxUsableSampleCount());

	ImGui_ImplVulkan_Init(&init_info);

	const std::string* fontPath = Settings::get<SettingType::FILE_PATH>("Appearance/Font");
	std::pair<char*, unsigned long long> fontData;
	if (fontPath) {
		std::string realFontPath = DirectoryManager::extendPath(*fontPath).generic_string();
		fontData = readFileAsBytes_noVec(realFontPath);
		if (fontData.second == 0) {
			std::filesystem::path fallBackPath = (DirectoryManager::getResourceDirectory() / "gui/fonts/Consolas.ttf");
			logError("Failed to read font from \"{}\". Falling back to \"{}\"", "ImGuiRenderer::ImGuiRenderer", realFontPath, fallBackPath.generic_string());
			fontData = readFileAsBytes_noVec(fallBackPath);
		} else {
			logInfo("Loaded, {}", "ImGuiRenderer::ImGuiRenderer", realFontPath);
		}
	} else {
		std::filesystem::path fallBackPath = (DirectoryManager::getResourceDirectory() / "gui/fonts/Consolas.ttf");
		logError("Failed to load file path from setting. Falling back to \"{}\"", "ImGuiRenderer::ImGuiRenderer", fallBackPath.generic_string());
		fontData = readFileAsBytes_noVec(fallBackPath);
	}

	if (fontData.second == 0) {
		logError("No font could be loaded! Popups may not work :/", "ImGuiRenderer::ImGuiRenderer");
	} else {
		ImFontConfig font_cfg;
		io.Fonts->AddFontFromMemoryTTF(fontData.first, fontData.second, 17.f, &font_cfg);
		ImGui::MergeIconsWithLatestFont(16.f, false);
	}

	Settings::registerListener<SettingType::FILE_PATH>("Appearance/Font", [this](const std::string& fontFilePath) {
		std::pair<char*, unsigned long long> fontData;
		std::string realFontPath = DirectoryManager::extendPath(fontFilePath).generic_string();
		fontData = readFileAsBytes_noVec(realFontPath);
		if (fontData.second == 0) {
			std::filesystem::path fallBackPath = (DirectoryManager::getResourceDirectory() / "gui/fonts/Consolas.ttf");
			logError("Failed to read font from \"{}\". Falling back to \"{}\"", "ImGuiRenderer::ImGuiRenderer", realFontPath, fallBackPath.generic_string());
			fontData = readFileAsBytes_noVec(fallBackPath);
		} else {
			logInfo("Loaded, {}", "ImGuiRenderer::ImGuiRenderer", realFontPath);
		}

		if (fontData.second == 0) {
			logError("No font could be loaded!", "ImGuiRenderer::ImGuiRenderer");
		} else {
			ImFontConfig font_cfg;
			ImGuiIO& io = ImGui::GetIO();
			io.Fonts->AddFontFromMemoryTTF(fontData.first, fontData.second, 17.f, &font_cfg);
			ImGui::MergeIconsWithLatestFont(16.f, false);
		}
	});
}

ImGuiRenderer::~ImGuiRenderer() {
	std::lock_guard<std::mutex> lock(mutex);
	imGuiRenderers.erase(&mainWindow);

	if (!context) return;

	ImGui::SetCurrentContext(context);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();

	ImGui::DestroyContext(context);
	context = nullptr;

	imguiDescriptorPool.reset();
}

void ImGuiRenderer::beginFrame() {
	mutex.lock();

	ImGui::SetCurrentContext(context);

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void ImGuiRenderer::endFrame(vk::CommandBuffer cmd) {
	mutex.unlock();

	App::runOnMain_blocking([this]() {
		std::lock_guard lock(mutex);
		ImGui::SetCurrentContext(context);
		ImGui::Render();
	});

	std::lock_guard lock(mutex);
	ImGui::SetCurrentContext(context);

	{
		std::lock_guard guard(MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueueLock());
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd));
	}

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	std::lock_guard workLock(postFrameWorkLock);
	for (std::function<void()>& work : postFrameWork) work();
	postFrameWork.clear();
}

std::unique_lock<std::mutex> ImGuiRenderer::setActiveContext() const {
	std::unique_lock lock(mutex);
	ImGui::SetCurrentContext(context);
	return std::move(lock);
}

void ImGuiRenderer::addPostFrameWork(std::function<void()> work) {
	std::lock_guard workLock(postFrameWorkLock);
	postFrameWork.push_back(work);
}

ImGuiRenderer* ImGuiRenderer::getImGuiRenderer(SDL_Window& sdlWindow) {
	auto iter = imGuiRenderers.find(&sdlWindow);
	if (iter == imGuiRenderers.end()) return nullptr;
	return &iter->second;
}

void ImGuiRenderer::allProcessEvent(const SDL_Event& e) {
	std::lock_guard<std::mutex> lock(mutex);
	for (std::pair<SDL_Window*, ImGuiRenderer&> imGuiRenderer : imGuiRenderers) {
		ImGui::SetCurrentContext(imGuiRenderer.second.context);
		ImGui_ImplSDL3_ProcessEvent(&e);
	}
}
