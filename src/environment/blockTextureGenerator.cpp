#include "blockTextureGenerator.h"

BlockTextureGenerator::BlockTextureGenerator(std::shared_ptr<Font> font) : font(std::move(font)) {}

void BlockTextureGenerator::createCustomBlockTexture(const BlockData& blockData, CpuImage& img, int scale) const {
	img.addRect(
		{ 5 * scale / 256, 5 * scale / 256 },
		img.getSize() - Vec2Int(10 * scale / 256, 10 * scale / 256),
		{ 76, 76, 76, 255 }
	);

	Rect blockNameArea1{
		{scale / 2, scale / 2},
		img.getSize() - Vec2Int(scale, scale)
	};
	Rect blockNameArea2 {
		{img.getSize().x * 2 / 10, img.getSize().y * 2 / 10},
		{img.getSize().x * 6 / 10, img.getSize().y * 6 / 10}
	};
	Rect blockNameArea = blockNameArea1.intersect(blockNameArea2);
	if (blockNameArea.empty()) {
		blockNameArea = blockNameArea2;
	}
	blockNameArea.size.x = std::max(blockNameArea.size.x, 1);
	blockNameArea.size.y = std::max(blockNameArea.size.y, 1);
	blockNameArea.pos.x = std::clamp(blockNameArea.pos.x, 0, std::max(0, img.getSize().x - blockNameArea.size.x));
	blockNameArea.pos.y = std::clamp(blockNameArea.pos.y, 0, std::max(0, img.getSize().y - blockNameArea.size.y));

	std::vector<Rect> reservedAreas;

	drawBlockName(blockData, img, scale, blockNameArea, reservedAreas);
	drawConnectionLabels(blockData, img, scale, reservedAreas);
}

void BlockTextureGenerator::createBusBlockTexture(const BlockData& blockData, CpuImage& img, int scale) const {
	img.fill({ 0, 0, 0, 0 });
	int minY = 0;
	int maxY = 0;
	bool first = true;
	for (const std::pair<const connection_end_id_t, BlockData::ConnectionData>& connection : blockData.getConnections()) {
		Vec2Int portTexturePos = getPortTexturePosition(connection.second, scale);
		int laneCount = connection.second.getBitWidth();
		int lineSize = std::max(5, std::min(5 * laneCount, 5 * 8)) * scale / 256;
		int thisMinY = portTexturePos.y - lineSize;
		int thisMaxY = portTexturePos.y + lineSize;
		if (first || thisMinY < minY) {
			minY = thisMinY;
		}
		if (first || thisMaxY > maxY) {
			maxY = thisMaxY;
		}
		first = false;

		int x1 = img.getSize().x / 2;
		int x2 = portTexturePos.x;
		img.addRect(
			{ std::min(x1, x2), portTexturePos.y - lineSize },
			{ std::abs(x1 - x2), lineSize * 2 },
			{ 76, 76, 76, 255 }
		);
		drawConnectionNodeCircle(img, portTexturePos, laneCount, scale);
	}

	int usingRadius = (9 * 4) * scale / 256 + 1;
	img.addLine(
		{ img.getSize().x / 2, minY + usingRadius },
		{ img.getSize().x / 2, maxY - usingRadius },
		usingRadius,
		{ 76, 76, 76, 255 },
		true
	);
}

// Renders the block's display name inside the provided rectangle, rotating if the block is tall
void BlockTextureGenerator::drawBlockName(const BlockData& blockData, CpuImage& img, int scale, const Rect& labelArea, std::vector<Rect>& reservedAreas) const {
	if (!font || labelArea.empty()) return;

	const CpuImage::Pixel textColor{ 255, 255, 255, 255 };
	const bool rotate = blockData.getSize().w < blockData.getSize().h;
	const Rotation rotation = rotate ? Rotation::NINETY : Rotation::ZERO;

	auto [textPosition, textSize] = img.writeStringInArea(
		font,
		blockData.getName(),
		labelArea.pos,
		labelArea.size,
		textColor,
		rotation,
		true,
		true
	);
	Rect textArea{ textPosition, textSize };
	if (!textArea.empty()) {
		textArea = Rect{
			textArea.pos - Vec2Int(6, 6),
			textArea.size + Vec2Int(12, 12)
		};
		// img.addOutlinedRect(
		// 	textArea.pos,
		// 	textArea.size,
		// 	4,
		// 	{ 255, 255, 255, 80 },
		// 	true
		// );
		reservedAreas.push_back(textArea);
	}
}

