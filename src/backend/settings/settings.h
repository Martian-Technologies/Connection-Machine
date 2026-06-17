#ifndef settings_h
#define settings_h

#include "settingsMap.h"
#include "util/algorithm.h"

extern std::thread::id mainThreadId;

namespace Settings {
	std::mutex& getSettingsMapReadLock(); // only needed if you are on another thread; (dont write from another thread!)
	SettingsMap& getSettingsMap();
	template<SettingType settingType>
	inline void registerSetting(const std::string& name) {
		if (mainThreadId == std::this_thread::get_id()) {
			std::lock_guard lock(getSettingsMapReadLock());
			getSettingsMap().registerSetting<settingType>(name);
			return;
		}
		getSettingsMap().registerSetting<settingType>(name);
	}
	template<SettingType settingType>
	inline void registerSetting(const std::string& name, const typename SettingTypeToType<settingType>::type& value) {
		if (mainThreadId == std::this_thread::get_id()) {
			std::lock_guard lock(getSettingsMapReadLock());
			getSettingsMap().registerSetting<settingType>(name, value);
			return;
		}
		getSettingsMap().registerSetting<settingType>(name, value);
	}
	template<SettingType settingType>
	inline const SettingTypeToType<settingType>::type* get(const std::string& key) {
		return getSettingsMap().get<settingType>(key);
	}
	template<SettingType settingType>
	inline const SettingTypeToType<settingType>::type* getDefault(const std::string& key) {
		return getSettingsMap().getDefault<settingType>(key);
	}
	template<SettingType settingType>
	inline const SettingTypeToType<settingType>::type& get(const std::string& key, const typename SettingTypeToType<settingType>::type& fallbackValue) {
		return valueOr(getSettingsMap().get<settingType>(key), fallbackValue);
	}
	template<SettingType settingType>
	inline bool set(const std::string& key, const typename SettingTypeToType<settingType>::type& value) {
		if (mainThreadId == std::this_thread::get_id()) {
			std::lock_guard lock(getSettingsMapReadLock());
			return getSettingsMap().set<settingType>(key, value);
		}
		return getSettingsMap().set<settingType>(key, value);
	}
	inline SettingType getType(const std::string& key) {
		return getSettingsMap().getType(key);
	}
	inline bool isDefalut(const std::string& key) {
		return getSettingsMap().isDefault(key);
	}
	inline bool hasKey(const std::string& key) {
		return getSettingsMap().hasKey(key);
	}
	template<SettingType settingType>
	inline void registerListener(const std::string& key, SettingsMap::ListenerFunction<settingType> listener) {
		if (mainThreadId == std::this_thread::get_id()) {
			std::lock_guard lock(getSettingsMapReadLock());
			getSettingsMap().registerListener<settingType>(key, std::move(listener));
			return;
		}
		getSettingsMap().registerListener<settingType>(key, std::move(listener));
	}
	inline void registerListener(SettingsMap::AllListenerFunction listener) {
		if (mainThreadId == std::this_thread::get_id()) {
			std::lock_guard lock(getSettingsMapReadLock());
			getSettingsMap().registerListener(std::move(listener));
			return;
		}
		getSettingsMap().registerListener(std::move(listener));
	}
};

#endif /* settings_h */
