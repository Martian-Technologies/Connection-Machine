#include "blockSelectorWidget.h"

#include "../mainWindow.h"

#include "app.h"
#include "gui/mainWindow/widgets/circuitViewWidget.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_stdlib.h"

BlockSelectorWidget::BlockSelectorWidget(WidgetId widgetId, MainWindow& mainWindow) :
	Widget(widgetId, mainWindow), dataUpdateEventReceiver(getBackend().getDataUpdateEventManager()) {
	{
		// =================================== Init all tree data ===================================
		std::lock_guard mux(pathsMux);
		const BlockDataManager& blockDataManager = getBackend().getBlockDataManager();
		const CircuitBlockDataManager& circuitBlockDataManager = getBackend().getCircuitManager().getCircuitBlockDataManager();
		for (unsigned int type = 0; type < blockDataManager.maxBlockId(); type++) {
			const BlockData* blockData = blockDataManager.getBlockData((BlockType)(type + 1));
			if (blockData) {
				if (!blockData->isPlaceable()) continue;;
				circuit_id_t circuitId = circuitBlockDataManager.getCircuitId((BlockType)(type + 1));
				if (circuitId == 0) {
					addPath(blockData->getPath() + "/" + blockData->getName(), (BlockType)(type + 1));
				} else {
					addPath(blockData->getPath() + "/" + blockData->getName(), std::make_pair((BlockType)(type + 1), circuitId));
				}
			}
		}
		// Procedural Circuits
		for (const auto& iter : getBackend().getCircuitManager().getProceduralCircuitManager().getProceduralCircuits()) {
			addPath(iter.second->getPath(), iter.second->getUUID());
		}
		addPath("Other/Bus", "Other/Bus");
	}

	dataUpdateEventReceiver.linkFunction("blockDataUpdate", [this](const DataUpdateEventManager::EventData* event) {
		const DataUpdateEventManager::EventDataWithValue<BlockType>* data = event->cast<BlockType>();
		assert(data);
		const BlockData* blockData = getBackend().getBlockDataManager().getBlockData(data->get());
		circuit_id_t circuitId = getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitId(data->get());
		std::lock_guard mux(pathsMux);
		if (!blockData->isPlaceable()) {
			if (circuitId == 0) removePath(blockData->getPath() + "/" + blockData->getName(), data->get());
			removePath(blockData->getPath() + "/" + blockData->getName(), std::make_pair(data->get(), circuitId));
			return;
		}
		if (circuitId == 0) addPath(blockData->getPath() + "/" + blockData->getName(), data->get());
		else addPath(blockData->getPath() + "/" + blockData->getName(), std::make_pair(data->get(), circuitId));
	});
	dataUpdateEventReceiver.linkFunction("proceduralCircuitPathUpdate", [this](const DataUpdateEventManager::EventData* event) {
		const DataUpdateEventManager::EventDataWithValue<std::string>* data = event->cast<std::string>();
		assert(data);
		SharedProceduralCircuit proceduralCircuit = getBackend().getCircuitManager().getProceduralCircuitManager().getProceduralCircuit(data->get());
		std::lock_guard mux(pathsMux);
		addPath(proceduralCircuit->getPath(), proceduralCircuit->getPath());
	});
	dataUpdateEventReceiver.linkFunction("setToolBlockTypeUpdate", [this](const DataUpdateEventManager::EventData* event) {
		const DataUpdateEventManager::EventDataWithValue<BlockType>* data = event->cast<BlockType>();
		assert(data);
		circuit_id_t circuitId = getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitId(data->get());
		if (circuitId != 0) {
			const CircuitBlockData* circuitBlockData = getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
			if (circuitBlockData->getProceduralCircuitUUID().has_value()) {
				setGUIValue<std::string>("selectedProceduralCircuitOrBus", circuitBlockData->getProceduralCircuitUUID().value());
				setGUIValue<BlockType>("selectedBlockType", BlockType::NONE);
			}
		}
		setGUIValue<std::string>("selectedProceduralCircuitOrBus", "");
		setGUIValue<BlockType>("selectedBlockType", data->get());
	});
	setupGUIValue<BlockType>("selectedBlockType", BlockType::NONE, [this](const BlockType& blockType) { getMainWindow().getToolManagerManager().setBlock(blockType); });
	setupGUIValue<std::string>("selectedProceduralCircuitOrBus", "", [this](const std::string& path) {
		std::lock_guard lock(proceduralCircuitOrBusParameterMux);
		auto pair = proceduralCircuitOrBusParameter.try_emplace(path);
		if (!pair.second) return;
		if (path == "Other/Bus") return;
		const std::string* uuid = getBackend().getCircuitManager().getProceduralCircuitManager().getProceduralCircuitUUID(path);
		if (uuid == nullptr) return;
		SharedProceduralCircuit proceduralCircuit = getBackend().getCircuitManager().getProceduralCircuitManager().getProceduralCircuit(*uuid);
		if (proceduralCircuit == nullptr) return;
		pair.first->second = proceduralCircuit->getParameterDefaults().parameters;
	});
	setupGUIValue<bool>("doCreateBlock", false, [this](const bool& doCreateBlock) {
		if (doCreateBlock) {
			const std::string& selectedProceduralCircuitOrBus = getGUIValue<std::string>("selectedProceduralCircuitOrBus");
			if (selectedProceduralCircuitOrBus == "") return;
			if (selectedProceduralCircuitOrBus == "Other/Bus") {
				std::lock_guard lock(proceduralCircuitOrBusParameterMux);
				auto iter = proceduralCircuitOrBusParameter.find("Other/Bus");
				assert(iter != proceduralCircuitOrBusParameter.end());
				getMainWindow().getToolManagerManager().setBlock(getBackend().getBlockDataManager().getBusBlock(
					std::get<unsigned int>(iter->second.at("Number of Ports")),
					std::get<unsigned int>(iter->second.at("Port Bit Widths"))
				));
			} else {
				std::lock_guard lock(proceduralCircuitOrBusParameterMux);
				auto iter = proceduralCircuitOrBusParameter.find(selectedProceduralCircuitOrBus);
				assert(iter != proceduralCircuitOrBusParameter.end());
				const std::string* uuid = getBackend().getCircuitManager().getProceduralCircuitManager().getProceduralCircuitUUID(selectedProceduralCircuitOrBus);
				assert(uuid);
				SharedProceduralCircuit proceduralCircuit = getBackend().getCircuitManager().getProceduralCircuitManager().getProceduralCircuit(*uuid);
				assert(proceduralCircuit);
				ProceduralCircuitParameters parameters;
				parameters.parameters = iter->second;
				circuit_id_t circuitId = proceduralCircuit->getCircuitId(parameters);
				const CircuitBlockData* circuitBlockData = getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
				assert(circuitBlockData);
				getMainWindow().getToolManagerManager().setBlock(circuitBlockData->getBlockType());
			}
		}
	});
}

