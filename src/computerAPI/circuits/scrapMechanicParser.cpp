#include "scrapMechanicParser.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

bool validSMJsonChild(nlohmann::json child) {
	if (!child.contains("controller") || !child.at("controller").is_object()) {
		logError("Invalid child {}. \"controller\" not valid.", "ScrapMechanicParser", child.dump());
		return false;
	}
	auto controller = child.at("controller");
	if (!controller.contains("id") || !controller.at("id").is_number_unsigned()) {
		logError("Invalid controller {}. \"id\" not valid.", "ScrapMechanicParser", controller.dump());
		return false;
	}
	return true;
}

nlohmann::json getSMJsonControllerControllers(nlohmann::json controller) {
	if (!controller.contains("controllers") || !controller.at("controllers").is_array()) {
		return nlohmann::json();
	}
	return controller.at("controllers");
}
FPosition getSMJsonChildPos(nlohmann::json child) {
	if (!child.contains("pos") || !child.at("pos").is_object()) {
		logError("No pos in child {}.", "ScrapMechanicParser", child.dump());
		return FPosition::getInvalid();
	}
	auto posData = child.at("pos");
	if (
		!posData.contains("x") || !posData.at("x").is_number_integer() ||
		!posData.contains("y") || !posData.at("y").is_number_integer() ||
		!posData.contains("z") || !posData.at("z").is_number_integer()
	) {
		logError("Invalid pos {}.", "ScrapMechanicParser", posData.dump());
		return FPosition::getInvalid();
	}
	return FPosition(posData.at("x").get<int>(), -posData.at("y").get<int>() + posData.at("z").get<int>()*100);
}

