#ifndef blockTextureGenerator_h
#define blockTextureGenerator_h

#include "gpu/cpuImageEditor/cpuImage.h"
#include "backend/blockData/blockData.h"

class BlockTextureGenerator {
public:
	explicit BlockTextureGenerator(std::shared_ptr<Font> font);

	void setFont(std::shared_ptr<Font> newFont) { font = std::move(newFont); }
	std::shared_ptr<Font> getFont() const { return font; }

	void createCustomBlockTexture(const BlockData& blockData, CpuImage& img, int scale) const;
	void createBusBlockTexture(const BlockData& blockData, CpuImage& img, int scale) const;

private:
	struct Rect {
		Vec2Int pos;
		Vec2Int size;

		bool empty() const { return size.x <= 0 || size.y <= 0; }
		bool intersects(const Rect& other) const {
			return !(pos.x + size.x <= other.pos.x || other.pos.x + other.size.x <= pos.x ||
				pos.y + size.y <= other.pos.y || other.pos.y + other.size.y <= pos.y);
		}
		Rect intersect(const Rect& other) const {
			int x1 = std::max(pos.x, other.pos.x);
			int y1 = std::max(pos.y, other.pos.y);
			int x2 = std::min(pos.x + size.x, other.pos.x + other.size.x);
			int y2 = std::min(pos.y + size.y, other.pos.y + other.size.y);
			if (x2 <= x1 || y2 <= y1) return Rect{ {0,0}, {0,0} };
			return Rect{ {x1, y1}, {x2 - x1, y2 - y1} };
		}
	};

	enum class PortLabelSide : uint8_t {
		LEFT = 0,
		RIGHT = 1,
		TOP = 2,
		BOTTOM = 3
	};

	void drawBlockName(const BlockData& blockData, CpuImage& img, int scale, const Rect& labelArea, std::vector<Rect>& reservedAreas) const;
	void drawConnectionLabels(
		const BlockData& blockData,
		CpuImage& img,
		int scale,
		std::vector<Rect>& reservedAreas
	) const;
	int drawConnectionNodeCircle(CpuImage& img, const Vec2Int& position, int bitWidth, int scale) const;

	struct LabelLayoutConfig {
		int targetWidth;
		int minWidth;
		int targetHeight;
		int minHeight;
		int basePadding;
		int labelSpacing;
		int obstaclePadding;
		int slideStep;
	};

	struct PortLabelRequest {
		std::string text;
		Vec2Int portTexturePos;
		PortLabelSide side;
		int paddingFromPort;
	};

	struct LabelPlacement {
		PortLabelRequest request;
		Vec2Int size;
		Vec2Int basePos;
		int axisAnchor = 0;
		int axisPos = 0;
	};

	LabelLayoutConfig buildLabelLayoutConfig(int scale) const;
	std::optional<LabelPlacement> buildPlacementForRequest(
		const PortLabelRequest& request,
		const Vec2Int& imageSize,
		const LabelLayoutConfig& config,
		const std::vector<Rect>& staticObstacles
	) const;
	int computeHorizontalSpan(
		bool growRight,
		int start,
		int axisCoord,
		int imageWidth,
		const LabelLayoutConfig& config,
		const std::vector<Rect>& staticObstacles
	) const;
	int computeVerticalSpan(
		bool growDown,
		int start,
		int axisCoord,
		int imageHeight,
		const LabelLayoutConfig& config,
		const std::vector<Rect>& staticObstacles
	) const;
	void distributeAlongAxis(std::vector<LabelPlacement*>& placements, int axisLimit, int spacing) const;
	bool resolveCollisions(
		Rect& rect,
		PortLabelSide side,
		int axisLimit,
		int slideStep,
		const std::vector<Rect>& occupiedAreas
	) const;
	static int placementAxisSize(const LabelPlacement* placement);
	static bool compareByAxisAnchor(const LabelPlacement* lhs, const LabelPlacement* rhs);
	static bool intersectsAny(const Rect& candidate, const std::vector<Rect>& occupiedAreas);
	static bool overlapsAxis(int coord, int rectStart, int rectSize, int padding);

	static PortLabelSide detectPreferredSide(const BlockData& blockData, const BlockData::ConnectionData& connection);
	static Vec2Int getPortTexturePosition(const BlockData::ConnectionData& connection, int scale);
	static int computePortCircleRadius(int bitWidth, int scale);

	std::shared_ptr<Font> font;
};

#endif /* blockTextureGenerator_h */
