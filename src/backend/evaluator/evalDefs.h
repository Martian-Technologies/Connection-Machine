#ifndef evalDefs_h
#define evalDefs_h

#include "backend/position/position.h"

typedef unsigned int evaluator_id_t;
typedef unsigned int eval_circuit_id_t;
typedef unsigned int middle_id_t;
typedef unsigned int connection_port_id_t;
typedef unsigned int simulator_id_t;

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

class Evaluator;
typedef std::shared_ptr<class Evaluator> SharedEvaluator;

#endif /* evalDefs_h */
