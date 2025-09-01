#include "contentManager.h"

#include "gui/helper/menuTreeListener.h"
#include "gui/helper/keybindHelpers.h"
#include "gui/helper/eventPasser.h"
#include "backend/settings/settings.h"
#include "util/algorithm.h"

ContentManager::ContentManager(Rml::ElementDocument* document) : document(document) {
	contentPanel = document->GetElementById("settings-content-panel");
	contentPanel->SetClass("settings-tree", true);
}

void ContentManager::load() {
	const auto& mapKeys = Settings::getSettingsMap().getAllKeys();
	std::vector<std::vector<std::string>> vecPaths(mapKeys.size());
	int i = 0;
	for (const auto& pair : mapKeys) {
		const std::string& key = pair.first;
		stringSplitInto(key, '/', vecPaths[i]);
		i++;
	}

	setPaths(vecPaths, contentPanel);

	// if (type == "general") {
	// 	std::vector<std::vector<std::string>> list = Settings::getGraphicsData("General");

	// 	if (formList.empty()) {
	// 		for (int i = 0; i < list.size(); i++) {
	// 			generateItem(list[i][1], list[i][0]);
	// 		}
	// 	} else {
	// 		// TODO: render only the formList items
	// 	}
	// } else if (type == "appearance") {
	// 	std::vector<std::vector<std::string>> appearance = Settings::getGraphicsData("Appearance");

	// } else if (type == "keybind") {
	// 	std::vector<std::string> keybinds = Settings::getKeybindGraphicsData();

	// } else {
	// 	logWarning("incorrect type, nothing for form grabbed");
	// }
}

void ContentManager::setPaths(const std::vector<std::vector<std::string>>& paths, Rml::Element* current) {
	if (!current) return;

	std::string pathStr = getPath(current);

	Rml::ElementList elements;
	current->GetElementsByClassName(elements, "settings-tree-header");
	if (elements.empty()) {
		Rml::ElementPtr newDiv = document->CreateElement("div");
		newDiv->SetClass("settings-tree-header", true);
		elements.push_back(current->AppendChild(std::move(newDiv)));
	}
	Rml::Element* div = elements[0];

	if (paths.empty()) {
		// remove class
		current->SetClass("parent", false);
		// remove ul
		elements.clear();
		div->GetElementsByTagName(elements, "ul");
		if (!elements.empty()) div->RemoveChild(elements[0]);
		// remove span
		elements.clear();
		current->GetElementsByTagName(elements, "span");
		if (!elements.empty()) current->RemoveChild(elements[0]);
		return;
	}
	std::map<std::string, std::vector<std::vector<std::string>>> pathsByRoot;
	for (const auto& path : paths) {
		if (path.size() == 1) {
			pathsByRoot[path[0]];
		} else {
			auto& pathVec = pathsByRoot[path[0]].emplace_back(path.size() - 1);
			for (unsigned int i = 1; i < path.size(); i++) {
				pathVec[i - 1] = path[i];
			}
		}
	}

	Rml::Element* listElement;
	elements.clear();
	div->GetElementsByTagName(elements, "ul");
	if (elements.empty()) {
		Rml::ElementPtr newList = document->CreateElement("ul");
		// current->SetClass("collapsed", current != contentPanel); // closed by default
		listElement = div->AppendChild(std::move(newList));
		current->SetClass("parent", true);
		if (current != contentPanel) {
			Rml::ElementPtr arrow = document->CreateElement("span");
			arrow->SetInnerRML(">");
			arrow->AddEventListener("click", new MenuTreeListener());
			current->InsertBefore(std::move(arrow), div);
		}
	} else {
		listElement = elements[0];
		std::vector<Rml::Element*> children;
		for (unsigned int i = 0; i < listElement->GetNumChildren(); i++) {
			children.push_back(listElement->GetChild(i));
		}
		for (auto element : children) {
			getPath(element);
			const std::string& id = element->GetId();
			unsigned int index = id.size() - 5;
			while (index != 0 && id[index] != '/') index--;
			auto iter = pathsByRoot.find(id.substr(index + (index != 0), id.size() - 5 - index - (index != 0)));
			if (iter == pathsByRoot.end()) {
				listElement->RemoveChild(element);
				continue;
			}
			setPaths(iter->second, element);
			pathsByRoot.erase(iter);
		}
	}
	if (!(pathStr.empty())) pathStr += "/";
	for (auto iter : pathsByRoot) {
		Rml::ElementPtr newItem = document->CreateElement("li");
		// add listener
		// if (!clickableName)
		newItem->AddEventListener("click", new MenuTreeListener(false, nullptr));
		// else {
		// 	newItem->AddEventListener("click", new EventPasser(
		// 		[this](Rml::Event& event) {
		// 			event.StopPropagation();
		// 			Rml::Element* target = event.GetCurrentElement();
		// 			if (listenerFunction)
		// 				listenerFunction(target->GetId().substr(0, target->GetId().size() - 5));
		// 		}
		// 	));
		// }
		// set id
		newItem->SetId(pathStr + iter.first + "-menu");
		// create div for text
		Rml::ElementPtr newDiv = document->CreateElement("div");
		newDiv->SetClass("settings-tree-header", true);
		newDiv->AppendChild(std::move(document->CreateTextNode(iter.first)));
		Rml::ElementPtr item = generateItem(pathStr + iter.first);
		if (item) newDiv->AppendChild(std::move(item));
		newItem->AppendChild(std::move(newDiv));
		Rml::Element* newItem2 = listElement->AppendChild(std::move(newItem));
		if (!iter.second.empty()) {
			setPaths(iter.second, newItem2);
		}
	}
}

