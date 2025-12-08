#ifndef fuzzRunner_h
#define fuzzRunner_h

#include "fuzzTestcase.h"
#include "backend/evaluator/evalDefs.h"
#include "backend/circuit/circuitDefs.h"
#include "util/indexableUnorderedSet.h"

class Environment;

class FuzzRunner {
public:
	FuzzRunner(Environment& environment, std::vector<FuzzBlockType> blockTypesUsed);

	bool applyEditAction(const FuzzEditAction& action);
	void applyTestAction(const FuzzTestAction& action);

	bool checkEvaluatorsMatch();

	FuzzEditAction createRandomEditAction(std::mt19937_64& gen);
	SetBlockStateAction createRandomSetStateAction(std::mt19937_64& gen);

private:
	Environment& environment;
	std::vector<SharedCircuit> circuits;
	std::vector<IndexableUnorderedSet<block_id_t>> blockIdsPerCircuit;
	std::vector<FuzzBlockType> blockTypesUsed;
	std::vector<unsigned int> circuitIndices = { 0 };
	SharedEvaluator testEvaluator;
	SharedEvaluator referenceEvaluator;
	std::vector<simulator_id_t> testEvaluatorBlockSimIds;
	std::vector<simulator_id_t> referenceEvaluatorBlockSimIds;
	bool simIdsInitialized = false;

	std::vector<BlockType> blockTypesUsedConverted;

	SharedCircuit getCircuit(unsigned int index);
	IndexableUnorderedSet<block_id_t>& getBlockIdsForCircuit(unsigned int index);
	BlockType getBlockTypeFromFuzzIndex(int fuzzBlockTypeIndex);

	void resetReferenceEvaluator();
	void ensureReferenceEvaluatorExists();

	void resetSimIds();
	void ensureSimIdsInitialized();

	void populateBlockTypesUsedConverted();
	BlockType getBlockTypeFromFuzzBlockType(const FuzzBlockType& fuzzBlockType);

	void addSimulatorIds(std::vector<simulator_id_t>& destination, Address rootAddress, SharedEvaluator evaluator, unsigned int circuitIndex);

	Address pickRandomAddress(std::mt19937_64& gen, Address root, unsigned int circuitIndex);
};

#endif /* fuzzRunner_h */
