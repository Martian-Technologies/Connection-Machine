#include "tutorialDataManager.h"
#include "backend/evaluator/simulator/logicState.h"
#include "computerAPI/circuits/textParser.h"
#include "computerAPI/directoryManager.h"
#include "computerAPI/tutorialLoader.h"
#include <vulkan/vulkan_core.h>

void TutorialDataManager::initializeTutorials() {
	tutorialsMutex.lock();
	tutorialNamesMutex.lock();
	tutorialNames.clear();
	tutorials.clear();
	logInfo("Tutorials Loaded:", "MainWindow");
	logInfo((DirectoryManager::getResourceDirectory() / "tutorials" / "").string());
	for (const auto& file : std::filesystem::directory_iterator((DirectoryManager::getResourceDirectory() / "tutorials" / "").string())) {
		Tutorial parsedTutorial = parseTutorialFile(file.path().string());
        auto pair = tutorials.emplace(parsedTutorial.info.find("name")->second, parsedTutorial);
		tutorialNames.emplace_back(pair.first->first);
	}
	tutorialsMutex.unlock();
	tutorialNamesMutex.unlock();
}

void writeTutorial(const Tutorial& tutorial, const std::string& filepath) {
	std::ofstream out;
	out.open(filepath);
    out << tutorial.info.find("version")->second << "\n";
	out << "Tutorial: \"" << tutorial.info.find("name")->second << " -w\"\n";
	for (int i = 0; i < tutorial.tutorialSteps.size(); i++) {
		TutorialStep cur = tutorial.tutorialSteps[i];
		TutorialAction curAction = cur.action;
		bool has_action = curAction.messages.size() != 0 || curAction.blockPreviews.size() != 0 || curAction.connectionPreviews.size() != 0 ||
						  curAction.blocks.size() != 0 || curAction.viewData.viewCenter.has_value();
		if (has_action) {
			out << STEP_TOKEN << "\n";
			out << "\t" << ACTION_TOKEN << "\n";
			for (int j = 0; j < curAction.messages.size(); j++) {
				TutorialAction::Message curEl = curAction.messages[j];
				out << "\t\t" << MESSAGE_TOKEN << " " << curEl.pos.toString() << " " << curEl.scale << " \"" << curEl.message << "\"\n";
			}
			for (int j = 0; j < curAction.blockPreviews.size(); j++) {
				TutorialAction::BlockInfo curEl = curAction.blockPreviews[j];
				out << "\t\t" << BLOCK_PREVIEW_TOKEN << " " << PREVIEW_TOKEN << " " << blockTypeToString(curEl.type) << " " << curEl.pos.toString() << " "
					<< orientationToString(curEl.orientation) << "\n";
			}
			for (int j = 0; j < curAction.connectionPreviews.size(); j++) {
				TutorialAction::ConnectionPreviewInfo curEl = curAction.connectionPreviews[j];
				out << "\t\t" << CONNECTION_PREVIEW_TOKEN << " " << PREVIEW_TOKEN << " " << curEl.pos1.toString() << " " << curEl.pos2.toString() << "\n";
			}
			for (int j = 0; j < curAction.blocks.size(); j++) {
				TutorialAction::BlockInfo curEl = curAction.blocks[j];
				out << "\t\t" << PLACE_BLOCK_TOKEN << " " << blockTypeToString(curEl.type) << " " << curEl.pos.toString() << " " << orientationToString(curEl.orientation)
					<< "\n";
			}
			if (curAction.viewData.viewCenter.has_value()) {
				out << "\t\t" << VIEW_CENTER_TOKEN << " Center: " << curAction.viewData.viewCenter.value().toString();
				if (curAction.viewData.zoom.has_value()) {
					out << " " << curAction.viewData.zoom.value();
				}
				out << "\n";
			}
		}
		TutorialCondition curCondition = cur.condition;
		if (curCondition.blocks.size() != 0 || curCondition.connections.size() != 0 || curCondition.logicStates.size() != 0) {
			if (!has_action) {
				out << STEP_TOKEN << "\n";
			}
			out << "\t" << CONDITION_TOKEN << "\n";
			for (int j = 0; j < curCondition.blocks.size(); j++) {
				TutorialCondition::BlockRequirement curEl = curCondition.blocks[j];
				out << "\t\t" << BLOCK_CONDITION_TOKEN << " " << blockTypeToString(curEl.type) << " " << curEl.pos.toString() << " "
					<< orientationToString(curEl.orientation) << "\n";
			}
			for (int j = 0; j < curCondition.connections.size(); j++) {
				TutorialCondition::ConnectionRequirement curEl = curCondition.connections[j];
				out << "\t\t" << CONNECTION_CONDITION_TOKEN << " " << curEl.pos1.toString() << " " << curEl.pos2.toString() << "\n";
			}
			for (int j = 0; j < curCondition.logicStates.size(); j++) {
				TutorialCondition::LogicStateRequirement curEl = curCondition.logicStates[j];
				out << "\t\t" << LOGIC_STATE_CONDITION_TOKEN << " " << logicstate_to_string(curEl.state) << " " << curEl.pos.toString() << " " << curEl.numSteps << "\n";
			}
		}
	}
	out.close();
}