std::string ContentManager::getPath(Rml::Element* item) {
	if (item == contentPanel) return "";
	return item->GetId().substr(0, item->GetId().size() - 5);
}

Rml::ElementPtr ContentManager::generateItem(const std::string& key) {
	if (!Settings::hasKey(key)) return nullptr;
	Rml::ElementPtr item = document->CreateElement("div");
	item->SetClass("settings-tree-item", true);
	switch (Settings::getType(key)) {
	case SettingType::INT:
		break;
	case SettingType::BOOL: {
		Rml::ElementPtr button = document->CreateElement("button");
		button->AppendChild(std::move(document->CreateTextNode((*Settings::get<SettingType::BOOL>(key)) ? "Enabled" : "Disabled")));
		button->AddEventListener(Rml::EventId::Click, new EventPasser([this, key](Rml::Event& event){
			bool newVal = !*Settings::get<SettingType::BOOL>(key);
			Settings::set<SettingType::BOOL>(key, newVal);
			event.GetCurrentElement()->SetInnerRML(newVal ? "Enabled" : "Disabled");
		}));
		item->AppendChild(std::move(button));
		break;
	}
	case SettingType::KEYBIND: {
		Rml::ElementPtr keybindText = document->CreateElement("div");
		keybindText->SetInnerRML(Settings::get<SettingType::KEYBIND>(key)->toString());
		keybindText->SetClass("keybind", true);

		keybindText->AddEventListener(Rml::EventId::Click, new EventPasser([this, key](Rml::Event& event){
			if (activeItem == key) return;
			activeItem = key;
			Rml::Element* keybindText = event.GetCurrentElement();
			keybindText->Focus();
			dynamic_cast<Rml::ElementText*>(keybindText->GetChild(0))->SetText("Press desired keys and then press Enter");
		}));
		keybindText->AddEventListener(Rml::EventId::Blur, new EventPasser([this, key](Rml::Event& event){
			dynamic_cast<Rml::ElementText*>(event.GetCurrentElement()->GetChild(0))->SetText(Settings::get<SettingType::KEYBIND>(key)->toString()); // this needs to happen even if it is not the activeItem
			if (activeItem != key) return;
			activeItem = "";
		}));
		keybindText->AddEventListener(Rml::EventId::Keydown, new EventPasser([this, key](Rml::Event& event){
			if (activeItem != key) return;
			Rml::Input::KeyIdentifier pressedKey = event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_UNKNOWN);
			if (pressedKey == Rml::Input::KeyIdentifier::KI_RETURN) {
				if (lastPressedKeys.getKeyCombined() != Rml::Input::KeyIdentifier::KI_UNKNOWN) {
					Settings::set<SettingType::KEYBIND>(key, lastPressedKeys);
					dynamic_cast<Rml::ElementText*>(event.GetCurrentElement()->GetChild(0))->SetText(lastPressedKeys.toString());
				} else {
					dynamic_cast<Rml::ElementText*>(event.GetCurrentElement()->GetChild(0))->SetText(Settings::get<SettingType::KEYBIND>(key)->toString());
				}
				activeItem = "";
			} else if (pressedKey == Rml::Input::KeyIdentifier::KI_ESCAPE) {
				dynamic_cast<Rml::ElementText*>(event.GetCurrentElement()->GetChild(0))->SetText(Settings::get<SettingType::KEYBIND>(key)->toString());
				activeItem = "";
			} else {
				lastPressedKeys = makeKeybind(event);
				dynamic_cast<Rml::ElementText*>(event.GetCurrentElement()->GetChild(0))->SetText(lastPressedKeys.toString());
			}
		}));
		item->AppendChild(std::move(keybindText));
		break;
	}
	case SettingType::STRING:
		break;
	case SettingType::FILE_PATH:
	{
		Rml::XMLAttributes attributes;
		attributes["type"] = "text";
		attributes["maxlength"] = "4000";
		attributes["rows"] = "3";
		attributes["cols"] = "100";
		Rml::ElementPtr pathElement = Rml::Factory::InstanceElement(document, "textarea", "textarea", attributes);

		Rml::ElementFormControlTextArea* elementFormControlTextArea = rmlui_dynamic_cast<Rml::ElementFormControlTextArea*>(pathElement.get());
		elementFormControlTextArea->SetValue(*Settings::get<SettingType::FILE_PATH>(key));

		// update setting on change
		pathElement->AddEventListener(Rml::EventId::Change,
			new EventPasser([this, key](Rml::Event& event) {
				auto* textarea = dynamic_cast<Rml::ElementFormControlTextArea*>(event.GetCurrentElement());
				if (!textarea) return;
				std::string text = textarea->GetValue();
				Settings::set<SettingType::FILE_PATH>(key, text);
			})
		);

		item->AppendChild(std::move(pathElement));
		break;
	}
	case SettingType::VOID:
		item->AppendChild(std::move(document->CreateTextNode("NULL TYPE")));
		break;
	default:
		break;
	}
	return item;
}


