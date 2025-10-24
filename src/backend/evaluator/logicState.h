#ifndef logicState_h
#define logicState_h

enum class logic_state_t : unsigned char {
	LOW = 0,
	HIGH = 1,
	FLOATING = 2,
	UNDEFINED = 3
};

inline bool isHigh(const logic_state_t& state) {
	return state == logic_state_t::HIGH;
}

inline bool isLow(const logic_state_t& state) {
	return state == logic_state_t::LOW;
}

inline bool isValid(const logic_state_t& state) {
	return state == logic_state_t::HIGH || state == logic_state_t::LOW;
}

inline bool toBool(const logic_state_t& state) {
	return state == logic_state_t::HIGH;
}

inline logic_state_t fromBool(bool value) {
	return value ? logic_state_t::HIGH : logic_state_t::LOW;
}

inline std::string to_string(logic_state_t state) {
	switch (state) {
		case logic_state_t::LOW:
			return "L";
		case logic_state_t::HIGH:
			return "H";
		case logic_state_t::FLOATING:
			return "Z";
		case logic_state_t::UNDEFINED:
			return "X";
		default:
			return "UNKNOWN_STATE";
	}
}

#endif /* logicState_h */
