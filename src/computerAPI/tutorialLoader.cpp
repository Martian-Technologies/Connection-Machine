#include "tutorialLoader.h"
#include "computerAPI/circuits/textParser.h"
#include "logging/logging.h"
#include <string>
#include <unordered_map>

bool parsePosition(std::stringstream& ss, int line, Position& out) {
	char ch;
	int x;
	int y;
	if (!(ss >> ch) || ch != '(') {
		logError("Invalid position. Expected '(' on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> x)) {
		logError("Invalid position. Expected x coordinate on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> ch) || ch != ',') {
		logError("Invalid position. Expected ',' on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> y)) {
		logError("Invalid position. Expected y coordinate on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> ch) || ch != ')') {
		logError("Invalid position. Expected ')' on line {}.", "TutorialLoader", line + 1);
		return false;
	}

	out = Position(x, y);
	return true;
}

bool parseFPosition(std::stringstream& ss, int line, FPosition& out) {
	char ch;
	float x;
	float y;
	if (!(ss >> ch) || ch != '(') {
		logError("Invalid position. Expected '(' on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> x)) {
		logError("Invalid position. Expected x coordinate on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> ch) || ch != ',') {
		logError("Invalid position. Expected ',' on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> y)) {
		logError("Invalid position. Expected y coordinate on line {}.", "TutorialLoader", line + 1);
		return false;
	}
	if (!(ss >> ch) || ch != ')') {
		logError("Invalid position. Expected ')' on line {}.", "TutorialLoader", line + 1);
		return false;
	}

	out = FPosition(x, y);
	return true;
}

void parsePreSteps(std::unordered_map<std::string, std::string>& info, std::unordered_map<std::string, std::string>& macros, const std::vector<std::string>& lines) {
	macros.clear();
	for (int i = 0; i < lines.size(); i++) {
		std::stringstream ss(lines[i]);
		std::string tok;
		ss >> tok;
		if (tok == "version_0" || tok == "version_1" || tok == "version_2") {
			// do something with versions once there is not backwards compatibility
			if (info.find("version") != info.end()) {
				logError("Duplicate version symbol on line {}.", "TutorialLoader", i + 1);
				continue;
			}
			info.emplace("version", tok);
		} else if ((tok == "Tutorial:")) {
			// Tutorial:
			if (info.find("name") != info.end()) {
				logError("Duplicate name symbol on line {}.", "TutorialLoader", i + 1);
				continue;
			}
			std::string tutorialName;
			if (!(ss >> tutorialName)) {
				logError("Invalid name on line {}, name should exist.", "TutorialLoader", i + 1);
				continue;
			}
			tutorialName = ss.str().substr(ss.str().find(tutorialName));
			if (tutorialName.starts_with("\"")) {
				tutorialName = tutorialName.substr(1);
			}
			if (tutorialName.ends_with("\"")) {
				tutorialName = tutorialName.substr(0, tutorialName.size() - 1);
			}
			info.emplace("name", tutorialName);
		} else if (tok.starts_with("$")) {
			// Macro (i.e. '$p1 (2, 5)')
			if (tok.size() == 1) {
				logError("Invalid macro on line {}, macro must contain a variable name.", "TutorialLoader", i + 1);
				continue;
			}
			std::string value;
			value = ss.str().substr(ss.str().find(tok) + tok.length() + 1);
			if (!macros.emplace("(" + tok + ")", value).second) {
				logError("Warning: Duplicate macro definition on line {}.", "TutorialLoader", i + 1);
				continue;
			}
		}
	}
}

void macroReplace(std::vector<std::string>& lines, std::unordered_map<std::string, std::string>& macros) {
	for (std::string& line : lines) {
		for (auto& macro : macros) {
			size_t pos = 0;
			while ((pos = line.find(macro.first, pos)) != std::string::npos) {
				line.replace(pos, macro.first.length(), macro.second);
				pos += macro.second.length();
			}
		}
	}
}

void parseBlock(std::stringstream& ss, int line, BlockType& blockTypeOut, Position& posOut, Orientation& orientationOut) {
	// name (x,y) orientation (optional)
	std::string tok;
	std::string tmp;
	ss >> tok;
	blockTypeOut = stringToBlockType(tok);
	parsePosition(ss, line, posOut);
	ss >> tok;
	orientationOut = stringToOrientation((tok));
}

void parseConnection(std::stringstream& ss, int line, Position& p1out, Position& p2out) {
	// (x,y) (x,y)
	parsePosition(ss, line, p1out);
	parsePosition(ss, line, p2out);
}

void parseCondition(TutorialCondition& condition, const std::vector<std::string>& lines, int& line) {
	for (int i = line; i < lines.size(); i++) {
		std::stringstream ss(lines[i]);
		std::string tok;
		std::string tmp;
		ss >> tok;
		if (tok == STEP_TOKEN || tok == ACTION_TOKEN) {
			line = i - 1;
			return;
		} else if (tok == BLOCK_CONDITION_TOKEN) {
			BlockType blockName;
			Position p;
			Orientation orientation;
			parseBlock(ss, i, blockName, p, orientation);
			condition.blocks.emplace_back(p, blockName, orientation);
		} else if (tok == CONNECTION_CONDITION_TOKEN) {
			Position connectionOutput;
			Position connectionInput;
			parseConnection(ss, i, connectionOutput, connectionInput);
			condition.connections.emplace_back(connectionOutput, connectionInput);
		} else if (tok == LOGIC_STATE_CONDITION_TOKEN) {
			// (logic_state: l,h,z,x) (x,y) (numsteps)
			ss >> tok;
			std::optional<logic_state_t> state = stringToLogicState(tok);
			if (!state.has_value()) {
				logError("Incorrectly formatted state on line {}.", "TutorialLoader", i + 1);
			}
			Position pos;
			parsePosition(ss, i, pos);
			int numSteps;
			ss >> numSteps;
			condition.logicStates.emplace_back(pos, state.value(), numSteps);
		} else if (tok == TRUTH_TABLE_TOKEN) {
			logError("Truth table not implemented (Line {})", "TutorialLoader", i + 1);
		}
	}
}

void parseAction(TutorialAction& action, const std::vector<std::string>& lines, int& line) {
	for (int i = line; i < lines.size(); i++) {
		std::stringstream ss(lines[i]);
		std::string tok;
		std::string tmp;
		ss >> tok;
		if (tok == STEP_TOKEN || tok == CONDITION_TOKEN) {
			line = i - 1;
			return;
		} else if (tok == MESSAGE_TOKEN) {
			// message: (x,y) scale <message>
			FPosition p;
			parseFPosition(ss, i, p);
			float scale;
			if (!(ss >> scale)) {
				logError("Invalid scale on line {}", "TutorialLoader", i + 1);
			}
			std::string message;
			message = ss.str().substr(ss.str().find("\""));
			if (message.starts_with("\"") && message.ends_with("\"")) {
				message = message.substr(1, message.length() - 2);
			}
			action.messages.emplace_back(p, scale, message);
		} else if (tok == BLOCK_PREVIEW_TOKEN) {
			// (name) (x,y) (orientation-optional)
			ss >> tok; // throw out 'Preview:'
			BlockType blockName;
			Position p;
			Orientation orientation;
			parseBlock(ss, i, blockName, p, orientation);
			action.blockPreviews.emplace_back(p, blockName, orientation);
		} else if (tok == CONNECTION_PREVIEW_TOKEN) {
			// (x,y) (x,y)
			ss >> tok; // throw out 'Preview:'
			Position connectionOutput;
			Position connectionInput;
			parseConnection(ss, i, connectionOutput, connectionInput);
			action.connectionPreviews.emplace_back(connectionOutput, connectionInput);
		} else if (tok == VIEW_CENTER_TOKEN) {
			// View Center: (x,y) <zoom>
			ss >> tok;
			FPosition viewCenter;
			parseFPosition(ss, i, viewCenter);
			action.viewData.viewCenter = viewCenter;
			float zoom;
			if (ss >> zoom) {
				action.viewData.zoom = zoom;
			}
		} else if (tok == PLACE_BLOCK_TOKEN) {
			// (name) (x,y) (orientation-optional)
			// ss >> tok; // throw out 'Preview:'
			BlockType blockName;
			Position p;
			Orientation orientation;
			parseBlock(ss, i, blockName, p, orientation);
			action.blocks.emplace_back(p, blockName, orientation);
		}
	}
}

void parseStep(std::vector<TutorialStep>& steps, const std::vector<std::string>& lines, int& line) {
	TutorialAction action;
	TutorialCondition condition;
	for (int i = line + 1; i < lines.size(); i++) {
		std::stringstream ss(lines[i]);
		std::string tok;
		ss >> tok;
		if (tok == STEP_TOKEN || i + 1 == lines.size()) {
			line = i - 1;
			steps.emplace_back(condition, action);
			return;
		} else if (tok == ACTION_TOKEN) {
			parseAction(action, lines, i);
		} else if (tok == CONDITION_TOKEN) {
			parseCondition(condition, lines, i);
		}
	}
}

void parseSteps(std::vector<TutorialStep>& steps, const std::vector<std::string>& lines) {
	for (int i = 0; i < lines.size(); i++) {
		std::stringstream ss(lines[i]);
		std::string tok;
		ss >> tok;
		if (tok == "Step:") {
			parseStep(steps, lines, i);
		}
	}
}

Tutorial parseTutorialFile(std::string filePath) {
	logInfo("Loading tutorial '" + filePath + "'", "TutorialLoader");
	std::ifstream istream(filePath);
	if (!istream.is_open()) {
		logError("Failed to open file '" + filePath + "'", "TutorialLoader");
		return {};
	}
	std::vector<std::string> lines;
	std::string buffer;
	while (std::getline(istream, buffer)) {
		lines.push_back(buffer);
	}
	std::unordered_map<std::string, std::string> info;
	std::unordered_map<std::string, std::string> macros;
	parsePreSteps(info, macros, lines);
	macroReplace(lines, macros);
	std::vector<TutorialStep> steps;
	parseSteps(steps, lines);
	return { steps, info };
}
