#include "generatedCircuitValidator.h"

#include "backend/blockData/blockDataManager.h"

void GeneratedCircuitValidator::validate() {
	bool isValid = true;

	isValid = isValid && validateBlockData();
	isValid = isValid && validateBlockTypes();
	isValid = isValid && handleInvalidConnections();
	isValid = isValid && setOverlapsUnpositioned();
	isValid = isValid && handleUnpositionedBlocks();

	generatedCircuit.valid = isValid;
}

bool GeneratedCircuitValidator::validateBlockData() {
	Size size = generatedCircuit.getSize();
	if (size.w == 0) size.w = 1;
	if (size.h == 0) size.h = 1;
	for (auto& port : generatedCircuit.getConnectionPorts()) {
		size.extentToFitTartgetCell(port.positionOnBlock);
	}
	generatedCircuit.setSize(size);
	return true;
}

bool GeneratedCircuitValidator::validateBlockTypes() {
	for (std::pair<const block_id_t, GeneratedCircuit::GeneratedCircuitBlockData>& blockData : generatedCircuit.blocks) {
		if (blockData.second.type == BlockType::NONE) {
			logWarning("Found a NONE type block, converting to buffer", "GeneratedCircuitValidator");
			blockData.second.type = BlockType::JUNCTION;
			// return false;
		}
	}
	return true;
}

bool GeneratedCircuitValidator::handleInvalidConnections() {
	// map to connection frequencies
	std::unordered_map<GeneratedCircuit::ConnectionData, int, ConnectionHash> connectionCounts;

	// count the connections
	for (auto& conn : generatedCircuit.connections) {
		++connectionCounts[conn];
	}

	// int i = 0;
	// while (i < (int)generatedCircuit.connections.size()) {
	// 	GeneratedCircuit::ConnectionData& conn = generatedCircuit.connections[i];

	// 	GeneratedCircuit::ConnectionData reversePair{
	// 		.outputBlockId = conn.inputBlockId,
	// 		.outputId = conn.inputId,
	// 		.inputBlockId = conn.outputBlockId,
	// 		.inputId = conn.outputId,
	// 	};

	// 	if (--connectionCounts[reversePair] < 0) {
	// 		generatedCircuit.connections.push_back(reversePair);
	// 		logInfo(
	// 			"Added reciprocated connection between: ({} {}) and ({} {})",
	// 			"GeneratedCircuitValidator",
	// 			conn.inputBlockId,
	// 			conn.outputBlockId,
	// 			reversePair.inputBlockId,
	// 			reversePair.outputBlockId
	// 		);
	// 		connectionCounts[reversePair] = 0;
	// 	}
	// 	++i;
	// }

	// check all remaining connections were found
	// for (const auto& [pair, count] : connectionCounts) {
	// 	if (count != 0) {
	// 		logWarning("Invalid connection handling, connection frequency: ({} {}) {}", "GeneratedCircuitValidator", pair.outputBlockId, pair.inputBlockId, count);
	// 		return false;
	// 	}
	// }

	return true;
}

bool GeneratedCircuitValidator::setOverlapsUnpositioned() {
	for (auto& [id, block] : generatedCircuit.blocks) {
		if (block.position.x == std::numeric_limits<coordinate_t>::max() || block.position.y == std::numeric_limits<coordinate_t>::max()) {
			continue;
		}

		Position intPos(static_cast<int>(block.position.x), static_cast<int>(block.position.y));

		BlockData* blockData = blockDataManager.getBlockData(block.type);
		if (!blockData) {
			logError("Could not find block type data for block type: {}", "GeneratedCircuitValidator", (unsigned int)block.type);
		}

		std::vector<Position> takenPositions;
		bool hasOverlap = false;
		for (auto iter = blockData->getSize(block.orientation).iter(); iter; iter++) {
			Position checkPos(intPos + *iter);
			if (occupiedPositions.count(checkPos)) {
				hasOverlap = true;
				break;
			} else {
				takenPositions.push_back(checkPos);
			}
		}

		if (hasOverlap) {
			// set the block position as undefined
			// logInfo(
			// 	"Found overlapped block position at {} --> {}, setting to undefined position",
			// 	"GeneratedCircuitValidator",
			// 	block.position.toString(),
			// 	intPos.toString()
			// );
			block.position.x = std::numeric_limits<coordinate_t>::max();
			block.position.y = std::numeric_limits<coordinate_t>::max();
		} else {
			// mark all positions as occupied
			occupiedPositions.insert(takenPositions.begin(), takenPositions.end());
		}
	}

	return true;
}

