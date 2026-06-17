#ifndef id_h
#define id_h

#define DECLARE_ID_TYPE(TypeName, RepType)\
	struct TypeName##__TAG__;\
	using TypeName = Id<TypeName##__TAG__, RepType>
#define DECLARE_ID_ID_MAP_PAGE_SIZE(TypeName, Size)\
	template <> struct IdMapTraits<TypeName>{ static const size_t PageSize = Size; }

template <class Tag, class Rep>
class Id {
public:
	using tag = Tag;
	using rep = Rep;

	constexpr Id() = default;
	constexpr Id(Rep v) : value(v) { }

	constexpr Rep get() const noexcept { return value; }

	friend constexpr bool operator==(Id, Id) = default;
	friend constexpr auto operator<=>(const Id&, const Id&) = default;

	class iterator {
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = Id<Tag, Rep>;
		using difference_type = std::ptrdiff_t;
		using reference = Id<Tag, Rep>;
		using pointer = void;

		iterator() = default;
		explicit iterator(Id<Tag, Rep> id) : x(id.get()) { }

		reference operator*() const { return Id<Tag, Rep>(x); }
		iterator& operator++() {
			++x;
			return *this;
		}
		iterator operator++(int) {
			auto t = *this;
			++(*this);
			return t;
		}
		iterator& operator--() {
			--x;
			return *this;
		}
		iterator operator--(int) {
			auto t = *this;
			--(*this);
			return t;
		}

		iterator& operator+=(difference_type n) {
			x += n;
			return *this;
		}
		iterator& operator-=(difference_type n) {
			x -= n;
			return *this;
		}
		iterator operator+(difference_type n) const { return iterator(this->x + n); }
		iterator operator-(difference_type n) const { return iterator(this->x - n); }

		difference_type operator-(const iterator& o) const { return static_cast<difference_type>(x - o.x); }

		bool operator==(const iterator& o) const = default;
		auto operator<=>(const iterator& o) const = default;

	private:
		Rep x;
	};
private:
	Rep value{};
};

template <class Tag, class Rep>
class IdRange {
public:
	using iterator = typename Id<Tag, Rep>::iterator;
	IdRange(Id<Tag, Rep> beginId, Id<Tag, Rep> endId) : b(iterator(beginId)), e(iterator(endId)) { }
	iterator begin() const { return b; }
	iterator end() const { return e; }
private:
	iterator b, e;
};

template <class Tag, class Rep>
IdRange<Tag, Rep> range(Id<Tag, Rep> beginId, Id<Tag, Rep> endId) {
	return IdRange<Tag, Rep>(beginId, endId);
}

namespace std {
template <class Tag, class Rep>
struct hash<Id<Tag, Rep>> {
	size_t operator()(const Id<Tag, Rep>& id) const noexcept(noexcept(std::hash<Rep>{}(id.get()))) { return std::hash<Rep>{}(id.get()); }
};
template <class Tag, class Rep>
string to_string(const Id<Tag, Rep>& id) {
	return std::to_string(id.get());
}
}; // namespace std

namespace fmt {
template <class Tag, class Rep>
struct formatter<Id<Tag, Rep>> : formatter<Rep> {
	template <typename FormatContext>
	auto format(const Id<Tag, Rep>& id, FormatContext& ctx) const /* GCOVR_EXCL_FUNCTION */ {
		return formatter<Rep>::format(id.get(), ctx);
	}
};
} // namespace fmt

template <class>
struct is_id : std::false_type { };

template <class Tag, class Rep>
struct is_id<Id<Tag, Rep>> : std::true_type { };

template <class T>
inline constexpr bool is_id_v = is_id<std::remove_cvref_t<T>>::value;

template <class>
struct id_traits;

template <class Tag, class Rep>
struct id_traits<Id<Tag, Rep>> {
	using tag = Tag;
	using rep = Rep;
};

template <class T>
concept IdType = is_id_v<T>;

template <IdType IdT>
inline std::string to_string(const IdT& id) {
	return std::to_string(id.get());
}

template <IdType IdT>
class IdProvider {
public:
	using id_type = std::remove_cv_t<IdT>;
	using rep = typename id_traits<id_type>::rep;

	constexpr IdProvider(rep initialValue) : nextId(initialValue), initialValue(initialValue) { }

	constexpr id_type getNewId() {
		if (unusedIds.empty()) {
			return id_type(nextId++);
		} else {
			rep id = *unusedIds.begin();
			unusedIds.erase(unusedIds.begin());
			return id_type(id);
		}
	}
	constexpr id_type getNewId(id_type preferredId) {
		if (unusedIds.contains(preferredId.get())) {
			unusedIds.erase(preferredId.get());
			return preferredId;
		}
		if (preferredId.get() == nextId) return id_type(nextId++);
		return getNewId();
	}
	constexpr void releaseId(id_type id) {
		if (!isIdUsed(id)) {
			return;
		}
		unusedIds.insert(id.get());
	}
	constexpr bool isIdUsed(id_type id) const { return id.get() < nextId && id.get() >= initialValue && !unusedIds.contains(id.get()); }
	constexpr id_type peekNext() const { return id_type(nextId); }
	void reset() {
		nextId = initialValue;
		unusedIds.clear();
	}
	std::vector<id_type> getUsedIds() const {
		std::vector<id_type> usedIds;
		for (rep id = initialValue; id < nextId; ++id) {
			if (!unusedIds.contains(id)) {
				usedIds.push_back(id_type(id));
			}
		}
		return usedIds;
	}
	nlohmann::json dumpState() const /* GCOVR_EXCL_FUNCTION */ {
		nlohmann::json stateJson;
		stateJson["nextId"] = nextId;
		stateJson["initialValue"] = initialValue;
		stateJson["unusedIds"] = nlohmann::json::array();
		for (const rep id : unusedIds) {
			stateJson["unusedIds"].push_back(id);
		}
		return stateJson;
	}
private:
	rep nextId;
	rep initialValue;
	std::set<rep> unusedIds;
};

