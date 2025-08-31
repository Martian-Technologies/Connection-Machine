#include "blockCreationWindow.h"

#include "gui/viewportManager/circuitView/tools/other/portSelector.h"
#include "gui/mainWindow/circuitView/circuitViewWidget.h"
#include "gui/mainWindow/mainWindow.h"
#include "gui/helper/eventPasser.h"

#include "backend/dataUpdateEventManager.h"
#include "backend/circuit/circuitManager.h"

#include "util/algorithm.h"


BlockCreationWindow::BlockCreationWindow(
	CircuitManager* circuitManager,
	MainWindow* mainWindow,
	DataUpdateEventManager* dataUpdateEventManager,
	ToolManagerManager* toolManagerManager,
	Rml::ElementDocument* document,
	Rml::Element* menu
) : document(document), dataUpdateEventReceiver(dataUpdateEventManager), circuitManager(circuitManager), mainWindow(mainWindow), toolManagerManager(toolManagerManager) {
	// enuTree(document, parent, true, false)
	this->menu = menu;
	outputList = menu->GetElementById("connection-list-output");
	inputList = menu->GetElementById("connection-list-input");
	// update
	Rml::Element* updateButton = menu->GetElementById("update-block-creation");
	updateButton->AddEventListener("click", new EventPasser(std::bind(&BlockCreationWindow::updateFromMenu, this)));
	// reset
	Rml::Element* resetButton = menu->GetElementById("reset-block-creation");
	resetButton->AddEventListener("click", new EventPasser(std::bind(&BlockCreationWindow::resetMenu, this)));
	// add
	Rml::Element* addInputConnection = menu->GetElementById("connection-list-add-input");
	addInputConnection->AddEventListener("click", new EventPasser(std::bind(&BlockCreationWindow::addListItem, this, true)));
	Rml::Element* addOutputConnection = menu->GetElementById("connection-list-add-output");
	addOutputConnection->AddEventListener("click", new EventPasser(std::bind(&BlockCreationWindow::addListItem, this, false)));
	// dataUpdateEvents
	dataUpdateEventReceiver.linkFunction("blockDataUpdate", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("circuitViewChangeCircuit", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("blockDataSetConnection", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("blockDataRemoveConnection", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("circuitBlockDataConnectionPositionSet", std::bind(&BlockCreationWindow::resetMenu, this));
	resetMenu();
}

void BlockCreationWindow::updateFromMenu() {
	Circuit* circuit = mainWindow->getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
	if (!circuit || !(circuit->isEditable())) return;
	circuit_id_t id = circuit->getCircuitId();
	CircuitBlockData* circuitBlockData = circuitManager->getCircuitBlockDataManager()->getCircuitBlockData(id);
	BlockData* blockData = circuitManager->getBlockDataManager()->getBlockData(circuitBlockData->getBlockType());
	std::string name;
	Size size;
	std::vector<std::tuple<connection_end_id_t, std::string, bool, Vector, Position>> portsData;
	try {
		// we dont update till the end because setting data will cause the UI to update
		Rml::Element* ele = menu->GetElementById("name-input");
		Rml::ElementFormControlInput* nameElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
		name = nameElement->GetValue();

		if (!blockData || !circuitBlockData) {
			// set data if nothing more to get
			circuit->setCircuitName(name);
			return;
		}

		ele = menu->GetElementById("width-input");
		Rml::ElementFormControlInput* widthElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
		ele = menu->GetElementById("height-input");
		Rml::ElementFormControlInput* heightElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
		size = Size(std::stoi(widthElement->GetValue()), std::stoi(heightElement->GetValue()));

		for (unsigned int i = 0; i < inputList->GetNumChildren(); i++) {
			Rml::Element* row = inputList->GetChild(i);
			const std::string& rowId = row->GetId();
			// get connection end id
			connection_end_id_t endId = std::stoi(rowId.substr(23, rowId.size() - 23));
			// get port name
			Rml::ElementList elements;
			row->GetElementsByClassName(elements, "connection-list-item-name");
			const std::string& portName = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue();
			// get port position on block
			Vector portPositionOnBlock;
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-on-block-x");
			portPositionOnBlock.dx = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-on-block-y");
			portPositionOnBlock.dy = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			// get port block position
			Position portBlockPosition;
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-x");
			portBlockPosition.x = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-y");
			portBlockPosition.y = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());

			portsData.emplace_back(endId, portName, true, portPositionOnBlock, portBlockPosition);
		}
		for (unsigned int i = 0; i < outputList->GetNumChildren(); i++) {
			Rml::Element* row = outputList->GetChild(i);
			const std::string& rowId = row->GetId();
			// get connection end id
			connection_end_id_t endId = std::stoi(rowId.substr(23, rowId.size() - 23));
			// get port name
			Rml::ElementList elements;
			row->GetElementsByClassName(elements, "connection-list-item-name");
			const std::string& portName = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue();
			// get port position on block
			Vector portPositionOnBlock;
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-on-block-x");
			portPositionOnBlock.dx = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-on-block-y");
			portPositionOnBlock.dy = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			// get port block position
			Position portBlockPosition;
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-x");
			portBlockPosition.x = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-y");
			portBlockPosition.y = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());

			portsData.emplace_back(endId, portName, false, portPositionOnBlock, portBlockPosition);
		}

	} catch (const std::exception& e) {
		// Top level fatal error catcher, logs issue
		logError("{}", "", e.what());
		return;
	}

	// check that data is good
	if (!size.isValid()) {
		logWarning("Can't update block data. Size of block cant be less than 1 currently is {}.", "BlockCreationWindow", size.toString());
		return;
	}

	std::unordered_set<Vector> inPortPositionsOnBlock;
	std::unordered_set<Vector> outPortPositionsOnBlock;
	for (auto row : portsData) {
		connection_end_id_t endId = std::get<0>(row);
		bool portIsInput = std::get<2>(row);
		Vector portPositionOnBlock = std::get<3>(row);
		if (!portPositionOnBlock.widthInSize(size)) {
			logWarning("Can't update block data. Port position {} is not on the block {}.", "BlockCreationWindow", portPositionOnBlock.toString(), size.toString());
			return;
		}
		if (portIsInput) {
			if (inPortPositionsOnBlock.contains(portPositionOnBlock)) {
				logWarning("Can't update block data. Port position {} is already used.", "BlockCreationWindow", portPositionOnBlock.toString());
				return;
			}
			inPortPositionsOnBlock.emplace(portPositionOnBlock);
		} else {
			if (outPortPositionsOnBlock.contains(portPositionOnBlock)) {
				logWarning("Can't update block data. Port position {} is already used.", "BlockCreationWindow", portPositionOnBlock.toString());
				return;
			}
			outPortPositionsOnBlock.emplace(portPositionOnBlock);
		}
	}

	// set all data at end
	circuit->setCircuitName(name);
	blockData->setSize(size);

	std::vector<connection_end_id_t> endIdsToRemove;
	for (auto iter : blockData->getConnections()) {
		bool found = false;
		for (auto port : portsData) {
			if (std::get<0>(port) == iter.first) {
				found = true;
				break;
			}
		}
		if (!found) {
			endIdsToRemove.push_back(iter.first);
		}
	}
	for (connection_end_id_t endId : endIdsToRemove) {
		blockData->removeConnection(endId);
	}

	for (auto row : portsData) {
		connection_end_id_t endId = std::get<0>(row);
		std::string portName = std::get<1>(row);
		bool portIsInput = std::get<2>(row);
		Vector portPositionOnBlock = std::get<3>(row);
		Position portBlockPosition = std::get<4>(row);
		if (blockData->connectionExists(endId)) {
			if (blockData->isConnectionInput(endId) != portIsInput) blockData->removeConnection(endId);
		}

		if (portIsInput) blockData->setConnectionInput(portPositionOnBlock, endId);
		else blockData->setConnectionOutput(portPositionOnBlock, endId);
		if (!portName.empty()) blockData->setConnectionIdName(endId, portName);

		circuitBlockData->setConnectionIdPosition(endId, portBlockPosition);
	}
}

void BlockCreationWindow::resetMenu() {
	// std::vector<std::pair<std::string, connection_end_id_t>> paths;
	// clear list
	while (inputList->GetNumChildren() > 0) inputList->RemoveChild(inputList->GetChild(0));
	while (outputList->GetNumChildren() > 0) outputList->RemoveChild(outputList->GetChild(0));
	if (!mainWindow->getActiveCircuitViewWidget()) return; // nothing to show
	Circuit* circuit = mainWindow->getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
	if (!circuit) return;
	circuit_id_t id = circuit->getCircuitId();
	Rml::Element* ele = menu->GetElementById("name-input");
	Rml::ElementFormControlInput* nameElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
	nameElement->SetValue(circuit->getCircuitName());

	const CircuitBlockData* circuitBlockData = circuitManager->getCircuitBlockDataManager()->getCircuitBlockData(id);
	if (!circuitBlockData) return;
	const BlockData* blockData = circuitManager->getBlockDataManager()->getBlockData(circuitBlockData->getBlockType());
	if (!blockData) return;

	ele = menu->GetElementById("width-input");
	Rml::ElementFormControlInput* widthElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
	widthElement->SetValue(std::to_string(blockData->getSize().w));
	ele = menu->GetElementById("height-input");
	Rml::ElementFormControlInput* heightElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
	heightElement->SetValue(std::to_string(blockData->getSize().h));

	const std::unordered_map<connection_end_id_t, std::pair<Vector, bool>>& conncections = blockData->getConnections();
	for (auto& iter : conncections) {
		connection_end_id_t endId = iter.first;
		bool isInputBool = iter.second.second;
		Vector positionOnBlock = iter.second.first;
		const std::string* connectionNamePtr = blockData->getConnectionIdToName(endId);
		std::string connectionName;
		if (connectionNamePtr) {
			connectionName = *connectionNamePtr;
		} else {
			connectionName = "";
		}
		const Position* positionPtr = circuitBlockData->getConnectionIdToPosition(endId);
		Rml::ElementPtr row = document->CreateElement("div");
		// name
		Rml::XMLAttributes nameAttributes;
		nameAttributes["type"] = "text";
		nameAttributes["maxlength"] = "7";
		nameAttributes["size"] = "7";
		Rml::ElementPtr name = Rml::Factory::InstanceElement(document, "input", "input", nameAttributes);
		Rml::ElementFormControlInput* nameElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(name.get());
		nameElement->SetValue(connectionName);
		// positionOnBlock
		Rml::XMLAttributes positionOnBlockAttributes;
		positionOnBlockAttributes["type"] = "text";
		positionOnBlockAttributes["maxlength"] = "3";
		positionOnBlockAttributes["size"] = "2";
		Rml::ElementPtr positionOnBlockX = Rml::Factory::InstanceElement(document, "input", "input", positionOnBlockAttributes);
		Rml::ElementPtr positionOnBlockY = Rml::Factory::InstanceElement(document, "input", "input", positionOnBlockAttributes);
		Rml::ElementFormControlInput* positionOnBlockXElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionOnBlockX.get());
		Rml::ElementFormControlInput* positionOnBlockYElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionOnBlockY.get());
		positionOnBlockXElement->SetValue(std::to_string(positionOnBlock.dx));
		positionOnBlockYElement->SetValue(std::to_string(positionOnBlock.dy));
		// position
		Rml::XMLAttributes positionAttributes;
		positionAttributes["type"] = "text";
		positionAttributes["maxlength"] = "6";
		positionAttributes["size"] = "5";
		Rml::ElementPtr positionX = Rml::Factory::InstanceElement(document, "input", "input", positionAttributes);
		Rml::ElementPtr positionY = Rml::Factory::InstanceElement(document, "input", "input", positionAttributes);
		Rml::ElementFormControlInput* positionXElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionX.get());
		Rml::ElementFormControlInput* positionYElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionY.get());
		if (positionPtr) {
			positionXElement->SetValue(std::to_string(positionPtr->x));
			positionYElement->SetValue(std::to_string(positionPtr->y));
		} else {
			positionXElement->SetValue("N/A");
			positionYElement->SetValue("N/A");
		}
		Rml::ElementPtr setPositionButton = document->CreateElement("button");
		setPositionButton->AppendChild(std::move(document->CreateTextNode("S")));
		setPositionButton->AddEventListener(Rml::EventId::Click, new EventPasser(
			[this, endId](Rml::Event& event) {
				auto tool = std::dynamic_pointer_cast<PortSelector>(mainWindow->getActiveCircuitViewWidget()->getCircuitView()->getToolManager().selectTool(std::make_shared<PortSelector>()));
				if (tool) {
					tool->setPort(endId, [this, endId](Position position) {
						Rml::Element* row = document->GetElementById("ConnectionListItem Id: " + std::to_string(endId));
						Rml::ElementList elements;
						row->GetElementsByClassName(elements, "connection-list-item-pos-x");
						rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->SetValue(std::to_string(position.x));
						elements.clear();
						row->GetElementsByClassName(elements, "connection-list-item-pos-y");
						rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->SetValue(std::to_string(position.y));
					});
				}
			}
		));
		// add children and set classes
		row->AppendChild(std::move(name))->SetClass("connection-list-item-name", true);
		row->AppendChild(std::move(positionOnBlockX))->SetClass("connection-list-item-on-block-x", true);
		row->AppendChild(std::move(positionOnBlockY))->SetClass("connection-list-item-on-block-y", true);
		// Flex wrap breaker to force subsequent elements (block/global position + buttons) onto next line without pushing Port Y down
		{
			Rml::ElementPtr breaker = document->CreateElement("div");
			row->AppendChild(std::move(breaker))->SetClass("connection-break", true);
		}
		row->AppendChild(std::move(positionX))->SetClass("connection-list-item-pos-x", true);
		row->AppendChild(std::move(positionY))->SetClass("connection-list-item-pos-y", true);
		row->AppendChild(std::move(setPositionButton))->SetClass("set-position-button", true);
		// remove row button
		Rml::ElementPtr remove = document->CreateElement("button");
		remove->AppendChild(std::move(document->CreateTextNode("-")));
		Rml::Element* removePtr = row->AppendChild(std::move(remove));
		removePtr->SetClass("remove-connection-button", true);
		// other data
		row->SetClass("connection-list-item", true);
		row->SetId("ConnectionListItem Id: " + std::to_string(endId));
		if (isInputBool) {
			Rml::Element* rowPtr = inputList->AppendChild(std::move(row));
			removePtr->AddEventListener(Rml::EventId::Click, new EventPasser(
				[this, rowPtr](Rml::Event& event) {
					inputList->RemoveChild(rowPtr);
				}
			));
		} else {
			Rml::Element* rowPtr = outputList->AppendChild(std::move(row));
			removePtr->AddEventListener(Rml::EventId::Click, new EventPasser(
				[this, rowPtr](Rml::Event& event) {
					outputList->RemoveChild(rowPtr);
				}
			));

		}
	}
}

