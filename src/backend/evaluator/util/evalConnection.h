#ifndef evalConnection_h
#define evalConnection_h

#include "backend/container/block/connectionEnd.h"
#include "../evalDefs.h"

struct EvalConnectionPoint {
	middle_id_t gateId;
	connection_end_id_t portId;

	EvalConnectionPoint() : gateId(middle_id_t(0)), portId(connection_end_id_t(0)) {};
	EvalConnectionPoint(middle_id_t gateId, connection_end_id_t portId)
		: gateId(gateId), portId(portId) {}

	bool operator==(const EvalConnectionPoint& other) const {
		return gateId == other.gateId && portId == other.portId;
	}

	bool operator!=(const EvalConnectionPoint& other) const {
		return !(*this == other);
	}

	bool operator<(const EvalConnectionPoint& other) const noexcept {
		return std::tie(gateId, portId) < std::tie(other.gateId, other.portId);
	}

	struct Hash {
		std::size_t operator()(const EvalConnectionPoint& point) const noexcept {
			return std::hash<middle_id_t>{}(point.gateId) ^
				   (std::hash<connection_end_id_t>{}(point.portId) << 1);
		}
	};

	std::string toString() const {
		return "ECP(" + std::to_string(gateId) + ", " + std::to_string(portId) + ")";
	}
	nlohmann::json dumpState() const {
		nlohmann::json stateJson;
		stateJson["gateId"] = gateId.get();
		stateJson["portId"] = portId.get();
		return stateJson;
	}
};

struct EvalConnection {
	EvalConnectionPoint source;
	EvalConnectionPoint destination;

	EvalConnection() : source(), destination() {};
	EvalConnection(EvalConnectionPoint source, EvalConnectionPoint destination)
		: source(source), destination(destination) {}

	EvalConnection(middle_id_t srcId, connection_end_id_t srcPort, middle_id_t destId, connection_end_id_t destPort)
		: source(srcId, srcPort), destination(destId, destPort) {}

	bool operator==(const EvalConnection& other) const {
		return source == other.source && destination == other.destination;
	}

	bool operator!=(const EvalConnection& other) const {
		return !(*this == other);
	}

	struct Hash {
		std::size_t operator()(const EvalConnection& connection) const noexcept {
			EvalConnectionPoint::Hash pointHash;
			return pointHash(connection.source) ^ (pointHash(connection.destination) << 1);
		}
	};
	std::string toString() const {
		return "EC( (" + std::to_string(source.gateId) + ", " + std::to_string(source.portId) + ") -> (" +
			   std::to_string(destination.gateId) + ", " + std::to_string(destination.portId) + ") )";
	}

	EvalConnection reverse() const {
		return EvalConnection(destination, source);
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson;
		stateJson["source"] = source.dumpState();
		stateJson["destination"] = destination.dumpState();
		return stateJson;
	}
};

namespace std {
	template <>
	struct hash<EvalConnectionPoint> {
		size_t operator()(const EvalConnectionPoint& point) const noexcept {
			return EvalConnectionPoint::Hash {}(point);
		}
	};

	template <>
	struct hash<EvalConnection> {
		size_t operator()(const EvalConnection& connection) const noexcept {
			return EvalConnection::Hash {}(connection);
		}
	};
}; // namespace std

#endif /* evalConnection_h */
