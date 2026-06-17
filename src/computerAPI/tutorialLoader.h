#ifndef tutorialloader_h
#define tutorialloader_h

#include "gui/viewportManager/circuitView/tutorial.h"

const std::string STEP_TOKEN = "Step:";

const std::string ACTION_TOKEN = "Action:";
const std::string MESSAGE_TOKEN = "Message:";
const std::string BLOCK_PREVIEW_TOKEN = "Block";
const std::string CONNECTION_PREVIEW_TOKEN = "Connection";
const std::string PREVIEW_TOKEN = "Preview:";
const std::string VIEW_CENTER_TOKEN = "View";
const std::string PLACE_BLOCK_TOKEN = "Block:";
const std::string PLACE_CONNECTION_TOKEN = "Connection:";
const std::string ACTIONS[] = {
	MESSAGE_TOKEN, BLOCK_PREVIEW_TOKEN, CONNECTION_PREVIEW_TOKEN, VIEW_CENTER_TOKEN, PLACE_BLOCK_TOKEN, PLACE_CONNECTION_TOKEN,
};

const std::string CONDITION_TOKEN = "Condition:";
const std::string BLOCK_CONDITION_TOKEN = "Block:";
const std::string CONNECTION_CONDITION_TOKEN = "Connection:";
const std::string LOGIC_STATE_CONDITION_TOKEN = "State:";
const std::string TRUTH_TABLE_TOKEN = "Truth";
const std::string CONDITIONS[] = {
	BLOCK_CONDITION_TOKEN,
	CONNECTION_CONDITION_TOKEN,
	LOGIC_STATE_CONDITION_TOKEN,
	TRUTH_TABLE_TOKEN,
};

Tutorial parseTutorialFile(std::string fileName);

#endif