#include "toolSelectorWidget.h"

#include "util/preprocessors.h"
#include "../mainWindow.h"

ToolSelectorWidget::ToolSelectorWidget(WidgetId widgetId, MainWindow& mainWindow) :
	Widget(widgetId, mainWindow), dataUpdateEventReceiver(getBackend().getDataUpdateEventManager()) {
	{
		// =================================== Init all tree data ===================================
		std::lock_guard mux(pathsMux);
		for (const auto& iter : getMainWindow().getToolManagerManager().getAllTools()) {
			addPath(iter.first, iter.first);
		}
	}
	dataUpdateEventReceiver.linkFunction("setToolModeUpdate", [this](const DataUpdateEventManager::EventData* event) {
		auto data = event->cast<std::string>();
		if (data) setGUIValue<std::string>("selectedToolMode", data->get());
		else setGUIValue<std::string>("selectedToolMode", "");
	});
	dataUpdateEventReceiver.linkFunction("setToolUpdate", [this](const DataUpdateEventManager::EventData* event) {
		const DataUpdateEventManager::EventDataWithValue<std::string>* data = event->cast<std::string>();
		assert(data);
		setGUIValue<std::string>("selectedTool", data->get());
		std::lock_guard lock(modesMux);
		toolModes = getMainWindow().getToolManagerManager().getActiveToolModes().value_or(std::vector<std::string>{});
		setGUIValue<std::string>("selectedToolMode", getMainWindow().getToolManagerManager().getActiveToolMode().value_or(""));
	});
	setupGUIValue<std::string>("selectedTool", "", [this](const std::string& toolPath) { getMainWindow().getToolManagerManager().setTool(toolPath); });
	setupGUIValue<std::string>("selectedToolMode", "", [this](const std::string& mode) {
		getMainWindow().getToolManagerManager().setMode(mode);
	});
}

void ToolSelectorWidget::addPath(const std::string& path, const std::string& data) {
	auto pair = paths.emplace(data, path);
	if (!pair.second) {
		if (pair.first->second == path) return;
		root.removePath(pair.first->second);
		pair.first->second = path;
	}
	root.addPath(path, data);
}

void ToolSelectorWidget::createTree(const SelectorTreeNode& node) {
	assert(!node.children.empty());
	if (node.children.begin()->second.data.has_value()) {
		for (auto& pair : node.children) {
			// leaf (never children)
			assert(pair.second.children.empty());
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
			if (getGUIValue_rendering<std::string>("selectedTool") == pair.second.data.value()) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			if (ImGui::TreeNodeEx(pair.first.c_str(), flags)) {
				if (ImGui::IsItemClicked()) {
					setGUIValue_rendering("selectedTool", pair.second.data.value());
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

void ToolSelectorWidget::render() {
	ImGui::SetNextWindowDockID(getMainWindow().getDockLeftId(), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(100, 300), ImGuiCond_FirstUseEver);
	getMainWindow().setNextWindowSideBarDockable();
	bool open = true;
	getMainWindow().pushWindowStyling();
	ifGui (ImGui::Begin(("Tools###" + getWidgetIdStr()).c_str(), &open),
		getMainWindow().popWindowStyling();
	) {
		{
			std::lock_guard mux(pathsMux);
			createTree(root);
		}
		std::lock_guard lock(modesMux);
		if (!toolModes.empty()) {
			ImGui::Separator();
			if (ImGui::BeginChild("Container")) {
				if (ImGui::BeginTable("Grid", 3, ImGuiTableFlags_SizingStretchSame)) {
					ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
					for (const std::string& mode : toolModes) {
						ImGui::TableNextColumn();
						if (ImGui::Selectable(mode.c_str(), mode == getGUIValue_rendering<std::string>("selectedToolMode"), ImGuiSelectableFlags_None)) {
							setGUIValue_rendering<std::string>("selectedToolMode", mode);
						}
					}
					ImGui::PopStyleVar(2);
					ImGui::EndTable();
				}
			}
			ImGui::EndChild();
		}
	}
	if (!open) {
		getMainWindow().destroyWidget(this->getWidgetId());
	}
	ImGui::End();
}