std::vector<circuit_id_t> ScrapMechanicParser::load(const std::string& path) {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
	logInfo("Parsing Scrap Mechanic File (.json)", "ScrapMechanicParser");

	std::ifstream inputFile(path, std::ios::in | std::ios::binary);
	if (!inputFile.is_open()) {
		logError("Couldn't open file at path: " + path, "ScrapMechanicParser");
		return {};
	}

	nlohmann::json json;
	inputFile >> json;
	if (!json.is_object()) {
		logError("Invalid Top level json object.", "ScrapMechanicParser");
		return {};
	}

	if (!json.contains("bodies") || !json.at("bodies").is_array()) {
		logError("Invalid \"bodies\".", "ScrapMechanicParser");
		return {};
	}
	nlohmann::json bodies = json.at("bodies");

	ParsedCircuit parsedCircuit;

	for (auto& body : json.at("bodies")) {
		if (!body.contains("childs") || !body.at("childs").is_array()) {
			logError("Invalid childs. {}", "ScrapMechanicParser", body.dump());
		}
		for (auto& child : body.at("childs")) {
			if (!child.is_object()) {
				logError("Invalid child. Not an object.", "ScrapMechanicParser");
				continue;
			}
			if (!child.contains("shapeId") || !child.at("shapeId").is_string()) {
				logError("Invalid child {}. \"shapeId\" not valid.", "ScrapMechanicParser", child.dump());
				continue;
			}
			std::string shapeId = child["shapeId"].get<std::string>();
			if (shapeId == "7cf717d7-d167-4f2d-a6e7-6b2c70aa3986") { // Switch
				if (!validSMJsonChild(child)) continue;
				auto controller = child.at("controller");
				unsigned int id = controller.at("id").get<unsigned int>();
				parsedCircuit.addBlock(id, getSMJsonChildPos(child), Orientation(), BlockType::SWITCH);
				for (auto& connection : getSMJsonControllerControllers(controller)) {
					if (!connection.contains("id") || !connection.at("id").is_number_unsigned()) {
						logError("Invalid connection {}. \"id\" not valid.", "ScrapMechanicParser", connection.dump());
						continue;
					}
					unsigned int otherId = connection.at("id").get<unsigned int>();
					parsedCircuit.addConnection(id, 0, otherId, 0);
				}
			} else if (shapeId == "1e8d93a4-506b-470d-9ada-9c0a321e2db5") { // Button
				if (!validSMJsonChild(child)) continue;
				auto controller = child.at("controller");
				unsigned int id = controller.at("id").get<unsigned int>();
				parsedCircuit.addBlock(id, getSMJsonChildPos(child), Orientation(), BlockType::BUTTON);
				for (auto& connection : getSMJsonControllerControllers(controller)) {
					if (!connection.contains("id") || !connection.at("id").is_number_unsigned()) {
						logError("Invalid connection {}. \"id\" not valid.", "ScrapMechanicParser", connection.dump());
						continue;
					}
					unsigned int otherId = connection.at("id").get<unsigned int>();
					parsedCircuit.addConnection(id, 0, otherId, 0);
				}
			} else if (shapeId == "9f0f56e8-2c31-4d83-996c-d00a9b296c3f") { // Logic Gate
				if (!validSMJsonChild(child)) continue;
				auto controller = child.at("controller");
				if (!controller.contains("mode") || !controller.at("mode").is_number_unsigned()) {
					logError("Invalid controller {}. \"id\" not valid.", "ScrapMechanicParser", controller.dump());
					continue;
				}
				unsigned int id = controller.at("id").get<unsigned int>();
				BlockType mode = (BlockType)(BlockType::AND + controller.at("mode").get<unsigned int>()); // and or xor nand nor xnor
				parsedCircuit.addBlock(id, getSMJsonChildPos(child), Orientation(), mode);
				for (auto& connection : getSMJsonControllerControllers(controller)) {
					if (!connection.contains("id") || !connection.at("id").is_number_unsigned()) {
						logError("Invalid connection {}. \"id\" not valid.", "ScrapMechanicParser", connection.dump());
						continue;
					}
					unsigned int otherId = connection.at("id").get<unsigned int>();
					parsedCircuit.addConnection(id, 1, otherId, 0);
				}
			} else if (shapeId == "8f7fd0e7-c46e-4944-a414-7ce2437bb30f") { // Timer
				if (!validSMJsonChild(child)) continue;
				auto controller = child.at("controller");
				if (!controller.contains("seconds") || !controller.at("seconds").is_number_unsigned()) {
					logError("Invalid controller {}. \"seconds\" not valid.", "ScrapMechanicParser", controller.dump());
					continue;
				}
				if (!controller.contains("ticks") || !controller.at("ticks").is_number_unsigned()) {
					logError("Invalid controller {}. \"ticks\" not valid.", "ScrapMechanicParser", controller.dump());
					continue;
				}
				unsigned int id = controller.at("id").get<unsigned int>();
				unsigned int seconds = controller.at("seconds").get<unsigned int>();
				unsigned int ticks = controller.at("ticks").get<unsigned int>() + seconds*40;
				parsedCircuit.addBlock(id, getSMJsonChildPos(child), Orientation(), BlockType::BUFFER); // should be timer
				logInfo("Timer of length {}.", "", ticks);
				for (auto& connection : getSMJsonControllerControllers(controller)) {
					if (!connection.contains("id") || !connection.at("id").is_number_unsigned()) {
						logError("Invalid connection {}. \"id\" not valid.", "ScrapMechanicParser", connection.dump());
						continue;
					}
					unsigned int otherId = connection.at("id").get<unsigned int>();
					parsedCircuit.addConnection(id, 1, otherId, 0);
				}
			}/* else if ( // Lights
				shapeId == "073f92af-f37e-4aff-96b3-d66284d5081c" || // Path Light Block // Creative Mode
				shapeId == "695d66c8-b937-472d-8bc2-f3d72dd92879" || // Warehouse Spotlight
				shapeId == "ed27f5e2-cac5-4a32-a5d9-49f116acc6af" || // Headlight
				shapeId == "16ba2d22-7b96-4c5e-9eb7-f6422ed80ad4" || // Warehouse Spotlight // Survival Mode
				shapeId == "47062936-5d28-43ec-81b5-8fdb619e97e4" || // Ship Light
				shapeId == "5e3dff9b-2450-44ae-ad46-d2f6b5148cbf" || // Warehouse Square Light
				shapeId == "7b2c96af-a4a1-420e-9370-ea5b58f23a7e" || // Warehouse Fluorescent Light
				shapeId == "abaef792-741e-4c6b-8e79-02461a37b035" || // Frame Beam Light
				shapeId == "da6e54df-a223-4a0e-b42f-ddeddd33f5b3" || // Work Light
				shapeId == "e91b0bf2-dafa-439e-a503-286e91461bb0" || // Warehouse Wall Light
				shapeId == "ebefa387-fe4a-4839-bdd9-b6b4da39368f" || // Shack Light
				shapeId == "5929670a-fe70-40a7-83e4-45cf179b0aef" || // On / Off Light Block
				shapeId == "9b9e4547-0846-42da-b812-9f1a5b22e52f"	 // Arrow Light Block
			) {
				if (!validSMJsonChild(child)) continue;
				auto controller = child.at("controller");
				unsigned int id = controller.at("id").get<unsigned int>();
				parsedCircuit.addBlock(id, getSMJsonChildPos(child), Orientation(), BlockType::BUFFER); // should be timer
			}*/ // endId will be wrong for now :(
		}
	}

	inputFile.close();

	return { loadParsedCircuit(parsedCircuit) };
}
