#ifndef treeIterator_h
#define treeIterator_h

#include <RmlUi/Core.h>

class TreeIterator {
public:
	// usings
	using iterator_category = std::random_access_iterator_tag;

	// constructors
	TreeIterator(Rml::Element* currentElement) : TreeIterator(currentElement, 0) {}
	TreeIterator(Rml::Element* currentElement, unsigned int index) : currentElement(currentElement), index(index) {
		assert(currentElement);
		if (currentElement->GetTagName() == "ul") {
			index = 0;
			if (currentElement->HasChildNodes()) {
				currentElement = currentElement->GetChild(0);
			} else {
				atUl = true;
			}
		} else {
			updateIndex();
		}
	}

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
	inline bool operator>(const TreeIterator& other) const { return index > other.index; }
	// assumes they have the same parent
	inline bool operator>=(const TreeIterator& other) const { return index >= other.index; }
	// assumes they have the same parent
	inline bool operator<(const TreeIterator& other) const { return index < other.index; }
	// assumes they have the same parent
	inline bool operator<=(const TreeIterator& other) const { return index <= other.index; }
	inline bool atEnd() const { return index == getEndIndex(); }
	inline bool atBegin() const { return index == 0; }
	inline unsigned int getIndex() const { return index; }
	inline unsigned int getEndIndex() const { if (atUl) return 0; return currentElement->GetParentNode()->GetNumChildren(); }
	inline bool sameParent(const TreeIterator& other) const {
		if (atUl || other.atUl) return currentElement == other.currentElement;
		return currentElement->GetParentNode() == other.currentElement->GetParentNode();
	}

	// parent/child
	inline std::optional<TreeIterator> getParent() const {
		Rml::Element* parent = getParentLiElement();
		if (parent) return TreeIterator(parent);
		return std::nullopt;
	}
	inline std::optional<TreeIterator> getChild(unsigned int indexOfChild) const {
		Rml::Element* childParent = getChildParentElement();
		if (!(childParent && childParent->HasChildNodes())) return std::nullopt;
		unsigned int count = childParent->GetNumChildren();
		if (count < indexOfChild) return std::nullopt;
		if (count == indexOfChild) {
			if (count == 0) return TreeIterator(childParent); // is the end iter because childParent is "ul"
			return TreeIterator(childParent->GetChild(count-1), count); // is the end iter because index = count of children
		}
		return TreeIterator(childParent->GetChild(indexOfChild), indexOfChild);
	}
	inline std::optional<TreeIterator> getBeginChild() const {
		Rml::Element* childParent = getChildParentElement();
		if (!childParent) return std::nullopt;
		if (!childParent->HasChildNodes()) return TreeIterator(childParent);
		return TreeIterator(childParent->GetChild(0), 0);
	}
	inline std::optional<TreeIterator> getEndChild() const {
		Rml::Element* childParent = getChildParentElement();
		if (!childParent) return std::nullopt;
		unsigned int count = childParent->GetNumChildren();
		if (count == 0) return TreeIterator(childParent);
		return TreeIterator(childParent->GetChild(count-1), count); // is the end iter because index = count of children
	}
	inline bool toParent() {
		Rml::Element* parent = getParentLiElement();
		if (parent) {
			currentElement = parent;
			updateIndex();
		}
		return parent != nullptr;
	}
	inline bool toChild() {
		Rml::Element* childParent = getChildParentElement();
		if (!(childParent && childParent->HasChildNodes())) return false;
		currentElement = childParent->GetChild(0);
		index = 0;
		return true;
	}
	inline bool toChild(unsigned int indexOfChild) {
		Rml::Element* childParent = getChildParentElement();
		if (!(childParent && childParent->HasChildNodes())) return false;
		unsigned int count = childParent->GetNumChildren();
		if (count < indexOfChild) return false;
		if (count == indexOfChild) {
			if (indexOfChild == 0) {
				currentElement = childParent;
				atUl = true;
				index = 0;
			} else {
				currentElement = childParent->GetChild(count-1);
				index = count;
			}
			return true;
		}
		currentElement = childParent->GetChild(indexOfChild);
		index = indexOfChild;
		return true;
	}
	inline bool atTop() const {
		return getParentLiElement() == nullptr;
	}
	inline bool atBottom() const {
		Rml::Element* childParent = getChildParentElement();
		if (childParent == nullptr) return true;
		return !(childParent->HasChildNodes());
	}

	// content
	Rml::Element* getTopContent() {
		if (atEnd()) return nullptr;
		return currentElement->GetFirstChild();
	}
	Rml::Element* getMiddleContent() {
		if (atEnd()) return nullptr;
		Rml::Element* bottomContainer = currentElement->GetLastChild();
		if (bottomContainer->GetNumChildren() == 3) return bottomContainer->GetChild(1);
		return bottomContainer->GetFirstChild();
	}