void BlockSelectorWidget::addPath(const std::string& path, const std::variant<BlockType, std::string, std::pair<BlockType, circuit_id_t>>& data) {
	auto pair = paths.emplace(data, path);
	if (!pair.second) {
		if (pair.first->second == path) return;
		root.removePath(pair.first->second);
		pair.first->second = path;
	} else if (std::holds_alternative<std::pair<BlockType, circuit_id_t>>(data)) {
		auto iter = paths.find(std::get<std::pair<BlockType, circuit_id_t>>(data).first);
		if (iter != paths.end()) {
			root.removePath(iter->second);
			paths.erase(iter);
		}
	}
	root.addPath(path, data);
}

void BlockSelectorWidget::removePath(const std::string& path, const std::variant<BlockType, std::string, std::pair<BlockType, circuit_id_t>>& data) {
	auto iter = paths.find(data);
	if (iter != paths.end()) {
		root.removePath(iter->second);
		paths.erase(iter);
	} else if (std::holds_alternative<std::pair<BlockType, circuit_id_t>>(data)) {
		iter = paths.find(std::get<std::pair<BlockType, circuit_id_t>>(data).first);
		if (iter != paths.end()) {
			root.removePath(iter->second);
			paths.erase(iter);
		}
	}
}

bool ArrowButtonColor(const char* id, ImGuiDir dir) {
	ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(-1, -1));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::InvisibleButton(id, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
	ImGui::PopStyleVar();

    ImU32 col;
    if (ImGui::IsItemActive()) col = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    else if (ImGui::IsItemHovered())col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    else col = ImGui::GetColorU32(ImGuiCol_Text);

	ImVec2 pos = ImGui::GetItemRectMin() + ImGui::GetItemRectSize() / 2 - ImVec2(ImGui::GetFontSize() / 2, ImGui::GetFontSize() / 2);

    ImGui::RenderArrow(
        ImGui::GetWindowDrawList(),
        pos,
        col,
        dir,
        1
    );

    return ImGui::IsItemClicked();
}

