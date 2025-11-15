#ifndef evalDefs_h
#define evalDefs_h

#include "backend/position/position.h"
#include "util/id.h"

DECLARE_ID_TYPE(evaluator_id_t, unsigned int);
DECLARE_ID_TYPE(eval_circuit_id_t, unsigned int);
DECLARE_ID_TYPE(middle_id_t, unsigned int);
DECLARE_ID_TYPE(simulator_id_t, unsigned int);

enum class SimulatorMappingUpdateType {
	BLOCK,
	PIN
};

struct SimulatorMappingUpdate {
	Position portPosition;
	std::variant<simulator_id_t, std::vector<simulator_id_t>> simulatorIds;
	SimulatorMappingUpdateType type;
};

typedef std::function<void(const std::vector<SimulatorMappingUpdate>&)> SimulatorMappingUpdateListenerFunction;

struct SimulatorMappingUpdateListener {
	eval_circuit_id_t evalCircuitId;
	std::function<void(const std::vector<SimulatorMappingUpdate>&)> callback;
};

enum class Direction {
	IN,
	OUT,
};
inline Direction operator!(Direction dir) {
	return (dir == Direction::IN) ? Direction::OUT : Direction::IN;
}

class Evaluator;
typedef std::shared_ptr<class Evaluator> SharedEvaluator;

#endif /* evalDefs_h */
