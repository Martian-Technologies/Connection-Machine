#include "numberControllers.h"

UiDataController<unsigned int>::UiDataController(
	Rml::Element* element,
	unsigned int initValue,
	std::function<void(unsigned int)> setValue,
	std::function<std::shared_ptr<void>(std::function<void(unsigned int)>)> attachListener,
	std::function<std::string(unsigned int, const std::string*)> formatter
) : element(element), setValue(setValue), currentValue(initValue), formatter(formatter) {
	// set formatter if none
	if (!this->formatter) this->formatter = [](unsigned int value, const std::string* string) { if (string) return *string; return std::to_string(value); };

	// set init value
	std::string str = this->formatter(currentValue, nullptr);
	element->SetInnerRML(std::string(str.size(), ' '));
	element->SetAttribute<Rml::String>("value", str);

	// make it update from data
	attachedListener = attachListener([this](unsigned int number) {
		this->currentValue = number;
		std::string beforeStr = this->element->GetAttribute<Rml::String>("value", "");
		std::string correctValue;
		for (char c : beforeStr) {
			if ('0' <= c && c <= '9') correctValue += c;
		}
		std::string str = this->formatter(number, &correctValue);
		this->element->SetInnerRML(std::string(str.size(), ' '));
		this->element->SetAttribute<Rml::String>("value", str);
	});

	// make it update data
	element->AddEventListener("change", this);
}

void UiDataController<unsigned int>::ProcessEvent(Rml::Event& event) {
	const Rml::String value = element->GetAttribute<Rml::String>("value", "");
	std::string correctValue;
	for (char c : value) {
		if ('0' <= c && c <= '9') correctValue += c;
	}
	std::stringstream ss(correctValue);
	unsigned int number = 0;
	ss >> number;
	std::string str = formatter(number, &correctValue);
	element->SetInnerRML(std::string(str.size(), ' '));
	element->SetAttribute<Rml::String>("value", str);
	if (!correctValue.empty() && currentValue != number) {
		currentValue = number;
		if (setValue) setValue(number);
	}
}

UiDataController<int>::UiDataController(
	Rml::Element* element,
	int initValue,
	std::function<void(int)> setValue,
	std::function<std::shared_ptr<void>(std::function<void(int)>)> attachListener,
	std::function<std::string(int, const std::string*)> formatter
) : element(element), setValue(setValue), currentValue(initValue), formatter(formatter) {
	// set formatter if none
	if (!this->formatter) this->formatter = [](int value, const std::string* string) { if (string) return *string; return std::to_string(value); };

	// set init value
	std::string str = this->formatter(currentValue, nullptr);
	element->SetInnerRML(std::string(str.size(), ' '));
	element->SetAttribute<Rml::String>("value", str);

	// make it update from data
	attachedListener = attachListener([this](int number) {
		this->currentValue = number;
		std::string beforeStr = this->element->GetAttribute<Rml::String>("value", "");
		std::string correctValue;
		for (char c : beforeStr) {
			if (('0' <= c && c <= '9') || (correctValue.empty() && c == '-')) correctValue += c;
		}
		std::stringstream ss(correctValue);
		std::string str = this->formatter(number, &correctValue);
		this->element->SetInnerRML(std::string(str.size(), ' '));
		this->element->SetAttribute<Rml::String>("value", str);
	});

	// make it update data
	element->AddEventListener("change", this);
}

void UiDataController<int>::ProcessEvent(Rml::Event& event) {
	const Rml::String value = element->GetAttribute<Rml::String>("value", "");
	std::string correctValue;
	for (char c : value) {
		if (('0' <= c && c <= '9') || (correctValue.empty() && c == '-')) correctValue += c;
	}
	std::stringstream ss(correctValue);
	int number = 0;
	ss >> number;
	std::string str = formatter(number, &correctValue);
	element->SetInnerRML(std::string(str.size(), ' '));
	element->SetAttribute<Rml::String>("value", str);
	if (!correctValue.empty() && currentValue != number) {
		currentValue = number;
		if (setValue) setValue(number);
	}
}

UiDataController<float>::UiDataController(
	Rml::Element* element,
	float initValue,
	std::function<void(float)> setValue,
	std::function<std::shared_ptr<void>(std::function<void(float)>)> attachListener,
	std::function<std::string(float, const std::string*)> formatter
) : element(element), setValue(setValue), currentValue(initValue), formatter(formatter) {
	// set formatter if none
	if (!formatter) formatter = [](float value, const std::string* string) {
		return (string && (string->empty() || string->back() == '.')) ? "" : fmt::format("{:.3f}", value);
	};

	// set init value
	std::string str = formatter(currentValue, nullptr);
	element->SetInnerRML(std::string(str.size(), ' '));
	element->SetAttribute<Rml::String>("value", str);

	// make it update from data
	attachedListener = attachListener([this](float number) {
		this->currentValue = number;
		std::string beforeStr = this->element->GetAttribute<Rml::String>("value", "");
		std::string correctValue;
		bool foundDecimal = false;
		for (char c : beforeStr) {
			if (('0' <= c && c <= '9') || (correctValue.empty() && c == '-')) {
				correctValue += c;
			} else if (c == '.' && !foundDecimal) {
				foundDecimal = true;
				correctValue += c;
			}
		}
		std::string str = this->formatter(number, &correctValue);
		this->element->SetInnerRML(std::string(str.size(), ' '));
		this->element->SetAttribute<Rml::String>("value", str);
	});

	// make it update data
	element->AddEventListener("change", this);
}

void UiDataController<float>::ProcessEvent(Rml::Event& event) {
	const Rml::String value = element->GetAttribute<Rml::String>("value", "");
	std::string correctValue;
	bool foundDecimal = false;
	for (char c : value) {
		if (('0' <= c && c <= '9') || (correctValue.empty() && c == '-')) {
			correctValue += c;
		} else if (c == '.' && !foundDecimal) {
			foundDecimal = true;
			correctValue += c;
		}
	}
	std::stringstream ss(correctValue);
	float number = 0;
	ss >> number;
	std::string str = formatter(number, &correctValue);
	element->SetInnerRML(std::string(str.size(), ' '));
	element->SetAttribute<Rml::String>("value", str);
	if (!correctValue.empty() && currentValue != number) {
		currentValue = number;
		if (setValue) setValue(number);
	}
}
