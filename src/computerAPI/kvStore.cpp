#include "kvStore.h"
#include "directoryManager.h"

#include <nlohmann/json.hpp>

std::unordered_map<std::string, std::weak_ptr<KVStore>> KVStore::storeInstances;
std::mutex KVStore::storeMutex;
FileListener KVStore::fileListener(std::chrono::milliseconds(200));

std::shared_ptr<KVStore> KVStore::getStore(const std::string& storeName) {
	std::lock_guard<std::mutex> lock(storeMutex);
	if (auto it = storeInstances.find(storeName); it != storeInstances.end()) {
		if (auto sp = it->second.lock()) {
			return sp;
		}
	}

	auto sp = std::shared_ptr<KVStore>(new KVStore(storeName));
	storeInstances[storeName] = sp;

	return sp;
}

KVStore::KVStore(std::string storeName) : storeName(std::move(storeName)) {
	path = (DirectoryManager::getConfigDirectory() / ("kv_" + this->storeName + ".json")).string();
	loadFile();
	fileWatchHandle = fileListener.watchFile(path, [this](const std::filesystem::path& path) {
		loadFile();
	});
}

KVStore::~KVStore() {
	std::lock_guard<std::mutex> lock(instanceMutex);
	fileListener.stopWatching(path, fileWatchHandle);
}

void KVStore::setValueInternal(const std::string& key, Value value) {
	std::lock_guard<std::mutex> lock(instanceMutex);
	store[key] = std::move(value);
	nlohmann::json jsonData;
	for (const auto& [k, v] : store) {
		if (std::holds_alternative<std::string>(v)) {
			jsonData[k] = std::get<std::string>(v);
		} else if (std::holds_alternative<std::int64_t>(v)) {
			jsonData[k] = std::get<std::int64_t>(v);
		} else if (std::holds_alternative<std::uint64_t>(v)) {
			jsonData[k] = std::get<std::uint64_t>(v);
		} else if (std::holds_alternative<double>(v)) {
			jsonData[k] = std::get<double>(v);
		} else if (std::holds_alternative<bool>(v)) {
			jsonData[k] = std::get<bool>(v);
		}
	}
	std::ofstream file(path);
	if (!file.is_open()) {
		logError("Failed to open KVStore file '{}' for writing", "KVStore::set", path);
		return;
	}
	file << jsonData.dump(4);
}

std::optional<KVStore::Value> KVStore::getValueInternal(const std::string& key) const {
	std::lock_guard<std::mutex> lock(instanceMutex);
	auto it = store.find(key);
	if (it == store.end()) return std::nullopt;
	return it->second;
}

void KVStore::loadFile() {
	std::lock_guard<std::mutex> lock(instanceMutex);
	if (!std::filesystem::exists(path)) {
		logInfo("KVStore file '{}' does not exist, starting with empty store", "KVStore::loadFile", path);
		store.clear();
		return;
	}
	std::ifstream file(path);
	if (!file.is_open()) {
		logError("Failed to open KVStore file '{}'", "KVStore::loadFile", path);
		return;
	}
	try {
		nlohmann::json jsonData;
		file >> jsonData;
		store.clear();
		for (auto& [key, value] : jsonData.items()) {
			if (value.is_string()) {
				store[key] = value.get<std::string>();
			} else if (value.is_number_unsigned()) {
				store[key] = value.get<std::uint64_t>();
			} else if (value.is_number_integer()) {
				store[key] = value.get<std::int64_t>();
			} else if (value.is_number_float()) {
				store[key] = value.get<double>();
			} else if (value.is_boolean()) {
				store[key] = value.get<bool>();
			} else {
				logError("Unsupported value type for key '{}' in KVStore file '{}'", "KVStore::loadFile", key, path);
			}
		}
		logInfo("Loaded KVStore file '{}'", "KVStore::loadFile", path);
	} catch (const nlohmann::json::parse_error& e) {
		logError("Failed to parse KVStore file '{}': {}", "KVStore::loadFile", path, e.what());
	}
}
