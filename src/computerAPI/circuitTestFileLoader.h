#ifndef circuitTestFileLoader_h
#define circuitTestFileLoader_h

#include "backend/circuitTests/circuitTestGroup.h"

class Backend;

namespace CircuitTestFileLoader {
    std::optional<CircuitTestGroup> getCircuitTestGroupFromFilePath(const std::string& path, Backend& backend);
    std::optional<CircuitTestGroup> getCircuitTestGroupFromTruthTableFilePath(std::ifstream& inputFile, Backend& backend);

    bool saveToTruthTableFile(const std::string& path, CircuitTestGroup& testGroup);
    bool saveToFile(const std::string& path, CircuitTestGroup& testGroup);
}

#endif /* circuitTestFileLoader_h */