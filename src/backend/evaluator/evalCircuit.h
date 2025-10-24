#ifndef evalCircuit_h
#define evalCircuit_h

#include "backend/circuit/circuit.h"
#include "circuitNode.h"

class EvalCircuit {
public:
	EvalCircuit(eval_circuit_id_t id, eval_circuit_id_t parentEvalId, circuit_id_t circuitId)
		: id(id), parentEvalId(parentEvalId), circuitId(circuitId) {}
	EvalCircuit(const EvalCircuit&) = delete;
	EvalCircuit& operator=(const EvalCircuit&) = delete;
	EvalCircuit(EvalCircuit&&) = default;
	EvalCircuit& operator=(EvalCircuit&&) = default;
	std::optional<CircuitNode> getNode(Position pos) const noexcept;
	circuit_id_t getCircuitId() const noexcept {
		return circuitId;
	}
	void setNode(Position pos, CircuitNode node) {
		circuitNodes.insert(pos, node);
	}
	void removeNode(Position pos) {
		circuitNodes.remove(pos);
	}
	void moveNode(Position oldPos, Position newPos) {
		std::optional<CircuitNode> node = getNode(oldPos);
		if (node) {
			circuitNodes.remove(oldPos);
			circuitNodes.insert(newPos, node.value());
		} else {
			logError("Node at position {} not found", "EvalCircuit::moveNode", oldPos.toString());
		}
	}
	template<typename F>
	void forEachNode(F&& func) const {
		circuitNodes.forEach([&func](Position pos, const CircuitNode& node) {
			func(pos, node);
		});
	}
	bool isRoot() const noexcept {
		return parentEvalId == id;
	}
	eval_circuit_id_t getId() const noexcept {
		return id;
	}
	eval_circuit_id_t getParentEvalId() const noexcept {
		return parentEvalId;
	}
	std::optional<Position> getPosition(CircuitNode node) const noexcept {
		std::optional<Position> result = std::nullopt;
		circuitNodes.forEach([&](Position pos, CircuitNode n) {
			if (n == node) {
				result = pos;
			}
		});
		return result;
	}
private:
	eval_circuit_id_t id;
	eval_circuit_id_t parentEvalId;
	circuit_id_t circuitId;
	Sparse2dArray<CircuitNode> circuitNodes;
};

#endif /* evalCircuit_h */
