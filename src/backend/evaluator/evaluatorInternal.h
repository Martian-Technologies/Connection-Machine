#ifndef evaluatorInternal_h
#define evaluatorInternal_h

#include "backend/address.h"
#include "evalDefs.h"
#include "layers/layerRunner.h"
#include "backend/circuit/circuitDefs.h"

class CircuitManager;
class SubcircuitEvalLayer;

typedef std::vector<std::vector<EvalConnectionPoint>> VecVecEvalConnectionPoint;
typedef std::vector<EvalConnectionPoint> VecEvalConnectionPoint;

class EvaluatorInternal {
public:
	struct InternalPointData {
		InternalPointData(std::optional<EvalConnectionPoint> connectionPoint, BlockData::ConnectionData::PortType portType, unsigned int bitWith) :
			connectionPoint(connectionPoint), portType(portType), bitWith(bitWith) {}
		InternalPointData(BlockData::ConnectionData::PortType portType, unsigned int bitWith) : portType(portType), bitWith(bitWith) {}
		std::optional<EvalConnectionPoint> connectionPoint;
		BlockData::ConnectionData::PortType portType;
		unsigned int bitWith;
	};
	EvaluatorInternal(const Circuit& circuit, Evaluator& evaluator, const CircuitManager& circuitManager, DataUpdateEventManager::DataUpdateEventReceiver& receiver);
	void startEdit();
	void endEdit();
	void addBlock(Position position, Orientation orientation, BlockType blockType);
	void removeBlock(Position position, Orientation orientation, BlockType blockType);
	void moveBlock(Position curPosition, Orientation curOrientation, Position newPosition, Orientation newOrientation);
	void removeConnection(Position outputBlockPosition, Position outputPosition, Position inputBlockPosition, Position inputPosition);
	void createConnection(Position outputBlockPosition, Position outputPosition, Position inputBlockPosition, Position inputPosition);

	const LayerRunner& getLayerRunner() const { return layerRunner; }

	std::optional<std::pair<Position, Position>> mapFromTopConnectionPointToPointAndBlockPosition(EvalConnectionPoint bottomConnectionPoint) const;
	EvalConnectionPoint mapFromPositionToTopConnectionPoint(Position blockPosition) const;
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> mapFromAddressToBottomConnectionPoints(const Address& address) const;
	EvalConnectionPoint mapFromAddressToBottomConnectionPointForOtherEvals(const Address& address) const;
	EvalConnectionPoint mapFromTopConnectionPointToBottomConnectionPointForOtherEvals(EvalConnectionPoint topConnectionPoint) const;
	// get all the top connection points from bottom connections points for a certain eval at address with the output containing the mappings
	VecVecEvalConnectionPoint mapFromBottomConnectionPointsToTopConnectionPointsForOtherEvals(VecEvalConnectionPoint bottomConnectionPoint, Address address) const;
	// get all the top connection points from bottom connections points for a certain eval at address with the output containing the mappings
	VecVecEvalConnectionPoint mapFromBottomConnectionPointGroupsToTopConnectionPointsForOtherEvals(VecVecEvalConnectionPoint bottomConnectionPoint, Address address) const;

	EvalConnectionPoint getConnectionPointFromConnectionEndId(connection_end_id_t connectionEndId) const;
	bool isConnectionIdSinglePin(connection_end_id_t connectionEndId) const;

	const std::unordered_map<Position, std::pair<eval_gate_id, Orientation>>& getPositionRemapping() const { return positionRemapping; }
	const IdMap<eval_gate_id, std::pair<Position, Orientation>>& getPositionReverseRemapping() const { return positionReverseRemapping; }
	const std::map<connection_end_id_t, InternalPointData>& getPortToInternalPointMapping() const { return portToInternalPointMapping; }

	std::vector<std::pair<Position, circuit_id_t>> getSubcircuits() const;

	void addEvaluator(SubcircuitEvalLayer& evaluator) const { evaluatorsUsingThisEvaluator[&evaluator] += 1; }
	void removeEvaluator(SubcircuitEvalLayer& evaluator) const {
		auto iter = evaluatorsUsingThisEvaluator.find(&evaluator);
		assert(iter != evaluatorsUsingThisEvaluator.end());
		if (iter->second == 1) evaluatorsUsingThisEvaluator.erase(iter);
		else iter->second -= 1;
	}

private:
	void sendPortUpdate(connection_end_id_t connectionEndId, EvalConnectionPoint preConnectionPoint, EvalConnectionPoint postConnectionPoint);

	// std::set<Position> positionsTo; nothing to do
	IdProvider<eval_gate_id> evalGateIdProvider;
	std::unordered_map<Position, std::pair<eval_gate_id, Orientation>> positionRemapping;
	IdMap<eval_gate_id, std::pair<Position, Orientation>> positionReverseRemapping;
	std::map<connection_end_id_t, InternalPointData> portToInternalPointMapping;
	mutable std::unordered_map<SubcircuitEvalLayer*, unsigned int> evaluatorsUsingThisEvaluator;
	LayerRunner layerRunner;
	const CircuitManager& circuitManager;
	const Circuit& circuit;
};

// void printCounts();

#endif /* evaluatorInternal_h */
