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

		nlohmann::json dumpState() const {
			nlohmann::json fileDataJson;
			fileDataJson["fileLocation"] = "fileLocation";
			fileDataJson["UUIDs"] = nlohmann::json::array();
			for (const auto& UUID : UUIDs) {
				fileDataJson["UUIDs"].push_back(UUID);
			}
			fileDataJson["lastSavedEdit"] = lastSavedEdit;
			return fileDataJson;
		}
	};

	CircuitFileManager(CircuitManager& circuitManager);

    std::vector<circuit_id_t> loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path, const std::string& UUID);
    bool save(const std::string& UUID);
    bool saveFile(const std::string& path);
    // bool saveAllDependencies(const std::string& UUID);

    // bool saveAsMultiFile(const std::unordered_set<std::string>& UUIDs, const std::string& fileLocation);
    // bool saveAsNewProject(const std::unordered_set<std::string>& UUIDs, const std::string& fileLocationPrefix);

	void setSaveFilePath(const std::string& UUID, std::string fileLocation, bool addDotCir = true);

	const std::string* getSavePath(const std::string& UUID) const;

	const std::map<std::string, FileData>& getAllFiles() const { return filePathToFile; }
	const FileData* getFileDataFromPath(std::string path) const;
	const FileData* getFileDataFromUUID(std::string UUID) const;

	nlohmann::json dumpState() const;

private:
	FileData* setSaveFilePathAndGetFileData(const std::string& UUID, std::string fileLocation, bool addDotCir = true);
	circuit_id_t loadParsedCircuit(ParsedCircuit& parsedCircuit);

	CircuitManager& circuitManager;
	std::map<std::string, FileData> filePathToFile;
	std::map<std::string, std::string> UUIDToFilePath;
};

#endif /* circuitFileManager_h */
