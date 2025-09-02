#ifndef parsedCircuitLoader_h
#define parsedCircuitLoader_h

#include "backend/circuit/circuitManager.h"
#include "backend/circuit/parsedCircuit.h"
#include "circuitFileManager.h"

class ParsedCircuitLoader {
public:
	ParsedCircuitLoader(CircuitFileManager* circuitFileManager, CircuitManager* circuitManager) : circuitFileManager(circuitFileManager), circuitManager(circuitManager) {}
	virtual ~ParsedCircuitLoader() {}

	virtual std::vector<circuit_id_t> load(const std::string& path) = 0;

	circuit_id_t loadParsedCircuit(ParsedCircuit& parsedCircuit) {
        return circuitFileManager->loadParsedCircuit(parsedCircuit);
    }

protected:
	CircuitManager* circuitManager;
	CircuitFileManager* circuitFileManager;
};

#endif /* parsedCircuitLoader_h */
