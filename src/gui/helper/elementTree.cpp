#include "elementTree.h"

#include "eventPasser.h"

ElementTree::ElementTree(
	Rml::ElementDocument* document,
	Rml::Element* root,
	std::optional<GeneratorFunction> generatorFunction,
	bool startOpen,
	bool reverseOrder,
	const std::string& pathToTree
) : document(document), root(root), generatorFunction(generatorFunction), startOpen(startOpen), reverseOrder(reverseOrder), pathToTree(pathToTree) {
	if (root->GetNumChildren() != 0) {
		logWarning("ElementTree root should be empty when creating a ElementTree. This can lead to unexpected behavior.", "ElementTree");
	}

	// setup classes
	if (pathToTree == "") {
		root->SetClass("element-tree", true);
	} else {
		Rml::ElementPtr arrow = document->CreateElement("span");
		arrow->SetInnerRML(">");
		arrow->AddEventListener("click", new EventPasser([](Rml::Event& event){
			Rml::Element* target = event.GetCurrentElement()->GetParentNode();
			target->SetClass("collapsed", target->GetClassNames().find("collapsed"));
		}));
		root->AppendChild(std::move(arrow));
	}
	// create tree
	Rml::Element* container = root->AppendChild(std::move(document->CreateElement("div")));
	if (!pathToTree.empty()) {
		size_t split = pathToTree.find_last_of('/');
		if (split == std::string::npos) {
			container->AppendChild(this->document->CreateTextNode(pathToTree));
		} else {
			container->AppendChild(this->document->CreateTextNode(pathToTree.substr(split + 1, std::string::npos)));
		}

	}
	tree = container->AppendChild(std::move(document->CreateElement("ul")));
	tree->SetClass("parent", true);
	if (!pathToTree.empty()) {
		tree->SetClass("collapsed", !startOpen);
	}
}

ElementTree::ElementTree(
	Rml::ElementDocument* document,
	Rml::Element* root,
	bool startOpen,
	bool reverseOrder,
	const std::string& pathToTree
) : document(document), root(root), generatorFunction(std::nullopt), startOpen(startOpen), reverseOrder(reverseOrder), pathToTree(pathToTree) {
	if (root->GetNumChildren() != 0) {
		logWarning("ElementTree root should be empty when creating a ElementTree. This can lead to unexpected behavior.", "ElementTree");
	}
	// create tree
	tree = root->AppendChild(std::move(document->CreateElement("ul")));
	tree->SetClass("parent", true);
	// setup classes and stuff
	if (pathToTree == "") {
		root->SetClass("element-tree", true);
	} else {
		Rml::ElementPtr arrow = document->CreateElement("span");
		arrow->SetInnerRML(">");
		arrow->AddEventListener("click", new EventPasser([](Rml::Event& event){
			Rml::Element* target = event.GetCurrentElement();
			target->SetClass("collapsed", target->GetClassNames().find("collapsed"));
		}));
		root->AppendChild(std::move(arrow));
		tree->SetClass("collapsed", !startOpen);
	}
}

