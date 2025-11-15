#include "blockCreationWindow.h"

#include "SDL3/SDL_dialog.h"
#include "gui/helper/eventPasser.h"
#include "gui/mainWindow/circuitView/circuitViewWidget.h"
#include "gui/mainWindow/mainWindow.h"
#include "gui/viewportManager/circuitView/tools/other/portSelector.h"

#include "backend/circuit/circuitManager.h"
#include "backend/dataUpdateEventManager.h"

#include "environment/environment.h"
#include "util/algorithm.h"

void makeNumberInputFunc(Rml::Element* element) {
	element->AddEventListener("change", new EventPasser([element](Rml::Event& event) {
		Rml::String value = element->GetAttribute<Rml::String>("value", "");
		std::string correctValue;
		for (char c : value) {
			if ('0' <= c && c <= '9') {
				correctValue += c;
			}
		}
		element->SetInnerRML(std::string(correctValue.size(), ' '));
		element->SetAttribute<Rml::String>("value", correctValue);
	}));
}

void makeNumberInputFunc(Rml::Element* element, std::function<void(unsigned int)> callBack = nullptr) {
	element->AddEventListener("change", new EventPasser([element, callBack](Rml::Event& event) {
		Rml::String value = element->GetAttribute<Rml::String>("value", "");
		std::string correctValue;
		for (char c : value) {
			if ('0' <= c && c <= '9') {
				correctValue += c;
			}
		}
		element->SetInnerRML(std::string(correctValue.size(), ' '));
		element->SetAttribute<Rml::String>("value", correctValue);
		if (callBack && !correctValue.empty()) {
			std::stringstream ss(correctValue);
			unsigned int number = 0;
			ss >> number;
			callBack(number);
		}
	}));
}

void setNumberInputValue(Rml::Element* element, const std::string& number) {
	element->SetInnerRML(std::string(number.size(), ' '));
	element->SetAttribute<Rml::String>("value", number);
}

