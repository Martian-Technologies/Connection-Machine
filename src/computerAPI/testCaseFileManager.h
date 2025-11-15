#ifndef testCaseFileManager_h
#define testCaseFileManager_h

#include "backend/circuitTestCase/circuitTestCase.h"

namespace TestCaseFileManager {
    std::optional<CircuitTestCase> getCircuitTestCaseFromFilePath(const std::string& path);
}

#endif /* testCaseFileManager_h */