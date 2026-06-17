#ifndef window_h
#define window_h

#include <SDL3/SDL_events.h>

#include "environment/environment.h"
#include "gui/sdl/sdlWindow.h"

#include "gui/viewportManager/circuitView/tutorialDataManager.h"
#include "tools/toolManagerManager.h"
#include "widget.h"
// #include "widgets/cornerLogWidget.h"

class MainWindow : public SdlWindow {
public:
	MainWindow();
	~MainWindow();

	// no copy
	MainWindow(const MainWindow&) = delete;
	MainWindow& operator=(const MainWindow&) = delete;

	Environment& getEnvironment() { return environment; }
	const Environment& getEnvironment() const { return environment; }

	ToolManagerManager& getToolManagerManager() { return toolManagerManager; }
	const ToolManagerManager& getToolManagerManager() const { return toolManagerManager; }

	template <class WidgetType, typename... Args>
	WidgetType& createWidget(Args&&... args) {
		WidgetId widgetId = widgetIdProvider.getNewId();
		std::unique_ptr<WidgetType> widget = std::make_unique<WidgetType>(widgetId, *this, std::forward<Args>(args)...);
		WidgetType& widgetRef = *widget;
		{
			std::lock_guard lock(widgetsMux);
			auto pair = widgets.emplace(widgetId, std::move(widget));
		}
		{
			std::lock_guard lock(windowToFocusMux);
			windowToFocus = widgetRef.getWidgetIdStr().c_str();
		}
		return widgetRef;
	}
	template <class WidgetType, typename... Args>
	WidgetType& createWidgetNoFocus(Args&&... args) {
		WidgetId widgetId = widgetIdProvider.getNewId();
		std::unique_ptr<WidgetType> widget = std::make_unique<WidgetType>(widgetId, *this, std::forward<Args>(args)...);
		WidgetType& widgetRef = *widget;
		{
			std::lock_guard lock(widgetsMux);
			auto pair = widgets.emplace(widgetId, std::move(widget));
		}
		return widgetRef;
	}
	void destroyWidget(WidgetId widgetId);

	// logging
	void log(const std::string& message);
	template <typename... Args>
	void log(const fmt::format_string<Args...>& formatString, Args&&... args) {
		std::ostringstream message;
		message << fmt::format(formatString, std::forward<Args>(args)...);
		log(message.str());
	}
	void logError(const std::string& message);
	template <typename... Args>
	void logError(const fmt::format_string<Args...>& formatString, Args&&... args) {
		std::ostringstream message;
		message << fmt::format(formatString, std::forward<Args>(args)...);
		logError(message.str());
	}

	bool isPressingKeybind(const Keybind& keybind) const;
	bool isPressingKeybind(const std::string& settingKey) const;
	bool isHoldingKeybind(const Keybind& keybind) const;
	bool isHoldingKeybind(const std::string& settingKey) const;
	const std::set<ImGuiKey>& getPressedKeys() const { return pressedKeys; }
	const std::set<ImGuiKey>& getHeldKeys() const { return heldKeys; }

	ImGuiID getDockMainId() const { return dockMainId; }
	ImGuiID getDockLeftId() const { return dockLeftId; }
	// ImGuiID getDockRightId() const { return dockRightId; }
	// ImGuiID getDockBottomId() const { return dockBottomId; }

	void createPopup(const std::string& message, const std::vector<std::pair<std::string, std::function<void()>>>& buttons);

	void setNextWindowMainDockable() const;
	void setNextWindowSideBarDockable() const;

	void loadDialog();
	void pushWindowStyling() const;
	void popWindowStyling() const;

	TutorialDataManager& getTutorialDataManager() { return tutorialDataManager; }

private:
	void doUpdate() override final;
	bool killWindow(bool forced) override final;
	void render() override final;
	void processEvent(SDL_Event& event) override final;
	nlohmann::json dumpState() const override final;

private:
	bool tryClose();
	void offsetUiScale(double delta);
	void applyUiScale(float scale);

	Environment environment;

	std::set<ImGuiKey> pressedKeys;
	std::set<ImGuiKey> heldKeys;
	std::atomic<unsigned long long> frameIndex;
	unsigned long long lastUpdatedFrame;

	TutorialDataManager tutorialDataManager;

	// inputs and tools
	// KeybindHandler keybindHandler;
	ToolManagerManager toolManagerManager;
	std::mutex uiScaleMux;
	float uiScale = 1.0f;
	static constexpr double kUiScaleStep = 0.1;
	static constexpr double kUiScaleMin = 0.5;
	static constexpr double kUiScaleMax = 3.0;

	std::mutex widgetsMux;
	std::unordered_map<WidgetId, std::unique_ptr<Widget>> widgets;
	IdProvider<WidgetId> widgetIdProvider;
	std::mutex widgetsToDestroyMux;
	std::unordered_set<WidgetId> widgetsToDestroy;
	std::mutex windowToFocusMux;
	std::optional<std::string> windowToFocus = std::nullopt;

	ImGuiID docRootId;
	ImGuiID dockMainId;
	ImGuiID dockLeftId;
	// ImGuiID dockRightId;
	// ImGuiID dockBottomId;

	float leftWidth = 150;

	// CornerLogWidget& cornerLog;

	// widgets
	// std::optional<EvalWindow> evalWindow;
	// std::optional<BlockCreationWindow> blockCreationWindow;
	// std::optional<SettingsWindow> settingsWindow;
};

#endif /* window_h */
