#include "logging.h"
#include "logger.h"

Logger mainLogger("connection_machine.log");

namespace {
	LogErrorCallback g_logErrorCallback;
	LogWarningCallback g_logWarningCallback;
}

void logInfo(const std::string& message, const std::string& subcategory) {
	mainLogger.log(LogType::Info, message, subcategory);
}

void logWarning(const std::string& message, const std::string& subcategory) {
	mainLogger.log(LogType::Warning, message, subcategory);
	if (g_logWarningCallback) {
		g_logWarningCallback(message, subcategory);
	}
}

void logError(const std::string& message, const std::string& subcategory) {
	mainLogger.log(LogType::Error, message, subcategory);
	if (g_logErrorCallback) {
		g_logErrorCallback(message, subcategory);
	}
}

void logFatalError(const std::string& message, const std::string& subcategory) {
	mainLogger.log(LogType::Fatal, message, subcategory);
}

void setLogErrorCallback(LogErrorCallback callback) {
	g_logErrorCallback = callback;
}

void setLogWarningCallback(LogWarningCallback callback) {
	g_logWarningCallback = callback;
}