// for each connection port, draws a circle to represent the port and attempts to place a label nearby
void BlockTextureGenerator::drawConnectionLabels(
	const BlockData& blockData,
	CpuImage& img,
	int scale,
	std::vector<Rect>& reservedAreas
) const {
	const Vec2Int imageSize = img.getSize();
	const LabelLayoutConfig config = buildLabelLayoutConfig(scale);

	std::array<std::vector<PortLabelRequest>, 4> labelsBySide;
	const std::vector<Rect> staticObstacles = reservedAreas;
	std::vector<Rect> occupiedAreas = reservedAreas;

	for (const std::pair<const connection_end_id_t, BlockData::ConnectionData>& connection : blockData.getConnections()) {
		Vec2Int portTexturePos = getPortTexturePosition(connection.second, scale);
		int laneCount = connection.second.getBitWidth();
		int circleRadius = drawConnectionNodeCircle(img, portTexturePos, laneCount, scale);

		if (!font) {
			continue;
		}

		std::optional<std::string> connectionName = blockData.getConnectionIdToName(connection.first);
		if (!connectionName || connectionName->empty()) {
			continue;
		}
		std::string labelText = *connectionName;

		const PortLabelSide side = detectPreferredSide(blockData, connection.second);
		const int paddingFromPort = config.basePadding + circleRadius;
		labelsBySide[static_cast<uint8_t>(side)].push_back(PortLabelRequest{
			std::move(labelText),
			portTexturePos,
			side,
			paddingFromPort
		});
	}

	if (!font) {
		return;
	}

	const CpuImage::Pixel textColor{ 255, 255, 255, 255 };

	for (uint8_t sideIndex : {0, 1, 2, 3}) {
		PortLabelSide side = static_cast<PortLabelSide>(sideIndex);
		std::vector<PortLabelRequest>& requests = labelsBySide[sideIndex];
		if (requests.empty()) {
			continue;
		}

		std::vector<LabelPlacement> placements;
		placements.reserve(requests.size());
		for (const PortLabelRequest& request : requests) {
			std::optional<LabelPlacement> placement = buildPlacementForRequest(request, imageSize, config, staticObstacles);
			if (placement) {
				placements.push_back(std::move(*placement));
			}
		}
		if (placements.empty()) {
			continue;
		}

		std::vector<LabelPlacement*> placementRefs;
		placementRefs.reserve(placements.size());
		for (LabelPlacement& placement : placements) {
			placementRefs.push_back(&placement);
		}

		const int axisLimit = (side == PortLabelSide::LEFT || side == PortLabelSide::RIGHT) ? imageSize.y : imageSize.x;
		distributeAlongAxis(placementRefs, axisLimit, config.labelSpacing);

		for (LabelPlacement& placement : placements) {
			Rect rect;
			rect.size = placement.size;
			Vec2Int pos = placement.basePos;
			if (side == PortLabelSide::LEFT || side == PortLabelSide::RIGHT) {
				pos.y = placement.axisPos;
			} else {
				pos.x = placement.axisPos;
			}
			rect.pos = pos;

			if (!resolveCollisions(rect, side, axisLimit, config.slideStep, occupiedAreas)) {
				continue;
			}

			occupiedAreas.push_back(rect);
			reservedAreas.push_back(rect);

			img.writeStringInArea(
				font,
				placement.request.text,
				rect.pos,
				rect.size,
				textColor,
				Rotation::ZERO,
				true,
				true
			);
		}
	}
}

// derives width, height, padding, and spacing targets for labels based on current texture scale
BlockTextureGenerator::LabelLayoutConfig BlockTextureGenerator::buildLabelLayoutConfig(int scale) const {
	LabelLayoutConfig config{};
	config.targetWidth = std::max((scale * 90) / 256, 32);
	config.minWidth = std::max((scale * 20) / 256, 10);
	config.targetHeight = std::max((scale * 50) / 256, 14);
	config.minHeight = std::max((scale * 18) / 256, 8);
	config.basePadding = std::max((scale * 10) / 256, 3);
	config.labelSpacing = std::max((scale * 10) / 256, 3);
	config.obstaclePadding = std::max((scale * 6) / 256, 2);
	config.slideStep = std::max(2, config.labelSpacing);
	return config;
}

