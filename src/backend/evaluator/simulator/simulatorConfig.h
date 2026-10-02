#ifndef simulatorConfig_h
#define simulatorConfig_h

#include <limits>

#include "backend/dataUpdateEventManager.h"
#include "backend/settings/settings.h"
#include "simulatorDefs.h"

class SimulatorConfig {
public:
	SimulatorConfig(simulator_id_t simulatorId, DataUpdateEventManager& dataUpdateEventManager) : simulatorId(simulatorId), dataUpdateEventManager(dataUpdateEventManager) {
		std::weak_ptr<int> lifetime = listenerLifetime;
		Settings::registerListener<SettingType::UINT>("Simulation/Max Thread Count", [this, lifetime](const unsigned int& count) {
			if (!lifetime.expired()) this->setMaxThreadCount(static_cast<int>(std::min(count, static_cast<unsigned int>(std::numeric_limits<int>::max()))));
		});
	}

	inline double getTargetTickrate() const {
		return targetTickrate.load();
	}

	inline void setTargetTickrate(double tickrate) {
		targetTickrate.store(tickrate);
		notifySubscribers();
		dataUpdateEventManager.sendEvent("evaluatorTargetTickrateSet", std::make_pair(simulatorId, tickrate));
	}

	inline bool isTickrateLimiterEnabled() const {
		return tickrateLimiter.load();
	}

	inline void setTickrateLimiter(bool enabled) {
		tickrateLimiter.store(enabled);
		notifySubscribers();
		dataUpdateEventManager.sendEvent("evaluatorTickrateLimiterSet", std::make_pair(simulatorId, enabled));
	}

	inline bool isRunning() const {
		return running.load();
	}

	inline void setRunning(bool run) {
		running.store(run);
		notifySubscribers();
		dataUpdateEventManager.sendEvent("evaluatorRunningSet", std::make_pair(simulatorId, run));
	}

	inline bool isRealistic() const {
		return realistic.load();
	}

	inline void setRealistic(bool value) {
		realistic.store(value);
		notifySubscribers();
		dataUpdateEventManager.sendEvent("evaluatorRealisticSet", std::make_pair(simulatorId, value));
	}

	inline int getMaxThreadCount() const {
		return maxThreadCount.load();
	}

	inline void setMaxThreadCount(int value) {
		maxThreadCount.store(value);
		notifySubscribers();
		dataUpdateEventManager.sendEvent("evaluatorMaxThreadCountSet", std::make_pair(simulatorId, value));
	}

	inline void addSprint(int nTicks) {
		sprintCounter.fetch_add(nTicks);
		notifySubscribers();
	}

	inline void resetSprintCount() {
		sprintCounter.store(0);
		notifySubscribers();
	}

	inline int getSprintCount() const {
		return sprintCounter.load();
	}

	inline bool canConsumeSprintTick() {
		return sprintCounter.load(std::memory_order_relaxed) > 0;
	}

	inline bool consumeSprintTick() {
		int expected = sprintCounter.load(std::memory_order_relaxed);
		while (expected > 0) {
			if (sprintCounter.compare_exchange_weak(expected, expected - 1, std::memory_order_acq_rel)) {
				return true;
			}
		}
		return false;
	}

	inline void subscribe(std::function<void()> callback) {
		std::lock_guard<std::mutex> lock(subscribersMutex);
		subscribers.push_back(callback);
	}

	inline void unsubscribe() {
		std::lock_guard<std::mutex> lock(subscribersMutex);
		subscribers.clear();
	}

	nlohmann::json dumpState() const /* GCOVR_EXCL_FUNCTION */ {
		nlohmann::json stateJson;
		stateJson["targetTickrate"] = getTargetTickrate();
		stateJson["tickrateLimiter"] = isTickrateLimiterEnabled();
		stateJson["running"] = isRunning();
		stateJson["realistic"] = isRealistic();
		stateJson["sprintCounter"] = getSprintCount();
		stateJson["maxThreadCount"] = getMaxThreadCount();
		return stateJson;
	}

private:
	// Settings listeners outlive simulators. Callbacks run on the application
	// thread; an expired token prevents them from touching a destroyed config.
	std::shared_ptr<int> listenerLifetime = std::make_shared<int>(0);
	DataUpdateEventManager& dataUpdateEventManager;
	simulator_id_t simulatorId;

	std::atomic<double> targetTickrate = 0.0;
	std::atomic<bool> tickrateLimiter = true;
	std::atomic<bool> running = false;
	std::atomic<bool> realistic = false;
	std::atomic<int> sprintCounter = 0;
	std::atomic<int> maxThreadCount = std::thread::hardware_concurrency() / 2;

	std::vector<std::function<void()>> subscribers;
	std::mutex subscribersMutex;

	void notifySubscribers() {
		std::lock_guard<std::mutex> lock(subscribersMutex);
		for (const auto& callback : subscribers) {
			callback();
		}
	}
};

#endif /* simulatorConfig_h */
