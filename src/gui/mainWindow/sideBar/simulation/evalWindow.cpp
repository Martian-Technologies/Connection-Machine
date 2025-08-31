#include "evalWindow.h"

#include "backend/evaluator/evaluatorManager.h"
#include "backend/dataUpdateEventManager.h"
#include "backend/circuit/circuitManager.h"

#include "gui/mainWindow/circuitView/circuitViewWidget.h"
#include "gui/mainWindow/mainWindow.h"

#include "util/algorithm.h"

EvalWindow::EvalWindow(
	const EvaluatorManager* evaluatorManager,
	const CircuitManager* circuitManager,
	MainWindow* mainWindow,
	DataUpdateEventManager* dataUpdateEventManager,
	Rml::ElementDocument* document,
	Rml::Element* parent
) : menuTree(document, parent, true, false), dataUpdateEventReceiver(dataUpdateEventManager), evaluatorManager(evaluatorManager), circuitManager(circuitManager), mainWindow(mainWindow) {
	dataUpdateEventReceiver.linkFunction("addressTreeMakeBranch", [this](const DataUpdateEventManager::EventData*) { refreshSidebar(true); });
	dataUpdateEventReceiver.linkFunction("blockDataUpdate", [this](const DataUpdateEventManager::EventData*) { refreshSidebar(true); });
	dataUpdateEventReceiver.linkFunction("circuitViewChangeEvaluator", [this](const DataUpdateEventManager::EventData*) { refreshSidebar(false); });
	// When the viewed circuit changes (due to navigating into/out of ICs), re-apply highlight.
	dataUpdateEventReceiver.linkFunction("circuitViewChangeCircuit", [this](const DataUpdateEventManager::EventData*) { refreshSidebar(false); });
	dataUpdateEventReceiver.linkFunction("circuitCreatedSelect", std::bind(&EvalWindow::onCircuitCreatedSelect, this, std::placeholders::_1));
	menuTree.setListener(std::bind(&EvalWindow::updateSelected, this, std::placeholders::_1));
	refreshSidebar(true);
}

void EvalWindow::updateList() {
	std::vector<std::vector<std::string>> paths;
	for (auto pair : this->evaluatorManager->getEvaluators()) {
		std::vector<std::string> path({ pair.second->getEvaluatorName() });
		makePaths(paths, path, pair.second->buildAddressTree());
	}
	menuTree.setPaths(paths);
}

void EvalWindow::refreshSidebar(bool rebuildItems) {
	if (rebuildItems) updateList();
	CircuitView* view = mainWindow->getActiveCircuitViewWidget() ? mainWindow->getActiveCircuitViewWidget()->getCircuitView() : nullptr;
	if (!view) return;
	Evaluator* activeEval = view->getEvaluator();
	if (!activeEval) return;
	const evaluator_id_t activeId = activeEval->getEvaluatorId();
	const Address& address = view->getAddress();
	Rml::Element* root = menuTree.getRootElement();
	if (!root) return;
	Rml::ElementList rows; root->GetElementsByTagName(rows, "li");
	for (auto* r : rows) r->SetClass("selected", false);

	// Find the top-level evaluator row matching the active evaluator id
	Rml::Element* evaluatorRow = nullptr;
	for (auto* r : rows) {
		std::string idAttr = r->GetId();
		if (idAttr.find('/') != std::string::npos) continue; // only top-level
		Rml::Element* labelDiv = nullptr;
		for (unsigned int i = 0; i < r->GetNumChildren(); ++i) {
			if (r->GetChild(i)->GetTagName() == "div") { labelDiv = r->GetChild(i); break; }
		}
		if (!labelDiv) continue;
		std::string label = labelDiv->GetInnerRML();
		std::stringstream ss(label);
		std::string word; evaluator_id_t idParsed = 0; ss >> word >> idParsed;
		if (ss.fail() || word != "Eval") continue;
		if (idParsed == activeId) { evaluatorRow = r; break; }
	}
	if (!evaluatorRow) return;

	// Descend according to the current address, matching by Position extracted from child labels "<Name>(x, y)"
	Rml::Element* selectedRow = evaluatorRow; // default to evaluator row if no deeper match
	Rml::Element* cur = evaluatorRow;
	for (int i = 0; i < address.size(); ++i) {
		Position wanted = address.getPosition(i);
		// Find UL under current LI
		Rml::ElementList childLists; cur->GetElementsByTagName(childLists, "ul");
		if (childLists.empty()) break;
		Rml::Element* ul = childLists[0];
		Rml::Element* matchLi = nullptr;
		for (unsigned int c = 0; c < ul->GetNumChildren(); ++c) {
			Rml::Element* li = ul->GetChild(c);
			if (li->GetTagName() != "li") continue;
			Rml::Element* div = nullptr;
			for (unsigned int k = 0; k < li->GetNumChildren(); ++k) {
				if (li->GetChild(k)->GetTagName() == "div") { div = li->GetChild(k); break; }
			}
			if (!div) continue;
			std::string text = div->GetInnerRML();
			auto lparen = text.find_last_of('(');
			if (lparen == std::string::npos || text.empty() || text.back() != ')') continue;
			std::string coord = text.substr(lparen + 1, text.size() - lparen - 2);
			std::stringstream ps(coord);
			Position p{}; char comma = 0;
			if (!(ps >> p.x)) continue;
			ps >> comma; if (comma != ',') continue;
			if (!(ps >> p.y)) continue;
			if (p == wanted) { matchLi = li; break; }
		}
		if (!matchLi) break; // stop descending if not found
		selectedRow = matchLi;
		cur = matchLi;
	}

	// Apply selection to the deepest matched row and expand ancestors
	selectedRow->SetClass("selected", true);
	Rml::Element* p = selectedRow->GetParentNode();
	while (p) {
		if (p->GetTagName() == "li") p->SetClass("collapsed", false);
		p = p->GetParentNode();
	}
}

