#ifndef evalConfig_h
#define evalConfig_h

#include "backend/settings/settings.h"

class EvalConfig {
public:
	EvalConfig() {
		Settings::registerListener<SettingType::UINT>("Simulation/Max Thread Count", [this](const int& newMaxThreadCount) { this->setMaxThreadCount(newMaxThreadCount); });
	}

	inline double getTargetTickrate() const {
		return targetTickrate.load();
	}

	inline void setTargetTickrate(double tickrate) {
		targetTickrate.store(tickrate);
		notifySubscribers();
	}

	inline bool isTickrateLimiterEnabled() const {
		return tickrateLimiter.load();
	}

	inline void setTickrateLimiter(bool enabled) {
		tickrateLimiter.store(enabled);
		notifySubscribers();
	}

	inline bool isRunning() const {
		return running.load();
	}

	inline void setRunning(bool run) {
		running.store(run);
		notifySubscribers();
	}

	inline bool isRealistic() const {
		return realistic.load();
	}

	inline void setRealistic(bool value) {
		realistic.store(value);
		notifySubscribers();
	}

	inline int getMaxThreadCount() const {
		return maxThreadCount.load();
	}

	inline void setMaxThreadCount(int value) {
		maxThreadCount.store(value);
		notifySubscribers();
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

private:
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

#endif /* evalConfig_h */
