#ifndef evalCircuitContainer_h
#define evalCircuitContainer_h

#include "backend/address.h"
#include "evalCircuit.h"
#include "idProvider.h"

struct EvalPosition {
	Position position;
	eval_circuit_id_t evalCircuitId;

	inline EvalPosition()
		: position(0, 0), evalCircuitId(0) {}

	inline EvalPosition(Position position, eval_circuit_id_t evalCircuitId)
		: position(position), evalCircuitId(evalCircuitId) {}

	inline std::string toString() const {
		return position.toString() + " in evalCircuitId " + std::to_string(evalCircuitId);
	}

	inline bool operator==(const EvalPosition& other) const {
		return position == other.position && evalCircuitId == other.evalCircuitId;
	}
};

template<>
struct std::hash<EvalPosition> {
	inline std::size_t operator()(const EvalPosition& ep) const noexcept {
		std::size_t h1 = std::hash<Position>()(ep.position);
		std::size_t h2 = std::hash<eval_circuit_id_t>()(ep.evalCircuitId);
		return h1 ^ (h2 << 1);
	}
};

class EvalCircuitContainer {
public:
	EvalCircuitContainer() = default;
	EvalCircuitContainer(const EvalCircuitContainer&) = delete;
	EvalCircuitContainer& operator=(const EvalCircuitContainer&) = delete;
	eval_circuit_id_t addCircuit(eval_circuit_id_t parentEvalId, circuit_id_t circuitId);
	void removeCircuit(eval_circuit_id_t evalCircuitId);
	std::optional<CircuitNode> getNode(EvalPosition pos) const noexcept;
	std::optional<CircuitNode> getNode(Position pos, eval_circuit_id_t evalCircuitId) const noexcept;
	EvalCircuit* getCircuit(eval_circuit_id_t evalCircuitId) const noexcept;

	inline size_t size() const noexcept {
		return circuits.size();
	}
	inline bool empty() const noexcept {
		return circuits.empty();
	}

	std::optional<eval_circuit_id_t> traverse(const Address& address) const;
	std::optional<eval_circuit_id_t> traverse(eval_circuit_id_t startingPoint, const Address& address) const;
	eval_circuit_id_t traverseToTopLevelIC(const Address& address) const;
	eval_circuit_id_t traverseToTopLevelIC(eval_circuit_id_t startingPoint, const Address& address) const;

	std::optional<eval_circuit_id_t> getCircuitId(eval_circuit_id_t evalCircuitId) const noexcept;

private:
	std::vector<EvalCircuit*> circuits;
	IdProvider<eval_circuit_id_t> evalCircuitIdProvider;
};

#endif /* evalCircuitContainer_h */
