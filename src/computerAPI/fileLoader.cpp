#include "fileLoader.h"

std::vector<char> readFileAsBytes(const std::filesystem::path& path) {
	// open file at end
	std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
		logError("Could not open file (" + path.string() + ")");
		return {};
    }

	// get size of file and return to start
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

	// read file as bytes
    std::vector<char> buffer(size);
	if (!file.read(buffer.data(), size)) {
		logError("Could not read file (" + path.string() + ")");
		return {};
	}

    return buffer;
}
