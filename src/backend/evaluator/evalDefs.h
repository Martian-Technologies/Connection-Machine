#ifndef evalDefs_h
#define evalDefs_h

#include "backend/address.h"
#include "backend/blockData/blockData.h"

DECLARE_ID_TYPE(simulator_id_t, unsigned int);
DECLARE_ID_TYPE(eval_gate_id, unsigned int);

typedef unsigned int EvalGateType;

inline EvalGateType getEvalGateType(BlockType blockType) { return (EvalGateType)blockType; }
inline BlockType getBlockType(EvalGateType evalGateType) { return (BlockType)evalGateType; }

struct EvalConnectionPoint {
	EvalConnectionPoint(eval_gate_id gateId, connection_end_id_t connectionEndId) : gateId(gateId), connectionEndId(connectionEndId) { }
	static inline EvalConnectionPoint null() { return EvalConnectionPoint(0, 0); }
	bool isNull() const { return gateId == 0; }

	eval_gate_id gateId;
	connection_end_id_t connectionEndId;

	bool operator==(EvalConnectionPoint other) const { return gateId == other.gateId && connectionEndId == other.connectionEndId; }

	std::string toString() const { return "(" + std::to_string(gateId) + ", " + std::to_string(connectionEndId) + ")"; }
};

template <>
struct std::hash<EvalConnectionPoint> {
	inline std::size_t operator()(EvalConnectionPoint evalConnectionPoint) const noexcept {
		std::size_t h1 = std::hash<eval_gate_id>{}(evalConnectionPoint.gateId);
		std::size_t h2 = std::hash<connection_end_id_t>{}(evalConnectionPoint.connectionEndId);
		std::size_t seed = h1 + 0x9e3779b9;
		return h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
};

template <>
struct fmt::formatter<EvalConnectionPoint> : fmt::formatter<std::string> {
	auto format(EvalConnectionPoint connectionPoint, format_context& ctx) const /* GCOVR_EXCL_FUNCTION */ {
		return formatter<std::string>::format(
			"(" + fmt::to_string(connectionPoint.gateId) + ", " + fmt::to_string(connectionPoint.connectionEndId) + ")",
			ctx
		);
	}
};

struct EvalConnection {
	EvalConnection(eval_gate_id gateAId, connection_end_id_t gateAConnectionId, eval_gate_id gateBId, connection_end_id_t gateBConnectionId) :
		EvalConnection(EvalConnectionPoint(gateAId, gateAConnectionId), EvalConnectionPoint(gateBId, gateBConnectionId)) { }
	EvalConnection(EvalConnectionPoint connectionPointA, EvalConnectionPoint connectionPointB) :
		connectionPointA(connectionPointA), connectionPointB(connectionPointB) { }

	EvalConnectionPoint connectionPointA;
	EvalConnectionPoint connectionPointB;

	bool operator==(const EvalConnection other) const {
		return (
			(connectionPointA == other.connectionPointA && connectionPointB == other.connectionPointB) ||
			(connectionPointA == other.connectionPointB && connectionPointB == other.connectionPointA)
		);
	}
};

template <>
struct std::hash<EvalConnection> {
	inline std::size_t operator()(EvalConnection evalConnection) const noexcept {
		std::size_t h1 = std::hash<EvalConnectionPoint>{}(evalConnection.connectionPointA);
		std::size_t h2 = std::hash<EvalConnectionPoint>{}(evalConnection.connectionPointB);
		if (h1 > h2) std::swap(h1, h2);
		std::size_t seed = h1 + 0x9e3779b9;
		return h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
};

template <>
struct fmt::formatter<EvalConnection> : fmt::formatter<std::string> {
	auto format(EvalConnection evalConnection, format_context& ctx) const /* GCOVR_EXCL_FUNCTION */ {
		return formatter<std::string>::format(
			"(" + fmt::to_string(evalConnection.connectionPointA) + ", " + fmt::to_string(evalConnection.connectionPointB) + ")",
			ctx
		);
	}
};

class Evaluator;

#endif /* evalDefs_h */