template <IdType IdT>
class LinearIdProvider {
public:
	using id_type = std::remove_cv_t<IdT>;
	using rep = typename id_traits<id_type>::rep;

	constexpr LinearIdProvider(rep initialValue) : initialValue(initialValue), nextId(initialValue) { }

	constexpr id_type getNewId() { return id_type(nextId++); }
	constexpr id_type peekNext() const { return id_type(nextId); }
	constexpr id_type lastIdProvided() const { return id_type(nextId - 1); }
	void reset() { nextId = initialValue; }

private:
	rep initialValue;
	rep nextId;
};

template <IdType IdT, class T>
class IdVector {
public:
	using id_type = std::remove_cv_t<IdT>;
	using tag = typename id_traits<id_type>::tag;
	using rep = typename id_traits<id_type>::rep;
	using value_type = T;

	static_assert(std::is_integral_v<rep>, "Id::rep must be an integral type");

	constexpr IdVector() = default;
	explicit IdVector(id_type n) : storage(rep(n)) { }
	IdVector(id_type n, const T& value) : storage(rep(n), value) { }

	inline id_type size() const noexcept { return id_type{ rep(storage.size()) }; }
	inline bool empty() const noexcept { return storage.empty(); }

	inline id_type capacity() const noexcept { return id_type{ rep(storage.capacity()) }; }

	inline void reserve(id_type n) { storage.reserve(n.get()); }
	inline void resize(id_type n) { storage.resize(n.get()); }
	inline void resize(id_type n, const T& value) { storage.resize(n.get(), value); }
	inline void resizeWithOffset(id_type n, int offset) { storage.resize(n.get() + offset); }
	inline void resizeWithOffset(id_type n, int offset, const T& value) { storage.resize(n.get() + offset, value); }

	inline void smartResize(id_type n) { // exponential growth strategy
		if (n.get() > storage.size()) {
			size_t newSize = storage.size() == 0 ? 1 : storage.size();
			while (newSize < n.get()) {
				newSize *= 2;
			}
			storage.resize(newSize);
		}
	}
	inline void smartResize(id_type n, T& value) {
		if (n.get() > storage.size()) {
			size_t oldSize = storage.size();
			size_t newSize = oldSize == 0 ? 1 : oldSize;
			while (newSize < n.get()) {
				newSize *= 2;
			}
			storage.resize(newSize, value);
		}
	}
	inline void smartResizeWithOffset(id_type n, int offset) {
		if (n.get() + offset > storage.size()) {
			size_t newSize = storage.size() == 0 ? 1 : storage.size();
			while (newSize < n.get() + offset) {
				newSize *= 2;
			}
			storage.resize(newSize);
		}
	}
	inline void smartResizeWithOffset(id_type n, int offset, const T& value) {
		if (n.get() + offset > storage.size()) {
			size_t oldSize = storage.size();
			size_t newSize = oldSize == 0 ? 1 : oldSize;
			while (newSize < n.get() + offset) {
				newSize *= 2;
			}
			storage.resize(newSize, value);
		}
	}

	inline void clear() noexcept { storage.clear(); }

	inline T& operator[](id_type id) { return storage[id.get()]; }
	inline const T& operator[](id_type id) const { return storage[id.get()]; }

	inline T& at(id_type id) { return storage.at(id.get()); }
	inline const T& at(id_type id) const { return storage.at(id.get()); }

	inline id_type push_back(const T& v) {
		storage.push_back(v);
		return last_id_();
	}
	inline id_type push_back(T&& v) {
		storage.push_back(std::move(v));
		return last_id_();
	}
	template <class... Args>
	inline id_type emplace_back(Args&&... args) {
		storage.emplace_back(std::forward<Args>(args)...);
		return last_id_();
	}

	inline id_type begin_id() const noexcept { return id_type(0); }
	inline id_type end_id() const noexcept { return size(); }

	inline bool contains(id_type id) const noexcept { return (id.get() < storage.size()); }

	inline IdRange<tag, rep> ids() const { return range(begin_id(), end_id()); }

	inline auto begin() noexcept { return storage.begin(); }
	inline auto end() noexcept { return storage.end(); }
	inline auto begin() const noexcept { return storage.begin(); }
	inline auto end() const noexcept { return storage.end(); }
	inline const std::vector<T>& raw() const noexcept { return storage; }
	inline std::vector<T>& raw() noexcept { return storage; }

private:
	inline id_type last_id_() const {
		auto n = storage.size();
		return id_type{ rep(n - 1) };
	}

	std::vector<T> storage;
};

template <IdType IdT>
struct IdMapTraits {
    static const size_t PageSize = 1024;
};