// creates a candidate rectangle for a label on a specific side while respecting available spans
std::optional<BlockTextureGenerator::LabelPlacement> BlockTextureGenerator::buildPlacementForRequest(
	const PortLabelRequest& request,
	const Vec2Int& imageSize,
	const LabelLayoutConfig& config,
	const std::vector<Rect>& staticObstacles
) const {
	LabelPlacement placement;
	placement.request = request;

	const bool horizontalSide = request.side == PortLabelSide::LEFT || request.side == PortLabelSide::RIGHT;
	const int axisLimit = horizontalSide ? imageSize.y : imageSize.x;
	const int axisPreferred = horizontalSide ? config.targetHeight : config.targetWidth;
	const int axisMin = horizontalSide ? config.minHeight : config.minWidth;
	const int axisSize = std::clamp(axisPreferred, axisMin, axisLimit);
	const int acrossPreferred = horizontalSide ? config.targetWidth : config.targetHeight;
	const int acrossMin = horizontalSide ? config.minWidth : config.minHeight;

	if (horizontalSide) {
		const bool growRight = request.side == PortLabelSide::LEFT;
		int start = growRight
			? request.portTexturePos.x + request.paddingFromPort
			: request.portTexturePos.x - request.paddingFromPort;
		start = std::clamp(start, 0, imageSize.x);
		const int span = computeHorizontalSpan(growRight, start, request.portTexturePos.y, imageSize.x, config, staticObstacles);
		if (span < acrossMin) {
			return std::nullopt;
		}
		int width = std::clamp(acrossPreferred, acrossMin, span);
		if (!growRight) {
			if (width > start) {
				width = start;
			}
			if (width < acrossMin) {
				return std::nullopt;
			}
			placement.basePos = { start - width, 0 };
		} else {
			placement.basePos = { start, 0 };
		}
		placement.size = { width, axisSize };
	} else {
		const bool growDown = request.side == PortLabelSide::TOP;
		int start = growDown
			? request.portTexturePos.y + request.paddingFromPort
			: request.portTexturePos.y - request.paddingFromPort;
		start = std::clamp(start, 0, imageSize.y);
		const int span = computeVerticalSpan(growDown, start, request.portTexturePos.x, imageSize.y, config, staticObstacles);
		if (span < acrossMin) {
			return std::nullopt;
		}
		int height = std::clamp(acrossPreferred, acrossMin, span);
		if (!growDown) {
			if (height > start) {
				height = start;
			}
			if (height < acrossMin) {
				return std::nullopt;
			}
			placement.basePos = { 0, start - height };
		} else {
			placement.basePos = { 0, start };
		}
		placement.size = { axisSize, height };
	}

	if (placement.size.x <= 0 || placement.size.y <= 0) {
		return std::nullopt;
	}
	placement.axisAnchor = horizontalSide ? request.portTexturePos.y : request.portTexturePos.x;
	placement.axisPos = placement.axisAnchor;
	return placement;
}

// determines how many pixels a horizontal label can extend before hitting the texture edge or obstacles
int BlockTextureGenerator::computeHorizontalSpan(
	bool growRight,
	int start,
	int axisCoord,
	int imageWidth,
	const LabelLayoutConfig& config,
	const std::vector<Rect>& staticObstacles
) const {
	int limit = growRight ? imageWidth : 0;
	for (const Rect& rect : staticObstacles) {
		if (!overlapsAxis(axisCoord, rect.pos.y, rect.size.y, config.obstaclePadding)) {
			continue;
		}
		if (growRight) {
			limit = std::min(limit, rect.pos.x - config.obstaclePadding);
		} else {
			limit = std::max(limit, rect.pos.x + rect.size.x + config.obstaclePadding);
		}
	}
	if (growRight) {
		int span = std::max(0, limit - start);
		return span;
	}
	int span = std::max(0, start - limit);
	return span;
}

// determines how many pixels a vertical label can extend before hitting the texture edge or obstacles
int BlockTextureGenerator::computeVerticalSpan(
	bool growDown,
	int start,
	int axisCoord,
	int imageHeight,
	const LabelLayoutConfig& config,
	const std::vector<Rect>& staticObstacles
) const {
	int limit = growDown ? imageHeight : 0;
	for (const Rect& rect : staticObstacles) {
		if (!overlapsAxis(axisCoord, rect.pos.x, rect.size.x, config.obstaclePadding)) {
			continue;
		}
		if (growDown) {
			limit = std::min(limit, rect.pos.y - config.obstaclePadding);
		} else {
			limit = std::max(limit, rect.pos.y + rect.size.y + config.obstaclePadding);
		}
	}
	if (growDown) {
		int span = std::max(0, limit - start);
		return span;
	}
	int span = std::max(0, start - limit);
	return span;
}

