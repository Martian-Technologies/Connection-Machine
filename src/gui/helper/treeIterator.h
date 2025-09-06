#ifndef treeIterator_h
#define treeIterator_h

#include "RmlUi/Core/Element.h"

class TreeIterator {
public:
	TreeIterator(Rml::Element* curentElement) : curentElement(curentElement) { assert(curentElement); updateIndex(); }
	TreeIterator(Rml::Element* curentElement, unsigned int index) : curentElement(curentElement), index(index) { assert(curentElement); updateIndex(); }

	// compare
	inline bool operator==(const TreeIterator& other) const {
		if (index != other.index) return false;
		return sameParent(other);
	}
	inline bool operator!=(const TreeIterator& other) const { return !(operator==(other)); }

	// list
	inline TreeIterator operator+(int amount) const {
		TreeIterator iter = *this;
		return (iter += amount);;
	}
	inline TreeIterator operator-(int amount) const {
		TreeIterator iter = *this;
		return (iter -= amount);
	}
	inline TreeIterator& operator+=(int amount) {
		if (amount > 0) next(amount);
		else previous(-amount);
		return *this;
	}
	inline TreeIterator& operator-=(int amount) {
		if (amount > 0) previous(amount);
		else next(-amount);
		return *this;
	}
	inline TreeIterator& operator++() {
		next();
		return *this;
	}
	inline TreeIterator& operator--() {
		previous();
		return *this;
	}
	inline TreeIterator operator++(int) {
		TreeIterator tmp = *this;
		next();
		return tmp;
	}
	inline TreeIterator operator--(int) {
		TreeIterator tmp = *this;
		previous();
		return tmp;
	}
	// assumes they have the same parent
	inline int operator-(const TreeIterator& other) const { return index - other.index; }
	// assumes they have the same parent
	inline int operator>(const TreeIterator& other) const { return index > other.index; }
	// assumes they have the same parent
	inline int operator>=(const TreeIterator& other) const { return index >= other.index; }
	// assumes they have the same parent
	inline int operator<(const TreeIterator& other) const { return index < other.index; }
	// assumes they have the same parent
	inline int operator<=(const TreeIterator& other) const { return index <= other.index; }
	inline bool atLast() const { return index == curentElement->GetParentNode()->GetNumChildren(); }
	inline bool atFirst() const { return index == 0; }
	inline unsigned int getIndex() const { return index; }
	inline unsigned int getEndIndex() const { return curentElement->GetParentNode()->GetNumChildren(); }
	inline bool sameParent(const TreeIterator& other) const { return curentElement->GetParentNode() == other.curentElement->GetParentNode(); }

	// parent/child
	inline std::optional<TreeIterator> getParent() const {
		Rml::Element* parent = getParentElement();
		if (parent) return TreeIterator(parent);
		return std::nullopt;
	}
	inline std::optional<TreeIterator> getChild(unsigned int indexOfChild) const {
		Rml::Element* childParent = getChildParentElement();
		if (!childParent->HasChildNodes()) return std::nullopt;
		unsigned int count = childParent->GetNumChildren();
		if (count >= indexOfChild) return TreeIterator(childParent->GetChild(count-1), count);
		return TreeIterator(childParent->GetChild(indexOfChild), indexOfChild);
	}
	inline std::optional<TreeIterator> getBeginChild() const {
		Rml::Element* childParent = getChildParentElement();
		if (!childParent->HasChildNodes()) return std::nullopt;
		return TreeIterator(childParent->GetChild(0), 0);
	}
	inline std::optional<TreeIterator> getEndChild() const {
		Rml::Element* childParent = getChildParentElement();
		if (!childParent->HasChildNodes()) return std::nullopt;
		unsigned int count = childParent->GetNumChildren();
		return TreeIterator(childParent->GetChild(count-1), count);
	}
	inline bool toParent() {
		Rml::Element* parent = getParentElement();
		if (parent) {
			curentElement = parent;
			updateIndex();
		}
		return parent != nullptr;
	}
	inline bool toChild() {
		Rml::Element* childParent = getChildParentElement();
		if (!childParent->HasChildNodes()) return false;
		curentElement = childParent->GetChild(0);
		index = 0;
		return true;
	}
	inline bool toChild(unsigned int indexOfChild) {
		Rml::Element* childParent = getChildParentElement();
		if (!childParent->HasChildNodes()) return false;
		unsigned int count = childParent->GetNumChildren();
		if (count >= indexOfChild) {
			curentElement = childParent->GetChild(count-1);
			index = count;
			return true;
		}
		curentElement = childParent->GetChild(indexOfChild);
		index = indexOfChild;
		return true;
	}
	inline bool atTop() const {
		return getParentElement() == nullptr;
	}
	inline bool atBottom() const {
		Rml::Element* childParent = getChildParentElement();
		if (childParent == nullptr) return false;
		return childParent->HasChildNodes();
	}

private:
	// should return a "ul"
	Rml::Element* getChildParentElement() const {
		return curentElement->GetChild(0); // TODO: make this correct
	}
	// should return a "ul"
	Rml::Element* getParentElement() const {
		return curentElement->GetParentNode(); // TODO: make this correct
	}
	void updateIndex() {
		Rml::Element* parent = curentElement->GetParentNode();
		unsigned int count = parent->GetNumChildren();
		if (index >= count) { // is end iter
			index = count;
			return;
		}
		if (parent->GetChild(index) == curentElement) return;
		for (unsigned int i = 0; i < count; i++) {
			if (index == i) continue;
			if (parent->GetChild(i) == curentElement) {
				index = i;
				return;
			}
		}
		logError("Failed to find index of element {}. This should not happen.", "TreeIterator", curentElement);
	}
	void next() {
		Rml::Element* parent = curentElement->GetParentNode();
		unsigned int count = parent->GetNumChildren();
		if (index == count || ++index == count) return;
		curentElement = parent->GetChild(index);
	}
	void next(unsigned int amount) {
		if (amount == 0) return;
		Rml::Element* parent = curentElement->GetParentNode();
		unsigned int count = parent->GetNumChildren();
		if (index == count) return;
		index += amount;
		if (index >= count) {
			index = count;
			return;
		}
		curentElement = parent->GetChild(index);
	}
	void previous() {
		if (index == 0) return;
		--index;
		curentElement = curentElement->GetParentNode()->GetChild(index);
	}
	void previous(unsigned int amount) {
		if (index == 0 || amount == 0) return;
		index -= amount;
		if (index < 0) index = 0;
		curentElement = curentElement->GetParentNode()->GetChild(index);
	}

	unsigned int index = 0;
	Rml::Element* curentElement; // should be a "li"
};

#endif /* treeIterator_h */
