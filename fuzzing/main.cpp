#include "failingCaseFinder.h"
#include "testcaseMinifier.h"
#include "fuzzTestcase.h"

#include "computerAPI/directoryManager.h"

int main(int argc, char** argv) {
	DirectoryManager::findDirectories();
	setupFuzzLogCapture();
	std::filesystem::create_directories(DirectoryManager::getConfigDirectory() / "fuzzing");
	std::string failingTestcasePath = (DirectoryManager::getConfigDirectory() / "fuzzing" / "failing_testcase.json").string();
	FailingCaseFinder finder;
	std::vector<FuzzBlockType> blockTypesUsed = {
		// FuzzPrimitiveType { BlockType::AND },
		// FuzzPrimitiveType { BlockType::OR },
		// FuzzPrimitiveType { BlockType::XOR },
		// FuzzPrimitiveType { BlockType::NAND },
		// FuzzPrimitiveType { BlockType::NOR },
		// FuzzPrimitiveType { BlockType::XNOR },
		// FuzzPrimitiveType { BlockType::BUFFER },
		// FuzzPrimitiveType { BlockType::NOT },
		// FuzzPrimitiveType { BlockType::TRISTATE_BUFFER },
		// FuzzPrimitiveType { BlockType::JUNCTION },
		// FuzzPrimitiveType { BlockType::JUNCTION_L },
		// FuzzPrimitiveType { BlockType::JUNCTION_H },
		// FuzzPrimitiveType { BlockType::JUNCTION_X },
		// FuzzPrimitiveType { BlockType::LIGHT },
		// FuzzPrimitiveType { BlockType::SWITCH },
		// FuzzPrimitiveType { BlockType::BUTTON },

		FuzzPrimitiveType { BlockType::AND },
		FuzzPrimitiveType { BlockType::OR },
		FuzzPrimitiveType { BlockType::XOR },
		FuzzPrimitiveType { BlockType::NAND },
		FuzzPrimitiveType { BlockType::NOR },
		FuzzPrimitiveType { BlockType::XNOR },
		FuzzPrimitiveType { BlockType::BUFFER },
		FuzzPrimitiveType { BlockType::NOT },
		FuzzPrimitiveType { BlockType::JUNCTION },
		FuzzPrimitiveType { BlockType::JUNCTION_L },
		FuzzPrimitiveType { BlockType::JUNCTION_H },
		FuzzPrimitiveType { BlockType::JUNCTION_X },
		FuzzPrimitiveType { BlockType::TRISTATE_BUFFER },
		FuzzPrimitiveType { BlockType::BUTTON },
		FuzzPrimitiveType { BlockType::TICK_BUTTON },
		FuzzPrimitiveType { BlockType::SWITCH },
		FuzzPrimitiveType { BlockType::CONSTANT_OFF },
		FuzzPrimitiveType { BlockType::CONSTANT_ON },
		FuzzPrimitiveType { BlockType::CONSTANT_Z },
		FuzzPrimitiveType { BlockType::CONSTANT_X },
		FuzzPrimitiveType { BlockType::LIGHT },


		FuzzPrimitiveType { BlockType::CONSTANT_OFF },
		FuzzPrimitiveType { BlockType::CONSTANT_ON },
		FuzzPrimitiveType { BlockType::CONSTANT_Z },
		FuzzPrimitiveType { BlockType::CONSTANT_X },
		FuzzBusType { 2, 1, 1, 2 },
		FuzzBusType { 4, 1, 1, 4 },
		FuzzBusType { 2, 1, 2, 4 },
		FuzzBusType { 8, 1, 1, 8 },
		FuzzBusType { 4, 1, 2, 8 },
		FuzzBusType { 2, 1, 4, 8 },
		FuzzBusType { 2, 4, 4, 2 },
		FuzzCustomCircuitType { "circuits/passthrough.cir" },
		FuzzCustomCircuitType { "circuits/full_adder.cir" },
		FuzzCustomCircuitType { "circuits/bus_tristate_2.cir" },
		FuzzCustomCircuitType { "circuits/nested_passthrough.cir" }
	};
	std::unique_ptr<FuzzTestcase> testcase = finder.findFailingCases(1000, blockTypesUsed);
	if (testcase) {
		logInfo("Found failing testcase with {} edit actions and {} test actions", "", testcase->getEditActions().size(), testcase->getTestActions().size());
		TestcaseMinifier minifier;
		FuzzTestcase minifiedTestcase = minifier.minifyTestcase(*testcase);
		logInfo("Minified testcase has {} edit actions and {} test actions", "", minifiedTestcase.getEditActions().size(), minifiedTestcase.getTestActions().size());
		std::string serialized = minifiedTestcase.serialize();
		std::ofstream outFile(failingTestcasePath);
		outFile << serialized;
		outFile.close();
		logInfo("Serialized failing testcase written to {}", "", failingTestcasePath);
	} else {
		logInfo("No failing testcase found");
	}
	return 0;
}