BlockCreationWindow::BlockCreationWindow(
	CircuitManager& circuitManager,
	Environment& environment,
	MainWindow& mainWindow,
	DataUpdateEventManager& dataUpdateEventManager,
	ToolManagerManager& toolManagerManager,
	Rml::ElementDocument* document,
	Rml::Element* menu
) :
	document(document), dataUpdateEventReceiver(dataUpdateEventManager), circuitManager(circuitManager), environment(environment), mainWindow(mainWindow),
	toolManagerManager(toolManagerManager) {
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
	addInputConnection->AddEventListener("click", new EventPasser([this](Rml::Event&) { this->addListItem(true); }));
	Rml::Element* addOutputConnection = menu->GetElementById("connection-list-add-output");
	addOutputConnection->AddEventListener("click", new EventPasser([this](Rml::Event&) { this->addListItem(false); }));
	// texture
	menu->GetElementById("pick-texture")->AddEventListener("click", new EventPasser([this](Rml::Event& event) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		// static const SDL_DialogFileFilter filters[] = {}; // error C2466: cannot allocate an array of constant size 0
		SDL_ShowOpenFileDialog([](void* userData, const char* const* filePaths, int filter) {
			if (!filePaths || !filePaths[0]) return;

			BlockCreationWindow* blockCreationWindow = (BlockCreationWindow*)userData;
			Circuit* circuit = blockCreationWindow->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
			if (!circuit || !(circuit->isEditable())) return;
			BlockData* blockData = blockCreationWindow->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());

			std::string filePath = filePaths[0];
			if (filePath.empty()) return;
			blockData->setTexturePath(filePath);

			blockCreationWindow->menu->GetElementById("block-texture-menu-no-texture")->SetClass("invisible", true);
			blockCreationWindow->menu->GetElementById("block-texture-menu-has-texture")->SetClass("invisible", false);
		}, this, nullptr, nullptr, 0, nullptr, true);
	}));
	menu->GetElementById("pick-texture")->AddEventListener("dropFile", new EventPasser([this](Rml::Event& event) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());

		std::string filePath = event.GetParameter<Rml::String>("file_path", "");
		if (filePath.empty()) return;
		blockData->setTexturePath(filePath);

		this->menu->GetElementById("block-texture-menu-no-texture")->SetClass("invisible", true);
		this->menu->GetElementById("block-texture-menu-has-texture")->SetClass("invisible", false);
	}));
	menu->GetElementById("pick-new-texture")->AddEventListener("click", new EventPasser([this](Rml::Event& event) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		// static const SDL_DialogFileFilter filters[] = {}; // error C2466: cannot allocate an array of constant size 0
		SDL_ShowOpenFileDialog([](void* userData, const char* const* filePaths, int filter) {
			if (!filePaths || !filePaths[0]) return;

			BlockCreationWindow* blockCreationWindow = (BlockCreationWindow*)userData;
			Circuit* circuit = blockCreationWindow->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
			if (!circuit || !(circuit->isEditable())) return;
			BlockData* blockData = blockCreationWindow->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());

			std::string filePath = filePaths[0];
			blockData->setTexturePath(filePath);
		}, this, nullptr, nullptr, 0, nullptr, true);
	}));
	menu->GetElementById("pick-new-texture")->AddEventListener("dropFile", new EventPasser([this](Rml::Event& event) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());

		std::string filePath = event.GetParameter<Rml::String>("file_path", "");
		if (filePath.empty()) return;
		blockData->setTexturePath(filePath);
	}));
	menu->GetElementById("reload-texture")->AddEventListener("click", new EventPasser([this](Rml::Event& event) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		this->environment.getBlockRenderDataFeeder().refreshBlockTexture(circuit->getBlockType());
	}));
	menu->GetElementById("remove-texture")->AddEventListener("click", new EventPasser([this](Rml::Event& event) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setTexturePath("");
		this->menu->GetElementById("block-texture-menu-no-texture")->SetClass("invisible", false);
		this->menu->GetElementById("block-texture-menu-has-texture")->SetClass("invisible", true);
	}));
	// menu->GetElementById("embed-texture")->AddEventListener("click", new EventPasser([this](Rml::Event& event){
	// 	Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
	// 	if (!circuit || !(circuit->isEditable())) return;
	// 	logError("Embed texture not supported yet.");
	// }));
	menu->GetElementById("texture-uses-tilemap")->AddEventListener("click", new EventPasser([this](Rml::Event& event) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) {
			this->menu->GetElementById("texture-uses-tilemap")->SetClass("checked", false);
			return;
		}
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setUsesTileMapTexture(!blockData->getUsesTileMapTexture());

		this->menu->GetElementById("block-texture-tile-menu")->SetClass("invisible", !blockData->getUsesTileMapTexture());
		this->menu->GetElementById("texture-uses-tilemap")->SetClass("checked", blockData->getUsesTileMapTexture());
	}));

	makeNumberInputFunc(menu->GetElementById("block-texture-tile-size-x"), [this](int x) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setTextureTileSize({ x, blockData->getTextureTileSize().y });
	});
	makeNumberInputFunc(menu->GetElementById("block-texture-tile-size-y"), [this](int y) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setTextureTileSize({ blockData->getTextureTileSize().x, y });
	});
	makeNumberInputFunc(menu->GetElementById("block-texture-smallest-cord-tile-x"), [this](int x) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setTextureSmallestCordTile({ x, blockData->getTextureSmallestCordTile().y });
	});
	makeNumberInputFunc(menu->GetElementById("block-texture-smallest-cord-tile-y"), [this](int y) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setTextureSmallestCordTile({ blockData->getTextureSmallestCordTile().x, y });
	});
	makeNumberInputFunc(menu->GetElementById("block-texture-block-tile-size-x"), [this](int x) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setTextureBlockTileSize({ x, blockData->getTextureBlockTileSize().y });
	});
	makeNumberInputFunc(menu->GetElementById("block-texture-block-tile-size-y"), [this](int y) {
		Circuit* circuit = this->mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
		if (!circuit || !(circuit->isEditable())) return;
		BlockData* blockData = this->circuitManager.getBlockDataManager().getBlockData(circuit->getBlockType());
		blockData->setTextureBlockTileSize({ blockData->getTextureBlockTileSize().x, y });
	});

	// dataUpdateEvents
	dataUpdateEventReceiver.linkFunction("blockDataUpdate", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("circuitViewChangeCircuit", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("blockDataSetConnection", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("blockDataRemoveConnection", std::bind(&BlockCreationWindow::resetMenu, this));
	dataUpdateEventReceiver.linkFunction("circuitBlockDataConnectionPositionSet", std::bind(&BlockCreationWindow::resetMenu, this));
	resetMenu();
}

constexpr float edgeDistanceIC = 0.4f;

void BlockCreationWindow::updateFromMenu() {
	Circuit* circuit = mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
	if (!circuit || !(circuit->isEditable())) return;
	circuit_id_t id = circuit->getCircuitId();
	CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(id);
	BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
	std::string name;
	Size size;
	std::vector<std::tuple<connection_end_id_t, std::string, bool, Vector, FVector, Position, unsigned int>> portsData;
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
			connection_end_id_t endId = connection_end_id_t(std::stoi(rowId.substr(23, rowId.size() - 23)));
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
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-switch-port-offset");
			char portOffset = rmlui_dynamic_cast<Rml::ElementText*>(elements.front()->GetChild(0))->GetText().front();
			FVector portOffsetVec = FVector(0.5f, 0.5f);
			switch (portOffset) {
			case 'L': portOffsetVec = FVector(0.5f - edgeDistanceIC, 0.5f/* - sideShift*/); break;
			case 'R': portOffsetVec = FVector(0.5f + edgeDistanceIC, 0.5f/* + sideShift*/); break;
			case 'U': portOffsetVec = FVector(0.5f/* - sideShift*/, 0.5f - edgeDistanceIC); break;
			case 'D': portOffsetVec = FVector(0.5f/* + sideShift*/, 0.5f + edgeDistanceIC); break;
			}
			// get port block position
			Position portBlockPosition;
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-x");
			portBlockPosition.x = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-y");
			portBlockPosition.y = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());

			// bitWidth
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-bit-width");
			unsigned int bitWidth = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());

			portsData.emplace_back(endId, portName, true, portPositionOnBlock, portOffsetVec, portBlockPosition, bitWidth);
		}
		for (unsigned int i = 0; i < outputList->GetNumChildren(); i++) {
			Rml::Element* row = outputList->GetChild(i);
			const std::string& rowId = row->GetId();
			// get connection end id
			connection_end_id_t endId = connection_end_id_t(std::stoi(rowId.substr(23, rowId.size() - 23)));
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
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-switch-port-offset");
			char portOffset = rmlui_dynamic_cast<Rml::ElementText*>(elements.front()->GetChild(0))->GetText().front();
			FVector portOffsetVec = FVector(0.5f, 0.5f);
			switch (portOffset) {
			case 'L': portOffsetVec = FVector(0.5f - edgeDistanceIC, 0.5f/* - sideShift*/); break;
			case 'R': portOffsetVec = FVector(0.5f + edgeDistanceIC, 0.5f/* + sideShift*/); break;
			case 'U': portOffsetVec = FVector(0.5f/* - sideShift*/, 0.5f - edgeDistanceIC); break;
			case 'D': portOffsetVec = FVector(0.5f/* + sideShift*/, 0.5f + edgeDistanceIC); break;
			}
			// get port block position
			Position portBlockPosition;
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-x");
			portBlockPosition.x = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-pos-y");
			portBlockPosition.y = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());
			// bitWidth
			elements.clear();
			row->GetElementsByClassName(elements, "connection-list-item-bit-width");
			unsigned int bitWidth = std::stoi(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->GetValue());

			portsData.emplace_back(endId, portName, false, portPositionOnBlock, portOffsetVec, portBlockPosition, bitWidth);
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
		unsigned int bitWidth = std::get<6>(row);
		if (bitWidth == 0) {
			logWarning("Can't update block data. Port bit width {} has to be more than 0.", "BlockCreationWindow", bitWidth);
			return;
		}
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
		FVector portOffset = std::get<4>(row);
		Position portBlockPosition = std::get<5>(row);
		unsigned int bitWidth = std::get<6>(row);
		if (blockData->connectionExists(endId)) {
			if (blockData->isConnectionInput(endId) != portIsInput) blockData->removeConnection(endId);
		}

		if (portIsInput) blockData->setConnectionInput(portPositionOnBlock, endId);
		else blockData->setConnectionOutput(portPositionOnBlock, endId);
		if (!portName.empty()) blockData->setConnectionIdName(endId, portName);

		blockData->setConnectionBitConfiguration(endId, bitWidth);
		blockData->setConnnectionPortOffset(endId, portOffset);

		circuitBlockData->setConnectionIdPosition(endId, portBlockPosition);
	}
}

