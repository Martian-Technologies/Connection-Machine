#ifndef sparse2d_h
#define sparse2d_h

#include <parallel_hashmap/phmap.h>

#include "position.h"

/*
Should be defined as:
template <class T>
class Sparse2d {
public:
	Sparse2d();
	Sparse2d(const Sparse2d& other);
	Sparse2d<T>& operator=(const Sparse2d<T>& other)
	T* get(Position position);
	const T* get(Position position) const;
	void insert(Position position, const T& value);
	void remove(Position position);
	void clear();
}
*/

template <class T>
class Sparse2dArray;

template <class T>
using Sparse2d = Sparse2dArray<T>;

template <class T>
class Sparse2dArray {
public:
	inline T* get(Position position);
	inline const T* get(Position position) const;
	inline unsigned int size() const { return data.size(); }

	inline void insert(Position position, const T& value);
	inline void remove(Position position);
	inline void clear();

	template <typename F>
	void forEach(F&& func) const {
		for (auto& [pos, value] : data) {
			func(pos, value);
		}
	}
	nlohmann::json dumpState() const;
	nlohmann::json dumpStateAndInner() const;

private:
	phmap::parallel_flat_hash_map<Position, T> data;

	typedef typename phmap::parallel_flat_hash_map<Position, T>::iterator iterator;
	typedef typename phmap::parallel_flat_hash_map<Position, T>::const_iterator const_iterator;
};

template <class T>
T* Sparse2dArray<T>::get(Position position) {
	iterator iter = data.find(position);
	if (iter == data.end()) {
		return nullptr;
	} else {
		return &iter->second;
	}
}

template <class T>
const T* Sparse2dArray<T>::get(Position position) const {
	const_iterator iter = data.find(position);
	if (iter == data.end()) {
		return nullptr;
	} else {
		return &iter->second;
	}
}

template <class T>
void Sparse2dArray<T>::insert(Position position, const T& value) {
	iterator iter = data.find(position);
	if (iter == data.end()) {
		data.insert(std::make_pair(position, value));
	} else {
		iter->second = value;
	}
}

template <class T>
void Sparse2dArray<T>::remove(Position position) {
	iterator iter = data.find(position);
	if (iter != data.end())
		data.erase(iter);
}

template <class T>
void Sparse2dArray<T>::clear() {
	data.clear();
}

template <class T>
nlohmann::json Sparse2dArray<T>::dumpState() const {
	nlohmann::json stateJson;
	for (const auto& [pos, value] : data) {
		stateJson[to_string(pos.x) + "," + to_string(pos.y)] = value;
	}
	return stateJson;
}

template <class T>
nlohmann::json Sparse2dArray<T>::dumpStateAndInner() const {
	nlohmann::json stateJson;
	for (const auto& [pos, value] : data) {
		stateJson[std::to_string(pos.x) + "," + std::to_string(pos.y)] = value.dumpState();
	}
	return stateJson;
}

#endif /* sparse2d_h */
