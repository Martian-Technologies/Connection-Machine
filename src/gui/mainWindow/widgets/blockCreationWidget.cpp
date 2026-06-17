#include "blockCreationWidget.h"

#include "SDL3/SDL_dialog.h"
#include "gui/viewportManager/circuitView/tools/other/portAdder.h"
#include "gui/viewportManager/circuitView/events/customEvents.h"
#include "gui/viewportManager/circuitView/tools/other/portSelector.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_stdlib.h"

#include "gui/viewportManager/circuitView/tools/toolStack.h"
#include "gui/viewportManager/circuitView/circuitView.h"
#include "util/preprocessors.h"
#include "gui/mainWindow/mainWindow.h"
#include "gui/mainWindow/guiColors.h"
#include "app.h"

BlockCreationWidget::BlockCreationWidget(WidgetId widgetId, MainWindow& mainWindow, circuit_id_t circuitId) :
	Widget(widgetId, mainWindow), dataUpdateEventReceiver(getBackend().getDataUpdateEventManager()) {
	{
		ViewportId viewportId = MainRenderer::get().registerViewport(getMainWindow().getWindowId(), { 100, 100 });
		elementCreator.setup(viewportId);
		circuitView = std::make_unique<CircuitView>(mainWindow.getEnvironment(), viewportId);
		setupGUIValue<float>("AspectRatio", 1, [&](const float& aspectRatio) { circuitView->getViewManager().setAspectRatio(aspectRatio); });
		setupGUIValue<bool>("MouseLeftDown", false, [&](const bool& state) {
			if (state) {
				if (!getGUIValue<bool>("MouseInView")) return;
				if (getMainWindow().isHoldingKeybind("Keybinds/Camera/Pan")) {
					if (circuitView->getEventRegister().doEvent(PositionEvent("View Attach Anchor", circuitView->getViewManager().getPointerPosition()))) {
						return;
					}
				}
				circuitView->getEventRegister().doEvent(PositionEvent("Tool Primary Activate", circuitView->getViewManager().getPointerPosition()));
			} else {
				circuitView->getEventRegister().doEvent(PositionEvent("View Dettach Anchor", circuitView->getViewManager().getPointerPosition()));
				circuitView->getEventRegister().doEvent(PositionEvent("Tool Primary Deactivate", circuitView->getViewManager().getPointerPosition()));
			}
		});
		setupGUIValue<bool>("MouseRightDown", false, [&](const bool& state) {
			if (state) {
				if (!getGUIValue<bool>("MouseInView")) return;
				circuitView->getEventRegister().doEvent(PositionEvent("Tool Secondary Activate", circuitView->getViewManager().getPointerPosition()));
			} else {
				circuitView->getEventRegister().doEvent(PositionEvent("Tool Secondary Deactivate", circuitView->getViewManager().getPointerPosition()));
			}
		});
		setupGUIValue<Vec2>("MousePosition", {0, 0}, [&](const Vec2& viewPos) {
			circuitView->getEventRegister().doEvent(ViewPositionEvent("Pointer Move On View", viewPos));
		});
		setupGUIValue<bool>("MouseInView", false, [&](const bool& inView) {
			if (inView) {
				circuitView->getEventRegister().doEvent(PositionEvent("pointer enter view", circuitView->getViewManager().getPointerPosition()));
			} else {
				circuitView->getEventRegister().doEvent(PositionEvent("pointer exit view", circuitView->getViewManager().getPointerPosition()));
			}
		});
		setupGUIValue<Vec2>("CircuitViewSize", Vec2(1, 1), nullptr);
		setupGUIValue<bool>("CircuitViewIsFocused", false, nullptr);
		setupGUIValue<float>("WindowScalingSize", getMainWindow().getWindowScalingSize(), nullptr);
		setupGUIValue<FPosition>("MouseGridPos", circuitView->getViewManager().getPointerPosition(), nullptr);
		setupGUIValue<std::string>("CircuitName", "NULL", nullptr);
		setupGUIValue<std::string>("StatusBar", "", nullptr);
		circuitView->getEventRegister().registerFunction("status bar changed", [this](const Event* event) -> bool {
			const EventWithValue<std::string>* data = event->cast2<std::string>();
			if (!data) return false;
			setGUIValue<std::string>("StatusBar", data->get());
			return true;
		});
		setupGUIValue<FPosition>("ViewportCenter", circuitView->getViewManager().getViewCenter(), nullptr);
		setupGUIValue<FVector>("ViewportCellSize", FVector(circuitView->getViewManager().getViewHeight(), circuitView->getViewManager().getViewWidth()), nullptr);
		circuitView->getEventRegister().registerFunction("ViewCenterMove", [this](const Event* event) -> bool {
			const PositionEvent* positionEvent = event->cast<PositionEvent>();
			if (positionEvent) setGUIValue<FPosition>("ViewportCenter", positionEvent->getFPosition());
			return false;
		});
		circuitView->getEventRegister().registerFunction("ViewSizeChange", [this](const Event* event) -> bool {
			const VectorEvent* vectorEvent = event->cast<VectorEvent>();
			if (vectorEvent) setGUIValue<FVector>("ViewportCellSize", vectorEvent->getFVector());
			return false;
		});
	}
	{
		// =================================== Init rendering circuit ===================================
		renderingCircuitId = getBackend().getCircuitManager().createNewCircuit(true);
		assert(renderingCircuitId);
		Circuit* circuit = getBackend().getCircuitManager().getCircuit(renderingCircuitId);
		assert(circuit);
		circuit->setEditable(false);
		BlockData* blockData = getBackend().getBlockDataManager().getBlockData(circuit->getBlockType());
		assert(blockData);
		blockData->setIsPlaceable(false);
	}
	{
		// =================================== Init all data ===================================
		const CircuitBlockData* circuitBlockData = getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
		if (circuitBlockData) {
			const BlockData* blockData = getBackend().getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
			assert(blockData);
			blockType = blockData->getBlockType();
			{
				std::lock_guard mux(blockDataCopyMux);
				blockDataCopy = blockData->getBlockDataCopy();
			}
			{
				std::lock_guard mux(circuitBlockDataCopyMux);
				circuitBlockDataCopy = circuitBlockData->getCircuitBlockDataCopy();
			}
		}
		{
			std::lock_guard mux(circuitsMux);
			for (const auto& circuit : getBackend().getCircuitManager().getCircuits()) {
				if (circuit.second.isEditable()) circuits.emplace(circuit.first, circuit.second.getCircuitName());
			}
		}
	}
	dataUpdateEventReceiver.linkFunction("blockDataUpdate", [this](const DataUpdateEventManager::EventData* event) {
		// update circuit list
		{
			std::lock_guard mux(circuitsMux);
			circuits.clear();
			for (const auto& circuit : getBackend().getCircuitManager().getCircuits()) {
				if (circuit.second.isEditable()) circuits.emplace(circuit.first, circuit.second.getCircuitName());
			}
		}
		// update renderData
		{
			const DataUpdateEventManager::EventDataWithValue<BlockType>* data = event->cast<BlockType>();
			assert(data);
			if (blockType != data->get()) return;
			const BlockData* blockData = getBackend().getBlockDataManager().getBlockData(blockType);
			assert(blockData);
			std::lock_guard mux(blockDataCopyMux);
			blockDataCopy = blockData->getBlockDataCopy();
		}
	});
	dataUpdateEventReceiver.linkFunction("circuitBlockDataUpdate", [this](const DataUpdateEventManager::EventData* event) {
		const DataUpdateEventManager::EventDataWithValue<std::pair<BlockType, circuit_id_t>>* data = event->cast<std::pair<BlockType, circuit_id_t>>();
		assert(data);
		if (blockType != data->get().first) return;
		const CircuitBlockData* circuitBlockData = getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(data->get().second);
		std::lock_guard mux(circuitBlockDataCopyMux);
		circuitBlockDataCopy = circuitBlockData->getCircuitBlockDataCopy();
	});
	setupGUIValue<std::optional<connection_end_id_t>>("currentlyEditingPort", std::nullopt, nullptr);
	setupGUIValue<circuit_id_t>("circuitId", circuitId, [this](const circuit_id_t& circuitId) {
		const CircuitBlockData* circuitBlockData = getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
		if (circuitBlockData == nullptr) {
			{
				std::lock_guard mux(blockDataCopyMux);
				blockDataCopy = std::nullopt;
			}
			{
				std::lock_guard mux(circuitBlockDataCopyMux);
				circuitBlockDataCopy = std::nullopt;
			}
			circuitView->setCircuit(0);
			return;
		}
		const BlockData* blockData = getBackend().getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
		assert(blockData);
		blockType = blockData->getBlockType();
		{
			std::lock_guard mux(blockDataCopyMux);
			blockDataCopy = blockData->getBlockDataCopy();
		}
		{
			std::lock_guard mux(circuitBlockDataCopyMux);
			circuitBlockDataCopy = circuitBlockData->getCircuitBlockDataCopy();
		}
		Circuit* circuit = getBackend().getCircuitManager().getCircuit(renderingCircuitId);
		circuit->clear();
		circuit->tryInsertBlock(Position(), Orientation(), blockData->getBlockType());
		circuitView->setCircuit(renderingCircuitId);
		for (auto& iter : circuitView->getBackend().getSimulatorManager().getSimulators()) {
			if (iter.second->getCircuitId() == renderingCircuitId) {
				iter.second->setPause(true);
				circuitView->setSimulator(iter.second.get());
				break;
			}
		}
		circuitView->getViewManager().focus();
		setGUIValue<std::optional<connection_end_id_t>>("currentlyEditingPort", std::nullopt);
	});
	setupGUIValue<bool>("doingPortMapping", false, [this](const bool& doingPortMapping) {
	});
	setupGUIValue<bool>("showingCircuitInternals", false, [this](const bool& showingCircuitInternals) {
		if (circuitView->getSimulator() != 0 && blockType != BlockType::NONE) {
			circuitView->getToolManager().clearStacks();
			if (showingCircuitInternals) {
				circuitView->setSimulator(circuitView->getSimulator(), Address(Position()));
			} else {
				circuitView->setSimulator(circuitView->getSimulator());
			}
		}
	});
	setupGUIValue<std::optional<Position>>("selectedCell", std::nullopt, [this](const std::optional<Position>& doingPortMapping) {
		elementCreator.clear();
		if (doingPortMapping) {
			elementCreator.addSelectionElement(SelectionElement(doingPortMapping.value()));
		}
	});
}

