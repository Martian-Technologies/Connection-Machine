#ifndef treeIterator_h
#define treeIterator_h

#include <RmlUi/Core.h>

class TreeIterator {
public:
	// usings
	using iterator_category = std::random_access_iterator_tag;

	// constructors
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
	inline bool atEnd() const { return index == curentElement->GetParentNode()->GetNumChildren(); }
	inline bool atBegin() const { return index == 0; }
	inline unsigned int getIndex() const { return index; }
	inline unsigned int getEndIndex() const { return curentElement->GetParentNode()->GetNumChildren(); }
	inline bool sameParent(const TreeIterator& other) const { return curentElement->GetParentNode() == other.curentElement->GetParentNode(); }

	// parent/child
	inline std::optional<TreeIterator> getParent() const {
		Rml::Element* parent = getParentLiElement();
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
		Rml::Element* parent = getParentLiElement();
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
		return getParentLiElement() == nullptr;
	}
	inline bool atBottom() const {
		Rml::Element* childParent = getChildParentElement();
		if (childParent == nullptr) return false;
		return childParent->HasChildNodes();
	}

	// content
	Rml::Element* getTopContent() {
		if (atEnd()) return nullptr;
		return curentElement->GetFirstChild();
	}
	Rml::Element* getMiddleContent() {
		if (atEnd()) return nullptr;
		Rml::Element* bottomContainer = curentElement->GetLastChild();
		if (bottomContainer->GetNumChildren() == 3) return bottomContainer->GetChild(1);
		return bottomContainer->GetFirstChild();
	}

	// modifying
	Rml::Element* addList() {
		if (atEnd()) return nullptr;
		Rml::Element* bottomContainer = curentElement->GetLastChild();
		if (bottomContainer->GetNumChildren() == 3) return bottomContainer->GetLastChild();
		Rml::ElementPtr arrow = bottomContainer->GetOwnerDocument()->CreateElement("span");
		arrow->SetInnerRML(">");
		// arrow->AddEventListener("click", ); // TODO: add collapse listener
		bottomContainer->InsertBefore(std::move(arrow), bottomContainer->GetFirstChild());
		return bottomContainer->AppendChild(bottomContainer->GetOwnerDocument()->CreateElement("ul"));
	}
	void removeList() {
		if (atEnd()) return;
		Rml::Element* bottomContainer = curentElement->GetLastChild();
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
		return TreeIterator(li, ul->GetNumChildren(0));
	}
	std::optional<TreeIterator> addChild(unsigned int indexToPutChild) {
		Rml::Element* ul = addList();
		if (!ul) return std::nullopt;
		Rml::ElementDocument* document = ul->GetOwnerDocument();
		unsigned int count = ul->GetNumChildren();
		Rml::Element* li;
		if (count >= indexToPutChild) {
			indexToPutChild = count-1;
			li = ul->AppendChild(document->CreateElement("li"));
		} else {
			li = ul->InsertBefore(document->CreateElement("li"), ul->GetChild(indexToPutChild));
		}
		li->AppendChild(document->CreateElement("div"));
		Rml::Element* bottomContainer = li->AppendChild(document->CreateElement("div"));
		bottomContainer->AppendChild(document->CreateElement("div"));
		return TreeIterator(li, indexToPutChild);
	}

private:
	// should return a "ul"
	Rml::Element* getChildParentElement() const {
		if (atEnd()) return nullptr;
		Rml::Element* bottomContainer = curentElement->GetLastChild();
		if (bottomContainer->GetNumChildren() == 1) return nullptr;
		return bottomContainer->GetLastChild();
	}
	// should return a "li"
	Rml::Element* getParentLiElement() const {
		Rml::Element* li = curentElement->GetParentNode()->GetParentNode()->GetParentNode();
		if (li->GetTagName() == "li") return li;
		return nullptr;
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