#define ID_MAP
#ifdef ID_MAP
template <IdType IdT, class T>
class IdMap {
public:
	static const unsigned int PageSize = IdMapTraits<IdT>::PageSize;
	using id_type = std::remove_cv_t<IdT>;
	using tag = typename id_traits<id_type>::tag;
	using rep = typename id_traits<id_type>::rep;
	using value_type = std::pair<id_type, T>;

private:
	struct PageData {
		PageData() = default;
		~PageData() {
			if (size != 0) {
				for (unsigned short index = 0; index < PageSize; ++index) {
					if (mask[index]) std::launder(reinterpret_cast<T*>(&data[index]))->~T();
				}
			}
		}
		PageData(PageData&& other) : size(other.size), data(std::move(other.data)), mask(std::move(other.mask)) { other.size = 0; }
		PageData& operator=(PageData&& other) {
			if (this == &other) return *this;
			if (size != 0) {
				for (unsigned short index = 0; index < PageSize; ++index) {
					if (mask[index]) std::launder(reinterpret_cast<T*>(&data[index]))->~T();
				}
			}
			size = other.size;
			data = std::move(other.data);
			mask = std::move(other.mask);
			other.size = 0;
		}
		template<class... Args>
		void emplace(unsigned short index, Args&&... args) {
			::new(&data[index]) T(std::forward<Args>(args)...);
			mask[index] = true;
			++size;
		}
		template<class... Args>
		void assign(unsigned short index, Args&&... args) {
			assert(mask[index]);
			std::launder(reinterpret_cast<T*>(&data[index]))->~T();
			::new(&data[index]) T(std::forward<Args>(args)...);
		}
		void destroy(unsigned short index) {
			assert(mask[index]);
			std::launder(reinterpret_cast<T*>(&data[index]))->~T();
			mask[index] = false;
			--size;
			assert(size != 0 && "You should just clear this page.");
		}
		T& get(unsigned short index) { return *std::launder(reinterpret_cast<T*>(&data[index])); }
		const T& get(unsigned short index) const { return *std::launder(reinterpret_cast<const T*>(&data[index])); }
		bool check(unsigned short index) const { return mask[index]; }
		void init() {
			assert(size == 0); // size should not stay 0
			data = std::vector<std::aligned_storage_t<sizeof(T), alignof(T)>>(PageSize);
			mask = std::vector<bool>(PageSize, false);
		}
		void clear() {
			assert(size != 0);
			for (unsigned short index = 0; index < PageSize; ++index) {
				if (mask[index]) std::launder(reinterpret_cast<T*>(&data[index]))->~T();
			}
			size = 0;
			data.clear();
			mask.clear();
		}
		size_t size = 0;
		std::vector<std::aligned_storage_t<sizeof(T), alignof(T)>> data;
		std::vector<bool> mask;
	};

public:
	struct reference {
		const id_type first;
		T& second;
	};

	struct const_reference {
		const id_type first;
		const T& second;
	};

	struct pointer {
		reference ref;
		reference* operator->() { return &ref; }
	};

	struct const_pointer {
		const_reference ref;
		const_reference* operator->() { return &ref; }
	};

	static_assert(std::is_integral_v<rep>, "Id::rep must be an integral type");