void BlockCreationWindow::addListItem(bool isInput) {
	Circuit* circuit = mainWindow->getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
	if (!circuit) return;
	circuit_id_t id = circuit->getCircuitId();
	const CircuitBlockData* circuitBlockData = circuitManager->getCircuitBlockDataManager()->getCircuitBlockData(id);
	if (!circuitBlockData) return;
	const BlockData* blockData = circuitManager->getBlockDataManager()->getBlockData(circuitBlockData->getBlockType());
	if (!blockData) return;

	connection_end_id_t endId = 0;
	while (true) {
		while (blockData->connectionExists(endId)) ++endId;
		if (
			inputList->GetElementById("ConnectionListItem Id: " + std::to_string(endId)) == nullptr &&
			outputList->GetElementById("ConnectionListItem Id: " + std::to_string(endId)) == nullptr
		) break;
		++endId;
	}

	Rml::ElementPtr row = document->CreateElement("div");
	// name
	Rml::XMLAttributes nameAttributes;
	nameAttributes["type"] = "text";
	nameAttributes["maxlength"] = "7";
	nameAttributes["size"] = "7";
	Rml::ElementPtr name = Rml::Factory::InstanceElement(document, "input", "input", nameAttributes);
	Rml::ElementFormControlInput* nameElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(name.get());
	nameElement->SetValue("");
	// positionOnBlock
	Rml::XMLAttributes positionOnBlockAttributes;
	positionOnBlockAttributes["type"] = "text";
	positionOnBlockAttributes["maxlength"] = "3";
	positionOnBlockAttributes["size"] = "2";
	Rml::ElementPtr positionOnBlockX = Rml::Factory::InstanceElement(document, "input", "input", positionOnBlockAttributes);
	Rml::ElementPtr positionOnBlockY = Rml::Factory::InstanceElement(document, "input", "input", positionOnBlockAttributes);
	Rml::ElementFormControlInput* positionOnBlockXElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionOnBlockX.get());
	Rml::ElementFormControlInput* positionOnBlockYElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionOnBlockY.get());
	positionOnBlockXElement->SetValue(std::to_string(0));
	positionOnBlockYElement->SetValue(std::to_string(0));
	// position
	Rml::XMLAttributes positionAttributes;
	positionAttributes["type"] = "text";
	positionAttributes["maxlength"] = "6";
	positionAttributes["size"] = "5";
	Rml::ElementPtr positionX = Rml::Factory::InstanceElement(document, "input", "input", positionAttributes);
	Rml::ElementPtr positionY = Rml::Factory::InstanceElement(document, "input", "input", positionAttributes);
	Rml::ElementFormControlInput* positionXElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionX.get());
	Rml::ElementFormControlInput* positionYElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionY.get());
	positionXElement->SetValue("N/A");
	positionYElement->SetValue("N/A");

	Rml::ElementPtr setPositionButton = document->CreateElement("button");
	setPositionButton->AppendChild(std::move(document->CreateTextNode("S")));
	setPositionButton->AddEventListener(Rml::EventId::Click, new EventPasser(
		[this, endId](Rml::Event& event) {
			auto tool = std::dynamic_pointer_cast<PortSelector>(mainWindow->getActiveCircuitViewWidget()->getCircuitView()->getToolManager().selectTool(std::make_shared<PortSelector>()));
			if (tool) {
				tool->setPort(endId, [this, endId](Position position) {
					Rml::Element* row = document->GetElementById("ConnectionListItem Id: " + std::to_string(endId));
					Rml::ElementList elements;
					row->GetElementsByClassName(elements, "connection-list-item-pos-x");
					rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->SetValue(std::to_string(position.x));
					elements.clear();
					row->GetElementsByClassName(elements, "connection-list-item-pos-y");
					rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->SetValue(std::to_string(position.y));
				});
			}
		}
	));
	// add children and set classes
	row->AppendChild(std::move(name))->SetClass("connection-list-item-name", true);
	row->AppendChild(std::move(positionOnBlockX))->SetClass("connection-list-item-on-block-x", true);
	row->AppendChild(std::move(positionOnBlockY))->SetClass("connection-list-item-on-block-y", true);
	// Flex break element ensures second line starts after Port Y
	{
		Rml::ElementPtr breaker = document->CreateElement("div");
		row->AppendChild(std::move(breaker))->SetClass("connection-break", true);
	}
	row->AppendChild(std::move(positionX))->SetClass("connection-list-item-pos-x", true);
	row->AppendChild(std::move(positionY))->SetClass("connection-list-item-pos-y", true);
	row->AppendChild(std::move(setPositionButton))->SetClass("set-position-button", true);
	// remove row button
	Rml::ElementPtr remove = document->CreateElement("button");
	remove->AppendChild(std::move(document->CreateTextNode("-")));
	Rml::Element* removePtr = row->AppendChild(std::move(remove));
	removePtr->SetClass("remove-connection-button", true);
	// other data
	row->SetClass("connection-list-item", true);
	row->SetId("ConnectionListItem Id: " + std::to_string(endId));
	if (isInput) {
		Rml::Element* rowPtr = inputList->AppendChild(std::move(row));
		removePtr->AddEventListener(Rml::EventId::Click, new EventPasser(
			[this, rowPtr](Rml::Event& event) {
				inputList->RemoveChild(rowPtr);
			}
		));
	} else {
		Rml::Element* rowPtr = outputList->AppendChild(std::move(row));
		removePtr->AddEventListener(Rml::EventId::Click, new EventPasser(
			[this, rowPtr](Rml::Event& event) {
				outputList->RemoveChild(rowPtr);
			}
		));
	}
}

void BlockCreationWindow::makePaths(std::vector<std::vector<std::string>>& paths, std::vector<std::string>& path, const EvalAddressTree& addressTree) {
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

void BlockCreationWindow::updateSelected(std::string string) {
	std::vector<std::string> parts = stringSplit(string, '/');
	std::stringstream evalName(parts.front());
	std::string str;
	evaluator_id_t evalId;
	evalName >> str >> evalId;
	Address address;
	for (unsigned int i = 1; i < parts.size(); i++) {
		std::string part = parts[i];
		unsigned int index = part.size();
		while (index != 0 && part[index - 1] != '(') index--;
		std::stringstream posString(part.substr(index, part.size() - index - 1));
		Position position;
		char c;
		posString >> position.x >> c >> position.y;
		logInfo(position.toString());
		address.addBlockId(position);
	}

	CircuitView* circuitView = mainWindow->getActiveCircuitViewWidget()->getCircuitView();
	circuitView->setEvaluator(circuitView->getBackend(), evalId, address);
}