void ElementTree::setItems(const std::vector<std::string>& items, bool reuseItems) {
	if (reuseItems) {
		std::map<std::string, std::pair<bool, std::vector<std::string>>> itemsMap;
		for (const std::string& item : items) {
			if (item.empty()) continue;
			size_t index = item.find_first_of('/');
			if (index == std::string::npos) {
				itemsMap.try_emplace(item).first->second.first = true;
				logInfo(item);
			} else {
				itemsMap.try_emplace(item.substr(0, index)).first->second.second.emplace_back(item.substr(index + 1, std::string::npos));
				logInfo(item.substr(0, index));
			}
		}
		for (unsigned int i = 0; i < treeItems.size(); i++) {
			const std::string& string = treeItems[i].string;
			auto iter = itemsMap.find(string);
			if (iter == itemsMap.end()) {
				tree->RemoveChild(tree->GetChild(i));
				treeItems.erase(treeItems.begin() + i);
				i--;
				continue;
			}
			Rml::Element* element = tree->GetChild(i);
			Rml::Element* topTreeItem = element->GetFirstChild();
			Rml::Element* bottomTreeItem = element->GetLastChild();
			if (iter->second.first) { // should have item
				if (!treeItems[i].isItem) {
					treeItems[i].isItem = true;
					if (generatorFunction) {
						(*generatorFunction)((pathToTree.empty()) ? string : (pathToTree + "/" + string), topTreeItem);
					} else {
						topTreeItem->AppendChild(document->CreateTextNode(string));
					}
					for (const auto& listenerFunction : listenerFunctions) {
						topTreeItem->AddEventListener(listenerFunction.first, new EventPasser(
							[listenerFunction=listenerFunction.second, string = (pathToTree.empty()) ? string : (pathToTree + "/" + string)](Rml::Event& event){
								listenerFunction(string);
							}
						));
					}
				}
			} else { // should not have item
				if (treeItems[i].isItem) {
					treeItems[i].isItem = false;
					while (topTreeItem->HasChildNodes()) {
						topTreeItem->RemoveChild(topTreeItem->GetLastChild());
					}
				}
			}
			if (iter->second.second.empty()) { // shuold not have sub tree
				if (treeItems[i].tree) {
					treeItems[i].tree.reset();
					while (bottomTreeItem->HasChildNodes()) {
						bottomTreeItem->RemoveChild(bottomTreeItem->GetLastChild());
					}
				}
			} else { // shuold have sub tree
				if (!treeItems[i].tree) {
					treeItems[i].tree = std::make_unique<ElementTree>(document, bottomTreeItem, generatorFunction, startOpen, reverseOrder, (pathToTree.empty()) ? string : (pathToTree + "/" + string));
					treeItems[i].tree->setItems(iter->second.second);
					for (const auto& listenerFunction : listenerFunctions) {
						treeItems[i].tree->addEventListener(listenerFunction.first, listenerFunction.second);
					}
				}
			}
			itemsMap.erase(iter); // remove it because we already added it to the tree
		}
		for (const auto& pair : itemsMap) {
			if (pair.second.first) addItem(pair.first);
			for (const std::string& item : pair.second.second) {
				addItem(pair.first + "/" + item);
			}
		}
	} else {
		clearItems();
		for (auto item : items) {
			addItem(item);
		}
	}
}

void ElementTree::setItems(const std::vector<std::vector<std::string>>& items, bool reuseItems) {
	std::vector<std::string> itemsVec;
	itemsVec.reserve(items.size());
	for (const std::vector<std::string>& item : items) {
		itemsVec.push_back("");
		for (const std::string& str : item) {
			if (itemsVec.back().empty())
				itemsVec.back() = str;
			else
				itemsVec.back() += "/" + str;
		}
	}
	setItems(itemsVec, reuseItems);
}

void ElementTree::clearItems() {
	while (tree->HasChildNodes()) {
		tree->RemoveChild(tree->GetLastChild());
	}
	treeItems.clear();
}

