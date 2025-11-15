#include "fileListener.h"

FileListener::FileListener(std::chrono::milliseconds interval)
	: running_(true), checkInterval_(interval) {
	watcherThread_ = std::thread(&FileListener::watcherThreadFunc, this);
}

FileListener::~FileListener() {
	{
		std::lock_guard<std::mutex> lock(cvMutex_);
		running_ = false;
	}
	cv_.notify_one();
	if (watcherThread_.joinable()) {
		watcherThread_.join();
	}
}

FileListener::CallbackHandle FileListener::watchFile(const std::string& path, Callback callback) {
	std::lock_guard<std::mutex> lock(watchedFilesMutex_);
	auto& entry = watchedFiles_[path];
	if (entry.callbacks.empty() && std::filesystem::exists(path)) {
		entry.lastWriteTime = std::filesystem::last_write_time(path);
	}

	CallbackHandle handle = nextCallbackId_++;
	entry.callbacks.emplace(handle, std::move(callback));
	return handle;
}

void FileListener::stopWatching(const std::string& path, CallbackHandle handle) {
	std::lock_guard<std::mutex> lock(watchedFilesMutex_);
	auto it = watchedFiles_.find(path);
	if (it != watchedFiles_.end()) {
		it->second.callbacks.erase(handle);
		if (it->second.callbacks.empty()) {
			watchedFiles_.erase(it);
		}
	}
}

void FileListener::processCallbacks() {
	std::queue<std::pair<std::string, Callback>> localQueue;
	{
		std::lock_guard<std::mutex> lock(callbackQueueMutex_);
		std::swap(localQueue, callbackQueue_);
	}

	while (!localQueue.empty()) {
		auto [path, callback] = localQueue.front();
		localQueue.pop();
		callback(path);
	}
}

void FileListener::setInterval(std::chrono::milliseconds interval) {
	checkInterval_ = interval;
	cv_.notify_one();
}

void FileListener::watcherThreadFunc() {
	while (true) {
		{
			std::unique_lock<std::mutex> lock(cvMutex_);
			if (!running_) break;
		}

		{
			std::lock_guard<std::mutex> lock(watchedFilesMutex_);
			for (auto& [path, file] : watchedFiles_) {
				if (!std::filesystem::exists(path)) continue;

				auto currentWriteTime = std::filesystem::last_write_time(path);
				if (currentWriteTime != file.lastWriteTime) {
					file.lastWriteTime = currentWriteTime;

					std::lock_guard<std::mutex> queueLock(callbackQueueMutex_);
					for (auto& cbPair : file.callbacks) {
						callbackQueue_.emplace(path, cbPair.second);
					}
				}
			}
		}

		std::unique_lock<std::mutex> lock(cvMutex_);
		cv_.wait_for(lock, checkInterval_);
	}
}

nlohmann::json FileListener::dumpState() const {
	nlohmann::json stateJson;
	try {
		std::lock_guard<std::mutex> lock(watchedFilesMutex_);
		for (const auto& [path, file] : watchedFiles_) {
			try {
				nlohmann::json fileJson;
				fileJson["lastWriteTime"] = file.lastWriteTime.time_since_epoch().count();
				nlohmann::json callbacksJson = nlohmann::json::array();
				for (const auto& [handle, _] : file.callbacks) {
					callbacksJson.push_back(handle);
				}
				fileJson["callbacks"] = callbacksJson;
				stateJson[path] = fileJson;
			} catch (std::exception& e) {
				stateJson[path] = "Error dumping state: " + std::string(e.what());
			}
		}
	} catch (std::exception& e) {
		stateJson["error"] = e.what();
	}
	return stateJson;
}