BlockCreationWidget::~BlockCreationWidget() {
	ViewportId viewportId = circuitView->getViewportId();
	circuitView.reset();
	MainRenderer::get().deregisterViewport(viewportId);
}

void BlockCreationWidget::processEvent(SDL_Event& event) {
	if (getGUIValue<bool>("CircuitViewIsFocused")) {

	}
	if (getGUIValue<bool>("MouseInView")) {
		if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			Vec2 movement(-event.wheel.x * getMainWindow().getWindowScalingSize(), event.wheel.y * getMainWindow().getWindowScalingSize());
			if (!Settings::get<SettingType::BOOL>("Keybinds/Camera/Scroll Panning", false)) {
				circuitView->getEventRegister().doEvent(DeltaEvent("view zoom", (float)(movement.y) / 15.f));
			} else if (getMainWindow().isHoldingKeybind("Keybinds/Camera/Zoom")) {
				// do zoom
				circuitView->getEventRegister().doEvent(DeltaEvent("view zoom", (float)(movement.y) / 15.f));
			} else {
				Vec2 size = getGUIValue<Vec2>("CircuitViewSize");
				float scaleAmout = 5; //App::getDetlaTime() * 750.f;
				circuitView->getEventRegister().doEvent(DeltaXYEvent(
					"view pan",
					movement.x / size.x * circuitView->getViewManager().getViewWidth() * scaleAmout,
					movement.y / size.y * circuitView->getViewManager().getViewHeight() * scaleAmout
				));
			}
		}
	}
}

