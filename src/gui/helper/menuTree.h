#ifndef menuTree_h
#define menuTree_h

#include <RmlUi/Core.h>

class MenuTree {
public:
	typedef std::function<void(std::string)> ListenerFunction;
	MenuTree(Rml::ElementDocument* document, Rml::Element* parent, bool clickableName = true, bool startOpen = true);
	void setListener(ListenerFunction listenerFunction) { this->listenerFunction = listenerFunction; }

	// Modifying items
	void setPaths(const std::vector<std::string>& paths);
	void setPaths(const std::vector<std::vector<std::string>>& paths) { setPaths(paths, parent); }

	// Control whether parent (expandable) nodes are selectable. Default: selectable.
	void disallowParentSelection(bool disallow = true) { if (parent) parent->SetClass("no-parent-select", disallow); }

	// Access underlying root element for external DOM operations (e.g., programmatic selection).
	Rml::Element* getRootElement() const { return parent; }

private:
	std::string getPath(Rml::Element* item);
	void setPaths(const std::vector<std::vector<std::string>>& paths, Rml::Element* current);

	ListenerFunction listenerFunction;
	Rml::ElementDocument* document;
	Rml::Element* parent;
	bool clickableName;
	bool startOpen;
};

#endif /* menuTree_h */