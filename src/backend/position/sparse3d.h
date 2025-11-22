#ifndef sparse3d_h
#define sparse3d_h

#include <parallel_hashmap/phmap.h>

#include "position3d.h"

/*
Should be defined as:
template <class T>
class Sparse3d {
public:
	Sparse3d();
	Sparse3d(const Sparse3d& other);
	Sparse3d<T>& operator=(const Sparse3d<T>& other)
	T* get(Position3 position);
	const T* get(Position3 position) const;
	void insert(Position3 position, const T& value);
	void remove(Position3 position);
	void clear();
}
*/

template <class T>
class Sparse3dArray;

template <class T>
using Sparse3d = Sparse3dArray<T>;

template <class T>
class Sparse3dArray {
public:
	inline T* get(Position3 position);
	inline const T* get(Position3 position) const;
	inline unsigned int size() const { return data.size(); }

	inline void insert(Position3 position, const T& value);
	inline void remove(Position3 position);
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
	phmap::parallel_flat_hash_map<Position3, T> data;

	typedef typename phmap::parallel_flat_hash_map<Position3, T>::iterator iterator;
	typedef typename phmap::parallel_flat_hash_map<Position3, T>::const_iterator const_iterator;
};

template <class T>
T* Sparse3dArray<T>::get(Position3 position) {
	iterator iter = data.find(position);
	if (iter == data.end()) {
		return nullptr;
	} else {
		return &iter->second;
	}
}

template <class T>
const T* Sparse3dArray<T>::get(Position3 position) const {
	const_iterator iter = data.find(position);
	if (iter == data.end()) {
		return nullptr;
	} else {
		return &iter->second;
	}
}

template <class T>
void Sparse3dArray<T>::insert(Position3 position, const T& value) {
	iterator iter = data.find(position);
	if (iter == data.end()) {
		data.insert(std::make_pair(position, value));
	} else {
		iter->second = value;
	}
}

template <class T>
void Sparse3dArray<T>::remove(Position3 position) {
	iterator iter = data.find(position);
	if (iter != data.end())
		data.erase(iter);
}

template <class T>
void Sparse3dArray<T>::clear() {
	data.clear();
}

template <class T>
nlohmann::json Sparse3dArray<T>::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json stateJson;
	for (const auto& [pos, value] : data) {
		stateJson[pos.toString()] = value;
	}
	return stateJson;
}

template <class T>
nlohmann::json Sparse3dArray<T>::dumpStateAndInner() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json stateJson;
	for (const auto& [pos, value] : data) {
		stateJson[pos.toString()] = value.dumpState();
	}
	return stateJson;
}

#endif /* sparse3d_h */
