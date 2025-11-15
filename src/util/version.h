struct Version {
	int major;
	int minor;
	int patch;
	std::string toString() const { return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch); }
	auto operator<=>(const Version&) const = default;
};

Version parseVersionString(const std::string& versionStr);
Version getCurrentVersion();
