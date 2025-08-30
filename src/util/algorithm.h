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

#endif /* algorithm_h */
