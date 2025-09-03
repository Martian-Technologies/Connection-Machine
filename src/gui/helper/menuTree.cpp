#include "menuTree.h"
#include "menuTreeListener.h"
#include "eventPasser.h"

#include "util/algorithm.h"

MenuTree::MenuTree(Rml::ElementDocument* document, Rml::Element* parent, bool clickableName, bool startOpen) : document(document), parent(parent), clickableName(clickableName), startOpen(startOpen) {
	parent->SetClass("menutree", true);
}

void MenuTree::setPaths(const std::vector<std::string>& paths) {
	std::vector<std::vector<std::string>> vecPaths(paths.size());
	for (unsigned int i = 0; i < vecPaths.size(); i++) {
		stringSplitInto(paths[i], '/', vecPaths[i]);
	}
	setPaths(vecPaths, parent);
}


void MenuTree::setPaths(const std::vector<std::vector<std::string>>& paths, Rml::Element* current) {
	Rml::ElementList elements;
	current->GetElementsByTagName(elements, "div");
	if (elements.empty()) {
		Rml::ElementPtr newDiv = document->CreateElement("div");
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

	Rml::Element* elementList;
	elements.clear();
	div->GetElementsByTagName(elements, "ul");
	if (elements.empty()) {
		Rml::ElementPtr newList = document->CreateElement("ul");
		current->SetClass("collapsed", !startOpen && current != parent);
		elementList = div->AppendChild(std::move(newList));
		current->SetClass("parent", true);
		if (current != parent) {
			Rml::ElementPtr arrow = document->CreateElement("span");
			arrow->SetInnerRML(">");
			arrow->AddEventListener("click", new MenuTreeListener());
			current->InsertBefore(std::move(arrow), div);
		}
	} else {
		elementList = elements[0];
		std::vector<Rml::Element*> children;
		for (unsigned int i = 0; i < elementList->GetNumChildren(); i++) {
			children.push_back(elementList->GetChild(i));
		}
		for (auto element : children) {
			getPath(element);
			const std::string& id = element->GetId();
			unsigned int index = id.size() - 5;
			while (index != 0 && id[index] != '/') index--;
			auto iter = pathsByRoot.find(id.substr(index + (index != 0), id.size() - 5 - index - (index != 0)));
			if (iter == pathsByRoot.end()) {
				elementList->RemoveChild(element);
				continue;
			}
			setPaths(iter->second, element);
			pathsByRoot.erase(iter);
		}
	}
	std::string pathStr = getPath(current);
	if (!(pathStr.empty())) pathStr += "/";
	for (auto iter : pathsByRoot) {
		Rml::ElementPtr newItem = document->CreateElement("li");
		// add listener
		if (!clickableName) newItem->AddEventListener("click", new MenuTreeListener(false, &listenerFunction));
		else {
			newItem->AddEventListener("click", new EventPasser(
				[this](Rml::Event& event) {
					event.StopPropagation();
					Rml::Element* target = event.GetCurrentElement();
					if (listenerFunction)
						listenerFunction(target->GetId().substr(0, target->GetId().size() - 5));
				}
			));
		}
		// Unified selection handling (applies .selected to clicked li, clears others). Runs in addition to any existing click callback.
		newItem->AddEventListener("click", new EventPasser(
			[](Rml::Event& e) {
				Rml::Element* current = e.GetCurrentElement();
				if (!current) return;
				// Skip if the immediate target is the expand/collapse arrow span to preserve prior selection.
				Rml::Element* rawTarget = e.GetTargetElement();
				if (rawTarget && rawTarget->GetTagName() == "span" && rawTarget->GetInnerRML() == ">") return;
				// Conditionally disallow selecting parent nodes if tree root has class no-parent-select.
				if (current->IsClassSet("parent")) {
					Rml::Element* rootCheck = current;
					while (rootCheck && !rootCheck->IsClassSet("menutree")) rootCheck = rootCheck->GetParentNode();
					if (rootCheck && rootCheck->IsClassSet("no-parent-select")) return;
				}
				Rml::Element* root = current;
				while (root && !root->IsClassSet("menutree")) root = root->GetParentNode();
				if (!root) return;
				Rml::ElementList rows;
				root->GetElementsByTagName(rows, "li");
				for (auto* r : rows) r->SetClass("selected", false);
				current->SetClass("selected", true);
			}
		));
		// Precise hover highlighting: only the deepest target under the cursor should have the class.
		newItem->AddEventListener("mouseover", new EventPasser(
			[](Rml::Event& e) {
				Rml::Element* current = e.GetCurrentElement();
				Rml::Element* target  = e.GetTargetElement();
				if (!current || !target) return;
				// Determine the nearest ancestor <li> of the real target.
				Rml::Element* nearestLi = target;
				while (nearestLi && nearestLi->GetTagName() != "li") nearestLi = nearestLi->GetParentNode();
				// Only highlight if THIS li is that nearest li (prevents ancestor highlight while allowing arrow/text children).
				if (nearestLi != current) return;
				Rml::Element* root = current;
				while (root && !root->IsClassSet("menutree")) root = root->GetParentNode();
				if (!root) return;
				Rml::ElementList rows;
				root->GetElementsByTagName(rows, "li");
				for (auto* r : rows) r->SetClass("hovered-leaf", false);
				current->SetClass("hovered-leaf", true);
			}
		));
		newItem->AddEventListener("mouseout", new EventPasser(
			[](Rml::Event& e) {
				Rml::Element* current = e.GetCurrentElement();
				Rml::Element* target  = e.GetTargetElement();
				if (!current || !target) return;
				// Similar logic: only clear if leaving the nearest li (not moving within children)
				Rml::Element* nearestLi = target;
				while (nearestLi && nearestLi->GetTagName() != "li") nearestLi = nearestLi->GetParentNode();
				if (nearestLi != current) return;
				current->SetClass("hovered-leaf", false);
				// Attempt to highlight parent li (so parent shows hover when pointer moves from child into its padding region)
				Rml::Element* parentLi = current->GetParentNode();
				while (parentLi && parentLi->GetTagName() != "li" && !parentLi->IsClassSet("menutree")) parentLi = parentLi->GetParentNode();
				if (parentLi && parentLi->GetTagName() == "li") {
					// Find menutree root to clear any stale hovered-leaf classes first
					Rml::Element* root = parentLi;
					while (root && !root->IsClassSet("menutree")) root = root->GetParentNode();
					if (root) {
						Rml::ElementList rows;
						root->GetElementsByTagName(rows, "li");
						for (auto* r : rows) if (r != parentLi) r->SetClass("hovered-leaf", false);
					}
					parentLi->SetClass("hovered-leaf", true);
				}
			}
		));
		// set id
		newItem->SetId(pathStr + iter.first + "-menu");
		// create div for text
		Rml::ElementPtr newDiv = document->CreateElement("div");
		newDiv->AppendChild(std::move(document->CreateTextNode(iter.first)));
		newItem->AppendChild(std::move(newDiv));
		Rml::Element* newItem2 = elementList->AppendChild(std::move(newItem));
		if (!iter.second.empty()) {
			setPaths(iter.second, newItem2);
		}
	}
}

std::string MenuTree::getPath(Rml::Element* item) {
	if (item == parent) return "";
	return item->GetId().substr(0, item->GetId().size() - 5);
}