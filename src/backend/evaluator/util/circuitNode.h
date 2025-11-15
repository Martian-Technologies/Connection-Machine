#ifndef circuitNode_h
#define circuitNode_h

#include "../evalDefs.h"

class CircuitNode {
public:
	static CircuitNode fromMiddle(middle_id_t id) {
		return CircuitNode(id.get() << 1);
	}
	static CircuitNode fromIC(eval_circuit_id_t id) {
		return CircuitNode((id.get() << 1) | 1);
	}
	inline bool isIC() const { return (id_and_type & 1) == 1; }
	// inline unsigned int getId() const { return id_and_type >> 1; }
	inline middle_id_t getMiddleId() const {
		assert(!isIC() && "Attempted to get middle ID from an IC CircuitNode");
		return middle_id_t(id_and_type >> 1);
	}
	inline eval_circuit_id_t getEvalCircuitId() const {
		assert(isIC() && "Attempted to get eval circuit ID from a middle CircuitNode");
		return eval_circuit_id_t(id_and_type >> 1);
	}
	inline bool operator==(const CircuitNode & other) const {
		return id_and_type == other.id_and_type;
	}
	inline auto operator<=>(const CircuitNode& other) const {
		return id_and_type <=> other.id_and_type;
	}
	inline std::string toString() const {
		return isIC() ? "IC(" + std::to_string(getEvalCircuitId()) + ")" : "mid(" + std::to_string(getMiddleId()) + ")";
	}
	nlohmann::json dumpState() const {
		nlohmann::json stateJson;
		if (isIC()) {
			stateJson["type"] = "IC";
			stateJson["evalCircuitId"] = getEvalCircuitId().get();
		} else {
			stateJson["type"] = "middle";
			stateJson["middleId"] = getMiddleId().get();
		}
		return stateJson;
	}
private:
	explicit CircuitNode(unsigned int value) : id_and_type(value) {}
	unsigned int id_and_type;
};

namespace std {
	template<>
	struct hash<CircuitNode> {
		size_t operator()(const CircuitNode& node) const noexcept {
			return std::hash<unsigned int>()(node.isIC() ? (node.getEvalCircuitId().get() << 1) | 1 : (node.getMiddleId().get() << 1));
		}
	};
}

#endif /* circuitNode_h */
