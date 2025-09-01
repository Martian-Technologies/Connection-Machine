#ifndef elementCreator_h
#define elementCreator_h

#include "gpu/mainRenderer.h"

class ElementCreator {
public:
	ElementCreator() : viewportId(0) { }
	ElementCreator(ViewportId viewportId) : viewportId(viewportId) { assert(viewportId); }
	ElementCreator(const ElementCreator& elementCreator) = delete;
	ElementCreator& operator=(const ElementCreator& elementCreator) = delete;

	~ElementCreator() { clear(); }

	void setup(ViewportId viewportId) { clear(); this->viewportId = viewportId; }
	bool isSetup() { return (bool)viewportId; }

	void removeElement(ElementId id) {
		assert(viewportId);
		auto iter = ids.find(id);
		if (iter == ids.end()) return;
		switch (iter->second) {
		case SelectionElement:
			MainRenderer::get().removeSelectionElement(viewportId, iter->first);
			break;
		case BlockPreview:
			MainRenderer::get().removeBlockPreview(viewportId, iter->first);
			break;
		case ConnectionPreview:
			MainRenderer::get().removeConnectionPreview(viewportId, iter->first);
			break;
		case HalfConnectionPreview:
			MainRenderer::get().removeHalfConnectionPreview(viewportId, iter->first);
			break;
		}
		ids.erase(iter);
	}

	inline void clear() {
		if (!viewportId) return;
		for (auto pair : ids) {
			switch (pair.second) {
			case SelectionElement:
				MainRenderer::get().removeSelectionElement(viewportId, pair.first);
				break;
			case BlockPreview:
				MainRenderer::get().removeBlockPreview(viewportId, pair.first);
				break;
			case ConnectionPreview:
				MainRenderer::get().removeConnectionPreview(viewportId, pair.first);
				break;
			case HalfConnectionPreview:
				MainRenderer::get().removeHalfConnectionPreview(viewportId, pair.first);
				break;
		}
		}
		ids.clear();
	}

	inline bool hasElement(ElementId id) { return ids.find(id) != ids.end(); }

	inline ElementId addSelectionElement(const SelectionObjectElement& selection) {
		assert(viewportId);
		ElementId id = MainRenderer::get().addSelectionObjectElement(viewportId, selection);
		ids[id] = ElementType::SelectionElement;
		return id;
	}

	inline ElementId addSelectionElement(const SelectionElement& selection) {
		assert(viewportId);
		ElementId id = MainRenderer::get().addSelectionElement(viewportId, selection);
		ids[id] = ElementType::SelectionElement;
		return id;
	}

	ElementId addBlockPreview(BlockPreview&& blockPreview) {
		assert(viewportId);
		ElementId id = MainRenderer::get().addBlockPreview(viewportId, std::move(blockPreview));
		ids[id] = ElementType::BlockPreview;
		return id;
	}

	void shiftBlockPreview(ElementId id, Vector shift) {
		assert(viewportId);
		if (ids.contains(id)) MainRenderer::get().shiftBlockPreview(viewportId, id, shift);
	}

	ElementId addConnectionPreview(const ConnectionPreview& connectionPreview) {
		assert(viewportId);
		ElementId id = MainRenderer::get().addConnectionPreview(viewportId, connectionPreview);
		ids[id] = ElementType::ConnectionPreview;
		return id;
	}

	ElementId addHalfConnectionPreview(const HalfConnectionPreview& halfConnectionPreview) {
		assert(viewportId);
		ElementId id = MainRenderer::get().addHalfConnectionPreview(viewportId, halfConnectionPreview);
		ids[id] = ElementType::HalfConnectionPreview;
		return id;
	}

private:
	enum ElementType {
		SelectionElement,
		ConnectionPreview,
		HalfConnectionPreview,
		BlockPreview,
	};

	ViewportId viewportId;
	std::map<ElementId, ElementType> ids;
};

#endif /* elementCreator_h */
