#ifndef fuzzRunner_h
#define fuzzRunner_h

#include "fuzzTestcase.h"
#include "backend/evaluator/evalDefs.h"
#include "backend/circuit/circuitDefs.h"
#include "util/indexableUnorderedSet.h"

class Environment;

class FuzzRunner {
public:
	FuzzRunner(Environment& environment);

	bool applyEditAction(const FuzzEditAction& action);
	void applyTestAction(const FuzzTestAction& action);

private:
	Environment& environment;
	std::vector<SharedCircuit> circuits;
	std::vector<IndexableUnorderedSet<block_id_t>> blockIdsPerCircuit;
	SharedEvaluator testEvaluator;
	SharedEvaluator referenceEvaluator;

	SharedCircuit getCircuit(unsigned int index);
	IndexableUnorderedSet<block_id_t>& getBlockIdsForCircuit(unsigned int index);
	BlockType getBlockTypeFromFuzzIndex(int fuzzBlockTypeIndex);

	void resetReferenceEvaluator();
	void ensureReferenceEvaluatorExists();
};

#endif /* fuzzRunner_h */