#ifndef numberController_h
#define numberController_h

#include "uiDataController.h"

template <>
class UiDataController<unsigned int> : public Rml::EventListener {
public:
	UiDataController(
		Rml::Element* textAreaElement,
		unsigned int initValue,
		std::function<void(unsigned int)> setValue,
		std::function<std::shared_ptr<void>(std::function<void(unsigned int)>)> attachListener,
		std::function<std::string(unsigned int, const std::string*)> formatter = nullptr
	);
	void ProcessEvent(Rml::Event& event) override final;
	void OnDetach(Rml::Element* element) override final { delete this; }
	void detach(Rml::Element* element) { element->RemoveEventListener("change", this); }

private:
	Rml::Element* element;
	std::function<void(unsigned int)> setValue;
	unsigned int currentValue;
	std::shared_ptr<void> attachedListener;
	std::function<std::string(unsigned int, const std::string*)> formatter;
};

template <>
class UiDataController<int> : public Rml::EventListener {
public:
	UiDataController(
		Rml::Element* textAreaElement,
		int initValue,
		std::function<void(int)> setValue,
		std::function<std::shared_ptr<void>(std::function<void(int)>)> attachedListener,
		std::function<std::string(int, const std::string*)> formatter = nullptr
	);
	void ProcessEvent(Rml::Event& event) override final;
	void OnDetach(Rml::Element* element) override final { delete this; }

private:
	Rml::Element* element;
	std::function<void(int)> setValue;
	int currentValue;
	std::shared_ptr<void> attachedListener;
	std::function<std::string(int, const std::string*)> formatter;
};

template <>
class UiDataController<float> : public Rml::EventListener {
public:
	UiDataController(
		Rml::Element* textAreaElement,
		float initValue,
		std::function<void(float)> setValue,
		std::function<std::shared_ptr<void>(std::function<void(float)>)> attachedListener,
		std::function<std::string(float, const std::string*)> formatter = nullptr
	);
	void ProcessEvent(Rml::Event& event) override final;
	void OnDetach(Rml::Element* element) override final { delete this; }

private:
	Rml::Element* element;
	std::function<void(float)> setValue;
	float currentValue;
	std::shared_ptr<void> attachedListener;
	std::function<std::string(float, const std::string*)> formatter;
};

#endif /* numberController_h */
