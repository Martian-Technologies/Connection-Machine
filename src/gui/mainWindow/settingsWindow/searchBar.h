#ifndef searchBar_h
#define searchBar_h

#include <RmlUi/Core.h>

// TODO: not fully implemented, figure out plan moving forward

class SearchBar {
public:
	SearchBar(Rml::Element* document);;

	void reapplyFilter();

	// inline void setActiveCategory(const ACTIVE_CATEGORIES category) { activeCategory = category; }

private:
	void Initialize();
	void queryContext(const std::string& text);

	static bool match(const std::string& haystack, const std::string& needle);
	void applyFilter(const std::string& needle);
	void clearFilter();


	Rml::Element* context;
	// ACTIVE_CATEGORIES activeCategory; // should always start as general
};

#endif /* searchBar_h */
