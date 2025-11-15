#ifndef kvStore_h
#define kvStore_h

#include "fileListener/fileListener.h"

enum class KVType {
	STRING,
	INT,
	UINT,
	DOUBLE,
	BOOL
};

template<KVType>
struct KVTypeToType {
	static_assert([]{ return false; }(),
				  "Invalid KVType. Do not use KVType::VOID (not supported).");
	using type = void;
};

template<> struct KVTypeToType<KVType::STRING> { using type = std::string; };
template<> struct KVTypeToType<KVType::INT>    { using type = std::int64_t; };
template<> struct KVTypeToType<KVType::UINT>   { using type = std::uint64_t; };
template<> struct KVTypeToType<KVType::DOUBLE> { using type = double; };
template<> struct KVTypeToType<KVType::BOOL>   { using type = bool; };

class KVStore {
public:
	static std::shared_ptr<KVStore> getStore(const std::string& storeName);

	template<KVType kvType>
	void set(const std::string& key, const typename KVTypeToType<KVType(kvType)>::type& value);

	template<KVType kvType>
	std::optional<typename KVTypeToType<KVType(kvType)>::type>
	get(const std::string& key) const;

	explicit KVStore(std::string storeName);
	~KVStore();

private:
	using Value = std::variant<std::string, std::int64_t, std::uint64_t, double, bool>;

	std::string storeName;
	std::unordered_map<std::string, Value> store;
	mutable std::mutex instanceMutex;
	std::string path;
	FileListener::CallbackHandle fileWatchHandle;

	static std::mutex storeMutex;
	static std::unordered_map<std::string, std::weak_ptr<KVStore>> storeInstances;
	static FileListener fileListener;

	void loadFile();
	void setValueInternal(const std::string& key, Value value);
	std::optional<Value> getValueInternal(const std::string& key) const;
};

template<KVType kvType>
inline void KVStore::set(const std::string& key, const typename KVTypeToType<KVType(kvType)>::type& value) {
	setValueInternal(key, KVStore::Value(value));
}

template<KVType kvType>
inline std::optional<typename KVTypeToType<KVType(kvType)>::type>
KVStore::get(const std::string& key) const {
	auto rawValue = getValueInternal(key);
	if (!rawValue) return std::nullopt;
	using T = typename KVTypeToType<KVType(kvType)>::type;
	KVStore::Value& storedValue = rawValue.value();
	if (const T* p = std::get_if<T>(&storedValue)) return *p;
	logError("Type mismatch when getting key '{}' from KVStore '{}'", "KVStore", key, storeName);
	return std::nullopt;
}

#endif /* kvStore_h */
