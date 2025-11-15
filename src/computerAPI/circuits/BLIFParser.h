#ifndef BLIFParser_h
#define BLIFParser_h

#include "backend/circuit/parsedCircuit.h"
#include "backend/circuit/circuit.h"
#include "parsedCircuitLoader.h"
#include "circuitFileManager.h"

class BLIFParser: public ParsedCircuitLoader {
public:
    BLIFParser(CircuitFileManager& circuitFileManager, CircuitManager& circuitManager) : ParsedCircuitLoader(circuitFileManager, circuitManager) {}
    std::vector<circuit_id_t> load(const std::string& path) override;
    // bool save(const CircuitFileManager::FileData& fileData, bool compress);

private:
	struct BLIFParsedCircuitData {
		SharedParsedCircuit parsedCircuit;
		std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>> customBlocksToAdd;
		std::unordered_map<std::string, ConnectionEnd> nameToConnectionEnd;
		std::vector<std::pair<std::string, ConnectionEnd>> connectionsToMake;
		LinearIdProvider<connection_end_id_t> endIdProvider {0};
		coordinate_t inPortY;
		coordinate_t outPortY;
		block_id_t blockIdCounter;
		BlockType type = BlockType::NONE;
	};

	std::map<std::string, BLIFParsedCircuitData> BLIFParsedCircuits;
	std::unordered_set<std::string> importedFiles;
};

#endif /* BLIFParser_h */