void BlockSelectorWidget::createTree(const SelectorTreeNode& node) {
	assert(!node.children.empty());
	if (node.children.begin()->second.data.has_value()) {
		for (auto& pair : node.children) {
			// leaf (never children)
			assert(pair.second.children.empty());
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
			if (std::holds_alternative<BlockType>(pair.second.data.value())) {
				if (getGUIValue_rendering<BlockType>("selectedBlockType") == std::get<BlockType>(pair.second.data.value())) {
					flags |= ImGuiTreeNodeFlags_Selected;
				}
			} else if (std::holds_alternative<std::pair<BlockType, circuit_id_t>>(pair.second.data.value())) {
				const std::pair<BlockType, circuit_id_t>& blockTypeCircuitIdPair = std::get<std::pair<BlockType, circuit_id_t>>(pair.second.data.value());
				if (getGUIValue_rendering<BlockType>("selectedBlockType") == blockTypeCircuitIdPair.first) {
					flags |= ImGuiTreeNodeFlags_Selected;
				}
			} else {
				if (getGUIValue_rendering<std::string>("selectedProceduralCircuitOrBus") == std::get<std::string>(pair.second.data.value())) {
					flags |= ImGuiTreeNodeFlags_Selected;
				};
			}

			ImGui::SetNextItemAllowOverlap();
			if (ImGui::TreeNodeEx(pair.first.c_str(), flags)) {
				if (ImGui::IsItemClicked()) {
					if (std::holds_alternative<BlockType>(pair.second.data.value())) {
						setGUIValue_rendering<BlockType>("selectedBlockType", std::get<BlockType>(pair.second.data.value()));
						setGUIValue_rendering<std::string>("selectedProceduralCircuitOrBus", "");
					} else if (std::holds_alternative<std::pair<BlockType, circuit_id_t>>(pair.second.data.value())) {
						const std::pair<BlockType, circuit_id_t>& blockTypeCircuitIdPair = std::get<std::pair<BlockType, circuit_id_t>>(pair.second.data.value());
						setGUIValue_rendering<BlockType>("selectedBlockType", blockTypeCircuitIdPair.first);
						setGUIValue_rendering<std::string>("selectedProceduralCircuitOrBus", "");
					} else {
						const std::string& data = std::get<std::string>(pair.second.data.value());
						setGUIValue_rendering<BlockType>("selectedBlockType", BlockType::NONE);
						setGUIValue_rendering<std::string>("selectedProceduralCircuitOrBus", data);
					}
				}
				if (std::holds_alternative<std::pair<BlockType, circuit_id_t>>(pair.second.data.value())) {
					ImGui::SameLine();
					if (ArrowButtonColor("##ViewButton", ImGuiDir_Right)) {
						const std::pair<BlockType, circuit_id_t>& blockTypeCircuitIdPair = std::get<std::pair<BlockType, circuit_id_t>>(pair.second.data.value());
						App::runOnMain([this, circuitId = blockTypeCircuitIdPair.second]() {
							CircuitViewWidget& circuitViewWidget = getMainWindow().createWidget<CircuitViewWidget>();
							for (auto& sim : getMainWindow().getEnvironment().getBackend().getSimulatorManager().getSimulators()) {
								if (sim.second->getCircuitId() == circuitId) {
									circuitViewWidget.getCircuitView().setSimulator(sim.second->getSimulatorId());
									return;
								}
							}
							circuitViewWidget.getCircuitView().setCircuit(circuitId);
						});
					}
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
						ImGui::SetTooltip("View");
					}
				}
				ImGui::TreePop();
			}
		}
	} else {
		for (auto& pair : node.children) {
			assert(!pair.second.data.has_value());
			// non-leaf (sometimes children)
			if (ImGui::TreeNodeEx(pair.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				createTree(pair.second);
				ImGui::TreePop();
			}
		}
	}
}

