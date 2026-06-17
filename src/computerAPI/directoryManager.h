#ifndef directoryManager_h
#define directoryManager_h

class DirectoryManager {
public:
	static void findDirectories();

	static std::filesystem::path getExecutablePath();
	static std::filesystem::path getBundlePath();

	// The resource data holds immutable data shipped with the application (images, shaders, etc)
	inline static const std::filesystem::path& getResourceDirectory() { return resourceDirectory; }
	// The working directory is the location of the open "project"
	inline static const std::filesystem::path& getProjectDirectory() { return projectDirectory; }
	// The config directory is the location that config files should placed (shared between "projects")
	inline static const std::filesystem::path& getConfigDirectory() { return configDirectory; }

	static std::string shortenPath(std::filesystem::path path);
	static std::filesystem::path extendPath(const std::string& path);

private:
	static std::filesystem::path resourceDirectory;
	static std::filesystem::path projectDirectory;
	static std::filesystem::path configDirectory;
};

#endif /* directoryManager_h */
