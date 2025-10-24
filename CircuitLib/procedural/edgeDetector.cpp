#include "functions.h"

extern "C" {

const char* UUID = "abd7f5c1-2e89-4630-bf12-d596c812dde8";
const char* name = "Edge Detector";
const char* defaultParameters = "(\"pulse width\": 3, \"{1:R,2:F,3:B}\": 2)";

bool generateCircuit() {
	int pulse = getParameter("pulse width");
	int kind = getParameter("{1:R,2:F,3:B}");

	BlockType BlockType_AND = getPrimitiveType("AND");
	BlockType BlockType_NOR = getPrimitiveType("NOR");
	BlockType BlockType_SWITCH = getPrimitiveType("SWITCH");
	BlockType BlockType_LIGHT = getPrimitiveType("LIGHT");

	setSize(1, 1);

	if (pulse < 1) {
		logError("Pulse width should not be less than 1.");
		return false;
	}
 	++pulse;// x state edges
	block_id_t endGate;
	if (kind == 1)
		endGate = createBlockAtPosition(pulse+2, 0, 0, BlockType_AND);
	else if (kind == 2)
		endGate = createBlockAtPosition(pulse+2, 0, 0, BlockType_NOR);
	else if (kind == 3) {
		BlockType BlockType_XNOR = getPrimitiveType("XNOR");
		endGate = createBlockAtPosition(pulse+2, 0, 0, BlockType_XNOR);
	} else {
		logError("{1:R,2:F,3:B} should only be 1, 2, or 3 and nothing else!");
		return false;
	}
	block_id_t input = createBlockAtPosition(0, 0, 0, BlockType_SWITCH);
	addConnectionInput(0, 0, input, 0);
	createConnection(input, 0, endGate, 0);
	block_id_t wait = createBlockAtPosition(1, 1, 0, BlockType_NOR);
	createConnection(input, 0, wait, 0);
	for (int i = 0; i < pulse; i++) {
		block_id_t tmp = createBlockAtPosition(1, 1+i, 0, BlockType_AND);
		createConnection(wait, 1, tmp, 0);
		wait = tmp;
	}
	createConnection(wait, 1, endGate, 0);
	block_id_t output = createBlockAtPosition(pulse+3, 0, 0, BlockType_LIGHT);
	createConnection(endGate, 1, output, 0);
	addConnectionOutput(0, 0, output, 0);

	return true;
}

}