void BlockCreationWindow::resetMenu() {
	// std::vector<std::pair<std::string, connection_end_id_t>> paths;
	// clear list
	while (inputList->GetNumChildren() > 0) inputList->RemoveChild(inputList->GetChild(0));
	while (outputList->GetNumChildren() > 0) outputList->RemoveChild(outputList->GetChild(0));
	if (!mainWindow.getActiveCircuitViewWidget()) return; // nothing to show
	Circuit* circuit = mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
	if (!circuit) return;
	circuit_id_t id = circuit->getCircuitId();
	Rml::Element* ele = menu->GetElementById("name-input");
	Rml::ElementFormControlInput* nameElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
	nameElement->SetValue(circuit->getCircuitName());

	const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(id);
	if (!circuitBlockData) return;
	const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
	if (!blockData) return;

	ele = menu->GetElementById("width-input");
	Rml::ElementFormControlInput* widthElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
	widthElement->SetValue(std::to_string(blockData->getSize().w));
	ele = menu->GetElementById("height-input");
	Rml::ElementFormControlInput* heightElement = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(ele);
	heightElement->SetValue(std::to_string(blockData->getSize().h));

	if (blockData->getTexturePath() == "") {
		menu->GetElementById("block-texture-menu-no-texture")->SetClass("invisible", false);
		menu->GetElementById("block-texture-menu-has-texture")->SetClass("invisible", true);
	} else {
		menu->GetElementById("block-texture-menu-no-texture")->SetClass("invisible", true);
		menu->GetElementById("block-texture-menu-has-texture")->SetClass("invisible", false);
		if (blockData->getUsesTileMapTexture()) {
			menu->GetElementById("texture-uses-tilemap")->SetClass("checked", false);
			menu->GetElementById("block-texture-tile-menu")->SetClass("invisible", false);
		} else {
			menu->GetElementById("texture-uses-tilemap")->SetClass("checked", true);
			menu->GetElementById("block-texture-tile-menu")->SetClass("invisible", true);
		}
		setNumberInputValue(menu->GetElementById("block-texture-tile-size-x"), std::to_string(blockData->getTextureTileSize().x));
		setNumberInputValue(menu->GetElementById("block-texture-tile-size-y"), std::to_string(blockData->getTextureTileSize().y));
		setNumberInputValue(menu->GetElementById("block-texture-smallest-cord-tile-x"), std::to_string(blockData->getTextureSmallestCordTile().x));
		setNumberInputValue(menu->GetElementById("block-texture-smallest-cord-tile-y"), std::to_string(blockData->getTextureSmallestCordTile().y));
		setNumberInputValue(menu->GetElementById("block-texture-block-tile-size-x"), std::to_string(blockData->getTextureBlockTileSize().x));
		setNumberInputValue(menu->GetElementById("block-texture-block-tile-size-y"), std::to_string(blockData->getTextureBlockTileSize().y));
	}

	const std::unordered_map<connection_end_id_t, BlockData::ConnectionData>& conncections = blockData->getConnections();
	for (auto& iter : conncections) {
		connection_end_id_t endId = iter.first;
		bool isInputBool = iter.second.portType == BlockData::ConnectionData::INPUT;
		Vector positionOnBlock = iter.second.positionOnBlock;
		std::optional<std::string> connectionName = blockData->getConnectionIdToName(endId);
		const Position* positionPtr = circuitBlockData->getConnectionIdToPosition(endId);
		char portOffset = 'C';
		if (iter.second.portOffset == FVector(0.5f - edgeDistanceIC, 0.5f/* - sideShift*/)) portOffset = 'L';
		else if (iter.second.portOffset == FVector(0.5f + edgeDistanceIC, 0.5f/* + sideShift*/)) portOffset = 'R';
		else if (iter.second.portOffset == FVector(0.5f/* - sideShift*/, 0.5f - edgeDistanceIC)) portOffset = 'U';
		else if (iter.second.portOffset == FVector(0.5f/* + sideShift*/, 0.5f + edgeDistanceIC)) portOffset = 'D';
		addListItem(
			isInputBool,
			false,
			endId.get(),
			connectionName.value_or(""),
			positionOnBlock,
			portOffset,
			positionPtr ? std::optional<Position>(*positionPtr) : std::nullopt,
			iter.second.getBitWidth()
		);
	}
}

