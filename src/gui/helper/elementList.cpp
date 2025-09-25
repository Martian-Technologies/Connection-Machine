#include "elementList.h"

#include "eventPasser.h"

ElementList::ElementList(Rml::ElementDocument* document, Rml::Element* root, GeneratorFunction generatorFunction) :
	document(document), root(root), generatorFunction(generatorFunction) {
	if (root->GetNumChildren() != 0) {
		logWarning("ElementList root should be empty when creating a ElementList. This can lead to unexpected behavior.", "ElementList");
	}
	root->SetClass("element-list", true);
	list = root->AppendChild(std::move(document->CreateElement("ul")));
}

void ElementList::setItems(const std::vector<std::string>& items, bool reuseItems) {
	if (reuseItems) {
		std::map<std::string, unsigned int> neededItemsCount;
		for (const std::string& item : items) {
			neededItemsCount[item]++;
		}
		unsigned int i;
		for (i = 0; i < listItems.size(); i++) {
			if (items.size() <= i) { // if there are no more items to set remove the rest of the items
				for (unsigned int j = 0; j < listItems.size() - items.size(); j++) {
					list->RemoveChild(list->GetLastChild());
					listItems.pop_back();
				}
				return;
			}
			if (listItems[i] == items[i]) { // if the curent item it right
				// update neededItemsCount
				auto iter = neededItemsCount.find(items[i]);
				if (iter->second == 1) {
					neededItemsCount.erase(iter);
				} else {
					iter->second--;
				}
			} else { // if the curent item it wrong
				if (neededItemsCount.contains(listItems[i])) {
					insertItem(i, items[i]);
				} else {
					list->RemoveChild(list->GetChild(i));
					listItems.erase(listItems.begin() + i);
					i--; // do this item again
				}
			}
		}
		// add rest of items
		for (; i < items.size(); i++) {
			appendItem(items[i]);
		}
	} else {
		clearItems();
		for (auto item : items) {
			appendItem(item);
		}
	}
}

void ElementList::clearItems() {
	while (list->HasChildNodes()) {
		list->RemoveChild(list->GetLastChild());
	}
	listItems.clear();
}

void ElementList::appendItem(const std::string& item) {
	// create the item
	Rml::ElementPtr listItem = document->CreateElement("li");
	// fill it with stuff
	generatorFunction(item, listItem.get());
	for (const auto& listenerFunction : listenerFunctions) {
		listItem->AddEventListener(listenerFunction.first, new EventPasser([listenerFunction=listenerFunction.second, item](Rml::Event& event){
			listenerFunction(item);
		}));
	}
	// save the item string
	listItems.emplace_back(item);
	// add it to the list
	list->AppendChild(std::move(listItem));
	// add event listeners
	for (auto listener : listenerFunctions) {
		addEventListener(listener.first, listener.second, listItems.size()-1);
	}
}

void ElementList::insertItem(unsigned int index, const std::string& item) {
	// if its at the end we can append instead
	if (index >= listItems.size()) {
		appendItem(item);
		return;
	}
	// create the item
	Rml::ElementPtr listItem = document->CreateElement("li");
	// fill it with stuff
	generatorFunction(item, listItem.get());
	// save the item string
	listItems.insert(listItems.begin() + index, item);
	// we need to use the item that will be after the new item when inserting
	Rml::Element* elementAfter = list->GetChild(index);
	list->InsertBefore(std::move(listItem), elementAfter);
	// add event listeners
	for (auto listener : listenerFunctions) {
		addEventListener(listener.first, listener.second, index);
	}
}

void ElementList::removeIndex(unsigned int index) {
	// cant remove non existent indexs
	if (index >= listItems.size()) return;
	// removes the child at index
	list->RemoveChild(list->GetChild(index));
	listItems.erase(listItems.begin() + index);
}

unsigned int ElementList::getIndex(const std::string& item, unsigned int matchesToSkip) const {
	auto iter = std::find(listItems.begin(), listItems.end(), item);
	while (matchesToSkip > 0) {
		iter = std::find(iter, listItems.end(), item);
		if (iter == listItems.end()) return listItems.size();
		--matchesToSkip;
	}
	return distance(listItems.begin(), iter);
}

Rml::Element* ElementList::getItem(unsigned int index) const {
	// cant get non existent indexs
	if (index >= listItems.size()) return nullptr;
	// returns the item at index
	return list->GetChild(index);
}

void ElementList::addEventListener(Rml::EventId eventId, ListenerFunction listenerFunction) {
	for (unsigned int i = 0; i < list->GetNumChildren(); i++) {
		addEventListener(eventId, listenerFunction, i);
	}
	listenerFunctions.emplace_back(eventId, listenerFunction);
}

void ElementList::addEventListener(Rml::EventId eventId, ListenerFunction listenerFunction, unsigned int index) {
	list->GetChild(index)->AddEventListener(eventId, new EventPasser(
		[listenerFunction, item=listItems[index]](Rml::Event& event){
			listenerFunction(item);
		}
	));
}
