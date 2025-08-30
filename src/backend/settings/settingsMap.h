#ifndef settingsMap_h
#define settingsMap_h

#include "keybind.h"

enum SettingType {
	VOID,
	STRING,
	INT,
	BOOL,
	KEYBIND,
	FILE_PATH
};

template<SettingType settingType>
struct SettingTypeToType {
	static_assert(
		settingType == SettingType::VOID ||
		settingType == SettingType::STRING ||
		settingType == SettingType::INT ||
		settingType == SettingType::KEYBIND ||
		settingType == SettingType::FILE_PATH,
		"Invalid SettingType. Must be one of VOID, STRING, INT, or KEYBIND."
	);
	using type = void;
};

template<> struct SettingTypeToType<SettingType::VOID> { using type = char; };
template<> struct SettingTypeToType<SettingType::STRING> { using type = std::string; };
template<> struct SettingTypeToType<SettingType::INT> { using type = int; };
template<> struct SettingTypeToType<SettingType::BOOL> { using type = bool; };
template<> struct SettingTypeToType<SettingType::KEYBIND> { using type = Keybind; };
template<> struct SettingTypeToType<SettingType::FILE_PATH> { using type = std::string; };

class SettingsMap {
public:
	template<SettingType settingType>
	using ListenerFunction = std::function<void(const typename SettingTypeToType<settingType>::type&)>;

	class SettingListenerBase {
	public:
		SettingListenerBase(SettingType type) : type(type) { }
		virtual ~SettingListenerBase() = default;
		SettingType getType() const { return type; };
	private:
		SettingType type;
	};

	class SettingEntryBase {
		friend SettingsMap;
	public:
		SettingEntryBase(SettingType type) : type(type) { }
		virtual ~SettingEntryBase() = default;
		SettingType getType() const { return type; };
		template<SettingType settingType>
		void addListener(ListenerFunction<settingType> listener) {
			listeners.emplace_back(std::make_unique<SettingListener<settingType>>(listener));
		}
	protected:
		std::vector<std::unique_ptr<SettingListenerBase>> listeners;
	private:
		SettingType type = SettingType::VOID;
	};

	template<SettingType settingType>
	void registerSetting(std::string key) {
		std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::iterator iter = mappings.find(key);
		if (iter == mappings.end()) {
			mappings[key] = std::make_unique<SettingEntry<settingType>>();
		} else {
			changeType<settingType>(iter->second);
		}
	}
	template<SettingType settingType>
	void registerSetting(std::string key, const SettingTypeToType<settingType>::type& value) {
		std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::iterator iter = mappings.find(key);
		if (iter == mappings.end()) {
			mappings[key] = std::make_unique<SettingEntry<settingType>>(value);
		} else {
			changeType<settingType>(iter->second, value);
		}
	}
	template<SettingType settingType>
	void registerListener(std::string key, ListenerFunction<settingType> listener) {
		std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::iterator iter = mappings.find(key);
		if (iter == mappings.end()) {
			(mappings[key] = std::make_unique<SettingEntryBase>(SettingType::VOID))->template addListener<settingType>(listener);
		} else {
			iter->second->addListener<settingType>(listener);
		}
	}

	// -- Getters --
	const std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>& getAllKeys() const { return mappings; }
	template<SettingType settingType>
	const SettingTypeToType<settingType>::type* get(const std::string& key) const {
		std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::const_iterator iter = mappings.find(key);
		if (iter == mappings.end() || settingType != iter->second->getType()) return nullptr;
		const SettingEntry<settingType>* settingEntry = dynamic_cast<const SettingEntry<settingType>*>(iter->second.get());
		if (!settingEntry) {
			logError("Failed to get value. Type and SettingType mismatched internal state bad. Please report error and relaunch the app.", "SettingsMap");
			return nullptr;
		}
		return &(settingEntry->getValue());
	}
	SettingType getType(const std::string& key) const {
		std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::const_iterator iter = mappings.find(key);
		if (iter == mappings.end()) return SettingType::VOID;
		return iter->second->getType();
	}
	bool hasKey(const std::string& key) const {
		std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::const_iterator iter = mappings.find(key);
		return iter != mappings.end() && iter->second->getType() != SettingType::VOID;
	}