void ElementTree::addItem(const std::string& item) {
	logInfo("adding item {}", "", item);
	if (item.empty()) return;
	size_t index = item.find_first_of('/');
	std::string string;
	std::string restOfItem;
	if (index == std::string::npos) {
		string = std::move(item);
	} else {
		string = item.substr(0, index);
		restOfItem = item.substr(index + 1, std::string::npos);
	}
	auto itemIter = std::find_if(treeItems.begin(), treeItems.end(), [string](const TreeItem& treeItem){ return treeItem.string == string; });
	if (itemIter == treeItems.end()) { // create new item
		// add it to the tree
		unsigned int itemIndex;
		for (itemIndex = 0; itemIndex < treeItems.size(); itemIndex++) {
			if (reverseOrder ^ (treeItems[itemIndex].string > string)) {
				treeItems.insert(treeItems.begin() + itemIndex, string);
				break;
			}
		}
		if (itemIndex >= treeItems.size()) {
			treeItems.emplace_back(string);
		}

		// create top and bottom tree items
		Rml::ElementPtr topTreeItem = document->CreateElement("div");
		Rml::ElementPtr bottomTreeItem = document->CreateElement("div");
		if (index == std::string::npos) {
			treeItems[itemIndex].isItem = true;
			if (generatorFunction) {
				(*generatorFunction)((pathToTree.empty()) ? string : (pathToTree + "/" + string), topTreeItem.get());
			} else {
				topTreeItem->AppendChild(document->CreateTextNode(string));
			}
			for (const auto& listenerFunction : listenerFunctions) {
				topTreeItem->AddEventListener(listenerFunction.first, new EventPasser(
					[listenerFunction=listenerFunction.second, string = (pathToTree.empty()) ? string : (pathToTree + "/" + string)](Rml::Event& event){
						listenerFunction(string);
					}
				));
			}
		} else {
			treeItems[itemIndex].tree = std::make_unique<ElementTree>(document, bottomTreeItem.get(), generatorFunction, startOpen, reverseOrder, (pathToTree.empty()) ? string : (pathToTree + "/" + string));
			treeItems[itemIndex].tree->addItem(restOfItem);
			for (const auto& listenerFunction : listenerFunctions) {
				treeItems[itemIndex].tree->addEventListener(listenerFunction.first, listenerFunction.second);
			}
		}
		// make tree item
		Rml::ElementPtr treeItem = document->CreateElement("li");
		treeItem->AppendChild(std::move(topTreeItem));
		treeItem->AppendChild(std::move(bottomTreeItem));

		tree->InsertBefore(std::move(treeItem), tree->GetChild(itemIndex));
	} else { // edit old item
		if (index == std::string::npos) { // adding item here
			if (itemIter->isItem) return; // item already added
			itemIter->isItem = true;
			Rml::Element* topTreeItem = tree->GetChild(distance(treeItems.begin(), itemIter))->GetFirstChild();
			if (generatorFunction) {
				(*generatorFunction)((pathToTree.empty()) ? string : (pathToTree + "/" + string), topTreeItem);
			} else {
				topTreeItem->AppendChild(document->CreateTextNode(string));
			}
			for (const auto& listenerFunction : listenerFunctions) {
				topTreeItem->AddEventListener(listenerFunction.first, new EventPasser(
					[listenerFunction=listenerFunction.second, string = (pathToTree.empty()) ? string : (pathToTree + "/" + string)](Rml::Event& event){
						listenerFunction(string);
					}
				));
			}
		} else if (itemIter->tree) { // adding item later
			itemIter->tree->addItem(restOfItem);
		} else { // adding list here and item later
			Rml::Element* bottomTreeItem = tree->GetChild(distance(treeItems.begin(), itemIter))->GetLastChild();
			itemIter->tree = std::make_unique<ElementTree>(document, bottomTreeItem, generatorFunction, startOpen, reverseOrder, (pathToTree.empty()) ? string : (pathToTree + "/" + string));
			itemIter->tree->addItem(restOfItem);
			for (const auto& listenerFunction : listenerFunctions) {
				itemIter->tree->addEventListener(listenerFunction.first, listenerFunction.second);
			}
		}
	}
}

