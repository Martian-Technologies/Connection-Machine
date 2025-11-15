#ifndef algorithm_h
#define algorithm_h

#include <numeric>

template <class Iterator, class T>
inline bool contains(Iterator firstIter, Iterator lastIter, const T& value) {
	while (firstIter != lastIter) {
		if (*firstIter == value) return true;
		++firstIter;
	}
	return false;
}

template <typename T1, typename T2>
inline void sortVectorWithOther(std::vector<T1>& sortBy, std::vector<T2>& other) {
    assert(sortBy.size() == other.size());

    std::vector<int> idx(sortBy.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int i, int j) {
        return sortBy[i] > sortBy[j];
    });

    for (unsigned int i = 0; i < idx.size(); i++) {
        if (idx[i] < 0) continue;

        int j = i;
        while (idx[j] >= 0) {
            int k = idx[j];
            std::swap(sortBy[j], sortBy[k]);
            std::swap(other[j], other[k]);

            idx[j] = -1;
            j = k;
        }
    }
}

template <typename K, typename V>
inline K findUnusedKey(const std::map<K, V>& map, K minKey = 0) {
	for (const auto& pair : map) {
		if (minKey != pair.first) {
			return minKey;
		}
		++minKey;
	}
	return minKey;
}

inline std::vector<std::string> stringSplit(const std::string& s, const char delimiter) {
	size_t start = 0;
	size_t end = 0;

	std::vector<std::string> output;

	while (end != std::string::npos) {
		end = s.find_first_of(delimiter, start);
		output.emplace_back(s.substr(start, end - start));
		start = end + 1;
	}

	return output;
}

inline void stringSplitInto(const std::string& s, const char delimiter, std::vector<std::string>& vectorToFill) {
	size_t start = 0;
	size_t end = 0;
	while (end != std::string::npos) {
		end = s.find_first_of(delimiter, start);
		vectorToFill.emplace_back(s.substr(start, end - start));
		start = end + 1;
	}
}

template <typename T>
inline std::string to_string(const std::vector<T>& vec) {
	std::string result = "[";
	for (size_t i = 0; i < vec.size(); i++) {
		result += to_string(vec[i]);
		if (i < vec.size() - 1) {
			result += ", ";
		}
	}
	result += "]";
	return result;
}

template <typename... Ts>
inline std::string to_string(const std::variant<Ts...>& var) {
	return std::visit([](const auto& value) {
		return to_string(value);
	}, var);
}

template <typename T>
inline std::string to_string(const T& value) {
	return std::to_string(value);
}

template <typename T>
inline std::vector<std::optional<T>> to_optional_vector(const std::vector<T>& input) {
	std::vector<std::optional<T>> result;
	result.reserve(input.size());
	for (const auto& value : input) {
		result.emplace_back(value);
	}
	return result;
}

template <typename T>
struct std::hash<std::vector<T>> {
	inline std::size_t operator()(std::vector<T> const& vec) const noexcept {
		std::size_t seed = vec.size();
		std::hash<T> hasher;

		for (auto const& x : vec) {
			std::size_t h = hasher(x);
			h = ((h >> 16) ^ h) * 0x45d9f3b;
			h = ((h >> 16) ^ h) * 0x45d9f3b;
			h = (h >> 16) ^ h;

			seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
};

#endif /* algorithm_h */