	// -- Setters --
	template<SettingType settingType>
	bool set(const std::string& key, const SettingTypeToType<settingType>::type& value) {
		std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::const_iterator iter = mappings.find(key);
		if (iter == mappings.end()) {
			logError("Failed to set value. Could not find key \"" + key + "\"", "SettingsMap");
			return false;
		}
		if (settingType != iter->second->getType()) {
			logError("Failed to set value. Could not find key \"" + key + "\"", "SettingsMap");
			return false;
		}
		SettingEntry<settingType>* settingEntry = dynamic_cast<SettingEntry<settingType>*>(iter->second.get());
		if (!settingEntry) {
			logError("Failed to set value. Type and SettingType mismatched internal state bad. Please report error and relaunch the app.", "SettingsMap");
			return false;
		}
		settingEntry->setValue(value);
		return true;
	}

private:
	template <SettingType settingType>
	void changeType(std::unique_ptr<SettingEntryBase>& settingEntryBase) {
		if (settingEntryBase->getType() == settingType) return;
		std::vector<std::unique_ptr<SettingListenerBase>> listeners = std::move(settingEntryBase->listeners);
		settingEntryBase = std::make_unique<SettingEntry<settingType>>();
		settingEntryBase->listeners = std::move(listeners);
	}

	template <SettingType settingType>
	void changeType(std::unique_ptr<SettingEntryBase>& settingEntryBase, const SettingTypeToType<settingType>::type& value) {
		if (settingEntryBase->getType() == settingType) {
			SettingEntry<settingType>* settingEntry = dynamic_cast<SettingEntry<settingType>*>(settingEntryBase.get());
			if (!settingEntry) {
				logError("Failed to set value. Type and SettingType mismatched internal state bad. Please report error and relaunch the app.", "SettingsMap");
			} else {
				settingEntry->setValue(value);
			}
			return;
		}
		std::vector<std::unique_ptr<SettingListenerBase>> listeners = std::move(settingEntryBase->listeners);
		settingEntryBase = std::make_unique<SettingEntry<settingType>>(value);
		settingEntryBase->listeners = std::move(listeners);
		for (const auto& listener : listeners) {
			if (listener->getType() == settingType) {
				SettingListener<settingType>* listenerCast = dynamic_cast<SettingListener<settingType>*>(listener.get());
				if (!listenerCast) {
					logError("Failed to set value. Type and SettingType mismatched internal state bad. Please report error and relaunch the app.", "SettingsMap");
				} else {
					listenerCast->call(value);
				}
			}
		}
	}

	template <SettingType settingType>
	class SettingListener : public SettingListenerBase {
	public:
		SettingListener(ListenerFunction<settingType> listener) : SettingListenerBase(settingType), listener(listener) { }
		void call(const SettingTypeToType<settingType>::type& value) const { listener(value); }

	private:
		ListenerFunction<settingType> listener;
	};

	template <SettingType settingType>
	class SettingEntry : public SettingEntryBase {
	public:
		SettingEntry() : SettingEntryBase(settingType) { }
		SettingEntry(const SettingTypeToType<settingType>::type& value) : SettingEntryBase(settingType), value(value) { }

		const SettingTypeToType<settingType>::type& getValue() const { return value; }
		void setValue(const SettingTypeToType<settingType>::type& value) {
			this->value = value;
			for (const auto& listener : listeners) {
				if (listener->getType() == settingType) {
					SettingListener<settingType>* listenerCast = dynamic_cast<SettingListener<settingType>*>(listener.get());
					if (!listenerCast) {
						logError("Failed to set value. Type and SettingType mismatched internal state bad. Please report error and relaunch the app.", "SettingsMap");
					} else {
						listenerCast->call(value);
					}
				}
			}
		}

	private:
		SettingTypeToType<settingType>::type value;
	};

	std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>> mappings;
};

template<>
inline void SettingsMap::registerSetting<SettingType::VOID>(std::string key) {
	std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::iterator iter = mappings.find(key);
	if (iter != mappings.end()) {
		changeType<SettingType::VOID>(iter->second);
	}
}

template<>
inline void SettingsMap::registerSetting<SettingType::VOID>(std::string key, const SettingTypeToType<SettingType::VOID>::type& value) { // IDK how you would even call this
	std::unordered_map<std::string, std::unique_ptr<SettingEntryBase>>::iterator iter = mappings.find(key);
	if (iter != mappings.end()) {
		changeType<SettingType::VOID>(iter->second, value);
	}
}

template<>
inline bool SettingsMap::set<SettingType::VOID>(const std::string& key, const SettingTypeToType<SettingType::VOID>::type& value) { // IDK how you would even call this
	logError("Failed to set value. Could not set type VOID.", "SettingsMap");
	return false;
}

#endif /* settingsMap_h */
