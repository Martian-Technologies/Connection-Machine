#ifndef dataUpdateEventManager_h
#define dataUpdateEventManager_h

class DataUpdateEventManager {
	friend class DataUpdateEventReceiver;
public:
	template <class T>
	class EventDataWithValue;

	class EventData {
	public:
		virtual ~EventData() { }
		template <class V>
		inline const DataUpdateEventManager::EventDataWithValue<V>* cast() const;
	};

	template <class T>
	class EventDataWithValue : public EventData {
	public:
		EventDataWithValue(const T& value) : value(value) { }
		inline const T& operator*() const { return value; }
		inline const T* operator->() const { return &value; }
		inline const T& get() const { return value; }
	private:
		T value;
	};

	class DataUpdateEventReceiver {
		friend class DataUpdateEventManager;
	public:
		DataUpdateEventReceiver(DataUpdateEventManager& eventManager);
		DataUpdateEventReceiver(DataUpdateEventReceiver&& other);
		// will just create the DataUpdateEventReceiver as if it was passed the eventManager in other
		DataUpdateEventReceiver(const DataUpdateEventReceiver& other) : DataUpdateEventReceiver(*other.eventManager) {}
		// // will just reset the DataUpdateEventReceiver and set it up as if it was passed the eventManager in other
		// DataUpdateEventReceiver& operator=(const DataUpdateEventReceiver& other) { reset(*other.eventManager); return *this; }
		~DataUpdateEventReceiver();

		void reset(DataUpdateEventManager& eventManager) { clearAllLinks(); this->eventManager = &eventManager; }
		void linkFunction(const std::string& eventName, std::function<void(const EventData*)> function) { functions[eventName] = function; }
		void clearAllLinks() { functions.clear(); }

	private:
		std::map<std::string, std::function<void(const EventData*)>> functions;
		DataUpdateEventManager* eventManager = nullptr;
	};

	DataUpdateEventManager() = default;
	DataUpdateEventManager(const DataUpdateEventManager& other) = delete;
	DataUpdateEventManager& operator=(const DataUpdateEventManager& other) = delete;
	~DataUpdateEventManager();

	void sendEvent(const std::string& eventName) {
		for (DataUpdateEventReceiver* dataUpdateEventReceiver : dataUpdateEventReceivers) {
			for (auto pair : dataUpdateEventReceiver->functions) {
				if (pair.first == eventName) pair.second(nullptr);
			}
		}
	}

	template <class V>
	void sendEvent(const std::string& eventName, const V& value) {
		DataUpdateEventManager::EventDataWithValue<V> eventDataWithValue(value);
		for (DataUpdateEventReceiver* DataUpdateeventReceiver : dataUpdateEventReceivers) {
			for (auto pair : DataUpdateeventReceiver->functions) {
				if (pair.first == eventName) pair.second((EventData*)&eventDataWithValue);
			}
		}
	}

private:
	std::set<DataUpdateEventReceiver*> dataUpdateEventReceivers;
};

template <class V>
inline const DataUpdateEventManager::EventDataWithValue<V>* DataUpdateEventManager::EventData::cast() const { return dynamic_cast<const EventDataWithValue<V>*>(this); }

#endif /* dataUpdateEventManager_h */