void EvalWindow::makePaths(std::vector<std::vector<std::string>>& paths, std::vector<std::string>& path, const EvalAddressTree& addressTree) {
	auto& branches = addressTree.getBranches();
	if (branches.empty()) {
		paths.push_back(path);
	} else {
		for (auto& pair : branches) {
			path.push_back(circuitManager->getCircuit(pair.second.getContainerId())->getCircuitName() + pair.first.toString());
			makePaths(paths, path, pair.second);
			path.pop_back();
		}
	}
}

void EvalWindow::updateSelected(std::string string) {
	std::vector<std::string> parts = stringSplit(string, '/');
	std::stringstream evalName(parts.front());
	std::string str;
	evaluator_id_t evalId;
	evalName >> str >> evalId;
	Address address;
	for (unsigned int i = 1; i < parts.size(); i++) {
		const std::string& part = parts[i];
		size_t index = part.size();
		while (index != 0 && part[index - 1] != '(') index--;
		if (index == 0) continue;
		std::stringstream posString(part.substr(index, part.size() - index - 1));
		Position position{};
		char sep = 0;
		if (!(posString >> position.x)) continue;
		if (!(posString >> sep)) continue;
		if (!(posString >> position.y)) continue;
		address.addBlockId(position);
	}

	CircuitView* circuitView = mainWindow->getActiveCircuitViewWidget()->getCircuitView();
	circuitView->setEvaluator(circuitView->getBackend(), evalId, address);
	refreshSidebar(false);
}

void EvalWindow::selectEvaluatorForCircuit(circuit_id_t circuitId) {
	if (!this->evaluatorManager) return;
	for (auto& pair : this->evaluatorManager->getEvaluators()) {
		if (pair.second->getCircuitId() == circuitId) {
			evaluator_id_t wantedId = pair.first;
			Rml::Element* root = menuTree.getRootElement();
			if (!root) return;
			Rml::ElementList rows; root->GetElementsByTagName(rows, "li");
			for (auto* r : rows) {
				std::string idAttr = r->GetId();
				if (idAttr.find('/') != std::string::npos) continue; // only top-level evaluators
				Rml::Element* labelDiv = nullptr;
				for (unsigned int i = 0; i < r->GetNumChildren(); ++i) {
					if (r->GetChild(i)->GetTagName() == "div") { labelDiv = r->GetChild(i); break; }
				}
				if (!labelDiv) continue;
				std::string label = labelDiv->GetInnerRML();
				std::stringstream ss(label);
				std::string word; evaluator_id_t idParsed = 0; ss >> word >> idParsed;
				if (ss.fail() || word != "Eval") continue;
				if (idParsed != wantedId) continue;
				Rml::ElementList all; root->GetElementsByTagName(all, "li");
				for (auto* a : all) a->SetClass("selected", false);
				r->SetClass("selected", true);
				return;
			}
			return;
		}
	}
}

void EvalWindow::onCircuitCreatedSelect(const DataUpdateEventManager::EventData* eventData) {
	refreshSidebar(true);
}