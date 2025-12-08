#ifndef uuid_h
#define uuid_h

static std::string generate_uuid_v4() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> distrib(0, 15);
	std::uniform_int_distribution<> distrib2(8, 11);

	std::stringstream ss;
	for (int i = 0; i < 36; ++i) {
	    if (i == 8 || i == 13 || i == 18 || i == 23) {
	        ss << "-";
	    } else if (i == 14) {
	        ss << "4";
	    } else if (i == 19) {
	        ss << std::hex << distrib2(gen);
	    } else {
	        ss << std::hex << distrib(gen);
	    }
	}
	return ss.str();
}

#endif /* uuid_h */
