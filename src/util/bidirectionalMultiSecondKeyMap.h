#ifndef BidirectionalMultiSecondKeyMap_h
#define BidirectionalMultiSecondKeyMap_h

template <class T1, class T2>
class BidirectionalMultiSecondKeyMap {
public:
	typedef typename std::unordered_multimap<T2, T1>::const_iterator constIteratorT2;
	typedef typename std::pair<constIteratorT2, constIteratorT2> constIteratorPairT2;

	void set(const T1& v1, const T2& v2) {
		auto iter = T2Vals.find(v1);
		if (iter == T2Vals.end()) {
			T2Vals[v1] = v2;
		} else if (iter->second == v2) {
			return;
		} else {
			for (auto [iter2, iter2End] = T1Vals.equal_range(iter->second); iter2 != iter2End; ++iter2) {
				if (iter2->second == v1) {
					T1Vals.erase(iter2);
					break;
				}
			}
			iter->second = v2;
		}
		T1Vals.emplace(v2, v1);
	}

	void remove(const T1& key) {
		auto iter = T2Vals.find(key);
		if (iter == T2Vals.end()) return;
		for (auto [iter2, iter2End] = T1Vals.equal_range(iter->second); iter2 != iter2End; ++iter2) {
			if (iter2->second == key) {
				T1Vals.erase(iter2);
				break;
			}
		}
		T2Vals.erase(iter);
	}

	const T2* get(const T1& key) const {
		auto iter = T2Vals.find(key);
		if (iter == T2Vals.end()) return nullptr;
		return &(iter->second);
	}

	std::pair<constIteratorT2, constIteratorT2> get(const T2& key) const {
		return T1Vals.equal_range(key);
	}

	const std::unordered_map<T1, T2>& getT2Map() const {
		return T2Vals;
	}

private:
	std::unordered_map<T1, T2> T2Vals;
	std::unordered_multimap<T2, T1> T1Vals;

};

template<class T2>
class BidirectionalMultiSecondKeyMap<unsigned int, T2> {
public:
	typedef typename std::unordered_multimap<T2, unsigned int>::const_iterator constIteratorT2;
	typedef typename std::pair<constIteratorT2, constIteratorT2> constIteratorPairT2;

	void set(const unsigned int& v1, const T2& v2) {
		if (v1 >= T2Vals.size()) {
			T2Vals.resize(v1+1);
			T2Vals[v1] = v2;
			T1Vals.emplace(v2, v1);
			return;
		}
		auto& value = T2Vals[v1];
		if (!value) {
			T2Vals[v1] = v2;
		} else if (value.value() == v2) {
			return;
		} else {
			for (auto [iter, iterEnd] = T1Vals.equal_range(value.value()); iter != iterEnd; ++iter) {
				if (iter->second == v1) {
					T1Vals.erase(iter);
					break;
				}
			}
			T2Vals[v1] = v2;
		}
		T1Vals.emplace(v2, v1);
	}

	void remove(const unsigned int& key) {
		if (key >= T2Vals.size()) return;
		auto& value = T2Vals[key];
		if (!value) return;
		for (auto [iter2, iter2End] = T1Vals.equal_range(value.value()); iter2 != iter2End; ++iter2) {
			if (iter2->second == key) {
				T1Vals.erase(iter2);
				break;
			}
		}
		T2Vals[key] = std::nullopt;
	}

	const T2* get(const unsigned int& key) const {
		if (key >= T2Vals.size()) return nullptr;
		if (T2Vals[key]) return &(T2Vals[key].value());
		return nullptr;
	}

	std::pair<constIteratorT2, constIteratorT2> get(const T2& key) const {
		return T1Vals.equal_range(key);
	}

private:
	std::vector<std::optional<T2>> T2Vals;
	std::unordered_multimap<T2, unsigned int> T1Vals;
};

#endif /* BidirectionalMultiSecondKeyMap_h */
