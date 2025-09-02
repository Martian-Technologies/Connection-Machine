#ifndef circuitFileManager_h
#define circuitFileManager_h

#include "backend/circuit/circuitManager.h"
#include "backend/circuit/parsedCircuit.h"

class CircuitFileManager {
	friend class ParsedCircuitLoader;
public:
	struct FileData {
		FileData(const std::string& fileLocation) : fileLocation(fileLocation) {}
		FileData(const FileData&) = delete;
		FileData& operator==(const FileData&) = delete;
		std::string fileLocation;
        std::unordered_map<std::string, unsigned long long> lastSavedEdit; // only for circuits
		std::unordered_set<std::string> UUIDs;
	};

	CircuitFileManager(CircuitManager* circuitManager);

    std::vector<circuit_id_t> loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path, const std::string& UUID);
    bool save(const std::string& UUID);
    // bool saveAllDependencies(const std::string& UUID);

    // bool saveAsMultiFile(const std::unordered_set<std::string>& UUIDs, const std::string& fileLocation);
    // bool saveAsNewProject(const std::unordered_set<std::string>& UUIDs, const std::string& fileLocationPrefix);

	void setSaveFilePath(const std::string& UUID, const std::string& fileLocation);

	const std::string* getSavePath(const std::string&) const;

private:
	circuit_id_t loadParsedCircuit(ParsedCircuit& parsedCircuit);

	CircuitManager* circuitManager;
	std::map<std::string, FileData> filePathToFile;
	std::map<std::string, std::string> UUIDToFilePath;
};

#endif /* circuitFileManager_h */
