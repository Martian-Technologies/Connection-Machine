#include <gtest/gtest.h>
// #include "backend/evaluator/evaluatorInternal.h"
#include "computerAPI/directoryManager.h"
#include "loggingTestSetup.h"

namespace {
class BufferedResultPrinter : public ::testing::TestEventListener {
public:
	explicit BufferedResultPrinter(::testing::TestEventListener* defaultPrinter)
		: defaultPrinter(defaultPrinter) {
	}

	~BufferedResultPrinter() override {
		delete defaultPrinter;
	}

	void OnTestProgramStart(const ::testing::UnitTest& unitTest) override {
		defaultPrinter->OnTestProgramStart(unitTest);
	}

	void OnTestIterationStart(const ::testing::UnitTest& unitTest, int iteration) override {
		defaultPrinter->OnTestIterationStart(unitTest, iteration);
	}

	void OnEnvironmentsSetUpStart(const ::testing::UnitTest& unitTest) override {
		defaultPrinter->OnEnvironmentsSetUpStart(unitTest);
	}

	void OnEnvironmentsSetUpEnd(const ::testing::UnitTest& unitTest) override {
		defaultPrinter->OnEnvironmentsSetUpEnd(unitTest);
	}

	void OnTestSuiteStart(const ::testing::TestSuite& testSuite) override {
		defaultPrinter->OnTestSuiteStart(testSuite);
	}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
	void OnTestCaseStart(const ::testing::TestCase& testCase) override {
		defaultPrinter->OnTestCaseStart(testCase);
	}
#endif

	void OnTestStart(const ::testing::TestInfo& testInfo) override {
		flushedCurrentTest = false;
		defaultPrinter->OnTestStart(testInfo);
	}

	void OnTestDisabled(const ::testing::TestInfo& testInfo) override {
		defaultPrinter->OnTestDisabled(testInfo);
	}

	void OnTestPartResult(const ::testing::TestPartResult& testPartResult) override {
		if (testPartResult.failed() && !flushedCurrentTest) {
			logging_test::flushCapturedLogs();
			flushedCurrentTest = true;
		}
		defaultPrinter->OnTestPartResult(testPartResult);
	}

	void OnTestEnd(const ::testing::TestInfo& testInfo) override {
		if (!testInfo.result()->Passed() && !flushedCurrentTest) {
			logging_test::flushCapturedLogs();
			flushedCurrentTest = true;
		}
		defaultPrinter->OnTestEnd(testInfo);
	}

	void OnTestSuiteEnd(const ::testing::TestSuite& testSuite) override {
		defaultPrinter->OnTestSuiteEnd(testSuite);
	}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
	void OnTestCaseEnd(const ::testing::TestCase& testCase) override {
		defaultPrinter->OnTestCaseEnd(testCase);
	}
#endif

	void OnEnvironmentsTearDownStart(const ::testing::UnitTest& unitTest) override {
		defaultPrinter->OnEnvironmentsTearDownStart(unitTest);
	}

	void OnEnvironmentsTearDownEnd(const ::testing::UnitTest& unitTest) override {
		defaultPrinter->OnEnvironmentsTearDownEnd(unitTest);
	}

	void OnTestIterationEnd(const ::testing::UnitTest& unitTest, int iteration) override {
		defaultPrinter->OnTestIterationEnd(unitTest, iteration);
	}

	void OnTestProgramEnd(const ::testing::UnitTest& unitTest) override {
		defaultPrinter->OnTestProgramEnd(unitTest);
	}

private:
	::testing::TestEventListener* defaultPrinter = nullptr;
	bool flushedCurrentTest = false;
};
} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
	::testing::TestEventListeners& listeners =
		::testing::UnitTest::GetInstance()->listeners();
	::testing::TestEventListener* defaultPrinter =
		listeners.Release(listeners.default_result_printer());
	if (defaultPrinter) {
		logging_test::setLogFlushHandledByResultPrinter(true);
		listeners.Append(new BufferedResultPrinter(defaultPrinter));
	}
    DirectoryManager::findDirectories();
    int exitCode = RUN_ALL_TESTS();
	// printCounts();
	return exitCode;
}
