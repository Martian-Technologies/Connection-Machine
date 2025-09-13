#include "simControlsManager.h"

#include <RmlUi/Core/Input.h>

#include "gui/helper/eventPasser.h"
#include "gui/mainWindow/circuitView/circuitViewWidget.h"

SimControlsManager::SimControlsManager(Rml::ElementDocument* document, std::shared_ptr<CircuitViewWidget> circuitViewWidget, DataUpdateEventManager* dataUpdateEventManager) : circuitViewWidget(circuitViewWidget), dataUpdateEventReceiver(dataUpdateEventManager) {
	toggleSimElement = document->GetElementById("toggle-simulation");
	realisticElement = document->GetElementById("realistic-button");
	limitSpeedElement = document->GetElementById("limit-speed-checkbox");
	tpsInputElement = document->GetElementById("tps-input");

	toggleSimElement->AddEventListener("click", new EventPasser(std::bind(&SimControlsManager::toggleSimulation, this)));
	realisticElement->AddEventListener("click", new EventPasser(std::bind(&SimControlsManager::setRealistic, this)));
	limitSpeedElement->AddEventListener("click", new EventPasser([this](Rml::Event& event) {
		limitSpeed();
		event.StopPropagation();
	}));
	tpsInputElement->AddEventListener("change", new EventPasser(std::bind(&SimControlsManager::setTPS, this)));

	dataUpdateEventReceiver.linkFunction("circuitViewChangeEvaluator", std::bind(&SimControlsManager::update, this));
	update();
}

void SimControlsManager::update() {
	Evaluator* evaluator = circuitViewWidget->getCircuitView()->getEvaluator();
	if (evaluator) {
		if (evaluator->isPause()) {
			toggleSimElement->SetClass("checked", false);
		} else {
			toggleSimElement->SetClass("checked", true);
		}
		if (evaluator->isRealistic()) {
			realisticElement->SetClass("checked", true);
			realisticElement->SetInnerRML("R");
		} else {
			realisticElement->SetClass("checked", false);
			realisticElement->SetInnerRML("S");
		}
		if (evaluator->getUseTickrate()) {
			limitSpeedElement->SetAttribute("checked", true);
		} else {
			limitSpeedElement->RemoveAttribute("checked");
		}
		Rml::String value = tpsInputElement->GetAttribute<Rml::String>("value", "");
		std::string correctValue;
		bool foundDecimal = false;
		for (char c : value) {
			if (('0' <= c && c <= '9') || (correctValue.empty() && c == '-')) {
				correctValue += c;
			} else if (c == '.' && !foundDecimal) {
				foundDecimal = true;
				correctValue += c;
			}
		}
		std::stringstream ss(correctValue);
		double tps = 0;
		ss >> tps;
		if (!approx_equals(evaluator->getTickrate(), tps)) {
			std::string tpsStr = fmt::format("{:.1f}", evaluator->getTickrate());
			if (tpsStr.back() == '0') tpsStr.pop_back();
			if (tpsStr.back() == '.') tpsStr.pop_back();
			tpsInputElement->SetInnerRML(std::string(tpsStr.size(), ' ') + "tps");
			tpsInputElement->SetAttribute<Rml::String>("value", tpsStr);
		}
	} else {
		toggleSimElement->SetClass("checked", false);
		realisticElement->SetClass("checked", false);
		realisticElement->SetInnerRML("S");
		limitSpeedElement->RemoveAttribute("checked");
		tpsInputElement->SetInnerRML("tps");
		tpsInputElement->SetAttribute<Rml::String>("value", "");
	}
}

void SimControlsManager::toggleSimulation() {
	Evaluator* evaluator = circuitViewWidget->getCircuitView()->getEvaluator();
	if (evaluator) {
		if (evaluator->isPause()) {
			evaluator->setPause(false);
		} else {
			evaluator->setPause(true);
		}
	}
	update();
}

void SimControlsManager::setRealistic() {
	Evaluator* evaluator = circuitViewWidget->getCircuitView()->getEvaluator();
	if (evaluator) {
		evaluator->setRealistic(!(evaluator->isRealistic()));
	}
	update();
}

void SimControlsManager::limitSpeed() {
	Evaluator* evaluator = circuitViewWidget->getCircuitView()->getEvaluator();
	if (evaluator) {
		evaluator->setUseTickrate(!(evaluator->getUseTickrate()));
	}
	update();
}

void SimControlsManager::setTPS() {
	Evaluator* evaluator = circuitViewWidget->getCircuitView()->getEvaluator();
	if (evaluator) {
		Rml::String value = tpsInputElement->GetAttribute<Rml::String>("value", "");
		std::string correctValue;
		bool foundDecimal = false;
		for (char c : value) {
			if (('0' <= c && c <= '9') || (correctValue.empty() && c == '-')) {
				correctValue += c;
			} else if (c == '.' && !foundDecimal) {
				foundDecimal = true;
				correctValue += c;
			}
		}
		std::stringstream ss(correctValue);
		double tps = 0;
		ss >> tps;
		tpsInputElement->SetInnerRML(std::string(correctValue.size(), ' ') + "tps");
		tpsInputElement->SetAttribute<Rml::String>("value", correctValue);
		evaluator->setTickrate(tps);
	} else {
		tpsInputElement->SetInnerRML("tps");
	}
	update();
}