void BlockCreationWidget::render() {
	setGUIValue_rendering<std::optional<Position>>("selectedCell", std::nullopt);
	ImGui::SetNextWindowDockID(getMainWindow().getDockMainId(), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(100, 300), ImGuiCond_FirstUseEver);
	getMainWindow().setNextWindowMainDockable();
	bool open = true;
	getMainWindow().pushWindowStyling();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
	ifGui (ImGui::Begin(("Block Creation###" + getWidgetIdStr()).c_str(), &open),
		ImGui::PopStyleVar();
		getMainWindow().popWindowStyling();
	) {
		circuit_id_t circuitId = getGUIValue_rendering<circuit_id_t>("circuitId");
		auto iter = circuits.find(circuitId);
		std::string circuitName = "NONE";
		if (iter != circuits.end()) circuitName = iter->second;
		{ // circuit selector
			if (ImGui::BeginCombo("##circuitSelector", circuitName.c_str())) {
				static ImGuiTextFilter filter;
				if (ImGui::IsWindowAppearing()) {
					ImGui::SetKeyboardFocusHere();
					filter.Clear();
				}
				ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
				filter.Draw("##Filter", -FLT_MIN);

				for (const std::pair<circuit_id_t, std::string>& circuit : circuits)  {
					if (filter.PassFilter(circuit.second.c_str())) {
						if (ImGui::Selectable(circuit.second.c_str(), circuit.first == circuitId)) {
							setGUIValue_rendering("circuitId", circuit.first);
						}
					}
				}
				ImGui::EndCombo();
			}
		}
		std::lock_guard mux1(blockDataCopyMux);
		std::lock_guard mux2(circuitBlockDataCopyMux);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, GUIColors::BACKGROUND);
		ifGui (ImGui::BeginChild("##mainView", ImVec2(ImGui::GetContentRegionAvail().x - 200, ImGui::GetContentRegionAvail().y), ImGuiChildFlags_ResizeX, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse), ImGui::PopStyleColor()) {
			renderViewport(circuitId);
		}
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, GUIColors::BACKGROUND);
		ifGui (ImGui::BeginChild("##SideBar"), ImGui::PopStyleColor()) {
			renderSideBar(circuitId);
		}
		ImGui::EndChild();
	}
	if (!open) {
		getMainWindow().destroyWidget(this->getWidgetId());
	}
	ImGui::End();
}

