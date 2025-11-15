#include "searchBar.h"

#include "gui/helper/eventPasser.h"
#include "backend/settings/settings.h"

SearchBar::SearchBar(Rml::Element* document) : context(document) {
	Initialize();
}

void SearchBar::reapplyFilter() {
	Rml::Element* search = context->GetElementById("settings-search");
	if (!search) return;
	auto* input = dynamic_cast<Rml::ElementFormControlInput*>(search);
	std::string text = input ? input->GetValue() : "";
	queryContext(text);
}

void SearchBar::Initialize() {
	Rml::Element* search = context->GetElementById("settings-search");
	if (!search) return;

	search->AddEventListener(Rml::EventId::Keyup, new EventPasser(
		[this](Rml::Event& event) {
			auto* input = dynamic_cast<Rml::ElementFormControlInput*>(event.GetCurrentElement());
			std::string text = input ? input->GetValue() : "";
			queryContext(text);
		}
	));
	search->AddEventListener(Rml::EventId::Change, new EventPasser(
		[this](Rml::Event& event) {
			auto* input = dynamic_cast<Rml::ElementFormControlInput*>(event.GetCurrentElement());
			std::string text = input ? input->GetValue() : "";
			queryContext(text);
		}
	));
}

bool SearchBar::match(const std::string& haystack, const std::string& needle) {
	if (needle.empty()) return true;
	std::string h(haystack), n(needle);
	std::transform(h.begin(), h.end(), h.begin(), ::tolower);
	std::transform(n.begin(), n.end(), n.begin(), ::tolower);
	return h.find(n) != std::string::npos;
}

void SearchBar::queryContext(const std::string& text) {
	if (text.empty()) {
		clearFilter();
	} else {
		applyFilter(text);
	}
}

void SearchBar::clearFilter() {
	Rml::Element* treeRoot = context->GetElementById("settings-content-panel");
	if (!treeRoot) return;
	Rml::ElementList all;
	treeRoot->GetElementsByTagName(all, "li");
	for (auto* el : all) {
		el->SetClass("search-hide", false);
		el->SetClass("search-match", false);
		Rml::ElementList headers;
		el->GetElementsByClassName(headers, "settings-tree-header");
		for (auto* h : headers) h->SetClass("search-nonresult-hide", false);
	}
}

void SearchBar::applyFilter(const std::string& needle) {
	Rml::Element* treeRoot = context->GetElementById("settings-content-panel");
	if (!treeRoot) return;

	Rml::ElementList listItems;
	treeRoot->GetElementsByTagName(listItems, "li");

	// Reset state
	for (auto* el : listItems) {
		el->SetClass("search-hide", true);
		el->SetClass("search-match", false);
		Rml::ElementList headers; el->GetElementsByClassName(headers, "settings-tree-header");
		for (auto* h : headers) h->SetClass("search-nonresult-hide", false);
	}

	// Prep structures for ancestor evaluation
	struct NodeInfo { Rml::Element* el; std::string id; std::string path; bool hasKey; bool matched = false; };
	std::vector<NodeInfo> nodes; nodes.reserve(listItems.size());
	for (auto* el : listItems) {
		std::string id = el->GetId(); if (id.size() < 5) continue; // ignore malformed
		std::string path = id.substr(0, id.size() - 5); // strip -menu
		bool hasKey = Settings::hasKey(path);
		nodes.push_back({el, id, path, hasKey, false});
	}

	int matchCount = 0;
	for (auto& n : nodes) {
		if (!n.hasKey) continue;
		if (!match(n.path, needle)) continue;
		n.matched = true; matchCount++;
		// Unhide chain to root
		Rml::Element* cur = n.el;
		while (cur && cur != treeRoot) { cur->SetClass("search-hide", false); cur = cur->GetParentNode(); }
		n.el->SetClass("search-match", true);
	}

	if (matchCount == 0) { clearFilter(); return; }

	for (auto& n : nodes) {
		if (n.el->IsClassSet("search-hide")) continue;
		if (n.el->IsClassSet("search-match")) continue;
		bool descendantMatch = false;
		for (auto& other : nodes) {
			if (!other.matched) continue;
			if (other.path.size() > n.path.size() && other.path.compare(0, n.path.size(), n.path) == 0 && other.path[n.path.size()] == '/') {
				descendantMatch = true; break;
			}
		}
		if (!descendantMatch) {
			n.el->SetClass("search-hide", true);
		} else {
			Rml::ElementList headers; n.el->GetElementsByClassName(headers, "settings-tree-header");
			for (auto* h : headers) h->SetClass("search-nonresult-hide", true);
		}
	}
}
