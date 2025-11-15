#ifndef id_h
#define id_h

#define DECLARE_ID_TYPE(TypeName, RepType)                                                                                                                               \
	struct TypeName##__TAG__;                                                                                                                                            \
	using TypeName = Id<TypeName##__TAG__, RepType>

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
	auto format(const Id<Tag, Rep>& id, FormatContext& ctx) const {
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
	constexpr bool isIdUsed(id_type id) const { return id.get() < nextId && !unusedIds.contains(id.get()); }
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
	nlohmann::json dumpState() const {
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

	constexpr LinearIdProvider(rep initialValue) : nextId(initialValue) { }

	constexpr id_type getNewId() { return id_type(nextId++); }
	constexpr id_type peekNext() const { return id_type(nextId); }
	constexpr id_type lastIdProvided() const { return id_type(nextId - 1); }
	void reset() { nextId = 0; }

private:
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

	inline bool contains(id_type id) const noexcept { return id.get() < storage.size(); }

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

#endif /* id_h */