void BlockCreationWidget::renderViewport(circuit_id_t circuitId) {
	ImGui::SetScrollX(0);
	ImGui::SetScrollY(0);
	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	ImVec2 viewportWindowScreenPos = ImGui::GetCursorScreenPos();
	ImVec2 viewportWindowPos = ImGui::GetCursorPos();
	{
		float windowScalingSize = getGUIValue_rendering<float>("WindowScalingSize");
		MainRenderer::get().resizeViewport(circuitView->getViewportId(), { viewportPanelSize.x * windowScalingSize, viewportPanelSize.y * windowScalingSize });
		VkDescriptorSet descriptorSet = MainRenderer::get().startViewportRendering(circuitView->getViewportId());
		if (descriptorSet != VK_NULL_HANDLE) {
			ImGui::Image(descriptorSet, ImVec2{ viewportPanelSize.x, viewportPanelSize.y });
		} else {
			ImGui::Text("RENDERING BROKEN!! :(");
		}
	}
	ImGui::SetCursorPos(viewportWindowPos);
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton("circuitViewInvisibleButton", viewportPanelSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	bool isHovered = ImGui::IsItemHovered();
	if (isHovered) ImGui::FocusItem();
	ImVec2 mousePos = ImGui::GetMousePos();
	mousePos = ImVec2(mousePos.x - viewportWindowScreenPos.x, mousePos.y - viewportWindowScreenPos.y);

	setGUIValue_rendering("AspectRatio", viewportPanelSize.x / viewportPanelSize.y);
	setGUIValue_rendering("MouseLeftDown", ImGui::IsMouseDown(ImGuiMouseButton_Left));
	setGUIValue_rendering("MouseRightDown", ImGui::IsMouseDown(ImGuiMouseButton_Right));
	setGUIValue_rendering("MousePosition", Vec2(mousePos.x / viewportPanelSize.x, mousePos.y / viewportPanelSize.y));
	setGUIValue_rendering("MouseInView", isHovered);
	setGUIValue_rendering("CircuitViewSize", Vec2(viewportPanelSize.x, viewportPanelSize.y));
	setGUIValue_rendering("CircuitViewIsFocused", ImGui::IsItemFocused());

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const int offset = 2;
	const int padding = 4;

	FPosition viewportCenter = getGUIValue_rendering<FPosition>("ViewportCenter");
	FVector viewportCellSize = getGUIValue_rendering<FVector>("ViewportCellSize");
	ImVec2 pixPerCell = ImVec2(viewportPanelSize.x / viewportCellSize.dx, viewportPanelSize.y / viewportCellSize.dy);
	{
		const auto* renderText = MainRenderer::get().getTextOnViewport(circuitView->getViewportId());
		if (renderText) {
			for (const auto& pair : *renderText) {
				ImVec2 textPos = viewportWindowPos + viewportPanelSize / 2 - ImVec2(
					pixPerCell.x * (viewportCenter.x - pair.second.pos.x),
					pixPerCell.y * (viewportCenter.y - pair.second.pos.y)
				);
				ImGui::SetCursorPos(textPos);
				float curScale = ImGui::GetCurrentWindow()->FontWindowScale;
				if (pair.second.scale != 0) {
					if (pair.second.scale > 0) {
						ImGui::SetWindowFontScale(pair.second.scale * pixPerCell.x * 0.02);
					}
					if (pair.second.scale < 0) {
						ImGui::SetWindowFontScale(-pair.second.scale * curScale);
					}
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
					ImGui::Text("%s", pair.second.text.c_str());
					ImGui::PopStyleColor();
				}
				ImGui::SetWindowFontScale(curScale);
			}
		}
	}
	if (!getGUIValue_rendering<bool>("doingPortMapping")) {
		std::optional<connection_end_id_t> currentlyEditingPort = getGUIValue<std::optional<connection_end_id_t>>("currentlyEditingPort");
		if (blockDataCopy.has_value()) {
			if (currentlyEditingPort.has_value()) {
				auto connectionIter = blockDataCopy->connections.find(currentlyEditingPort.value());
				if (connectionIter == blockDataCopy->connections.end()) {
					setGUIValue_rendering<std::optional<connection_end_id_t>>("currentlyEditingPort", std::nullopt);
				} else {
					BlockData::ConnectionData& connectionData = connectionIter->second;
					ImVec2 portSettingsSize;
					ImVec2 portSettingPos = viewportWindowPos + viewportPanelSize / 2 - ImVec2(
						pixPerCell.x * (viewportCenter.x - connectionData.positionOnBlock.dx),
						pixPerCell.y * (viewportCenter.y - connectionData.positionOnBlock.dy)
					) + ImVec2(5, 5) + ImVec2(pixPerCell.x * 0, pixPerCell.y);
					drawList->ChannelsSplit(2);
					{
						drawList->ChannelsSetCurrent(1);
						ImGui::SetCursorPos(portSettingPos);
						ImGui::BeginGroup();
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
							float xPos = ImGui::GetCursorPos().x;
							ImGui::Text("Name");
							ImGui::SetCursorPos(ImVec2(xPos, ImGui::GetCursorPos().y));
							ImGui::SetNextItemWidth(160);
							const std::string* portName = blockDataCopy->connectionIdNames.get(currentlyEditingPort.value());
							std::string portNameStr = "";
							if (portName != nullptr) portNameStr = *portName;
							if (ImGui::InputText("##PortName", &portNameStr)) {
								blockDataCopy->connectionIdNames.set(currentlyEditingPort.value(), portNameStr);
								App::runOnMain([this, blockType = blockDataCopy->blockType, port = currentlyEditingPort.value(), name = portNameStr](){
									Backend& backend = getEnvironment().getBackend();
									BlockData* blockData = backend.getBlockDataManager().getBlockData(blockType);
									if (!blockData) return;
									blockData->setConnectionIdName(port, name);
								});
							}
							ImGui::SetCursorPos(ImVec2(xPos, ImGui::GetCursorPos().y));
							{
								ImGui::Text("Pos");
								ImGui::SameLine();
								ImGui::SetNextItemWidth(50);
								bool portPosUpdated = ImGui::InputScalar("##xPortPos", ImGuiDataType_U32, &connectionData.positionOnBlock.dx);
								ImGui::SameLine();
								ImGui::SetNextItemWidth(50);
								portPosUpdated |= ImGui::InputScalar("##yPortPos", ImGuiDataType_U32, &connectionData.positionOnBlock.dy);
								if (portPosUpdated) {
									if (connectionData.positionOnBlock.dx >= blockDataCopy->blockSize.w) {
										connectionData.positionOnBlock.dx = blockDataCopy->blockSize.w - 1;
									}
									if (connectionData.positionOnBlock.dy >= blockDataCopy->blockSize.h) {
										connectionData.positionOnBlock.dy = blockDataCopy->blockSize.h - 1;
									}
									bool cant = false;
									for (const auto& connection : blockDataCopy->connections) {
										if (connection.second.portType == connectionData.portType) {
											if (connection.first != currentlyEditingPort.value() && connection.second.positionOnBlock == connectionData.positionOnBlock) {
												cant = true;
												break;
											}
										}
									}
									if (!cant) {
										App::runOnMain([this, blockType = blockDataCopy->blockType, port = currentlyEditingPort.value(), portType = connectionData.portType, pos = connectionData.positionOnBlock](){
											Backend& backend = getEnvironment().getBackend();
											BlockData* blockData = backend.getBlockDataManager().getBlockData(blockType);
											if (!blockData) return;
											if (portType == BlockData::ConnectionData::PortType::INPUT) {
												blockData->setConnectionInput(pos, port);
											} else {
												blockData->setConnectionOutput(pos, port);
											}
										});
									}
								}
							}
							{
								ImGui::Text("Offset");
								ImGui::SameLine();
								ImGui::SetNextItemWidth(50);
								bool portOffsetPosUpdated = ImGui::InputScalar("##xPortOffsetPos", ImGuiDataType_Float, &connectionData.portOffset.dx);
								ImGui::SameLine();
								ImGui::SetNextItemWidth(50);
								portOffsetPosUpdated |= ImGui::InputScalar("##yPortOffsetPos", ImGuiDataType_Float, &connectionData.portOffset.dy);
								if (portOffsetPosUpdated) {
									connectionData.portOffset.dx = std::clamp(connectionData.portOffset.dx, 0.0f, 1.0f);
									connectionData.portOffset.dy = std::clamp(connectionData.portOffset.dy, 0.0f, 1.0f);
									App::runOnMain([this, blockType = blockDataCopy->blockType, port = currentlyEditingPort.value(), portType = connectionData.portType, pos = connectionData.portOffset](){
										Backend& backend = getEnvironment().getBackend();
										BlockData* blockData = backend.getBlockDataManager().getBlockData(blockType);
										if (portType == BlockData::ConnectionData::PortType::INPUT) {
											if (!blockData) return;
											blockData->setConnectionPortOffset(port, pos);
										} else {
											blockData->setConnectionPortOffset(port, pos);
										}
									});
								}
							}
							{
								ImGui::Text("Bit Width");
								ImGui::SameLine();
								ImGui::SetNextItemWidth(50);
								unsigned int bitWidth = connectionData.getBitWidth();
								if (ImGui::InputScalar("##BitWidth", ImGuiDataType_U32, &bitWidth)) {
									if (bitWidth == 0) {
										bitWidth = 1;
									} else if (bitWidth > 65535) {
										bitWidth = 65535; // it should never be this big!
									}
									connectionData.bitConfiguration = bitWidth;
									App::runOnMain([this, blockType = blockDataCopy->blockType, port = currentlyEditingPort.value(), bitWidth](){
										Backend& backend = getEnvironment().getBackend();
										BlockData* blockData = backend.getBlockDataManager().getBlockData(blockType);
										if (!blockData) return;
										blockData->setConnectionBitConfiguration(port, bitWidth);
									});
								}
							}
							ImGui::SetCursorPos(ImVec2(xPos, ImGui::GetCursorPos().y));
							if (ImGui::SmallButton("Remove")) {
								ImGui::OpenPopup("Are you sure you want to remove this port");
							}
							if (ImGui::BeginPopup("Are you sure you want to remove this port")) {
								if (ImGui::Button("cancel")) {
									ImGui::CloseCurrentPopup();
								}
								if (ImGui::Button("Remove")) {
									App::runOnMain([this, blockType = blockDataCopy->blockType, port = currentlyEditingPort.value()](){
										Backend& backend = getEnvironment().getBackend();
										BlockData* blockData = backend.getBlockDataManager().getBlockData(blockType);
										if (!blockData) return;
										blockData->removeConnection(port);
									});
								}
								ImGui::EndPopup();
							}
							ImGui::SameLine();
							if (ImGui::SmallButton("Done")) {
								setGUIValue_rendering<std::optional<connection_end_id_t>>("currentlyEditingPort", std::nullopt);
							}
							ImGui::PopStyleColor();
						ImGui::EndGroup();
						portSettingsSize = ImGui::GetItemRectSize();
					}
					{
						drawList->ChannelsSetCurrent(0);
						drawList->AddRectFilled(
							{ viewportWindowScreenPos.x + portSettingPos.x - padding, viewportWindowScreenPos.y + portSettingPos.y - padding },
							{ viewportWindowScreenPos.x + portSettingPos.x + portSettingsSize.x + padding,
								viewportWindowScreenPos.y + portSettingPos.y + portSettingsSize.y + padding },
							ImColor(0.8f, 0.8f, 0.8f, 0.75f),
							5.f
						);
					}
					drawList->ChannelsMerge();
				}
			} else {
				for (const auto& connection : blockDataCopy->connections) {
					ImGui::SetCursorPos(viewportWindowPos + viewportPanelSize / 2 - ImVec2(
						pixPerCell.x * (viewportCenter.x - connection.second.positionOnBlock.dx),
						pixPerCell.y * (viewportCenter.y - connection.second.positionOnBlock.dy) + (
							(connection.second.portType == BlockData::ConnectionData::OUTPUT) ? -(ImGui::CalcTextSize("Edit Input").y + 5) : 0
						)
					) + ImVec2(5, 5));
					ImGui::PushID(connection.first.get());
					bool pressed = false;
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.77f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.2f, 0.79f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.4f, 0.90f));
					if (connection.second.portType == BlockData::ConnectionData::OUTPUT) pressed = ImGui::SmallButton("Edit Out");
					else pressed = ImGui::SmallButton("Edit In");
					ImGui::PopStyleColor(3);
					if (pressed) {
						setGUIValue_rendering<std::optional<connection_end_id_t>>("currentlyEditingPort", connection.first);
					}
					ImGui::PopID();
				}
			}
		}
		drawList->ChannelsSplit(2);
		ImVec2 simControlsSize;
		{
			drawList->ChannelsSetCurrent(1);
			ImGui::SetCursorPos({ viewportWindowPos.x + padding + offset, viewportWindowPos.y + padding + offset });
			ImGui::BeginGroup();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
				FPosition mouseGridPos = getGUIValue_rendering<FPosition>("MouseGridPos");
				ImGui::Text("Mouse: (%.2f, %.2f)", mouseGridPos.x, mouseGridPos.y);
				if (!currentlyEditingPort.has_value()) {
					if (ImGui::Button("Add Port"))
						ImGui::OpenPopup("port_popup");

					if (blockDataCopy.has_value()) {
						ImGui::SameLine();
						ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
						ifGui (ImGui::BeginPopup("port_popup"), ImGui::PopStyleColor();) {
							if (ImGui::Selectable("Input")) {
								App::runOnMain([this, circuitId, name = blockDataCopy->name](){
									auto tool = std::dynamic_pointer_cast<PortAdder>(
										circuitView->getToolManager().selectTool(std::make_shared<PortAdder>(getEnvironment()))
									);
									if (tool) {
										tool->setup(true, [this](Position position){
											BlockData* blockData = getBackend().getBlockDataManager().getBlockData(blockType);
											if (blockData == nullptr) return;
											blockData->setConnectionInput(position - Position(), blockData->getNewConnectionId());
										});
									} else {
										logError("Failed to set tool to PortAdder", "BlockCreationWidget");
									}
								});
							}
							if (ImGui::Selectable("Output")) {
								App::runOnMain([this, circuitId, name = blockDataCopy->name](){
									auto tool = std::dynamic_pointer_cast<PortAdder>(
										circuitView->getToolManager().selectTool(std::make_shared<PortAdder>(getEnvironment()))
									);
									if (tool) {
										tool->setup(false, [this](Position position){
											BlockData* blockData = getBackend().getBlockDataManager().getBlockData(blockType);
											if (blockData == nullptr) return;
											blockData->setConnectionOutput(position - Position(), blockData->getNewConnectionId());
										});
									} else {
										logError("Failed to set tool to PortAdder", "BlockCreationWidget");
									}
								});
							}
							ImGui::EndPopup();
						}
					}
				}
				ImGui::PopStyleColor();
			ImGui::EndGroup();
			simControlsSize = ImGui::GetItemRectSize();
		}
		{
			drawList->ChannelsSetCurrent(0);
			if (blockDataCopy.has_value()) {
				drawList->AddRectFilled(
					{ viewportWindowScreenPos.x + offset, viewportWindowScreenPos.y + offset },
					{ viewportWindowScreenPos.x + simControlsSize.x + padding * 2 + offset, viewportWindowScreenPos.y + simControlsSize.y + padding * 2 + offset },
					ImColor(0.f, 0.f, 0.f, 0.1f),
					5.f
				);
			} else {
				drawList->AddRectFilled(
					{ viewportWindowScreenPos.x + offset, viewportWindowScreenPos.y + offset },
					{ viewportWindowScreenPos.x + simControlsSize.x + padding * 2 + offset, viewportWindowScreenPos.y + simControlsSize.y + padding * 2 + offset },
					ImColor(0.f, 0.f, 0.f, 0.2f),
					5.f
				);
			}
		}
		drawList->ChannelsMerge();
	} else {
		drawList->ChannelsSplit(2);
		ImVec2 simControlsSize;
		{
			drawList->ChannelsSetCurrent(1);
			ImGui::SetCursorPos({ viewportWindowPos.x + padding + offset, viewportWindowPos.y + padding + offset });
			ImGui::BeginGroup();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
				FPosition mouseGridPos = getGUIValue_rendering<FPosition>("MouseGridPos");
				ImGui::Text("Mouse: (%.2f, %.2f)", mouseGridPos.x, mouseGridPos.y);
				ImGui::PopStyleColor();
			ImGui::EndGroup();
			simControlsSize = ImGui::GetItemRectSize();
		}
		{
			drawList->ChannelsSetCurrent(0);
			if (blockDataCopy.has_value()) {
				drawList->AddRectFilled(
					{ viewportWindowScreenPos.x + offset, viewportWindowScreenPos.y + offset },
					{ viewportWindowScreenPos.x + simControlsSize.x + padding * 2 + offset, viewportWindowScreenPos.y + simControlsSize.y + padding * 2 + offset },
					ImColor(0.f, 0.f, 0.f, 0.1f),
					5.f
				);
			} else {
				drawList->AddRectFilled(
					{ viewportWindowScreenPos.x + offset, viewportWindowScreenPos.y + offset },
					{ viewportWindowScreenPos.x + simControlsSize.x + padding * 2 + offset, viewportWindowScreenPos.y + simControlsSize.y + padding * 2 + offset },
					ImColor(0.f, 0.f, 0.f, 0.2f),
					5.f
				);
			}
		}
		drawList->ChannelsMerge();
	}

	std::string statusBarText = getGUIValue_rendering<std::string>("StatusBar");
	if (!statusBarText.empty()) {
		ImVec2 textSize = ImGui::CalcTextSize(statusBarText.data());
		drawList->AddRectFilled(
			ImVec2(viewportPanelSize.x / 2.f, viewportPanelSize.y) + viewportWindowScreenPos - textSize / 2.f - ImVec2(padding, 50 +padding),
			ImVec2(viewportPanelSize.x / 2.f, viewportPanelSize.y) + viewportWindowScreenPos + textSize / 2.f - ImVec2(-padding, 50 -padding),
			ImColor(0.9f, 0.9f, 0.9f, 0.9f),
			5.f
		);
		ImGui::SetCursorScreenPos(ImVec2(viewportPanelSize.x / 2.f, viewportPanelSize.y) + viewportWindowScreenPos - textSize / 2.f - ImVec2(0, 50));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		ImGui::Text("%s", statusBarText.data());
		ImGui::PopStyleColor();
	}
}