// repositions labels along their axis so that they keep spacing while staying within bounds
void BlockTextureGenerator::distributeAlongAxis(std::vector<LabelPlacement*>& placements, int axisLimit, int spacing) const {
	if (placements.empty()) {
		return;
	}

	std::sort(
		placements.begin(),
		placements.end(),
		compareByAxisAnchor
	);

	int prevBottom = -spacing;
	for (LabelPlacement* placement : placements) {
		const int axisSize = placementAxisSize(placement);
		int desiredTop = placement->axisAnchor - axisSize / 2;
		desiredTop = std::clamp(desiredTop, 0, std::max(0, axisLimit - axisSize));
		if (desiredTop < prevBottom + spacing) {
			desiredTop = prevBottom + spacing;
		}
		placement->axisPos = std::clamp(desiredTop, 0, std::max(0, axisLimit - axisSize));
		prevBottom = placement->axisPos + axisSize;
	}

	int nextTop = axisLimit;
	for (int i = static_cast<int>(placements.size()) - 1; i >= 0; --i) {
		LabelPlacement* placement = placements[i];
		const int axisSize = placementAxisSize(placement);
		int maxTop = nextTop - axisSize;
		if (i != static_cast<int>(placements.size()) - 1) {
			maxTop -= spacing;
		}
		maxTop = std::max(0, maxTop);
		if (placement->axisPos > maxTop) {
			placement->axisPos = maxTop;
		}
		nextTop = placement->axisPos;
	}
}

// slides a label rectangle up or down the axis until it stops intersecting earlier placements
bool BlockTextureGenerator::resolveCollisions(
	Rect& rect,
	PortLabelSide side,
	int axisLimit,
	int slideStep,
	const std::vector<Rect>& occupiedAreas
) const {
	if (!intersectsAny(rect, occupiedAreas)) {
		return true;
	}

	const bool horizontalSide = side == PortLabelSide::LEFT || side == PortLabelSide::RIGHT;
	const int axisSize = horizontalSide ? rect.size.y : rect.size.x;
	const int maxAxisValue = std::max(0, axisLimit - axisSize);
	const int step = std::max(1, slideStep);
	const int originalAxis = horizontalSide ? rect.pos.y : rect.pos.x;

	for (int delta = step; delta <= axisLimit + step; delta += step) {
		for (int dir : { -1, 1 }) {
			int candidateAxis = originalAxis + dir * delta;
			candidateAxis = std::clamp(candidateAxis, 0, maxAxisValue);
			Rect candidate = rect;
			if (horizontalSide) {
				candidate.pos.y = candidateAxis;
			} else {
				candidate.pos.x = candidateAxis;
			}
			if (intersectsAny(candidate, occupiedAreas)) {
				continue;
			}
			rect = candidate;
			return true;
		}
	}
	return false;
}

int BlockTextureGenerator::placementAxisSize(const LabelPlacement* placement) {
	return (placement->request.side == PortLabelSide::LEFT || placement->request.side == PortLabelSide::RIGHT)
		? placement->size.y
		: placement->size.x;
}

bool BlockTextureGenerator::compareByAxisAnchor(const LabelPlacement* lhs, const LabelPlacement* rhs) {
	return lhs->axisAnchor < rhs->axisAnchor;
}

bool BlockTextureGenerator::intersectsAny(const Rect& candidate, const std::vector<Rect>& occupiedAreas) {
	for (const Rect& reserved : occupiedAreas) {
		if (candidate.intersects(reserved)) {
			return true;
		}
	}
	return false;
}

bool BlockTextureGenerator::overlapsAxis(int coord, int rectStart, int rectSize, int padding) {
	const int minRange = rectStart - padding;
	const int maxRange = rectStart + rectSize + padding;
	return coord >= minRange && coord <= maxRange;
}

