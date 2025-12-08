#include "failingCaseFinder.h"
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"
#include "backend/container/block/blockDefs.h"
#include "fuzzRunner.h"

std::unique_ptr<FuzzTestcase> FailingCaseFinder::findFailingCases(unsigned int maxAttempts, const std::vector<FuzzBlockType>& blockTypesUsed) {
	for (unsigned int attempt = 0; attempt < maxAttempts; ++attempt) {
		logInfo("Attempting to generate failing testcase (attempt {}/{})", "", attempt + 1, maxAttempts);
		std::unique_ptr<FuzzTestcase> failingCase = tryMakeFailingCase(blockTypesUsed);
		if (failingCase) {
			return failingCase;
		}
	}
	return nullptr;
}

std::unique_ptr<FuzzTestcase> FailingCaseFinder::tryMakeFailingCase(const std::vector<FuzzBlockType>& blockTypesUsed) {
	std::unique_ptr<FuzzTestcase> testcase = std::make_unique<FuzzTestcase>(blockTypesUsed);
	Environment environment(false);
	FuzzRunner fuzzRunner(environment, blockTypesUsed);
	const unsigned int maxEditActions = 100;
	const unsigned int maxTestActions = 50;
	std::mt19937_64 gen(std::random_device {}());
	for (unsigned int i = 0; i < maxEditActions; ++i) {
		FuzzEditAction editAction = fuzzRunner.createRandomEditAction(gen);
		bool success = fuzzRunner.applyEditAction(editAction);
		if (success) {
			testcase->addEditAction(editAction);
		}
	}
	return nullptr;
}