// iterative dfs
template <class PreVisit, class PostVisit>
void depthFirstSearch(
	const std::unordered_map<block_id_t, std::vector<block_id_t>>& adj,
	block_id_t start,
	std::unordered_set<block_id_t>& visited,
	PreVisit preVisit,
	PostVisit postVisit
) {
	std::stack<block_id_t> stack;
	stack.push(start);
	visited.insert(start);
	preVisit(start);
	while (!stack.empty()) {
		block_id_t node = stack.top();
		bool hasUnvisited = false;
		if (adj.count(node)) {
			for (block_id_t neighbor : adj.at(node)) {
				if (visited.count(neighbor)) continue;
				visited.insert(neighbor);
				preVisit(neighbor);
				stack.push(neighbor);
				hasUnvisited = true;
				break;
			}
		}

		if (!hasUnvisited) {
			stack.pop();
			postVisit(node);
		}
	}
}

// SCC META GRAPH TOPOLOGICAL SORT
bool GeneratedCircuitValidator::handleUnpositionedBlocks() {
	// Separate components so that we can place down disconnected components independently
	std::unordered_map<block_id_t, std::vector<block_id_t>> undirectedAdj;
	undirectedAdj.reserve(generatedCircuit.blocks.size());
	for (const GeneratedCircuit::ConnectionData& conn : generatedCircuit.connections) {
		block_id_t u = conn.inputBlockId;
		block_id_t v = conn.outputBlockId;
		undirectedAdj[u].push_back(v);
		undirectedAdj[v].push_back(u);
	}

	// find cc's by dfs
	std::vector<std::unordered_set<block_id_t>> components;
	std::unordered_map<block_id_t, int> blockToComponent;
	std::unordered_set<block_id_t> visited;
	for (const auto& [id, block] : generatedCircuit.blocks) {
		if (!visited.count(id)) {
			components.push_back({});
			depthFirstSearch(undirectedAdj, id, visited, [&](block_id_t node) {
				components.back().insert(node);
				blockToComponent[node] = components.size() - 1;
			}, [&](block_id_t) {});
		}
	}

	// preprocess connected component connections
	std::vector<std::unordered_map<block_id_t, std::vector<block_id_t>>> componentAdjs(components.size());
	for (const GeneratedCircuit::ConnectionData& conn : generatedCircuit.connections) {
		auto iter = generatedCircuit.blocks.find(conn.outputBlockId);
		if (iter == generatedCircuit.blocks.end()) continue;

		if (blockDataManager.isConnectionInput(iter->second.type, conn.outputId)) {
			int cc = blockToComponent[conn.inputBlockId];
			componentAdjs[cc][conn.inputBlockId].push_back(conn.outputBlockId);
		}
	}

	// precompute input connections for each block for layer placement
	std::unordered_map<block_id_t, std::vector<block_id_t>> inputConnections;
	for (const auto& conn : generatedCircuit.connections) {
		inputConnections[conn.inputBlockId].push_back(conn.outputBlockId);
	}

	const int componentSpacing = 5;
	int currentYOffset = 0;
	for (int ccIndex = 0; ccIndex < componentAdjs.size(); ++ccIndex) {
		const std::unordered_map<block_id_t, std::vector<block_id_t>>& adj = componentAdjs[ccIndex];
		// logInfo("Parsing CC {}", "", ccIndex);

		// find SCC metagraph DAG for topological sort using kosaraju's
		// gather stack that shows decreasing post visit numbers
		std::stack<block_id_t> postNumber; // highest post number node is a part of a source SCC, run on G^R
		visited.clear();
		for (block_id_t id : components[ccIndex]) {
			if (!visited.count(id)) {
				depthFirstSearch(adj, id, visited, [&](block_id_t) {}, [&](block_id_t node) { postNumber.push(node); });
			}
		}

		// make reverse graph in adjacency list
		std::unordered_map<block_id_t, std::vector<block_id_t>> revAdj;
		for (const auto& [u, adjs] : adj) {
			for (block_id_t v : adjs) {
				revAdj[v].push_back(u);
			}
		}

		// run dfs from source SCC's in reverse graph, sink SCCs of regular
		std::vector<std::vector<block_id_t>> sccs;
		visited.clear();
		while (!postNumber.empty()) {
			block_id_t node = postNumber.top();
			postNumber.pop();
			if (!visited.count(node)) {
				std::vector<block_id_t> revCC;
				depthFirstSearch(adj, node, visited, [&](block_id_t node) { revCC.push_back(node); }, [&](block_id_t) {});
				sccs.push_back(revCC);
			}
		}

		// Make metagraph and run topo sort
		std::unordered_map<int, std::vector<int>> metaGraph;
		std::unordered_map<block_id_t, int> blockToScc;
		for (int i = 0; i < sccs.size(); ++i) {
			for (block_id_t block : sccs[i]) {
				blockToScc[block] = i;
			}
		}

		std::unordered_map<int, int> sccInDegree;
		for (const auto& [u, adjs] : adj) {
			int sccU = blockToScc[u];
			for (block_id_t v : adjs) {
				int sccV = blockToScc[v];
				if (sccU != sccV) {
					// add sccV as a new adjacency to sccU
					metaGraph[sccU].push_back(sccV);
					++sccInDegree[sccV]; // SCC gains an incoming edge
				}
			}
		}

		// topological sort via removing source nodes
		std::queue<int> q;
		for (int i = 0; i < sccs.size(); ++i) {
			if (sccInDegree[i] == 0) {
				q.push(i);
			}
		}

		std::vector<int> sccOrder;
		while (!q.empty()) {
			int u = q.front();
			q.pop();
			sccOrder.push_back(u);
			for (int v : metaGraph[u]) {
				if (--sccInDegree[v] == 0) {
					q.push(v);
				}
			}
		}

		if (sccOrder.size() != sccs.size()) {
			logError("Cycle in SCC meta graph. Topological sort failed.", "GeneratedCircuitValidator");
			return false;
		}

		std::unordered_map<block_id_t, int> layers;
		for (int sccIdx : sccOrder) {
			for (block_id_t id : sccs[sccIdx]) {
				int maxLayer = 0;
				auto it = inputConnections.find(id);
				if (it != inputConnections.end()) {
					for (block_id_t outputBlock : it->second) {
						maxLayer = std::max(maxLayer, layers[outputBlock] + 1);
					}
				}
				layers[id] = maxLayer;
			}
		}

		// place blocks in layers
		const int xSpacing = 2; // x spacing between layers
		std::unordered_map<int, int> layerYcounter;
		int maxYPlaced = 0;
		for (int sccIndex : sccOrder) {
			for (block_id_t id : sccs[sccIndex]) {
				auto iter = generatedCircuit.blocks.find(id);
				if (iter == generatedCircuit.blocks.end()) continue;
				GeneratedCircuit::GeneratedCircuitBlockData& block = iter->second;
				if (block.position.x != std::numeric_limits<coordinate_t>::max() && block.position.y != std::numeric_limits<coordinate_t>::max()) {
					continue;
				}

				// check block dimensions in case of custom block
				BlockData* blockData = blockDataManager.getBlockData(block.type);
				if (!blockData) {
					logError("Could not find block type data for block type: {}", "GeneratedCircuitValidator", (unsigned int)block.type);
				}
				Size blockSize = blockData->getSize(block.orientation);

				const int layer = layers[id];
				const int x = (layer - 1) * xSpacing;

				// find first available Y position in current layer column
				int y;
				if (layerYcounter.find(x) != layerYcounter.end()) {
					y = layerYcounter[x];
				} else {
					y = currentYOffset;
				}

				std::vector<Position> takenPositions;
				bool canPlace;
				do {
					canPlace = true;
					takenPositions.clear();
					std::vector<Position> takenPositions;
					canPlace = true;
					for (auto iter = blockSize.iter(); iter; iter++) {
						Position checkPos(Position(x, y) + *iter);
						if (occupiedPositions.count(checkPos)) {
							canPlace = false;
							break;
						} else {
							takenPositions.push_back(checkPos);
						}
					}
					if (!canPlace) ++y;
				} while (!canPlace);

				// mark the found positions
				occupiedPositions.insert(takenPositions.begin(), takenPositions.end());

				block.position = Position(x, y);
				layerYcounter[x] = y + blockSize.h;

				coordinate_t blockMaxY = y + blockSize.h - 1;

				maxYPlaced = std::max(maxYPlaced, blockMaxY);
			}
		}
		currentYOffset = componentSpacing + maxYPlaced;
	}

	return true;
}
