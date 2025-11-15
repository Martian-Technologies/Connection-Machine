#include <gtest/gtest.h>
#include "logging/logging.h"

namespace {
class LogErrorFailureEnvironment : public ::testing::Environment {
public:
	void SetUp() override {
		auto failureReporter = [](const std::string& level, const std::string& message, const std::string& subcategory) {
			const std::string context = subcategory.empty() ? "" : (" [" + subcategory + "]");
			ADD_FAILURE() << level << " called" << context << ": " << message;
		};

		setLogErrorCallback([failureReporter](const std::string& message, const std::string& subcategory) {
			failureReporter("logError", message, subcategory);
		});
		setLogWarningCallback([failureReporter](const std::string& message, const std::string& subcategory) {
			failureReporter("logWarning", message, subcategory);
		});
	}

	void TearDown() override {
		setLogErrorCallback({});
		setLogWarningCallback({});
	}
};

::testing::Environment* const logErrorFailureEnvironment =
	::testing::AddGlobalTestEnvironment(new LogErrorFailureEnvironment());
} // namespace
