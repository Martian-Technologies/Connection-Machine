#ifndef widget_h
#define widget_h

#include "util/id.h"
#include <SDL3/SDL_events.h>

DECLARE_ID_TYPE(WidgetId, unsigned int);

class MainWindow;
class Environment;
class Backend;

class Widget {
	template <class ValueType>
	struct GuiValue;
	struct GuiValueBase {
		virtual ~GuiValueBase() = default;
		virtual void renderDataUpdate() = 0;
		virtual nlohmann::json dumpState() const = 0;
		template <class ValueType>
		GuiValue<ValueType>* cast() {
			return dynamic_cast<GuiValue<ValueType>*>(this);
		}
		template <class ValueType>
		const GuiValue<ValueType>* cast() const {
			return dynamic_cast<const GuiValue<ValueType>*>(this);
		}
	};
	template <class ValueType>
	struct GuiValue : public GuiValueBase {
		GuiValue(const ValueType& initalValue, std::function<void(const ValueType&)> func) : value(initalValue), renderingValue(initalValue), listener(func) { }
		void renderDataUpdate() override {
			if (value != renderingValue) {
				if (updateFromRendering) {
					value = renderingValue;
					if (listener) listener(value);
				} else {
					renderingValue = value;
				}
			}
			updateFromRendering = true;
		}
		nlohmann::json dumpState() const override final {
			nlohmann::json json;
			auto formatValue = []<typename T>(const T& v) -> nlohmann::json {
				if constexpr (requires { nlohmann::json(v); }) {
					return v;
				} else if constexpr (fmt::is_formattable<T>::value) {
					return fmt::format("{}", v);
				} else if constexpr (requires {
										 v.has_value();
										 v.value();
									 }) {
					if constexpr (fmt::is_formattable<typename T::value_type>::value) {
						return v.has_value() ? nlohmann::json(fmt::format("{}", *v)) : nlohmann::json("nullopt");
					} else if constexpr (requires { nlohmann::json(*v); }) {
						return v.has_value() ? nlohmann::json(*v) : nlohmann::json("nullopt");
					} else {
						return "<not serializable>";
					}
				} else {
					return "<not serializable>";
				}
			};
			json["value"] = formatValue(value);
			json["renderingValue"] = formatValue(renderingValue);
			json["updateFromRendering"] = updateFromRendering;
			json["hasListener"] = listener != nullptr;
			return json;
		}
		bool updateFromRendering = true;
		ValueType value;
		ValueType renderingValue;
		std::function<void(const ValueType&)> listener;
	};
public:
	Widget(WidgetId widgetId, MainWindow& mainWindow) : widgetId(widgetId), mainWindow(mainWindow), widgetIdStr(fmt::format("Widget {}", widgetId)) { }
	virtual ~Widget() = default;
	void doRendering() {
		render();
		doRenderDataUpdate.store(true);
	}
	void doUpdate() {
		if (doRenderDataUpdate.load()) {
			doRenderDataUpdate.store(false);
			for (std::pair<const std::string, std::unique_ptr<GuiValueBase>>& guiValueBase : guiValues) guiValueBase.second->renderDataUpdate();
		}
		update();
	}
	MainWindow& getMainWindow() const { return mainWindow; }
	Environment& getEnvironment() const;
	Backend& getBackend() const;

	WidgetId getWidgetId() const { return widgetId; }
	const std::string& getWidgetIdStr() const { return widgetIdStr; }
	virtual void processEvent(SDL_Event& event) { }

	nlohmann::json dumpWidgetState() const;

protected:
	virtual void render() = 0;
	virtual void update() { };
	virtual nlohmann::json dumpState() const { return "dumpState not overridden"; }

	// gui values
	template <class ValueType>
	void setupGUIValue(const std::string& key, const ValueType& initalValue, std::function<void(const ValueType&)> func);
	template <class ValueType>
	const ValueType& getGUIValue(const std::string& key) const;
	template <class ValueType>
	void setGUIValue(const std::string& key, const ValueType& value);
	template <class ValueType>
	const ValueType& getGUIValue_rendering(const std::string& key) const;
	template <class ValueType>
	void setGUIValue_rendering(const std::string& key, const ValueType& value);
	template <class ValueType>
	ValueType* getGUIValueForImGui_rendering(const std::string& key);
private:
	MainWindow& mainWindow;
	WidgetId widgetId;
	std::string widgetIdStr;
	mutable std::mutex guiValuesMux;
	std::unordered_map<std::string, std::unique_ptr<GuiValueBase>> guiValues;
	std::atomic<bool> doRenderDataUpdate;
};

template <class ValueType>
void Widget::setupGUIValue(const std::string& key, const ValueType& initalValue, std::function<void(const ValueType&)> func) {
	std::lock_guard mux(guiValuesMux);
	auto pair = guiValues.emplace(key, std::make_unique<GuiValue<ValueType>>(initalValue, func));
	assert(pair.second);
}

template <class ValueType>
const ValueType& Widget::getGUIValue(const std::string& key) const {
	std::lock_guard mux(guiValuesMux);
	auto iter = guiValues.find(key);
	assert(iter != guiValues.end());
	const GuiValue<ValueType>* guiValue = iter->second->cast<ValueType>();
	assert(guiValue);
	return guiValue->value;
}

template <class ValueType>
void Widget::setGUIValue(const std::string& key, const ValueType& value) {
	std::lock_guard mux(guiValuesMux);
	auto iter = guiValues.find(key);
	assert(iter != guiValues.end());
	GuiValue<ValueType>* guiValue = iter->second->template cast<ValueType>();
	assert(guiValue);
	if (guiValue->value == value) return;
	guiValue->value = value;
	guiValue->updateFromRendering = false;
}

template <class ValueType>
const ValueType& Widget::getGUIValue_rendering(const std::string& key) const {
	std::lock_guard mux(guiValuesMux);
	auto iter = guiValues.find(key);
	assert(iter != guiValues.end());
	const GuiValue<ValueType>* guiValue = iter->second->cast<ValueType>();
	assert(guiValue);
	return guiValue->renderingValue;
}

template <class ValueType>
void Widget::setGUIValue_rendering(const std::string& key, const ValueType& value) {
	std::lock_guard mux(guiValuesMux);
	auto iter = guiValues.find(key);
	assert(iter != guiValues.end());
	GuiValue<ValueType>* guiValue = iter->second->cast<ValueType>();
	assert(guiValue);
	guiValue->renderingValue = value;
}

template <class ValueType>
ValueType* Widget::getGUIValueForImGui_rendering(const std::string& key) {
	std::lock_guard mux(guiValuesMux);
	auto iter = guiValues.find(key);
	assert(iter != guiValues.end());
	GuiValue<ValueType>* guiValue = iter->second->cast<ValueType>();
	assert(guiValue);
	return &guiValue->renderingValue;
}

#endif /* widget_h */
