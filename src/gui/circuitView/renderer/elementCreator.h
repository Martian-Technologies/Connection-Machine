#ifndef elementCreator_h
#define elementCreator_h

#include <map>

#include "renderer.h"

class ElementCreator {
public:
	ElementCreator() : renderer(nullptr) { }
	ElementCreator(Renderer* renderer) : renderer(renderer), ids() { assert(renderer); }

	~ElementCreator() { clear(); }

	bool isSetup() { return (bool)renderer; }

	void removeElement(ElementID id) {
		assert(renderer);
		auto iter = ids.find(id);
		if (iter == ids.end()) return;
		switch (iter->second) {
		case SelectionElement:
			renderer->removeSelectionElement(iter->first);
			break;
		case BlockPreview:
			renderer->removeBlockPreview(iter->first);
			break;
		case ConnectionPreview:
			renderer->removeConnectionPreview(iter->first);
			break;
		case HalfConnectionPreview:
			renderer->removeHalfConnectionPreview(iter->first);
			break;
		}
		ids.erase(iter);
	}

	inline void clear() {
		if (!renderer) return;
		for (auto pair : ids) {
			switch (pair.second) {
			case SelectionElement:
				renderer->removeSelectionElement(pair.first);
				break;
			case BlockPreview:
				renderer->removeBlockPreview(pair.first);
				break;
			case ConnectionPreview:
				renderer->removeConnectionPreview(pair.first);
				break;
			case HalfConnectionPreview:
				renderer->removeHalfConnectionPreview(pair.first);
				break;
			}
		}
		ids.clear();
	}

	inline bool hasElement(ElementID id) { return ids.find(id) != ids.end(); }

	inline ElementID addSelectionElement(const SelectionObjectElement selection) {
		assert(renderer);
		ElementID id = renderer->addSelectionElement(selection);
		ids[id] = ElementType::SelectionElement;
		return id;
	}

	inline ElementID addSelectionElement(const SelectionElement& selection) {
		assert(renderer);
		ElementID id = renderer->addSelectionElement(selection);
		ids[id] = ElementType::SelectionElement;
		return id;
	}

	ElementID addBlockPreview(const BlockPreview& blockPreview) {
		assert(renderer);
		ElementID id = renderer->addBlockPreview(blockPreview);
		ids[id] = ElementType::BlockPreview;
		return id;
	}

	ElementID addConnectionPreview(const ConnectionPreview& connectionPreview) {
		assert(renderer);
		ElementID id = renderer->addConnectionPreview(connectionPreview);
		ids[id] = ElementType::ConnectionPreview;
		return id;
	}

	ElementID addHalfConnectionPreview(const HalfConnectionPreview& halfConnectionPreview) {
		assert(renderer);
		ElementID id = renderer->addHalfConnectionPreview(halfConnectionPreview);
		ids[id] = ElementType::HalfConnectionPreview;
		return id;
	}

	void addConfetti(FPosition start) {
		assert(renderer);
		renderer->spawnConfetti(start);
	}

private:
	enum ElementType {
		SelectionElement,
		ConnectionPreview,
		HalfConnectionPreview,
		BlockPreview,
	};

	Renderer* renderer;
	std::map<ElementID, ElementType> ids;
};

#endif /* elementCreator_h */
