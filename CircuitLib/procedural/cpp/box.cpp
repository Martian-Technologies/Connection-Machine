#include "cm.h"

#include <cstdio>

/*
This is not ment to be used. I use as a test for procedural circuit stuff.
*/

extern "C" {

const char* UUID = "c23a393f-3965-446c-99af-471db04ce50e";
const char* name = "Box";
const char* defaultParameters = "(\"w\": 1, \"h\": 1)";

bool generateCircuit() {
	int w = getParameter("w");
	int h = getParameter("h");

	setSize(w, h);

	importFile("andGate.wasm");
	char buffer [50];
	snprintf(buffer, 50, "(\"size\": %d)", w);
	BlockType andGate = getProceduralCircuitType("8a92b940-456a-4d81-bc40-1f6e8bef4464", buffer);
	createBlockAtPosition(-3, 0, 0, andGate);

	if (w != 1 && h != 1) {
		snprintf(buffer, 50, "(\"w\": %d, \"h\": %d)", w-1, h-1);
		BlockType smaller = getProceduralCircuitType(UUID, buffer);
		createBlockAtPosition(0, 0, 0, smaller);
	}

	return true;
}

}