#include "directoryManager.h"

#include <cpplocate/cpplocate.h>
#include <cfgpath.h>

std::filesystem::path DirectoryManager::resourceDirectory("");
std::filesystem::path DirectoryManager::projectDirectory("");
std::filesystem::path DirectoryManager::configDirectory("");

void DirectoryManager::findDirectories() {
	char cfgdir[MAX_PATH];
	cfgpath::get_user_config_folder(cfgdir, sizeof(cfgdir), "ConnectionMachine");
	if (cfgdir[0] == 0) {
		logError("Could not find a config directory.", "DirectoryManager");
	}
	configDirectory = std::string(cfgdir);
	std::filesystem::create_directories(configDirectory);

	// check for resources directory relative to executable
	std::filesystem::path relativeToExecutable = getExecutablePath().parent_path() / "resources";
	if (std::filesystem::exists(relativeToExecutable)) {
		resourceDirectory = relativeToExecutable;
		logInfo("Found resource directory relative to executable at ({})", "DirectoryManager", resourceDirectory.generic_string());
	} else {
#ifdef __APPLE__
		// check relative to macOS bundle
		std::filesystem::path relativeToBundle = getBundlePath().parent_path() / "resources";
		if (std::filesystem::exists(relativeToBundle)) {
			resourceDirectory = relativeToBundle;
			logInfo("Found resource directory relative to MacOS bundle at ({})", "DirectoryManager", resourceDirectory.generic_string());
		} else {
#endif
			// check for resources directory relative to executable's parent
			std::filesystem::path relativeToExecutableParent = getExecutablePath().parent_path().parent_path() / "resources";
			if (std::filesystem::exists(relativeToExecutableParent)) {
				resourceDirectory = relativeToExecutableParent;
				logInfo("Found resource directory relative to executable's parent at ({})", "DirectoryManager", resourceDirectory.generic_string());
			} else {
				// check for resources directory relative to working directory
				std::filesystem::path relativeToWorkingDirectory = "resources";
				if (std::filesystem::exists(relativeToWorkingDirectory)) {
					resourceDirectory = relativeToWorkingDirectory;
					logWarning("Found resource directory strictly relative to working directory, this is probably not intended.", "DirectoryManager", resourceDirectory.generic_string());
				} else {
					logFatalError("Could not find resource directory. Make sure you are executing the program from the top of the source tree.", "DirectoryManager");
				}
			}
#ifdef __APPLE__
		}
#endif
	}
	logInfo("Found config directory at ({})", "DirectoryManager", configDirectory.string());
}

std::filesystem::path DirectoryManager::getExecutablePath() {
	return cpplocate::getExecutablePath();
}

std::filesystem::path DirectoryManager::getBundlePath() {
	return cpplocate::getBundlePath();
}

std::string DirectoryManager::shortenPath(std::filesystem::path path) {
	path = std::filesystem::weakly_canonical(path);
	auto base = getResourceDirectory();
	auto baseIter = base.begin();
    auto pathIter = path.begin();
    for (; baseIter != base.end() && pathIter != path.end(); ++baseIter, ++pathIter) {
        if (*baseIter != *pathIter) return path.generic_string();
    }
	if (baseIter != base.end()) return path.generic_string();

	std::string shortenedPath = "@ResourceDirectory";
	for (; pathIter != path.end(); pathIter++) shortenedPath += "/" + (*pathIter).generic_string();
	return shortenedPath;
}

std::filesystem::path DirectoryManager::extendPath(const std::string& path) {
	if (path.starts_with("@ResourceDirectory")) {
        std::filesystem::path extendedPath = getResourceDirectory();
		extendedPath /= path.c_str() + sizeof("@ResourceDirectory");
        return extendedPath;
	}
	return path;
}
