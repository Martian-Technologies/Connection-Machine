#include "cm.h"

extern "C" {

const char* UUID = "4c174b4c-584f-4480-9f29-97bab418d073";
const char* name = "Adder";
const char* defaultParameters = "(\"size\": 1)";

bool generateCircuit() {
	int size = getParameter("size");

	BlockType BlockType_AND = getPrimitiveType("AND");
	BlockType BlockType_XOR = getPrimitiveType("XOR");
	BlockType BlockType_OR = getPrimitiveType("XOR");
	BlockType BlockType_SWITCH = getPrimitiveType("SWITCH");
	BlockType BlockType_LIGHT = getPrimitiveType("LIGHT");

	setSize(2, size*2);

	block_id_t C;
	for (int i = 0; i < size; i++) {
		block_id_t A = createBlockAtPosition(-1, i*2, 0, BlockType_SWITCH); // A
		block_id_t B = createBlockAtPosition(-2, i*2, 0, BlockType_SWITCH); // B
		addConnectionInput(0, i, A, 0);
		addConnectionInput(0, i+size, B, 0);
		block_id_t AxorB = createBlockAtPosition(0, i*2, 0, BlockType_XOR); // A ^ B
		createConnection(A, 0, AxorB, 0);
		createConnection(B, 0, AxorB, 0);
		block_id_t out = createBlockAtPosition(2, i*2, 0, BlockType_LIGHT); // out
		addConnectionOutput(1, i, out, 0);
		if (i == 0) {
			createConnection(AxorB, 1, out, 0); // out = A ^ B
		} else {
			block_id_t AxorBxorC = createBlockAtPosition(1, i*2, 0, BlockType_XOR); // A ^ B ^ C
			createConnection(AxorB, 1, AxorBxorC, 0);
			createConnection(C, 1, AxorBxorC, 0);
			createConnection(AxorBxorC, 1, out, 0); // out = A ^ B ^ C
		}
		if (i + 1 < size) {
			if (i == 0) {
				C = createBlockAtPosition(2, i*2+1, 0, BlockType_AND); // A & B
				createConnection(A, 0, C, 0);
				createConnection(B, 0, C, 0);
			} else {
				block_id_t AandB = createBlockAtPosition(0, i*2+1, 0, BlockType_AND); // A & B
				createConnection(A, 0, AandB, 0);
				createConnection(B, 0, AandB, 0);

				block_id_t AxorBandC = createBlockAtPosition(1, i*2+1, 0, BlockType_AND); // (A ^ B) & C
				createConnection(AxorB, 1, AxorBandC, 0);
				createConnection(C, 1, AxorBandC, 0);
				C = createBlockAtPosition(2, i*2+1, 0, BlockType_OR); // ((A ^ B) & C) || (A & B)
				createConnection(AandB, 1, C, 0);
				createConnection(AxorBandC, 1, C, 0);
			}
		}
	}

	return true;
}

}