	class iterator {
		friend class IdMap<IdT, T>;
	public:
		iterator& operator++() {
			if (index == std::numeric_limits<rep>::max()) return *this;
			// search rest of page
			rep pageIndex = index / PageSize;
			const PageData& page = idMap->storage[pageIndex];
			assert(page.size != 0);
			for (unsigned short relIndex = index % PageSize + 1; relIndex < PageSize; ++relIndex) {
				if (page.check(relIndex)) {
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
			}
			++pageIndex;
			while (pageIndex < idMap->storage.size()) {
				const PageData& page = idMap->storage[pageIndex];
				if (page.size != 0) {
					// find first valid
					unsigned short relIndex = 0;
					while (!page.check(relIndex)) {
						++relIndex;
						assert(relIndex < PageSize);
					}
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
				++pageIndex;
			}
			// at end
			index = std::numeric_limits<rep>::max();
			return *this;
		}
		iterator operator++(int) {
			iterator tmp = *this;
			this->operator++();
			return tmp;
		}
		reference operator*() const {
			return reference{ index, idMap->storage[index / PageSize].get(index % PageSize) };
		}
		pointer operator->() const { return pointer{ operator*() }; }
		bool operator==(const iterator other) const {
			assert(this->idMap == other.idMap);
			return this->index == other.index;
		}
	private:
		iterator(IdMap<IdT, T>& idMap, rep index) : idMap(&idMap), index(index) {}
		IdMap<IdT, T>* idMap;
		rep index;
	};
	class const_iterator {
		friend class IdMap<IdT, T>;
	public:
		const_iterator& operator++() {
			if (index == std::numeric_limits<rep>::max()) return *this;
			// search rest of page
			rep pageIndex = index / PageSize;
			const PageData& page = idMap->storage[pageIndex];
			assert(page.size != 0);
			for (unsigned short relIndex = index % PageSize + 1; relIndex < PageSize; ++relIndex) {
				if (page.check(relIndex)) {
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
			}
			++pageIndex;
			while (pageIndex < idMap->storage.size()) {
				const PageData& page = idMap->storage[pageIndex];
				if (page.size != 0) {
					// find first valid
					unsigned short relIndex = 0;
					while (!page.check(relIndex)) {
						++relIndex;
						assert(relIndex < PageSize);
					}
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
				++pageIndex;
			}
			// at end
			index = std::numeric_limits<rep>::max();
			return *this;
		}
		const_iterator operator++(int) {
			const_iterator tmp = *this;
			this->operator++();
			return tmp;
		}
		const_reference operator*() const {
			const PageData& page = idMap->storage[index / PageSize];
			return const_reference{ index, idMap->storage[index / PageSize].get(index % PageSize) };
		}
		const_pointer operator->() const { return const_pointer{ operator*() }; }
		bool operator==(const const_iterator other) const {
			assert(this->idMap == other.idMap);
			return this->index == other.index;
		}
	private:
		const_iterator(const IdMap<IdT, T>& idMap, rep index) : idMap(&idMap), index(index) {}
		const IdMap<IdT, T>* idMap;
		rep index;
	};

	constexpr IdMap() = default;

	inline size_t size() const noexcept { return cur_size; }
	inline bool empty() const noexcept { return cur_size == 0; }

	inline void clear() noexcept { cur_size = 0; storage.clear(); }

	inline T& operator[](id_type id) { return emplaceWithKey(id).first->second; }

	inline T& at(id_type id) { return find(id)->second; }
	inline const T& at(id_type id) const { return find(id)->second; }

	inline bool contains(id_type id) const noexcept { return find(id) != end(); }

	template <class... Args>
	std::pair<iterator, bool> try_emplace(id_type id, Args&&... args) { return emplaceWithKey(id, std::forward<Args>(args)...); }
	template <class... Args>
	std::pair<iterator, bool> insert_or_assign(id_type id, Args&&... args) { return emplaceOrAssignWithKey(id, std::forward<Args>(args)...); }

	// assume iter valid
	// if anyone need the iter they can go get it them selves
	void erase(iterator pos) {
		cur_size--;
		rep index = pos.index;
		PageData& page = storage[index / PageSize];
		if (page.size == 1) {
			if (storage.size() - 1 == index / PageSize) {
				storage.resize(storage.size() - 1);
				return;
			}
			page.clear();
			return;
		}
		assert(page.check(index % PageSize));
		page.destroy(index % PageSize);
	}
	size_t erase(IdT id) {
		// check if exists
		rep index = id.get();
		rep pageIndex = index / PageSize;
		if (pageIndex >= storage.size()) return 0;
		PageData& page = storage[pageIndex];
		unsigned short relIndex = index % PageSize;
		if (page.size == 0 || !page.check(relIndex)) return 0;
		// remove
		cur_size--;
		if (page.size == 1) {
			if (storage.size() - 1 == pageIndex) {
				storage.resize(storage.size() - 1);
				return 1;
			}
			page.clear();
			return 1;
		}
		page.destroy(relIndex);
		return 1;
	}

	inline iterator find(id_type id) {
		rep index = id.get();
		if (storage.size() <= index / PageSize) return end();
		const PageData& page = storage[index / PageSize];
		if (page.size != 0 && page.check(index % PageSize)) return iterator(*this, index);
		return end();
	}
	inline const_iterator find(id_type id) const {
		rep index = id.get();
		if (storage.size() <= index / PageSize) return end();
		const PageData& page = storage[index / PageSize];
		if (page.size != 0 && page.check(index % PageSize)) return const_iterator(*this, index);
		return end();
	}

	inline iterator begin() noexcept {
		rep pageIndex = 0;
		while (pageIndex < storage.size()) {
			const PageData& page = storage[pageIndex];
			if (page.size != 0) {
				// find first valid
				unsigned short relIndex = 0;
				while (!page.check(relIndex)) {
					++relIndex;
					assert(relIndex < PageSize);
				}
				return iterator(*this, pageIndex * PageSize + relIndex);
			}
			++pageIndex;
		}
		return end();
	}
	inline iterator end() noexcept { return iterator(*this, std::numeric_limits<rep>::max()); }
	inline const_iterator begin() const noexcept {
		rep pageIndex = 0;
		while (pageIndex < storage.size()) {
			const PageData& page = storage[pageIndex];
			if (page.size != 0) {
				// find first valid
				unsigned short relIndex = 0;
				while (!page.check(relIndex)) {
					++relIndex;
					assert(relIndex < PageSize);
				}
				return const_iterator(*this, pageIndex * PageSize + relIndex);
			}
			++pageIndex;
		}
		return end();
	}
	inline const_iterator end() const noexcept { return const_iterator(*this, std::numeric_limits<rep>::max()); }

private:
	template <class... Args>
	std::pair<iterator, bool> emplaceWithKey(id_type id, Args&&... args) {
		rep index = id.get();
		rep pageIndex = index / PageSize;
		unsigned short relIndex = index % PageSize;
		if (storage.size() <= pageIndex) {
			storage.resize(pageIndex + 1);
			PageData& page = storage[index / PageSize];
			page.init();
			page.emplace(relIndex, std::forward<Args>(args)...);
			cur_size++;
			return { iterator(*this, index), true };
		}
		PageData& page = storage[index / PageSize];
		if (page.size == 0) {
			page.init();
			page.emplace(relIndex, std::forward<Args>(args)...);
			cur_size++;
			return { iterator(*this, index), true };
		}
		if (page.check(relIndex)) {
			return { iterator(*this, index), false };
		}
		page.emplace(relIndex, std::forward<Args>(args)...);
		cur_size++;
		return { iterator(*this, index), true };
	}

	template <class... Args>
	std::pair<iterator, bool> emplaceOrAssignWithKey(id_type id, Args&&... args) {
		rep index = id.get();
		rep pageIndex = index / PageSize;
		unsigned short relIndex = index % PageSize;
		if (storage.size() <= pageIndex) {
			storage.resize(pageIndex + 1);
			PageData& page = storage[index / PageSize];
			page.init();
			page.emplace(relIndex, std::forward<Args>(args)...);
			cur_size++;
			return { iterator(*this, index), true };
		}
		PageData& page = storage[index / PageSize];
		if (page.size == 0) {
			page.init();
			page.emplace(relIndex, std::forward<Args>(args)...);
			cur_size++;
			return { iterator(*this, index), true };
		}
		if (page.check(relIndex)) {
			page.assign(relIndex, std::forward<Args>(args)...);
			return { iterator(*this, index), false };
		}
		page.emplace(relIndex, std::forward<Args>(args)...);
		cur_size++;
		return { iterator(*this, index), true };
	}

	size_t cur_size = 0;
	std::vector<PageData> storage;
};
#else
#define IdMap std::unordered_map
#endif

#define ID_MULTI_MAP
#ifdef ID_MULTI_MAP
template <IdType IdT, class T>
class IdMultiMap {
public:
	static const unsigned int PageSize = IdMapTraits<IdT>::PageSize;
	using id_type = std::remove_cv_t<IdT>;
	using tag = typename id_traits<id_type>::tag;
	using rep = typename id_traits<id_type>::rep;
	using value_type = std::pair<id_type, T>;

private:
	struct PageData {
		template<class... Args>
		void emplace(unsigned short index, Args&&... args) {
			data[index].emplace_back(std::forward<Args>(args)...);
			++size;
		}
		void destroy(unsigned short index, size_t vecIndex) {
			if (!data[index].empty()) data[index][vecIndex] = std::move(data[index].back());
			data[index].pop_back();
			--size;
			assert(size != 0 && "You should just clear this page.");
		}
		// this also destroys at vecIndex
		size_t destroyRest(unsigned short index, size_t vecIndex) {
			size_t removed = data[index].size() - vecIndex;
			size -= removed;
			data[index].resize(vecIndex);
			assert(size != 0 && "You should just clear this page.");
			return removed;
		}
		size_t destroyBefore(unsigned short index, size_t vecIndex) {
			if (vecIndex == 0) return 0;
			std::move(data[index].begin() + vecIndex, data[index].end(), data[index].begin());
			size -= vecIndex;
			data[index].resize(data[index].size() - vecIndex);
			assert(size != 0);
			return vecIndex;
		}
		size_t destroyRange(unsigned short index, size_t vecStart, size_t vecEnd) {
			if (vecStart == vecEnd) return 0;
			std::move(data[index].begin() + vecEnd, data[index].end(), data[index].begin() + vecStart);
			size -= vecEnd - vecStart;
			data[index].resize(data[index].size() + vecStart - vecEnd);
			assert(size != 0);
			return vecEnd - vecStart;
		}
		size_t destroyAll(unsigned short index) {
			size_t removed = data[index].size();
			size -= removed;
			data[index].clear();
			return removed;
		}
		std::vector<T>& get(unsigned short index) { return data[index]; }
		const std::vector<T>& get(unsigned short index) const { return data[index]; }
		bool check(unsigned short index) const { return data[index].size() != 0; }
		void init() {
			assert(size == 0); // size should not stay 0
			data = std::vector<std::vector<T>>(PageSize);
		}
		void clear() {
			assert(size != 0);
			size = 0;
			data.clear();
		}
		size_t size = 0;
		std::vector<std::vector<T>> data;
	};

public:
	struct reference {
		const id_type first;
		T& second;
	};

	struct const_reference {
		const id_type first;
		const T& second;
	};

	struct pointer {
		reference ref;
		reference* operator->() { return &ref; }
	};

	struct const_pointer {
		const_reference ref;
		const_reference* operator->() { return &ref; }
	};

	static_assert(std::is_integral_v<rep>, "Id::rep must be an integral type");

	class iterator {
		friend class IdMultiMap<IdT, T>;
	public:
		iterator& operator++() {
			if (index == std::numeric_limits<rep>::max()) return *this;
			rep pageIndex = index / PageSize;
			const PageData& page = idMultiMap->storage[pageIndex];
			// search rest of vec
			unsigned short relIndex = index % PageSize;
			const std::vector<T>& vec = page.get(relIndex);
			if (vec.size() != vecIndex + 1) {
				++vecIndex;
				return *this;
			}
			vecIndex = 0;
			// search rest of page
			assert(page.size != 0);
			for (++relIndex; relIndex < PageSize; ++relIndex) {
				if (page.check(relIndex)) {
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
			}
			++pageIndex;
			while (pageIndex < idMultiMap->storage.size()) {
				const PageData& page = idMultiMap->storage[pageIndex];
				if (page.size != 0) {
					// find first valid
					unsigned short relIndex = 0;
					while (!page.check(relIndex)) {
						++relIndex;
						assert(relIndex < PageSize);
					}
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
				++pageIndex;
			}
			// at end
			index = std::numeric_limits<rep>::max();
			return *this;
		}
		iterator operator++(int) {
			iterator tmp = *this;
			this->operator++();
			return tmp;
		}
		reference operator*() const {
			return reference{ index, idMultiMap->storage[index / PageSize].get(index % PageSize)[vecIndex] };
		}
		pointer operator->() const { return pointer{ operator*() }; }
		bool operator==(const iterator other) const {
			assert(this->idMultiMap == other.idMultiMap);
			return this->index == other.index && this->vecIndex == other.vecIndex;
		}
	private:
		iterator(IdMultiMap<IdT, T>& idMultiMap, rep index, size_t vecIndex) : idMultiMap(&idMultiMap), index(index), vecIndex(vecIndex) {}
		IdMultiMap<IdT, T>* idMultiMap;
		rep index;
		size_t vecIndex;
	};
	class const_iterator {
		friend class IdMultiMap<IdT, T>;
	public:
		const_iterator& operator++() {
			if (index == std::numeric_limits<rep>::max()) return *this;
			rep pageIndex = index / PageSize;
			const PageData& page = idMultiMap->storage[pageIndex];
			// search rest of vec
			unsigned short relIndex = index % PageSize;
			const std::vector<T>& vec = page.get(relIndex);
			if (vec.size() != vecIndex + 1) {
				++vecIndex;
				return *this;
			}
			vecIndex = 0;
			// search rest of page
			assert(page.size != 0);
			for (++relIndex; relIndex < PageSize; ++relIndex) {
				if (page.check(relIndex)) {
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
			}
			++pageIndex;
			while (pageIndex < idMultiMap->storage.size()) {
				const PageData& page = idMultiMap->storage[pageIndex];
				if (page.size != 0) {
					// find first valid
					unsigned short relIndex = 0;
					while (!page.check(relIndex)) {
						++relIndex;
						assert(relIndex < PageSize);
					}
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
				++pageIndex;
			}
			// at end
			index = std::numeric_limits<rep>::max();
			return *this;
		}
		const_iterator operator++(int) {
			const_iterator tmp = *this;
			this->operator++();
			return tmp;
		}
		const_reference operator*() const {
			const PageData& page = idMultiMap->storage[index / PageSize];
			return const_reference{ index, idMultiMap->storage[index / PageSize].get(index % PageSize)[vecIndex] };
		}
		const_pointer operator->() const { return const_pointer{ operator*() }; }
		bool operator==(const const_iterator other) const {
			assert(this->idMultiMap == other.idMultiMap);
			return this->index == other.index && this->vecIndex == other.vecIndex;
		}
	private:
		const_iterator(const IdMultiMap<IdT, T>& idMultiMap, rep index, size_t vecIndex) : idMultiMap(&idMultiMap), index(index), vecIndex(vecIndex) {}
		const IdMultiMap<IdT, T>* idMultiMap;
		rep index;
		size_t vecIndex;
	};

	constexpr IdMultiMap() = default;

	inline size_t size() const noexcept { return cur_size; }
	inline bool empty() const noexcept { return cur_size == 0; }

	inline void clear() noexcept { cur_size = 0; storage.clear(); }

	// inline bool contains(id_type id) const noexcept { return find(id) != end(); }

	template <class... Args>
	iterator emplace(id_type id, Args&&... args) { return emplaceWithKey(id, std::forward<Args>(args)...); }
	template <class... Args>
	iterator emplace(iterator pos, Args&&... args) { return emplaceWithKey(pos.index, std::forward<Args>(args)...); }

	// assume iter valid
	// if anyone need the iter they can go get it them selves
	void erase(iterator pos) {
		cur_size--;
		rep index = pos.index;
		PageData& page = storage[index / PageSize];
		if (page.size == 1) {
			if (storage.size() - 1 == index / PageSize) {
				storage.resize(storage.size() - 1);
				return;
			}
			page.clear();
			return;
		}
		assert(page.check(index % PageSize));
		page.destroy(index % PageSize, pos.vecIndex);
	}
	void erase(iterator start, iterator end) {
		if (start == end) return;
		PageData& firstPage = storage[start.index / PageSize];
		if (start.index == end.index) { // if only sub block
			cur_size -= firstPage.destroyRange(start.index, start.vecIndex, end.vecIndex);
			return;
		}
		if (start.vecIndex == 0 && firstPage.size == firstPage.get(start.index % PageSize).size()) {
			if (end == this->end()) {
				for (unsigned int pageIndex = start.index / PageSize; pageIndex < storage.size(); ++pageIndex) {
					cur_size -= storage[pageIndex].size;
				}
				storage.resize(start.index / PageSize);
				return;
			}
			cur_size -= firstPage.size;
			firstPage.clear();
		} else {
			unsigned short relIndex = start.index % PageSize;
			cur_size -= firstPage.destroyRest(relIndex, start.vecIndex);
			if (end.index / PageSize == start.index / PageSize) {
				unsigned short endRelIndex = end.index % PageSize;
				for (++relIndex; relIndex < endRelIndex; ++relIndex) {
					cur_size -= firstPage.destroyAll(relIndex);
				}
				cur_size -= firstPage.destroyBefore(endRelIndex, end.vecIndex);
				return;
			}
			for (++relIndex; relIndex < PageSize ; ++relIndex) {
				cur_size -= firstPage.destroyAll(relIndex);
				if (firstPage.size == 0) {
					if (end == this->end()) {
						for (unsigned int pageIndex = start.index / PageSize; pageIndex < storage.size(); ++pageIndex) {
							cur_size -= storage[pageIndex].size;
						}
						storage.resize(start.index / PageSize);
						return;
					}
					firstPage.clear();
					break;
				}
			}
		}
		if (end == this->end()) {
			for (unsigned int pageIndex = start.index / PageSize + 1; pageIndex < storage.size(); ++pageIndex) {
				cur_size -= storage[pageIndex].size;
			}
			storage.resize(start.index / PageSize + 1);
			return;
		}
		for (rep pageIndex = start.index / PageSize + 1; pageIndex < end.index / PageSize; ++pageIndex) {
			PageData& page = storage[pageIndex];
			if (page.size != 0) {
				cur_size -= page.size;
				page.clear();
			}
		}
		PageData& endPage = storage[end.index / PageSize];
		unsigned short relIndexEnd = end.index % PageSize;
		cur_size -= endPage.destroyBefore(relIndexEnd, end.vecIndex);
		for (unsigned short relIndex = 0; relIndex < relIndexEnd ; ++relIndex) {
			cur_size -= endPage.destroyAll(relIndex);
			if (endPage.size == 0) {
				endPage.clear();
				break;
			}
		}
	}
	size_t erase(IdT id) {
		// check if exists
		rep index = id.get();
		rep pageIndex = index / PageSize;
		if (pageIndex >= storage.size()) return 0;
		PageData& page = storage[pageIndex];
		unsigned short relIndex = index % PageSize;
		if (page.size == 0 || !page.check(relIndex)) return 0;
		// remove
		size_t vecSize = page.get(relIndex).size();
		cur_size -= vecSize;
		if (page.size == vecSize) {
			if (storage.size() - 1 == pageIndex) {
				storage.resize(storage.size() - 1);
				return vecSize;
			}
			page.clear();
			return vecSize;
		}
		page.destroyAll(relIndex);
		return vecSize;
	}

	inline iterator find(id_type id) {
		rep index = id.get();
		if (storage.size() <= index / PageSize) return end();
		const PageData& page = storage[index / PageSize];
		if (page.size != 0 && page.check(index % PageSize)) return iterator(*this, index, 0);
		return end();
	}
	inline const_iterator find(id_type id) const {
		rep index = id.get();
		if (storage.size() <= index / PageSize) return end();
		const PageData& page = storage[index / PageSize];
		if (page.size != 0 && page.check(index % PageSize)) return const_iterator(*this, index, 0);
		return end();
	}
	inline std::pair<iterator, iterator> equal_range(id_type id) {
		rep index = id.get();
		if (storage.size() <= index / PageSize) return { end(), end()};
		const PageData& page = storage[index / PageSize];
		if (page.size != 0 && page.check(index % PageSize)) {
			return { iterator(*this, index, 0), ++iterator(*this, index, page.get(index % PageSize).size() - 1) };
		}
		return { end(), end() };
	}
	inline std::pair<const_iterator, const_iterator> equal_range(id_type id) const {
		rep index = id.get();
		if (storage.size() <= index / PageSize) return { end(), end() };
		const PageData& page = storage[index / PageSize];
		if (page.size != 0 && page.check(index % PageSize)) {
			return { const_iterator(*this, index, 0), ++const_iterator(*this, index, page.get(index % PageSize).size() - 1) };
		}
		return { end(), end() };
	}

	inline iterator begin() noexcept {
		rep pageIndex = 0;
		while (pageIndex < storage.size()) {
			const PageData& page = storage[pageIndex];
			if (page.size != 0) {
				// find first valid
				unsigned short relIndex = 0;
				while (!page.check(relIndex)) {
					++relIndex;
					assert(relIndex < PageSize);
				}
				return iterator(*this, pageIndex * PageSize + relIndex, 0);
			}
			++pageIndex;
		}
		return end();
	}
	inline iterator end() noexcept { return iterator(*this, std::numeric_limits<rep>::max(), 0); }
	inline const_iterator begin() const noexcept {
		rep pageIndex = 0;
		while (pageIndex < storage.size()) {
			const PageData& page = storage[pageIndex];
			if (page.size != 0) {
				// find first valid
				unsigned short relIndex = 0;
				while (!page.check(relIndex)) {
					++relIndex;
					assert(relIndex < PageSize);
				}
				return const_iterator(*this, pageIndex * PageSize + relIndex, 0);
			}
			++pageIndex;
		}
		return end();
	}
	inline const_iterator end() const noexcept { return const_iterator(*this, std::numeric_limits<rep>::max(), 0); }

private:
	template <class... Args>
	iterator emplaceWithKey(id_type id, Args&&... args) {
		rep index = id.get();
		rep pageIndex = index / PageSize;
		unsigned short relIndex = index % PageSize;
		if (storage.size() <= pageIndex) {
			storage.resize(pageIndex + 1);
			PageData& page = storage[index / PageSize];
			page.init();
			page.emplace(relIndex, std::forward<Args>(args)...);
			cur_size++;
			return iterator(*this, index, page.data[relIndex].size() - 1);
		}
		PageData& page = storage[index / PageSize];
		if (page.size == 0) {
			page.init();
			page.emplace(relIndex, std::forward<Args>(args)...);
			cur_size++;
			return iterator(*this, index, page.data[relIndex].size() - 1);
		}
		page.emplace(relIndex, std::forward<Args>(args)...);
		cur_size++;
		return iterator(*this, index, page.data[relIndex].size() - 1);
	}

	size_t cur_size = 0;
	std::vector<PageData> storage;
};
#else
#define IdMultiMap std::unordered_multimap
#endif

#define ID_SET
#ifdef ID_SET
template <IdType IdT>
class IdSet {
public:
	static const unsigned int PageSize = IdMapTraits<IdT>::PageSize;
	using id_type = std::remove_cv_t<IdT>;
	using tag = typename id_traits<id_type>::tag;
	using rep = typename id_traits<id_type>::rep;
	using value_type = id_type;

private:
	struct PageData {
		void emplace(unsigned short index) {
			mask[index] = true;
			++size;
		}
		void destroy(unsigned short index) {
			assert(mask[index]);
			mask[index] = false;
			--size;
			assert(size != 0 && "You should just clear this page.");
		}
		bool check(unsigned short index) const { return mask[index]; }
		void init() {
			assert(size == 0); // size should not stay 0
			mask = std::vector<bool>(PageSize, false);
		}
		void clear() {
			assert(size != 0);
			size = 0;
			mask.clear();
		}
		size_t size = 0;
		std::vector<bool> mask;
	};

public:
	struct pointer {
		id_type id;
		id_type* operator->() { return &id; }
	};

	static_assert(std::is_integral_v<rep>, "Id::rep must be an integral type");

	class iterator {
		friend class IdSet<IdT>;
	public:
		iterator& operator++() {
			if (index == std::numeric_limits<rep>::max()) return *this;
			// search rest of page
			rep pageIndex = index / PageSize;
			const PageData& page = idSet->storage[pageIndex];
			assert(page.size != 0);
			for (unsigned short relIndex = index % PageSize + 1; relIndex < PageSize; ++relIndex) {
				if (page.check(relIndex)) {
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
			}
			++pageIndex;
			while (pageIndex < idSet->storage.size()) {
				const PageData& page = idSet->storage[pageIndex];
				if (page.size != 0) {
					// find first valid
					unsigned short relIndex = 0;
					while (!page.check(relIndex)) {
						++relIndex;
						assert(relIndex < PageSize);
					}
					index = pageIndex * PageSize + relIndex;
					return *this;
				}
				++pageIndex;
			}
			// at end
			index = std::numeric_limits<rep>::max();
			return *this;
		}
		iterator operator++(int) {
			iterator tmp = *this;
			this->operator++();
			return tmp;
		}
		id_type operator*() const { return index; }
		pointer operator->() const { return pointer{ index }; }
		bool operator==(const iterator other) const {
			assert(this->idSet == other.idSet);
			return this->index == other.index;
		}
	private:
		iterator(const IdSet<IdT>& idSet, rep index) : idSet(&idSet), index(index) {}
		const IdSet<IdT>* idSet;
		rep index;
	};

	constexpr IdSet() = default;

	inline size_t size() const noexcept { return cur_size; }
	inline bool empty() const noexcept { return cur_size == 0; }

	inline void clear() noexcept { cur_size = 0; storage.clear(); }

	inline bool contains(id_type id) const noexcept { return find(id) != end(); }

	std::pair<iterator, bool> insert(id_type id) { return emplace(id); }
	std::pair<iterator, bool> emplace(id_type id) {
		rep index = id.get();
		rep pageIndex = index / PageSize;
		unsigned short relIndex = index % PageSize;
		if (storage.size() <= pageIndex) {
			storage.resize(pageIndex + 1);
			PageData& page = storage[index / PageSize];
			page.init();
			page.emplace(relIndex);
			cur_size++;
			return { iterator(*this, index), true };
		}
		PageData& page = storage[index / PageSize];
		if (page.size == 0) {
			page.init();
			page.emplace(relIndex);
			cur_size++;
			return { iterator(*this, index), true };
		}
		if (page.check(relIndex)) {
			return { iterator(*this, index), false };
		}
		page.emplace(relIndex);
		cur_size++;
		return { iterator(*this, index), true };
	}

	// assume iter valid
	// if anyone need the iter they can go get it them selves
	void erase(iterator pos) {
		cur_size--;
		rep index = pos.index;
		PageData& page = storage[index / PageSize];
		if (page.size == 1) {
			if (storage.size() - 1 == index / PageSize) {
				storage.resize(storage.size() - 1);
				return;
			}
			page.clear();
			return;
		}
		assert(page.check(index % PageSize));
		page.destroy(index % PageSize);
	}
	size_t erase(IdT id) {
		// check if exists
		rep index = id.get();
		rep pageIndex = index / PageSize;
		if (pageIndex >= storage.size()) return 0;
		PageData& page = storage[pageIndex];
		unsigned short relIndex = index % PageSize;
		if (page.size == 0 || !page.check(relIndex)) return 0;
		// remove
		cur_size--;
		if (page.size == 1) {
			if (storage.size() - 1 == pageIndex) {
				storage.resize(storage.size() - 1);
				return 1;
			}
			page.clear();
			return 1;
		}
		page.destroy(relIndex);
		return 1;
	}

	inline iterator find(id_type id) const {
		rep index = id.get();
		if (storage.size() <= index / PageSize) return end();
		const PageData& page = storage[index / PageSize];
		if (page.size != 0 && page.check(index % PageSize)) return iterator(*this, index);
		return end();
	}

	inline iterator begin() const noexcept {
		rep pageIndex = 0;
		while (pageIndex < storage.size()) {
			const PageData& page = storage[pageIndex];
			if (page.size != 0) {
				// find first valid
				unsigned short relIndex = 0;
				while (!page.check(relIndex)) {
					++relIndex;
					assert(relIndex < PageSize);
				}
				return iterator(*this, pageIndex * PageSize + relIndex);
			}
			++pageIndex;
		}
		return end();
	}
	inline iterator end() const noexcept { return iterator(*this, std::numeric_limits<rep>::max()); }

private:
	size_t cur_size = 0;
	std::vector<PageData> storage;
};
#else
#define IdSet std::unordered_set
#endif

#endif /* id_h */