void BlockCreationWidget::renderSideBar(circuit_id_t circuitId) {
	// blockData
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, GUIColors::WIDGET_BACKGROUND);
	ifGui (ImGui::BeginChild("Block Data", ImVec2(ImGui::GetContentRegionAvail().x, 100), ImGuiChildFlags_ResizeY | ImGuiChildFlags_AlwaysUseWindowPadding),
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	) {
		if (blockDataCopy.has_value()) {
			{
				ImGui::Text("Name");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
				if (ImGui::InputText("##Name", &blockDataCopy->name)) {
					App::runOnMain([this, circuitId, name = blockDataCopy->name](){
						Backend& backend = getEnvironment().getBackend();
						Circuit* circuit = backend.getCircuitManager().getCircuit(circuitId);
						if (!circuit) return;
						circuit->setCircuitName(name);
					});
				}
			}
			{
				ImGui::Text("Size");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x/2, 100));
				bool sizeChange = ImGui::InputScalar("##xSize", ImGuiDataType_S32, &blockDataCopy->blockSize.w);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x, 100));
				sizeChange |= ImGui::InputScalar("##ySize", ImGuiDataType_S32, &blockDataCopy->blockSize.h);
				if (sizeChange) {
					if (blockDataCopy->blockSize.w <= 0) blockDataCopy->blockSize.w = 1;
					if (blockDataCopy->blockSize.h <= 0) blockDataCopy->blockSize.h = 1;
					App::runOnMain([this, circuitId, size = blockDataCopy->blockSize](){
						Backend& backend = getMainWindow().getEnvironment().getBackend();
						const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
						if (!circuitBlockData) return;
						BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
						assert(blockData);
						blockData->setSize(size);
					});
				}
			}
			{

			}
		}
	}
	ImGui::EndChild();
	// renderDataList
	if (ImGui::BeginTabBar("Creation Tabs")) {
		if (ImGui::BeginTabItem("Textures")) {
			setGUIValue_rendering<bool>("doingPortMapping", false);
			setGUIValue_rendering<bool>("showingCircuitInternals", false);
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4);
			ImGui::PushStyleColor(ImGuiCol_ChildBg, GUIColors::WIDGET_BACKGROUND);
			ifGui (ImGui::BeginChild("Render Data", ImGui::GetContentRegionAvail(), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_MenuBar),
				ImGui::PopStyleColor();
				ImGui::PopStyleVar();
			) {
				if (ImGui::BeginMenuBar()) {
					if (ImGui::BeginMenu("New")) {
						if (ImGui::MenuItem("Texture")) {
							App::runOnMain([this, circuitId](){
								Backend& backend = getMainWindow().getEnvironment().getBackend();
								const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
								if (!circuitBlockData) return;
								BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
								assert(blockData);
								blockData->newRenderData<BlockData::BlockTextureData>();
							});
						}
						if (ImGui::MenuItem("Shape")) {
							getMainWindow().logError("Shape render data not real yet!");
						}
						ImGui::EndMenu();
					}
					ImGui::EndMenuBar();
				}
				if (blockDataCopy.has_value()) {
					for (unsigned int index = 0; index < blockDataCopy->renderData.size(); index++) {
						BlockData::RenderDataType& renderData = blockDataCopy->renderData[index];
						ImGui::PushID(index);
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
						ImGui::PushStyleColor(ImGuiCol_ChildBg, (index % 2 ==0) ? GUIColors::WIDGET_ALTERNATING_BACKGROUND_1 : GUIColors::WIDGET_ALTERNATING_BACKGROUND_2);
						ifGui (ImGui::BeginChild("Render Data Row", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding),
							ImGui::PopStyleColor();
							ImGui::PopStyleVar();
						) {
							if (std::holds_alternative<BlockData::BlockTextureData>(renderData)) {
								BlockData::BlockTextureData& blockTextureData = std::get<BlockData::BlockTextureData>(renderData);
								if (ImGui::TreeNodeEx("BlockTexture", ImGuiTreeNodeFlags_DefaultOpen)) {
									ImGui::TreePop();
									ImGui::Indent();
									if (ImGui::Button("Path")) {
										App::runOnMain([this, index]() {
											SDL_ShowOpenFileDialog([](void* userData, const char* const* filePaths, int filter) {
												App::runOnMain([&]() {
													if (!filePaths || !filePaths[0]) return;

													std::pair<BlockCreationWidget*, unsigned int>* data = (std::pair<BlockCreationWidget*, unsigned int>*)userData;

													circuit_id_t circuitId = data->first->getGUIValue<circuit_id_t>("circuitId");
													Circuit* circuit = data->first->getBackend().getCircuitManager().getCircuit(circuitId);
													if (!circuit) return;
													BlockData* blockData = data->first->getBackend().getBlockDataManager().getBlockData(circuit->getBlockType());

													std::string filePath = filePaths[0];
													if (filePath.empty()) return;
													const BlockData::RenderDataType* renderData = blockData->getRenderData(data->second);
													if (renderData == nullptr) return;
													if (std::holds_alternative<BlockData::BlockTextureData>(*renderData)) {
														blockData->setBlockTexturePath(data->second, filePath);
													}
													delete data;
												});
											}, new std::pair(this, index), nullptr, nullptr, 0, nullptr, true);
										});
									}
									ImGui::SameLine();
									ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
									if (ImGui::InputText("##Path", &blockTextureData.path)) {
										App::runOnMain([this, circuitId, index, path = blockTextureData.path](){
											Backend& backend = getMainWindow().getEnvironment().getBackend();
											const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
											if (!circuitBlockData) return;
											BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
											assert(blockData);
											if (!blockData->isRenderDataOfType<BlockData::BlockTextureData>(index)) return;
											blockData->setBlockTexturePath(index, path);
										});
									}
									if (ImGui::Checkbox("Use full texture", &blockTextureData.useFullTexture)) {
										App::runOnMain([this, circuitId, index, useFullTexture = blockTextureData.useFullTexture](){
											Backend& backend = getMainWindow().getEnvironment().getBackend();
											const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
											if (!circuitBlockData) return;
											BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
											assert(blockData);
											if (!blockData->isRenderDataOfType<BlockData::BlockTextureData>(index)) return;
											blockData->setBlockUseFullTexture(index, useFullTexture);
										});
									}
									if (!blockTextureData.useFullTexture) {
										{
											ImGui::Text("Size");
											ImGui::SameLine();
											ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x/2, 100));
											bool sizeUpdated = ImGui::InputScalar("##xSize", ImGuiDataType_U32, &blockTextureData.size.x);
											ImGui::SameLine();
											ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x, 100));
											sizeUpdated |= ImGui::InputScalar("##ySize", ImGuiDataType_U32, &blockTextureData.size.y);
											if (sizeUpdated) {
												App::runOnMain([this, circuitId, index, size = blockTextureData.size](){
													Backend& backend = getMainWindow().getEnvironment().getBackend();
													const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
													if (!circuitBlockData) return;
													BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
													assert(blockData);
													if (!blockData->isRenderDataOfType<BlockData::BlockTextureData>(index)) return;
													blockData->setBlockTextureSize(index, size);
												});
											}
										}
										{
											ImGui::Text("Top Left");
											ImGui::SameLine();
											ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x/2, 100));
											bool topLeftUpdated = ImGui::InputScalar("##xTopLeft", ImGuiDataType_U32, &blockTextureData.topLeft.x);
											ImGui::SameLine();
											ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x, 100));
											topLeftUpdated |= ImGui::InputScalar("##yTopLeft", ImGuiDataType_U32, &blockTextureData.topLeft.y);
											if (topLeftUpdated) {
												App::runOnMain([this, circuitId, index, topLeft = blockTextureData.topLeft](){
													Backend& backend = getMainWindow().getEnvironment().getBackend();
													const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
													if (!circuitBlockData) return;
													BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
													assert(blockData);
													if (!blockData->isRenderDataOfType<BlockData::BlockTextureData>(index)) return;
													blockData->setBlockTextureTopLeft(index, topLeft);
												});
											}
										}
										if (ImGui::Checkbox("Display state", &blockTextureData.renderState)) {
											App::runOnMain([this, circuitId, index, renderState = blockTextureData.renderState](){
												Backend& backend = getMainWindow().getEnvironment().getBackend();
												const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
												if (!circuitBlockData) return;
												BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
												assert(blockData);
												if (!blockData->isRenderDataOfType<BlockData::BlockTextureData>(index)) return;
												blockData->setBlockRenderState(index, renderState);
											});
										}
										if (blockTextureData.renderState) {
											{
												ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x, 100));
												if (ImGui::InputScalar("Virtual connection id##virtutal", ImGuiDataType_U32, &blockTextureData.virtualConnectionId)) {
													App::runOnMain([this, circuitId, index, virtualConnectionId = blockTextureData.virtualConnectionId](){
														Backend& backend = getMainWindow().getEnvironment().getBackend();
														const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
														if (!circuitBlockData) return;
														BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
														assert(blockData);
														if (!blockData->isRenderDataOfType<BlockData::BlockTextureData>(index)) return;
														blockData->setBlockTextureVirtualConnection(index, virtualConnectionId);
													});
												}
											}
											{
												ImGui::Text("State Offset");
												ImGui::SameLine();
												ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x/2, 100));
												bool stateOffsetUpdated = ImGui::InputScalar("##xStateOffset", ImGuiDataType_S32, &blockTextureData.stateOffset.x);
												ImGui::SameLine();
												ImGui::SetNextItemWidth(min(ImGui::GetContentRegionAvail().x, 100));
												stateOffsetUpdated |= ImGui::InputScalar("##yStateOffset", ImGuiDataType_S32, &blockTextureData.stateOffset.y);
												if (stateOffsetUpdated) {
													App::runOnMain([this, circuitId, index, stateOffset = blockTextureData.stateOffset](){
														Backend& backend = getMainWindow().getEnvironment().getBackend();
														const CircuitBlockData* circuitBlockData = backend.getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId);
														if (!circuitBlockData) return;
														BlockData* blockData = backend.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
														assert(blockData);
														if (!blockData->isRenderDataOfType<BlockData::BlockTextureData>(index)) return;
														blockData->setBlockTextureStateOffset(index, stateOffset);
													});
												}
											}
										}
									}
									ImGui::Unindent();
								}
							}
						}
						ImGui::EndChild();
						ImGui::PopID();
					}
				}
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Port Mappings")) {
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4);
			ImGui::PushStyleColor(ImGuiCol_ChildBg, GUIColors::WIDGET_BACKGROUND);
			ifGui (ImGui::BeginChild("PortMappings", ImGui::GetContentRegionAvail(), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_MenuBar),
				ImGui::PopStyleColor();
				ImGui::PopStyleVar();
			) {
				setGUIValue_rendering<bool>("doingPortMapping", true);
				if (blockDataCopy.has_value()) {
					unsigned int index = 0;
					bool showInternal = true;
					for (const auto& [endId, connectionData] : blockDataCopy->connections) {
						const Position* portPos = circuitBlockDataCopy->connectionIdPosition.get(endId);
						ImGui::PushID(index);
						ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0);
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
						ImGui::PushStyleColor(ImGuiCol_ChildBg, (index % 2 ==0) ? GUIColors::WIDGET_ALTERNATING_BACKGROUND_1 : GUIColors::WIDGET_ALTERNATING_BACKGROUND_2);
						ifGui (ImGui::BeginChild("Port Mapping Data Row", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding),
							ImGui::PopStyleColor();
							ImGui::PopStyleVar();
						) {
							if (ImGui::TreeNodeEx(fmt::format(
									"Port \"{}\"                              ",
									valueOr(blockDataCopy->connectionIdNames.get(endId), std::string(
										(connectionData.portType == BlockData::ConnectionData::INPUT) ? "IN" : "OUT"
									))//,
									// connectionData.positionOnBlock.toString()
								).c_str())) {
								ImGui::TreePop();
								ImGui::Indent();
								if (portPos == nullptr) {
									if (ImGui::Button("Add Mapping")) {
										App::runOnMain([this, circuitId, endId]() {
											auto tool = std::dynamic_pointer_cast<PortSelector>(
												circuitView->getToolManager().selectTool(std::make_shared<PortSelector>(getEnvironment()))
											);
											if (tool) {
												tool->setPort(endId, [this, endId, circuitId](Position position) {
													getBackend().getCircuitManager().getCircuitBlockDataManager()
														.getCircuitBlockData(circuitId)->setConnectionIdPosition(endId, position);
												});
											}

										});
									}
								} else {
									if (ImGui::Button("Remove Mapping")) {
										App::runOnMain([this, circuitId, endId]() {
											getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(circuitId)->removeConnectionIdPosition(endId);
										});
									}
								}
								ImGui::SameLine();
								ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
								ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(ImGuiCol_Button));
								ifGui (ImGui::BeginChild("ViewOnBlock", ImVec2(ImGui::CalcTextSize("View").x + 4, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding),
									ImGui::PopStyleColor();
									ImGui::PopStyleVar();
								) {
									ImGui::Text("View");
									if (ImGui::IsWindowHovered()) {
										showInternal = false;
										setGUIValue_rendering<std::optional<Position>>("selectedCell", Position() + connectionData.positionOnBlock);
									}
								}
								ImGui::EndChild();
								ImGui::Unindent();
							}
							if (showInternal && portPos) {
								if (ImGui::IsWindowHovered()) {
									setGUIValue_rendering<std::optional<Position>>("selectedCell", *portPos);
								}
							}
						}
						ImGui::EndChild();
						ImGui::PopStyleVar();
						ImGui::PopID();
						index++;
					}
					setGUIValue_rendering<bool>("showingCircuitInternals", showInternal);
				}
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void BlockCreationWidget::update() {
	if (circuitView->getCircuit()) {
		setGUIValue<std::string>("CircuitName", circuitView->getCircuit()->getCircuitName());
	} else {
		setGUIValue<std::string>("CircuitName", "NULL");
	}
	setGUIValue<FPosition>("MouseGridPos", circuitView->getViewManager().getPointerPosition());
	if (getGUIValue<bool>("CircuitViewIsFocused")) {
		if (getMainWindow().isPressingKeybind("Keybinds/Camera/Home")) {
			circuitView->getViewManager().focus();
		}
	}
}