BlockTextureGenerator::PortLabelSide BlockTextureGenerator::detectPreferredSide(const BlockData& blockData, const BlockData::ConnectionData& connection) {
	const float epsilon = 1e-3f;
	if (std::abs(connection.portOffset.dx - 0.5f) <= epsilon && std::abs(connection.portOffset.dy - 0.5f) <= epsilon) {
		const Size blockSize = blockData.getSize();
		const float blockWidth = static_cast<float>(blockSize.w);
		const float blockHeight = static_cast<float>(blockSize.h);
		const float centerX = std::clamp(static_cast<float>(connection.positionOnBlock.dx) + 0.5f, 0.0f, blockWidth);
		const float centerY = std::clamp(static_cast<float>(connection.positionOnBlock.dy) + 0.5f, 0.0f, blockHeight);

		const float distLeft = centerX;
		const float distRight = blockWidth - centerX;
		const float distTop = centerY;
		const float distBottom = blockHeight - centerY;

		const float minHorizontal = std::min(distLeft, distRight);
		const float minVertical = std::min(distTop, distBottom);

		Vector positionOnBlock = connection.positionOnBlock;

		if (blockData.getInputConnectionId(positionOnBlock) && blockData.getOutputConnectionId(positionOnBlock)) {
			switch (connection.portType) {
			case BlockData::ConnectionData::PortType::OUTPUT:
				return PortLabelSide::RIGHT;
			case BlockData::ConnectionData::PortType::INPUT:
				return PortLabelSide::LEFT;
			default:
				return PortLabelSide::LEFT;
			}
		}

		if (minHorizontal <= minVertical + epsilon) {
			if (std::abs(distLeft - distRight) <= epsilon) {
				switch (connection.portType) {
				case BlockData::ConnectionData::PortType::OUTPUT:
					return PortLabelSide::RIGHT;
				case BlockData::ConnectionData::PortType::INPUT:
					return PortLabelSide::LEFT;
				default:
					return PortLabelSide::LEFT;
				}
			}
			return distLeft <= distRight ? PortLabelSide::LEFT : PortLabelSide::RIGHT;
		}

		// vertical preference only wins when it is clearly closer than horizontal.
		if (std::abs(distTop - distBottom) <= epsilon) {
			return PortLabelSide::TOP;
		}
		return distTop <= distBottom ? PortLabelSide::TOP : PortLabelSide::BOTTOM;
	}
	const float dx = connection.portOffset.dx - 0.5f;
	const float dy = connection.portOffset.dy - 0.5f;
	if (std::abs(dx) >= std::abs(dy)) {
		return dx < 0 ? PortLabelSide::LEFT : PortLabelSide::RIGHT;
	}
	return dy < 0 ? PortLabelSide::TOP : PortLabelSide::BOTTOM;
}

Vec2Int BlockTextureGenerator::getPortTexturePosition(const BlockData::ConnectionData& connection, int scale) {
	return Vec2Int(
		connection.positionOnBlock.dx * scale,
		connection.positionOnBlock.dy * scale
	) + Vec2Int(
		connection.portOffset.dx * static_cast<float>(scale),
		connection.portOffset.dy * static_cast<float>(scale)
	);
}

// draws a circle representing a connection port, adding bit width text if applicable
int BlockTextureGenerator::drawConnectionNodeCircle(CpuImage& img, const Vec2Int& position, int bitWidth, int scale) const {
	const int circleRadius = computePortCircleRadius(bitWidth, scale);
	img.addCircle(position, circleRadius, { 0, 0, 0, 255 });
	if (bitWidth > 1 && font) {
		const CpuImage::Pixel textColor{ 255, 255, 255, 255 };
		// calculate fit square given circle radius
		int squareHalfSize = static_cast<int>(std::floor(circleRadius / std::sqrt(2)));
		Vec2Int textAreaSize { squareHalfSize * 2, squareHalfSize * 2 };
		textAreaSize.x = std::max(textAreaSize.x, 1);
		textAreaSize.y = std::max(textAreaSize.y, 1);
		Vec2Int textAreaPos { position.x - squareHalfSize, position.y - squareHalfSize };
		const Vec2Int imageSize = img.getSize();
		textAreaPos.x = std::clamp(textAreaPos.x, 0, std::max(0, imageSize.x - textAreaSize.x));
		textAreaPos.y = std::clamp(textAreaPos.y, 0, std::max(0, imageSize.y - textAreaSize.y));
		img.writeStringInArea(
			font,
			std::to_string(bitWidth),
			textAreaPos,
			textAreaSize,
			textColor,
			Rotation::ZERO,
			true,
			true
		);
	}
	return circleRadius;
}

int BlockTextureGenerator::computePortCircleRadius(int bitWidth, int scale) {
	const int baseSize = std::max(9, std::min(9 * bitWidth, 9 * 8)) + 4;
	const int scaledRadius = (baseSize * scale) / 256;
	return std::max(1, scaledRadius);
}
