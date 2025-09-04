#ifndef elementList_h
#define elementList_h

#include <RmlUi/Core.h>

class ElementList {
public:
	typedef std::function<void(const std::string&, Rml::Element*)> GeneratorFunction;
	ElementList(Rml::ElementDocument* document, Rml::Element* parent, GeneratorFunction generatorFunction);

	// Items
	void setItems(const std::vector<std::string>& items, bool reuseItems = true);
	void clearItems();
	void appendItem(const std::string& item);
	void insertItem(unsigned int index, const std::string& item);
	void removeIndex(unsigned int index);
	unsigned int getIndex(const std::string& item, unsigned int matchesToSkip = 0) const;
	unsigned int getSize() const { return listItems.size(); }

	// Get elements
	Rml::Element* getRoot() const { return root; }
	Rml::Element* getList() const { return list; }
	Rml::Element* getItem(unsigned int index) const;
	Rml::Element* getItem(const std::string& item, unsigned int matchesToSkip = 0) const { return getItem(getIndex(item, matchesToSkip)); }

	// Events
	typedef std::function<void(const std::string&)> ListenerFunction;
	void addEventListener(Rml::EventId eventId, ListenerFunction listenerFunction);

private:
	void addEventListener(Rml::EventId eventId, ListenerFunction listenerFunction, unsigned int index);

	GeneratorFunction generatorFunction;
	Rml::ElementDocument* document;
	Rml::Element* root;
	Rml::Element* list;

	std::vector<std::pair<Rml::EventId, ListenerFunction>> listenerFunctions;
	std::vector<std::string> listItems;
};

#endif /* elementList_h */