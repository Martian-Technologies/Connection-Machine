#include "version.h"

int parseIntWithJunk(const std::string& str) {
	std::string str2 = str;
	size_t i = 0;
	while (i < str2.size()) {
		try {
			int value = std::stoi(str2);
			return value;
		} catch (const std::invalid_argument&) {
			str2 = str2.substr(0, str2.size() - 1);
		} catch (const std::out_of_range&) {
			return 0;
		}
	}
	return 0;
}

Version parseVersionString(const std::string& versionStr) {
	Version version{ 0, 0, 0 };
	size_t firstDot = versionStr.find('.');
	size_t secondDot = versionStr.find('.', firstDot + 1);

	if (firstDot != std::string::npos) {
		version.major = parseIntWithJunk(versionStr.substr(0, firstDot));
		if (secondDot != std::string::npos) {
			version.minor = parseIntWithJunk(versionStr.substr(firstDot + 1, secondDot - firstDot - 1));
			version.patch = parseIntWithJunk(versionStr.substr(secondDot + 1));
		} else {
			version.minor = parseIntWithJunk(versionStr.substr(firstDot + 1));
		}
	} else {
		version.major = parseIntWithJunk(versionStr);
	}
	return version;
}

Version getCurrentVersion() { return parseVersionString(PROJECT_VERSION); }