	// modifying
	Rml::Element* addList() {
		if (atEnd()) return nullptr;
		Rml::Element* bottomContainer = currentElement->GetLastChild();
		if (bottomContainer->GetNumChildren() == 3) return bottomContainer->GetLastChild();
		Rml::ElementPtr arrow = bottomContainer->GetOwnerDocument()->CreateElement("span");
		arrow->SetInnerRML(">");
		// arrow->AddEventListener("click", ); // TODO: add collapse listener
		bottomContainer->InsertBefore(std::move(arrow), bottomContainer->GetFirstChild());
		return bottomContainer->AppendChild(bottomContainer->GetOwnerDocument()->CreateElement("ul"));
	}
	void removeList() {
		if (atEnd()) return;
		Rml::Element* bottomContainer = currentElement->GetLastChild();
		if (bottomContainer->GetNumChildren() == 1) return;
		bottomContainer->RemoveChild(bottomContainer->GetFirstChild());
		bottomContainer->RemoveChild(bottomContainer->GetLastChild());
	}
	std::optional<TreeIterator> addChild() {
		Rml::Element* ul = addList();
		if (!ul) return std::nullopt;
		Rml::ElementDocument* document = ul->GetOwnerDocument();
		Rml::Element* li =  ul->AppendChild(document->CreateElement("li"));
		li->AppendChild(document->CreateElement("div"));
		Rml::Element* bottomContainer = li->AppendChild(document->CreateElement("div"));
		bottomContainer->AppendChild(document->CreateElement("div"));
		return TreeIterator(li, ul->GetNumChildren()-1);
	}
	std::optional<TreeIterator> addChild(unsigned int indexToPutChild) {
		Rml::Element* ul = addList();
		if (!ul) return std::nullopt;
		Rml::ElementDocument* document = ul->GetOwnerDocument();
		unsigned int count = ul->GetNumChildren();
		Rml::Element* li;
		if (count < indexToPutChild) return std::nullopt;
		if (count == indexToPutChild) {
			li = ul->AppendChild(document->CreateElement("li"));
		} else {
			li = ul->InsertBefore(document->CreateElement("li"), ul->GetChild(indexToPutChild));
		}
		li->AppendChild(document->CreateElement("div"));
		Rml::Element* bottomContainer = li->AppendChild(document->CreateElement("div"));
		bottomContainer->AppendChild(document->CreateElement("div"));
		return TreeIterator(li, indexToPutChild);
	}
	// this points at the new element
	void addAtIterator() {
		if (atUl) {
			Rml::ElementDocument* document = currentElement->GetOwnerDocument();
			currentElement = currentElement->AppendChild(document->CreateElement("li"));
			currentElement->AppendChild(document->CreateElement("div"));
			Rml::Element* bottomContainer = currentElement->AppendChild(document->CreateElement("div"));
			bottomContainer->AppendChild(document->CreateElement("div"));
			return;
		}
		Rml::Element* ul = currentElement->GetParentNode();
		Rml::ElementDocument* document = ul->GetOwnerDocument();
		if (index >= ul->GetNumChildren()) {
			currentElement = ul->AppendChild(document->CreateElement("li"));
		} else {
			currentElement = ul->InsertBefore(document->CreateElement("li"), ul->GetChild(index));
		}
		currentElement->AppendChild(document->CreateElement("div"));
		Rml::Element* bottomContainer = currentElement->AppendChild(document->CreateElement("div"));
		bottomContainer->AppendChild(document->CreateElement("div"));
	}
	// this points to the element after the removed element
	void removeAtIterator() {
		if (atEnd()) return;
		Rml::Element* ul = currentElement->GetParentNode();
		bool getLast = false;
		if (ul->GetLastChild() == currentElement) getLast = true;
		ul->RemoveChild(currentElement);
		if (getLast) {
			if (index == 0) {
				currentElement = ul;
				atUl = true;
			} else {
				currentElement = ul->GetLastChild();
			}
		}
		else currentElement = ul->GetChild(index);
	}

private:
	// should return a "ul"
	Rml::Element* getChildParentElement() const {
		if (atEnd()) return nullptr;
		Rml::Element* bottomContainer = currentElement->GetLastChild();
		if (bottomContainer->GetNumChildren() == 1) return nullptr;
		return bottomContainer->GetLastChild();
	}
	// should return a "li"
	Rml::Element* getParentLiElement() const {
		Rml::Element* li;
		if (!atUl)
			li = currentElement->GetParentNode()->GetParentNode();
		else
			li = currentElement->GetParentNode()->GetParentNode()->GetParentNode();
		if (li->GetTagName() == "li") return li;
		return nullptr;
	}
	void updateIndex() {
		if (currentElement->GetTagName() == "ul") {
			index = 0;
			atUl = true;
			return;
		}
		atUl = false;
		Rml::Element* parent = currentElement->GetParentNode();
		unsigned int count = parent->GetNumChildren();
		if (index >= count) { // is end iter
			index = count;
			return;
		}
		if (parent->GetChild(index) == currentElement) return;
		for (unsigned int i = 0; i < count; i++) {
			if (index == i) continue;
			if (parent->GetChild(i) == currentElement) {
				index = i;
				return;
			}
		}
		logError("Failed to find index of element {}. This should not happen.", "TreeIterator", currentElement);
	}
	void next() {
		if (atUl) return;
		Rml::Element* parent = currentElement->GetParentNode();
		unsigned int count = parent->GetNumChildren();
		if (index == count || ++index == count) return;
		currentElement = parent->GetChild(index);
	}
	void next(unsigned int amount) {
		if (atUl) return;
		if (amount == 0) return;
		Rml::Element* parent = currentElement->GetParentNode();
		unsigned int count = parent->GetNumChildren();
		if (index == count) return;
		index += amount;
		if (index >= count) {
			index = count;
			return;
		}
		currentElement = parent->GetChild(index);
	}
	void previous() {
		if (index == 0) return;
		--index;
		currentElement = currentElement->GetParentNode()->GetChild(index);
	}
	void previous(unsigned int amount) {
		if (index == 0 || amount == 0) return;
		if (index < amount) index = 0;
		else index -= amount;
		currentElement = currentElement->GetParentNode()->GetChild(index);
	}

	unsigned int index;
	Rml::Element* currentElement; // should be a "li" unless atUl is true and it should be a "ul"
	bool atUl = false;
};

#endif /* treeIterator_h */
