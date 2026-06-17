#ifndef fileLoader_h
#define fileLoader_h

// TODO - not sure if this should be an optional, and be the responsibility of the caller to throw
std::vector<char> readFileAsBytes(const std::filesystem::path& path);
std::pair<char*, unsigned long long> readFileAsBytes_noVec(const std::filesystem::path& path);

#endif /* fileLoader_h */
