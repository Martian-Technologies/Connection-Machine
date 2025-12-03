#ifndef indexableUnorderedSet_h
#define indexableUnorderedSet_h

template <class T>
class IndexableUnorderedSet {
public:
	IndexableUnorderedSet() = default;

	bool empty() const;
	size_t size() const;

	void clear();
	void insert(const T& value);
	void erase(const T& value);
	bool contains(const T& value) const;

	const T& at(size_t index) const;
	size_t indexOf(const T& value) const;

private:
	std::unordered_map<T, size_t> set;
	std::vector<T> vec;
};

template <class T>
bool IndexableUnorderedSet<T>::empty() const {
	return vec.empty();
}

template <class T>
size_t IndexableUnorderedSet<T>::size() const {
	return vec.size();
}

template <class T>
void IndexableUnorderedSet<T>::clear() {
	set.clear();
	vec.clear();
}

template <class T>
void IndexableUnorderedSet<T>::insert(const T& value) {
	if (set.find(value) == set.end()) {
		set[value] = vec.size();
		vec.push_back(value);
	}
}

template <class T>
void IndexableUnorderedSet<T>::erase(const T& value) {
	auto it = set.find(value);
	if (it != set.end()) {
		size_t index = it->second;
		T lastValue = vec.back();
		vec[index] = lastValue;
		set[lastValue] = index;
		vec.pop_back();
		set.erase(it);
	}
}

template <class T>
bool IndexableUnorderedSet<T>::contains(const T& value) const {
	return set.find(value) != set.end();
}

template <class T>
const T& IndexableUnorderedSet<T>::at(size_t index) const {
	return vec.at(index);
}

template <class T>
size_t IndexableUnorderedSet<T>::indexOf(const T& value) const {
	return set.at(value);
}

#endif /* indexableUnorderedSet_h */