// adding a star to the end of the path will remove all items that follow that path
// "top/mid/*" works, "top/mid/bot*" won't work
// returns if the tree is now empty
bool ElementTree::removeItem(const std::string& item) {
	if (item.empty()) return !(tree->HasChildNodes());
	if (item == "*") {
		clearItems();
		return true;
	}
	size_t index = item.find_first_of('/');
	std::string string;
	std::string restOfItem;
	if (index == std::string::npos) {
		string = std::move(item);
	} else {
		string = item.substr(0, index);
		restOfItem = item.substr(index + 1, std::string::npos);
	}
	auto itemIter = std::find_if(treeItems.begin(), treeItems.end(), [string](const TreeItem& treeItem){ return treeItem.string == string; });
	if (itemIter == treeItems.end()) return !(tree->HasChildNodes()); // its gone!
	if (index == std::string::npos) { // this level
		if (itemIter->tree) { // also has a tree
			Rml::Element* topTreeItem = tree->GetChild(distance(treeItems.begin(), itemIter))->GetFirstChild();
			while (topTreeItem->HasChildNodes()) {
				topTreeItem->RemoveChild(topTreeItem->GetLastChild());
			}
			itemIter->isItem = false;
		} else { // delete the whole node
			tree->RemoveChild(tree->GetChild(distance(treeItems.begin(), itemIter)));
			treeItems.erase(itemIter);
		}
	} else { // remove later
		if (itemIter->tree->removeItem(restOfItem)) { // if the sub tree is empty
			if (itemIter->isItem) { // if this still has a item
				Rml::Element* bottomTreeItem = tree->GetChild(distance(treeItems.begin(), itemIter))->GetLastChild();
				while (bottomTreeItem->HasChildNodes()) {
					bottomTreeItem->RemoveChild(bottomTreeItem->GetLastChild());
				}
				itemIter->tree.reset();
			} else {
				tree->RemoveChild(tree->GetChild(distance(treeItems.begin(), itemIter)));
				treeItems.erase(itemIter);
			}
		}
	}
	return !(tree->HasChildNodes());
}

void ElementTree::addEventListener(Rml::EventId eventId, ListenerFunction listenerFunction) {
	for (unsigned int i = 0; i < treeItems.size(); i++) {
		if (treeItems[i].isItem) {
			Rml::Element* topTreeItem = tree->GetChild(i)->GetFirstChild();
			topTreeItem->AddEventListener(eventId, new EventPasser(
				[listenerFunction, string = (pathToTree.empty()) ? treeItems[i].string : (pathToTree + "/" + treeItems[i].string)](Rml::Event& event){
					listenerFunction(string);
				}
			));
		}
		if (treeItems[i].tree) {
			treeItems[i].tree->addEventListener(eventId, listenerFunction);
		}
	}
	listenerFunctions.emplace_back(eventId, listenerFunction);
}

// void ElementTree::setupItem(const std::string& item, Rml::Element* element) {
// 	element->AddEventListener(Rml::EventId::Mouseover, new EventPasser(
// 		[this](Rml::Event& event) {
// 			Rml::Element* current = event.GetCurrentElement();
// 			Rml::Element* target  = event.GetTargetElement();
// 			if (!current || !target) return;
// 			for (unsigned int i = 0; i < tree->GetNumChildren(); i++) {
// 				tree->GetChild(i)->SetClass("hovered-leaf", false);
// 			}
// 			target->SetClass("hovered-leaf", true);
// 		}
// 	));
// 	element->AddEventListener(Rml::EventId::Mouseout, new EventPasser(
// 		[](Rml::Event& event) {
// 			Rml::Element* current = event.GetCurrentElement();
// 			Rml::Element* target  = event.GetTargetElement();
// 			if (!current || !target) return;
// 			target->SetClass("hovered-leaf", true);
// 			// Similar logic: only clear if leaving the nearest li (not moving within children)
// 			Rml::Element* nearestLi = target;
// 			while (nearestLi && nearestLi->GetTagName() != "li") nearestLi = nearestLi->GetParentNode();
// 			if (nearestLi != current) return;
// 			current->SetClass("hovered-leaf", false);
// 			// Attempt to highlight parent li (so parent shows hover when pointer moves from child into its padding region)
// 			Rml::Element* parentLi = current->GetParentNode();
// 			while (parentLi && parentLi->GetTagName() != "li" && !parentLi->IsClassSet("menutree")) parentLi = parentLi->GetParentNode();
// 			if (parentLi && parentLi->GetTagName() == "li") {
// 				// Find menutree root to clear any stale hovered-leaf classes first
// 				Rml::Element* root = parentLi;
// 				while (root && !root->IsClassSet("menutree")) root = root->GetParentNode();
// 				if (root) {
// 					Rml::ElementList rows;
// 					root->GetElementsByTagName(rows, "li");
// 					for (auto* r : rows) if (r != parentLi) r->SetClass("hovered-leaf", false);
// 				}
// 				parentLi->SetClass("hovered-leaf", true);
// 			}
// 		}
// 	));
// }