void BlockCreationWindow::addListItem(
	bool isInput,
	bool findUnusedEndId,
	unsigned int endId,
	const std::string& nameValue,
	Vector posOnBlockValue,
	char portOffsetValue,
	std::optional<Position> posOfBlockValue,
	unsigned int bitWidthValue
) {
	Circuit* circuit = mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getCircuit();
	if (!circuit) return;
	circuit_id_t id = circuit->getCircuitId();
	const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(id);
	if (!circuitBlockData) return;
	const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
	if (!blockData) return;

	if (findUnusedEndId) {
		while (true) {
			while (blockData->connectionExists(connection_end_id_t(endId))) ++endId;
			if (inputList->GetElementById("ConnectionListItem Id: " + std::to_string(endId)) == nullptr &&
				outputList->GetElementById("ConnectionListItem Id: " + std::to_string(endId)) == nullptr)
				break;
			++endId;
		}
	}

	Rml::ElementPtr row = document->CreateElement("div");
	// name
	Rml::XMLAttributes nameAttributes;
	nameAttributes["type"] = "text";
	nameAttributes["maxlength"] = "7";
	nameAttributes["size"] = "7";
	Rml::ElementPtr name = Rml::Factory::InstanceElement(document, "input", "input", nameAttributes);
	Rml::ElementFormControlInput* nameFormControl = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(name.get());
	nameFormControl->SetValue(nameValue);
	// positionOnBlock
	Rml::XMLAttributes positionOnBlockAttributes;
	positionOnBlockAttributes["type"] = "text";
	positionOnBlockAttributes["maxlength"] = "3";
	positionOnBlockAttributes["size"] = "2";
	Rml::ElementPtr positionOnBlockX = Rml::Factory::InstanceElement(document, "input", "input", positionOnBlockAttributes);
	Rml::ElementPtr positionOnBlockY = Rml::Factory::InstanceElement(document, "input", "input", positionOnBlockAttributes);
	Rml::ElementFormControlInput* positionOnBlockXFormControl = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionOnBlockX.get());
	Rml::ElementFormControlInput* positionOnBlockYFormControl = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionOnBlockY.get());
	positionOnBlockXFormControl->SetValue(std::to_string(posOnBlockValue.dx));
	positionOnBlockYFormControl->SetValue(std::to_string(posOnBlockValue.dy));
	Rml::ElementPtr switchPortOffset = document->CreateElement("button");
	switchPortOffset->AppendChild(std::move(document->CreateTextNode(std::string(1, portOffsetValue))));
	switchPortOffset->AddEventListener(Rml::EventId::Click, new EventPasser([this, endId](Rml::Event& event) {
		Rml::Element* row = document->GetElementById("ConnectionListItem Id: " + std::to_string(endId));
		if (row == nullptr) return;
		Rml::ElementList elements;
		row->GetElementsByClassName(elements, "connection-list-item-switch-port-offset");
		Rml::ElementText* textElement = rmlui_dynamic_cast<Rml::ElementText*>(elements.front()->GetChild(0));
		if (!textElement) return;
		char portOffset = textElement->GetText().front();
		switch (portOffset) {
		case 'C': portOffset = 'L'; break;
		case 'L': portOffset = 'R'; break;
		case 'R': portOffset = 'U'; break;
		case 'U': portOffset = 'D'; break;
		case 'D': portOffset = 'C'; break;
		}
		textElement->SetText(std::string(1, portOffset));
	}));
	// position
	Rml::XMLAttributes positionAttributes;
	positionAttributes["type"] = "text";
	positionAttributes["maxlength"] = "6";
	positionAttributes["size"] = "5";
	Rml::ElementPtr positionX = Rml::Factory::InstanceElement(document, "input", "input", positionAttributes);
	Rml::ElementPtr positionY = Rml::Factory::InstanceElement(document, "input", "input", positionAttributes);
	Rml::ElementFormControlInput* positionXFormControl = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionX.get());
	Rml::ElementFormControlInput* positionYFormControl = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(positionY.get());
	if (posOfBlockValue.has_value()) {
		positionXFormControl->SetValue(std::to_string(posOfBlockValue->x));
		positionYFormControl->SetValue(std::to_string(posOfBlockValue->y));
	} else {
		positionXFormControl->SetValue("N/A");
		positionYFormControl->SetValue("N/A");
	}
	// bit width
	Rml::XMLAttributes bitWidthAttributes;
	bitWidthAttributes["type"] = "text";
	bitWidthAttributes["maxlength"] = "2";
	bitWidthAttributes["size"] = "4";
	Rml::ElementPtr bitWidth = Rml::Factory::InstanceElement(document, "input", "input", positionAttributes);
	Rml::ElementFormControlInput* bitWidthFormControl = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(bitWidth.get());
	bitWidthFormControl->SetValue(std::to_string(bitWidthValue));

	Rml::ElementPtr setPositionButton = document->CreateElement("button");
	setPositionButton->AppendChild(std::move(document->CreateTextNode("S")));
	setPositionButton->AddEventListener(Rml::EventId::Click, new EventPasser([this, endId](Rml::Event& event) {
		auto tool = std::dynamic_pointer_cast<PortSelector>(
			mainWindow.getActiveCircuitViewWidget()->getCircuitView()->getToolManager().selectTool(std::make_shared<PortSelector>(environment))
		);
		if (tool) {
			tool->setPort(connection_end_id_t(endId), [this, endId](Position position) {
				Rml::Element* row = document->GetElementById("ConnectionListItem Id: " + std::to_string(endId));
				if (row == nullptr) return;
				Rml::ElementList elements;
				row->GetElementsByClassName(elements, "connection-list-item-pos-x");
				rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->SetValue(std::to_string(position.x));
				elements.clear();
				row->GetElementsByClassName(elements, "connection-list-item-pos-y");
				rmlui_dynamic_cast<Rml::ElementFormControlInput*>(elements.front())->SetValue(std::to_string(position.y));
			});
		}
	}));

	row->SetClass("connection-list-item", true);
	row->SetId("ConnectionListItem Id: " + std::to_string(endId));
	// add children and set classes
	Rml::Element* rowInRow = row->AppendChild(document->CreateElement("div"));
	rowInRow->AppendChild(std::move(document->CreateElement("h1")))->AppendChild(std::move(document->CreateTextNode("Name")));
	rowInRow->AppendChild(std::move(name))->SetClass("connection-list-item-name", true);
	rowInRow = row->AppendChild(document->CreateElement("div"));
	rowInRow->AppendChild(std::move(document->CreateElement("h1")))->AppendChild(std::move(document->CreateTextNode("Port Pos")));
	rowInRow->AppendChild(std::move(positionOnBlockX))->SetClass("connection-list-item-on-block-x", true);
	rowInRow->AppendChild(std::move(positionOnBlockY))->SetClass("connection-list-item-on-block-y", true);
	rowInRow->AppendChild(std::move(switchPortOffset))->SetClass("connection-list-item-switch-port-offset", true);
	rowInRow = row->AppendChild(document->CreateElement("div"));
	rowInRow->AppendChild(std::move(document->CreateElement("h1")))->AppendChild(std::move(document->CreateTextNode("Block Pos")));
	rowInRow->AppendChild(std::move(positionX))->SetClass("connection-list-item-pos-x", true);
	rowInRow->AppendChild(std::move(positionY))->SetClass("connection-list-item-pos-y", true);
	rowInRow->AppendChild(std::move(setPositionButton))->SetClass("set-position-button", true);
	rowInRow = row->AppendChild(document->CreateElement("div"));
	rowInRow->AppendChild(std::move(document->CreateElement("h1")))->AppendChild(std::move(document->CreateTextNode("Bit width")));
	rowInRow->AppendChild(std::move(bitWidth))->SetClass("connection-list-item-bit-width", true);
	// remove row button
	Rml::ElementPtr remove = document->CreateElement("button");
	remove->AppendChild(std::move(document->CreateTextNode("-")));
	remove->SetClass("remove-connection-button", true);
	if (isInput) {
		Rml::Element* rowPtr = inputList->AppendChild(std::move(row));
		remove->AddEventListener(Rml::EventId::Click, new EventPasser([this, rowPtr](Rml::Event& event) { inputList->RemoveChild(rowPtr); }));
	} else {
		Rml::Element* rowPtr = outputList->AppendChild(std::move(row));
		remove->AddEventListener(Rml::EventId::Click, new EventPasser([this, rowPtr](Rml::Event& event) { outputList->RemoveChild(rowPtr); }));
	}
	rowInRow->AppendChild(std::move(remove));
}

void BlockCreationWindow::makePaths(std::vector<std::vector<std::string>>& paths, std::vector<std::string>& path, const EvalAddressTree& addressTree) {
	auto& branches = addressTree.getBranches();
	if (branches.empty()) {
		paths.push_back(path);
	} else {
		for (auto& pair : branches) {
			path.push_back(circuitManager.getCircuit(pair.second.getContainerId())->getCircuitName() + pair.first.toString());
			makePaths(paths, path, pair.second);
			path.pop_back();
		}
	}
}

void BlockCreationWindow::updateSelected(std::string string) {
	std::vector<std::string> parts = stringSplit(string, '/');
	std::stringstream evalName(parts.front());
	std::string str;
	unsigned int evalId;
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

	CircuitView* circuitView = mainWindow.getActiveCircuitViewWidget()->getCircuitView();
	circuitView->setEvaluator(evaluator_id_t(evalId), address);
}
