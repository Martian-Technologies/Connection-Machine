#ifndef evaluatorManager_h
#define evaluatorManager_h

#include "backend/circuit/circuit.h"
#include "evalDefs.h"

class CircuitManager;
class DataUpdateEventManager;

class EvaluatorManager {
public:
	EvaluatorManager(DataUpdateEventManager* dataUpdateEventManager) : dataUpdateEventManager(dataUpdateEventManager) {}

	inline const std::map<evaluator_id_t, SharedEvaluator>& getEvaluators() const { return evaluators; }

	inline SharedEvaluator getEvaluator(evaluator_id_t id) {
		auto iter = evaluators.find(id);
		if (iter == evaluators.end()) return nullptr;
		return iter->second;
	}
	inline const SharedEvaluator getEvaluator(evaluator_id_t id) const {
		auto iter = evaluators.find(id);
		if (iter == evaluators.end()) return nullptr;
		return iter->second;
	}

	evaluator_id_t createNewEvaluator(CircuitManager& circuitManager, circuit_id_t circuitId);
	inline void destroyEvaluator(evaluator_id_t id) {
		auto iter = evaluators.find(id);
		if (iter != evaluators.end()) {
			evaluators.erase(iter);
		}
	}

	typedef std::map<evaluator_id_t, SharedEvaluator>::iterator iterator;
	typedef std::map<evaluator_id_t, SharedEvaluator>::const_iterator const_iterator;

	inline iterator begin() { return evaluators.begin(); }
	inline iterator end() { return evaluators.end(); }
	inline const_iterator begin() const { return evaluators.begin(); }
	inline const_iterator end() const { return evaluators.end(); }

	void applyDiff(DifferenceSharedPtr difference, circuit_id_t circuitId);

private:
	evaluator_id_t getNewEvaluatorId() { return ++lastId; }

	DataUpdateEventManager* dataUpdateEventManager;

	evaluator_id_t lastId = 0;
	std::map<evaluator_id_t, SharedEvaluator> evaluators;
};

#endif /* evaluatorManager_h */
