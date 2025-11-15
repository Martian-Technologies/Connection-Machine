#include "./testCaseFileManager.h"
#include "backend/evaluator/simulator/logicState.h"
#include "logging/logging.h"

std::optional<logic_state_t> stringToLogicState(const std::string& str) {
    if (str == "LOW" || str == "L") return (logic_state_t)0;
    if (str == "HIGH" || str == "H") return (logic_state_t)1;
    if (str == "FLOATING" || str == "Z") return (logic_state_t)2;
    if (str == "UNDEFINED" || str == "X") return (logic_state_t)3;
    return std::nullopt;
}

std::optional<CircuitTestCase> TestCaseFileManager::getCircuitTestCaseFromFilePath(const std::string &path) {
    logInfo("Parsing test file (.tst)", "TestCaseFileManager");
	std::ifstream inputFile(path, std::ios::in | std::ios::binary);
	if (!inputFile.is_open()) {
		logError("Couldn't open file at path: " + path, "TestCaseFileManager");
		return std::nullopt;
	}

    std::string token;
    inputFile >> token;

    unsigned int version;
    const unsigned int latestVersion = 0;
    if (token == "version_0") {
        version = 0;
    }
    else {
        logError("Invalid test case file version: {}", "TestCaseFileManager", token);
        return std::nullopt;
    }

    while (inputFile >> token) {
        
    }

    return std::nullopt;
}