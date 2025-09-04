#ifndef itemTree_h
#define itemTree_h

#include <RmlUi/Core.h>

class ItemTree {
	struct TreeItem {
		TreeItem(const std::string& string) : string(string) {}
		std::string string;
		bool isItem = false;
		std::unique_ptr<ItemTree> tree;
	};
public:
	typedef std::function<void(const std::string&, Rml::Element*)> GeneratorFunction;
	ItemTree(
		Rml::ElementDocument* document,
		Rml::Element* parent,
		std::optional<GeneratorFunction> generatorFunction,
		bool startOpen = true,
		bool reverseOrder = false,
		const std::string& pathToTree = ""
	);
	ItemTree(
		Rml::ElementDocument* document,
		Rml::Element* parent,
		bool startOpen = true,
		bool reverseOrder = false,
		const std::string& pathToTree = ""
	);

	// Items
	void setItems(const std::vector<std::string>& items, bool reuseItems = true);
	void setItems(const std::vector<std::vector<std::string>>& items, bool reuseItems = true);
	void clearItems();
	void addItem(const std::string& item);
	bool removeItem(const std::string& item);

	// Get elements
	Rml::Element* getRoot() const { return root; }
	Rml::Element* getTree() const { return tree; }
	// Rml::Element* getItem(const std::string& item, unsigned int matchesToSkip = 0) const { return getItem(getIndex(item, matchesToSkip)); }

	// Events
	typedef std::function<void(const std::string&)> ListenerFunction;
	void addEventListener(Rml::EventId eventId, ListenerFunction listenerFunction);

private:
	void setupItem(const std::string& item, Rml::Element* element);

	std::optional<GeneratorFunction> generatorFunction;
	Rml::ElementDocument* document;
	Rml::Element* root;
	Rml::Element* tree;

	std::string pathToTree;

	std::vector<std::pair<Rml::EventId, ListenerFunction>> listenerFunctions;
	std::vector<TreeItem> treeItems;

	bool reverseOrder;
	bool startOpen;
};

#endif /* itemTree_h */