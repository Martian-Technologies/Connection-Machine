#include "cm.h"

extern "C" {

const char* UUID = "8a92b940-456a-4d81-bc40-1f6e8bef4464";
const char* name = "And Gate";
const char* defaultParameters = "(\"size\": 1)";

bool generateCircuit() {
	int size = getParameter("size");

	BlockType BlockType_AND = getPrimitiveType("AND");
	BlockType BlockType_SWITCH = getPrimitiveType("SWITCH");
	BlockType BlockType_LIGHT = getPrimitiveType("LIGHT");

	setSize(2, size);

	block_id_t light = createBlock(BlockType_LIGHT);
	addConnectionOutput(1, 0, light, 0);

	block_id_t andGate = createBlock(BlockType_AND);

	createConnection(andGate, 1, light, 0);

	for (int i = 0; i < size; i++) {
		block_id_t input = createBlock(BlockType_SWITCH);
		addConnectionInput(0, i, input, 0);
		createConnection(input, 0, andGate, 0);
	}

	return true;
}

}