#ifndef logger_h
#define logger_h

enum class LogType {
	Info = 1,
	Warning = 2,
	Error = 4,
	Fatal = 8
};

// Possible future improvements
// - [ ] Make ANSI optional
// - [ ] Detailed category system for suppressing certain messages

class Logger {
public:
	Logger(const std::filesystem::path& outputFile);

	void log(LogType type, const std::string& message, const std::string& subcategory = "");

private:
	std::filesystem::path outputFile;
	std::mutex loggingMutex;

	std::ofstream outputFileStream;
};

#endif /* logger_h */
