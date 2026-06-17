#ifndef blockSelectorWidget_h
#define blockSelectorWidget_h

#include "../widget.h"
#include "backend/circuit/circuitDefs.h"
#include "backend/container/block/blockDefs.h"
#include "backend/dataUpdateEventManager.h"

class BlockSelectorWidget : public Widget {
	struct SelectorTreeNode {
		void addPath(const std::string_view& path, const std::variant<BlockType, std::string, std::pair<BlockType, circuit_id_t>>& data) {
			size_t slashPos = path.find_first_of("/");
			if (slashPos == std::string::npos) {
				auto pair = children.emplace(path, SelectorTreeNode());
				pair.first->second.data = data;
			} else {
				auto pair = children.emplace(path.substr(0, slashPos), SelectorTreeNode());
				pair.first->second.addPath(path.substr(slashPos + 1), data);
			}
		}
		void removePath(const std::string_view& path) {
			size_t slashPos = path.find_first_of("/");
			if (slashPos == std::string::npos) {
				assert(children.at(std::string(path)).children.empty());
				assert(children.at(std::string(path)).data.has_value());
				bool deleted = children.erase(std::string(path));
				assert(deleted);
			} else {
				auto iter = children.find(std::string(path.substr(0, slashPos)));
				iter->second.removePath(path.substr(slashPos + 1));
				if (iter->second.children.empty()) children.erase(iter);
			}
		}
		std::optional<std::variant<BlockType, std::string, std::pair<BlockType, circuit_id_t>>> data = std::nullopt;
		std::unordered_map<std::string, SelectorTreeNode> children;
	};
public:
	BlockSelectorWidget(WidgetId widgetId, MainWindow& mainWindow);
private:
	void addPath(const std::string& path, const std::variant<BlockType, std::string, std::pair<BlockType, circuit_id_t>>& data);
	void removePath(const std::string& path, const std::variant<BlockType, std::string, std::pair<BlockType, circuit_id_t>>& data);

	void createTree(const SelectorTreeNode& node);
	void render() override final;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	std::mutex pathsMux;
	std::map<std::variant<BlockType, std::string, std::pair<BlockType, circuit_id_t>>, std::string> paths;
	SelectorTreeNode root;

	std::mutex proceduralCircuitOrBusParameterMux;
	std::map<std::string, std::map<std::string, std::variant<unsigned, int, float, std::string>>> proceduralCircuitOrBusParameter;
};

#endif /* blockSelectorWidget_h */
