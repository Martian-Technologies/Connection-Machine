#ifndef logging_h
#define logging_h

using LogErrorCallback = std::function<void(const std::string& message, const std::string& subcategory)>;
using LogWarningCallback = std::function<void(const std::string& message, const std::string& subcategory)>;

// basic string logging
void logInfo(const std::string& message, const std::string& subcategory = "");
void logWarning(const std::string& message, const std::string& subcategory = "");
void logError(const std::string& message, const std::string& subcategory = "");
void logFatalError(const std::string& message, const std::string& subcategory = "");

void setLogErrorCallback(LogErrorCallback callback);
void setLogWarningCallback(LogWarningCallback callback);

// fancy formatted logging overloads
template<typename ...Args>
void logInfo(const fmt::format_string<Args...>& formatString, const std::string& subcategory, Args&&...args) {
	std::ostringstream message;
	message << fmt::format(formatString, std::forward<Args>(args)...);
	logInfo(message.str(), subcategory);
}
template<typename ...Args>
void logWarning(const fmt::format_string<Args...>& formatString, const std::string& subcategory, Args&&...args) {
	std::ostringstream message;
	message << fmt::format(formatString, std::forward<Args>(args)...);
	logWarning(message.str(), subcategory);
}
template<typename ...Args>
void logError(const fmt::format_string<Args...>& formatString, const std::string& subcategory, Args&&...args) {
	std::ostringstream message;
	message << fmt::format(formatString, std::forward<Args>(args)...);
	logError(message.str(), subcategory);
}
template<typename ...Args>
void logFatalError(const fmt::format_string<Args...>& formatString, const std::string& subcategory, Args&&...args) {
	std::ostringstream message;
	message << fmt::format(formatString, std::forward<Args>(args)...);
	logFatalError(message.str(), subcategory);
}

// fancy one argument format overloads
template<typename T>
void logInfo(const T& obj, const std::string& subcategory = "") {
	logInfo("{}", subcategory, obj);
}
template<typename T>
void logWarning(const T& obj, const std::string& subcategory = "") {
	logWarning("{}", subcategory, obj);
}
template<typename T>
void logError(const T& obj, const std::string& subcategory = "") {
	logError("{}", subcategory, obj);
}
template<typename T>
void logFatalError(const T& obj, const std::string& subcategory = "") {
	logFatalError("{}", subcategory, obj);
}

#endif /* logging_h */