unsigned int slowStep = 1;
unsigned int fastStep = 10;

void BlockSelectorWidget::render() {
	ImGui::SetNextWindowDockID(getMainWindow().getDockLeftId(), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(100, 300), ImGuiCond_FirstUseEver);
	getMainWindow().setNextWindowSideBarDockable();
	bool open = true;
	getMainWindow().pushWindowStyling();
	if (ImGui::Begin(("Blocks###" + getWidgetIdStr()).c_str(), &open)) {
		getMainWindow().popWindowStyling();
		const std::string& selectedProceduralCircuitOrBus = getGUIValue_rendering<std::string>("selectedProceduralCircuitOrBus");
		if (selectedProceduralCircuitOrBus != "") {
			if (selectedProceduralCircuitOrBus == "Other/Bus") {
				if (ImGui::BeginChild("Bus Creator", { 0, 0 }, ImGuiChildFlags_AutoResizeY)) {
					std::lock_guard lock(proceduralCircuitOrBusParameterMux);
					auto pair = proceduralCircuitOrBusParameter.try_emplace("Other/Bus");
					auto numberOfPorts = pair.first->second.emplace("Number of Ports", (unsigned int)8);
					assert(std::holds_alternative<unsigned int>(numberOfPorts.first->second));
					ImGui::InputScalar("Number of Ports", ImGuiDataType_U32, &std::get<unsigned int>(numberOfPorts.first->second), &slowStep, &fastStep);
					auto portBitWidths = pair.first->second.emplace("Port Bit Widths", (unsigned int)1);
					ImGui::InputScalar("Port Bit Widths", ImGuiDataType_U32, &std::get<unsigned int>(portBitWidths.first->second), &slowStep, &fastStep);
					bool a = ImGui::Button("Create Bus");
					setGUIValue_rendering<bool>("doCreateBlock", a);
				}
				ImGui::EndChild();
			} else {
				if (ImGui::BeginChild("Procedural Circuit", { 0, 0 }, ImGuiChildFlags_AutoResizeY)) {
					ImGui::Text("(%s)", selectedProceduralCircuitOrBus.c_str());
					std::lock_guard lock(proceduralCircuitOrBusParameterMux);
					auto iter = proceduralCircuitOrBusParameter.find(selectedProceduralCircuitOrBus);
					if (iter == proceduralCircuitOrBusParameter.end()) {
						logError("Could not find {} in proceduralCircuitOrBusParameter. This should have been setup before this code runs.", "SelectorWidget::render");
					} else {
						for (auto& parameter : iter->second) {
							if (std::holds_alternative<int>(parameter.second)) {
								ImGui::InputScalar(parameter.first.c_str(), ImGuiDataType_S32, &std::get<int>(parameter.second), &slowStep, &fastStep);
							} else if (std::holds_alternative<unsigned int>(parameter.second)) {
								ImGui::InputScalar(parameter.first.c_str(), ImGuiDataType_U32, (int*)&std::get<unsigned int>(parameter.second), &slowStep, &fastStep);
							} else if (std::holds_alternative<float>(parameter.second)) {
								ImGui::InputScalar(parameter.first.c_str(), ImGuiDataType_Float, &std::get<float>(parameter.second), &slowStep, &fastStep);
							} else if (std::holds_alternative<std::string>(parameter.second)) {
								ImGui::InputText(parameter.first.c_str(), &std::get<std::string>(parameter.second));
							}
						}
					}
					setGUIValue_rendering<bool>("doCreateBlock", ImGui::Button("Create Circuit"));
				}
				ImGui::EndChild();
			}
		}
		std::lock_guard mux(pathsMux);
		createTree(root);
	} else getMainWindow().popWindowStyling();
	if (!open) {
		getMainWindow().destroyWidget(this->getWidgetId());
	}
	ImGui::End();
}
