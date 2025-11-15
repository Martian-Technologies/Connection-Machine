#ifndef fileListener_h
#define fileListener_h

class FileListener {
public:
	using Callback = std::function<void(const std::string&)>;
	using CallbackHandle = std::size_t;

	// Constructor. Starts the watcher thread with a given check interval.
	FileListener(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));

	// Destructor. Stops the watcher thread and cleans up.
	~FileListener();

	// Start watching a file. Returns a handle to the registered callback.
	CallbackHandle watchFile(const std::string& path, Callback callback);

	// Stop watching a file callback by handle.
	void stopWatching(const std::string& path, CallbackHandle handle);

	// Call this from the main thread to process pending file-change callbacks.
	void processCallbacks();

	// Set how often files are checked for changes.
	void setInterval(std::chrono::milliseconds interval);

	nlohmann::json dumpState() const;

private:
	struct WatchedFile {
		std::filesystem::file_time_type lastWriteTime;
		std::unordered_map<CallbackHandle, Callback> callbacks;
	};

	void watcherThreadFunc();

	std::unordered_map<std::string, WatchedFile> watchedFiles_;
	mutable std::mutex watchedFilesMutex_;

	std::queue<std::pair<std::string, Callback>> callbackQueue_;
	std::mutex callbackQueueMutex_;

	bool running_;
	std::thread watcherThread_;
	std::chrono::milliseconds checkInterval_;
	std::condition_variable cv_;
	std::mutex cvMutex_;

	CallbackHandle nextCallbackId_ = 1;
};

#endif /* fileListener_h */
