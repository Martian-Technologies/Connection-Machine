#ifndef functions_h
#define functions_h

#if defined(__clang__) || defined(__GNUC__)
#define externalFunction(functionName) __attribute__((import_module("env"))) __attribute__((import_name(#functionName)))
#else
#define externalFunction(functionName) // for MSVC
#endif

// #define exportedVar(type, name) type name; type getname
#define exportedVar(type, name, Name)                                                                                                                                    \
	extern const type name;                                                                                                                                              \
	const type get##Name() { return name; }

#ifdef __cplusplus
extern "C"
{
#endif
	typedef unsigned int BlockType;

	typedef unsigned int connection_end_id_t;
	typedef unsigned char block_size_t;
	typedef int block_id_t;
	typedef int cord_t;

	typedef char Rotation;

	exportedVar(char*, UUID, UUID) exportedVar(char*, name, Name) exportedVar(char*, defaultParameters, DefaultParameters);
	// returns number of files imported
	externalFunction(importFile) unsigned int importFile(const char* filePath);
	externalFunction(getParameter) int getParameter(const char* key);
	externalFunction(getPrimitiveType) BlockType getPrimitiveType(const char* primitiveName);
	externalFunction(getNonPrimitiveType) BlockType getNonPrimitiveType(const char* UUID);
	externalFunction(getProceduralCircuitType) BlockType getProceduralCircuitType(const char* UUID, const char* parameters);
	externalFunction(createBlock) block_id_t createBlock(BlockType blockType);
	externalFunction(createBlockAtPosition) block_id_t createBlockAtPosition(cord_t x, cord_t y, Rotation rotation, BlockType blockType);
	externalFunction(createConnection) void createConnection(block_id_t outputBlockId, int outputPortId, block_id_t inputBlockId, int inputPortId);
	externalFunction(addConnectionInput) void addConnectionInput(cord_t portX, cord_t portY, block_id_t internalBlockId, connection_end_id_t internalBlockPortId);
	externalFunction(addConnectionOutput) void addConnectionOutput(cord_t portX, cord_t portY, block_id_t internalBlockId, connection_end_id_t internalBlockPortId);
	externalFunction(setSize) void setSize(cord_t width, cord_t height);
	externalFunction(logInfo) void logInfo(const char* msg);
	externalFunction(logError) void logError(const char* msg);
#ifdef __cplusplus
}
#endif

#endif /* functions_h */
