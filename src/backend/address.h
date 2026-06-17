#ifndef address_h
#define address_h

#include "position/position.h"

class Address {
public:
	Address() { }
	Address(Position position) : addresses({ position }) { }
	inline int size() const { return addresses.size(); }
	inline Position getPosition(int index) const { return addresses[index]; }

	inline void appendPosition(Position position) { addresses.push_back(position); }
	inline void prependPosition(Position position) { addresses.insert(addresses.begin(), position); }

	Address popTopPosition() const {
		Address newAddress;
		newAddress.addresses = std::vector<Position>(addresses.begin() + 1, addresses.end());
		return newAddress;
	}
	Address popBottomPosition() const {
		Address newAddress;
		newAddress.addresses = std::vector<Position>(addresses.begin(), addresses.end() - 1);
		return newAddress;
	}

	std::string toString() const {
		std::string result;
		for (const auto& pos : addresses) {
			if (!result.empty()) {
				result += ".";
			}
			result += pos.toString();
		}
		return result;
	}

private:
	std::vector<Position> addresses;
};

#endif /* address_h */
