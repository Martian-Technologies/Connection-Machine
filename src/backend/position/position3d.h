#ifndef position3d_h
#define position3d_h

#include "util/fastMath.h"

typedef int coordinate_t;
typedef float f_coordinate_t;

struct Vector3;
struct FVector3;
struct Position3;
struct FPosition3;
struct Size3;
struct FSize3;

struct Vector3 {
	class Iterator;

	inline Vector3() noexcept : dx(0), dy(0), dz(0) { }
	inline Vector3(coordinate_t dx, coordinate_t dy, coordinate_t dz) noexcept : dx(dx), dy(dy), dz(dz) { }
	// allows the easy creation of vector3s that are all the same value
	inline Vector3(coordinate_t d) noexcept : dx(d), dy(d), dz(d) { }
	inline FVector3 free() const noexcept;

	inline std::string toString() const noexcept { return "<" + std::to_string(dx) + ", " + std::to_string(dy) + ", " + std::to_string(dz) + ">"; }

	inline bool operator==(Vector3 other) const noexcept { return dx == other.dx && dy == other.dy && dz == other.dz; }
	inline bool operator!=(Vector3 other) const noexcept { return !operator==(other); }

	inline bool hasZeros() const noexcept { return !(dx && dy && dz); }
	inline bool widthInSize3(Size3 size3) const noexcept;

	inline coordinate_t manhattenLength() const noexcept { return Abs(dx) + Abs(dy) + Abs(dz); }
	inline coordinate_t lengthSquared() const noexcept { return FastPower<2>(dx) + FastPower<2>(dy) + FastPower<2>(dz); }
	inline f_coordinate_t length() const noexcept { return sqrt(lengthSquared()); }

	inline Vector3 operator+(Vector3 other) const noexcept { return Vector3(dx + other.dx, dy + other.dy, dz + other.dz); }
	inline Vector3& operator+=(Vector3 other) noexcept {
		dx += other.dx;
		dy += other.dy;
		dz += other.dz;
		return *this;
	}
	inline Vector3 operator-(Vector3 other) const noexcept { return Vector3(dx - other.dx, dy - other.dy, dz - other.dz); }
	inline Vector3& operator-=(Vector3 other) noexcept {
		dx -= other.dx;
		dy -= other.dy;
		dz -= other.dz;
		return *this;
	}
	inline coordinate_t operator*(Vector3 vector3) const noexcept { return dx * vector3.dx + dy * vector3.dy; }
	inline Vector3 operator*(coordinate_t scalar) const noexcept { return Vector3(dx * scalar, dy * scalar, dz * scalar); }
	inline Vector3& operator*=(coordinate_t scalar) noexcept {
		dx *= scalar;
		dy *= scalar;
		dz *= scalar;
		return *this;
	}
	inline Vector3 operator/(coordinate_t scalar) const noexcept { return Vector3(dx / scalar, dy / scalar, dz / scalar); }
	inline Vector3& operator/=(coordinate_t scalar) noexcept {
		dx /= scalar;
		dy /= scalar;
		dz /= scalar;
		return *this;
	}

	inline Iterator iter() const noexcept;

	coordinate_t dx, dy, dz;
};

class Vector3::Iterator {
public:
	inline Iterator(Vector3 vector3) {
		if (vector3 == Vector3(0)) {
			end = 0;
			sizeX = 1;
			sizeY = 1;
			return;
		}
		xNeg = 1 - 2 * (vector3.dx < 0);
		sizeX = xNeg * vector3.dx + 1;
		yNeg = 1 - 2 * (vector3.dy < 0);
		sizeY = yNeg * vector3.dy + 1;
		zNeg = 1 - 2 * (vector3.dz < 0);
		end = (zNeg * vector3.dz + 1) * sizeX * sizeY - 1;
	}
	inline bool operator==(const Iterator& other) const {
		return (
			xNeg == other.xNeg && yNeg == other.yNeg && end == other.end && cur == other.cur && sizeX == other.sizeX && sizeY == other.sizeY && notDone == other.notDone
		);
	}
	inline bool operator!=(const Iterator& other) const {
		return (
			xNeg != other.xNeg || yNeg != other.yNeg || end != other.end || cur != other.cur || sizeX != other.sizeX || sizeY != other.sizeY || notDone != other.notDone
		);
	}
	inline Iterator& operator++() {
		next();
		return *this;
	}
	inline Iterator& operator--() {
		prev();
		return *this;
	}
	inline Iterator operator++(int) {
		Iterator tmp = *this;
		next();
		return tmp;
	}
	inline Iterator operator--(int) {
		Iterator tmp = *this;
		prev();
		return tmp;
	}
	inline explicit operator bool() const { return notDone; }
	inline Vector3 operator*() const { return Vector3(xNeg * cur % sizeX, yNeg * cur / sizeX % sizeY, zNeg * cur / sizeX / sizeY); }
	// inline Vector3 operator->() const { return *(*this); }

private:
	inline void next() {
		notDone = cur != end;
		cur += notDone;
	}
	inline void prev() {
		cur -= notDone && (cur != 0);
		notDone = true;
	}
	std::uint8_t xNeg;
	std::uint8_t yNeg;
	std::uint8_t zNeg;
	unsigned long long end;
	unsigned long long cur = 0;
	coordinate_t sizeX;
	coordinate_t sizeY;
	bool notDone = true;
};

Vector3::Iterator Vector3::iter() const noexcept { return Iterator(*this); }

template <>
struct std::hash<Vector3> {
	inline std::size_t operator()(Vector3 vec) const noexcept {
		std::size_t x = std::hash<coordinate_t>{}(vec.dx);
		std::size_t y = std::hash<coordinate_t>{}(vec.dy);
		std::size_t z = std::hash<coordinate_t>{}(vec.dz);
		return (std::size_t)x ^ ((std::size_t)y << 21) ^ ((std::size_t)z << 42);
	}
};

template <>
struct fmt::formatter<Vector3> : fmt::formatter<std::string> {
	auto format(Vector3 v, format_context& ctx) const { return formatter<std::string>::format(v.toString(), ctx); }
};

struct FVector3 {
	inline FVector3() noexcept : dx(0.0f), dy(0.0f), dz(0.0f) { }
	inline FVector3(f_coordinate_t dx, f_coordinate_t dy, f_coordinate_t dz) noexcept : dx(dx), dy(dy), dz(dz) { }
	// allows the easy creation of fvector3s that are all the same value
	inline FVector3(f_coordinate_t d) noexcept : dx(d), dy(d), dz(d) { }
	inline Vector3 snap() const noexcept;

	inline std::string toString() const noexcept { return "<" + std::to_string(dx) + ", " + std::to_string(dy) + ", " + std::to_string(dz) + ">"; }

	inline bool operator==(FVector3 other) const noexcept { return approx_equals(dx, other.dx) && approx_equals(dy, other.dy) && approx_equals(dz, other.dz); }
	inline bool operator!=(FVector3 other) const noexcept { return !operator==(other); }

	inline f_coordinate_t manhattenLength() const noexcept { return Abs(dx) + Abs(dy) + Abs(dz); }
	inline f_coordinate_t lengthSquared() const noexcept { return FastPower<2>(dx) + FastPower<2>(dy) + FastPower<2>(dz); }
	inline f_coordinate_t length() const noexcept { return sqrt(lengthSquared()); }

	inline FVector3 operator+(FVector3 other) const noexcept { return FVector3(dx + other.dx, dy + other.dy, dz + other.dz); }
	inline FVector3& operator+=(FVector3 other) noexcept {
		dx += other.dx;
		dy += other.dy;
		dz += other.dz;
		return *this;
	}
	inline FVector3 operator-(FVector3 other) const noexcept { return FVector3(dx - other.dx, dy - other.dy, dz - other.dz); }
	inline FVector3& operator-=(FVector3 other) noexcept {
		dx -= other.dx;
		dy -= other.dy;
		dz -= other.dz;
		return *this;
	}
	inline FVector3 operator*(f_coordinate_t scalar) const noexcept { return FVector3(dx * scalar, dy * scalar, dz * scalar); }
	inline FVector3& operator*=(f_coordinate_t scalar) noexcept {
		dx *= scalar;
		dy *= scalar;
		dz *= scalar;
		return *this;
	}
	inline f_coordinate_t operator*(FVector3 vector3) const noexcept { return dx * vector3.dx + dy * vector3.dy + dz * vector3.dz; }
	inline FVector3 operator/(f_coordinate_t scalar) noexcept { return FVector3(dx / scalar, dy / scalar, dz / scalar); }
	inline FVector3& operator/=(f_coordinate_t scalar) noexcept {
		dx /= scalar;
		dy /= scalar;
		dz /= scalar;
		return *this;
	}

	inline f_coordinate_t lengthAlongProjectToVec(FVector3 vector3) const noexcept { return (*this * vector3) / vector3.length(); }
	inline FVector3 projectToVec(FVector3 vector3) const noexcept { return vector3 * (*this * vector3) / vector3.lengthSquared(); }

	f_coordinate_t dx, dy, dz;
};

template <>
struct fmt::formatter<FVector3> : fmt::formatter<std::string> {
	auto format(FVector3 v, format_context& ctx) const { return formatter<std::string>::format(v.toString(), ctx); }
};

struct Position3 {
	class Iterator;

	inline Position3() noexcept : x(0), y(0), z(0) { }
	inline Position3(coordinate_t x, coordinate_t y, coordinate_t z) noexcept : x(x), y(y), z(z) { }
	inline FPosition3 free() const noexcept;

	inline std::string toString() const noexcept { return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")"; }

	inline bool operator==(Position3 position3) const noexcept { return x == position3.x && y == position3.y && z == position3.z; }
	inline bool operator!=(Position3 position3) const noexcept { return !operator==(position3); }
	inline auto operator<=>(Position3 position3) const noexcept {
		if (auto cmp = x <=> position3.x; cmp != 0) {
			return cmp;
		}
		if (auto cmp = y <=> position3.y; cmp != 0) {
			return cmp;
		}
		return z <=> position3.z;
	}
	inline bool withinArea(Position3 small, Position3 large) const noexcept { return small.x <= x && small.y <= y && large.x >= x && large.y >= y; }

	inline coordinate_t manhattenDistanceTo(Position3 position3) const noexcept { return Abs(x - position3.x) + Abs(y - position3.y) + Abs(z - position3.z); }
	inline coordinate_t manhattenDistanceToOrigin() const noexcept { return Abs(x) + Abs(y) + Abs(y); }
	inline coordinate_t distanceToSquared(Position3 position3) const noexcept { return FastPower<2>(x - position3.x) + FastPower<2>(y - position3.y) + FastPower<2>(z - position3.z); }
	inline coordinate_t distanceToOriginSquared() const noexcept { return FastPower<2>(x) + FastPower<2>(y) + FastPower<2>(z); }
	inline f_coordinate_t distanceTo(Position3 position3) const noexcept { return sqrt(distanceToSquared(position3)); }
	inline f_coordinate_t distanceToOrigin() const noexcept { return sqrt(distanceToOriginSquared()); }

	inline Position3 operator+(Vector3 vector3) const noexcept { return Position3(x + vector3.dx, y + vector3.dy, z + vector3.dz); }
	inline Position3& operator+=(Vector3 vector3) noexcept {
		x += vector3.dx;
		y += vector3.dy;
		z += vector3.dz;
		return *this;
	}
	inline Vector3 operator-(Position3 position3) const noexcept { return Vector3(x - position3.x, y - position3.y, z - position3.z); }
	inline Position3 operator-(Vector3 vector3) const noexcept { return Position3(x - vector3.dx, y - vector3.dy, z - vector3.dz); }
	inline Position3& operator-=(Vector3 vector3) noexcept {
		x -= vector3.dx;
		y -= vector3.dy;
		z -= vector3.dz;
		return *this;
	}

	inline Iterator iterTo(Position3 other) const noexcept;

	coordinate_t x, y, z;
};

class Position3::Iterator {
public:
	inline Iterator(Position3 start, Position3 end) noexcept {
		if (start == end) {
			this->end = 0;
			this->start = start;
			sizeX = 1;
			sizeY = 1;
			return;
		}
		if (start.x > end.x) {
			this->start.x = end.x;
			sizeX = start.x - end.x + 1;
		} else {
			this->start.x = start.x;
			sizeX = end.x - start.x + 1;
		}
		if (start.y > end.y) {
			this->start.y = end.y;
			sizeY = start.y - end.y + 1;
		} else {
			this->start.y = start.y;
			sizeY = end.y - start.y + 1;
		}
		if (start.z > end.z) {
			this->start.z = end.z;
			this->end = (start.z - end.z + 1) * sizeX * sizeY - 1;
		} else {
			this->start.z = start.z;
			this->end = (end.z - start.z + 1) * sizeX * sizeY - 1;
		}
	}
	inline bool operator==(const Iterator& other) const {
		return (end == other.end && cur == other.cur && sizeX == other.sizeX && sizeY == other.sizeY && notDone == other.notDone);
	}
	inline bool operator!=(const Iterator& other) const {
		return (end != other.end || cur != other.cur || sizeX != other.sizeX || sizeY != other.sizeY || notDone != other.notDone);
	}
	inline Iterator& operator++() noexcept {
		next();
		return *this;
	}
	inline Iterator& operator--() noexcept {
		prev();
		return *this;
	}
	inline Iterator operator++(int) noexcept {
		Iterator tmp = *this;
		next();
		return tmp;
	}
	inline Iterator operator--(int) noexcept {
		Iterator tmp = *this;
		prev();
		return tmp;
	}
	inline explicit operator bool() const noexcept { return notDone; }
	inline const Position3 operator*() const noexcept { return start + Vector3(cur % sizeX, cur / sizeX % sizeY, cur / sizeX / sizeY); }
	inline const Position3 operator->() const noexcept { return *(*this); }

private:
	inline void next() {
		notDone = cur != end;
		cur += notDone;
	}
	inline void prev() {
		cur -= notDone && (cur != 0);
		notDone = true;
	}
	Position3 start;
	unsigned int end;
	unsigned int cur = 0;
	coordinate_t sizeX;
	coordinate_t sizeY;
	bool notDone = true;
};

Position3::Iterator Position3::iterTo(Position3 other) const noexcept { return Iterator(*this, other); }

inline bool areaWithinArea(Position3 area1Small, Position3 area1Large, Position3 area2Small, Position3 area2Large) {
	return (
		area2Small.withinArea(area1Small, area1Large) || area2Large.withinArea(area1Small, area1Large) || area1Small.withinArea(area2Small, area2Large) ||
		area2Large.withinArea(area2Small, area2Large)
	);
}

template <>
struct std::hash<Position3> {
	inline std::size_t operator()(Position3 pos) const noexcept {
		std::size_t x = std::hash<coordinate_t>{}(pos.x);
		std::size_t y = std::hash<coordinate_t>{}(pos.y);
		std::size_t z = std::hash<coordinate_t>{}(pos.z);
		std::size_t h = y + 0x9e3779b9 + (x << 6) + (x >> 2);
		return h + 0x9e3779b9 + (x << 6) + (z >> 2);
	}
};

template <>
struct std::hash<std::pair<Position3, Position3>> {
	inline std::size_t operator()(const std::pair<Position3, Position3>& posPair) const noexcept {
		std::size_t a = std::hash<Position3>{}(posPair.first);
		std::size_t b = std::hash<Position3>{}(posPair.second);
		return a + 0x9e3779b9 + (b << 6) + (b >> 2);
	}
};

template <>
struct fmt::formatter<Position3> : fmt::formatter<std::string> {
	auto format(Position3 v, format_context& ctx) const { return formatter<std::string>::format(v.toString(), ctx); }
};

struct FPosition3 {
	static FPosition3 getInvalid() {
		return FPosition3(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
	}
	inline FPosition3() noexcept : x(0.0f), y(0.0f), z(0.0f) { }
	inline FPosition3(f_coordinate_t x, f_coordinate_t y, f_coordinate_t z) noexcept : x(x), y(y), z(z) { }
	inline Position3 snap() const noexcept;

	inline std::string toString() const noexcept { return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")"; }

	inline void setInvalid() { x = y = z = std::numeric_limits<double>::quiet_NaN(); }
	inline bool isValid() const { return !isInvalid(); }
	inline bool isInvalid() const { return (std::isnan(x) || std::isnan(y) || std::isnan(z)); }

	inline bool operator==(FPosition3 position3) const noexcept {
		return approx_equals(x, position3.x) && approx_equals(y, position3.y) && approx_equals(z, position3.z);
	}
	inline bool operator!=(FPosition3 position3) const noexcept { return !operator==(position3); }
	inline bool withinArea(FPosition3 small, FPosition3 large) const noexcept {
		return small.x <= x && small.y <= y && large.x >= x && large.y >= y && large.z >= z && large.z >= z;
	}

	inline f_coordinate_t manhattenDistanceTo(FPosition3 other) const noexcept { return Abs(x - other.x) + Abs(y - other.y) + Abs(z - other.z); }
	inline f_coordinate_t manhattenDistanceToOrigin() const noexcept { return Abs(x) + Abs(y) + Abs(z); }
	inline f_coordinate_t distanceToSquared(FPosition3 other) const noexcept { return FastPower<2>(x - other.x) + FastPower<2>(y - other.y) + FastPower<2>(z - other.z); }
	inline f_coordinate_t distanceToOriginSquared() const noexcept { return FastPower<2>(x) + FastPower<2>(y) + FastPower<2>(z); }
	inline f_coordinate_t distanceTo(FPosition3 other) const noexcept { return sqrt(distanceToSquared(other)); }
	inline f_coordinate_t distanceToOrigin() const noexcept { return sqrt(distanceToOriginSquared()); }

	inline FPosition3 operator+(FVector3 vector3) const noexcept { return FPosition3(x + vector3.dx, y + vector3.dy, z + vector3.dz); }
	inline FPosition3& operator+=(FVector3 vector3) noexcept {
		x += vector3.dx;
		y += vector3.dy;
		z += vector3.dz;
		return *this;
	}
	inline FVector3 operator-(FPosition3 position3) const noexcept { return FVector3(x - position3.x, y - position3.y, z - position3.z); }
	inline FPosition3 operator-(FVector3 vector3) const noexcept { return FPosition3(x - vector3.dx, y - vector3.dy, z - vector3.dz); }
	inline FPosition3& operator-=(FVector3 vector3) noexcept {
		x -= vector3.dx;
		y -= vector3.dy;
		z -= vector3.dz;
		return *this;
	}
	inline FPosition3 operator*(f_coordinate_t scalar) const noexcept { return FPosition3(x * scalar, y * scalar, z * scalar); }
	inline f_coordinate_t lengthAlongProjectToVec(FPosition3 orginOfVec, FVector3 vector3) const noexcept {
		return (*this - orginOfVec).lengthAlongProjectToVec(vector3);
	}
	inline FPosition3 projectToVec(FPosition3 orginOfVec, FVector3 vector3) const noexcept { return orginOfVec + (*this - orginOfVec).projectToVec(vector3); }

	f_coordinate_t x, y, z;
};

inline bool areaWithinArea(FPosition3 area1Small, FPosition3 area1Large, FPosition3 area2Small, FPosition3 area2Large) noexcept {
	return (
		area2Small.withinArea(area1Small, area1Large) || area2Large.withinArea(area1Small, area1Large) || area1Small.withinArea(area2Small, area2Large) ||
		area2Large.withinArea(area2Small, area2Large)
	);
}

template <>
struct fmt::formatter<FPosition3> : fmt::formatter<std::string> {
	auto format(FPosition3 v, format_context& ctx) const { return formatter<std::string>::format(v.toString(), ctx); }
};

struct Size3 {
	class Iterator;

	inline Size3() noexcept : w(0), h(0) { }
	inline Size3(coordinate_t w, coordinate_t h, coordinate_t d) noexcept : w(w), h(h), d(d) { }
	// makes the size3 for hypercube with some edges length
	inline Size3(coordinate_t sideLength) noexcept : w(sideLength), h(sideLength), d(sideLength) { }
	inline Size3(Position3 cornerA, Position3 cornerB) noexcept : w(Abs(cornerA.x - cornerB.x)), h(Abs(cornerA.y - cornerB.y)), d(Abs(cornerA.z - cornerB.z)) { }
	inline FSize3 free() const noexcept;

	inline void extentToFitTartgetCell(Vector3 vector3) noexcept {
		w = std::max(w, vector3.dx + 1);
		h = std::max(h, vector3.dy + 1);
		d = std::max(h, vector3.dy + 1);
	}
	// inline void extentToFitVector3(Vector3 vector3) noexcept {
	// 	w = std::max(w, vector3.dx);
	// 	h = std::max(h, vector3.dy);
	// }

	inline bool containsTartgetCell(Vector3 vector3) const noexcept { return vector3.dx >= 0 && vector3.dy >= 0 && vector3.dx + 1 <= w && vector3.dy + 1 <= h; }
	// inline bool containsVector3(Vector3 vector3) const noexcept { return vector3.dx >= 0 && vector3.dy >= 0 && vector3.dx <= w && vector3.dy <= h; }

	inline std::string toString() const noexcept { return std::to_string(w) + "x" + std::to_string(h) + "x" + std::to_string(d); }

	inline bool operator==(Size3 other) const noexcept { return w == other.w && h == other.h; }
	inline bool operator!=(Size3 other) const noexcept { return !operator==(other); }

	// w != 0 and h != 0
	inline bool isValid() const noexcept { return w > 0 && h > 0 && d > 0; }

	inline coordinate_t volume() const noexcept { return w * h * d; }
	inline coordinate_t surfaceArea() const noexcept { return w * d * 2 + h * w * 2 + d * h * 2; }

	inline Iterator iter() const noexcept;

	inline Vector3 getLargestVector3InArea() { return Vector3(w - 1, h - 1, d - 1); }

	coordinate_t w, h, d;
};

template <>
struct fmt::formatter<Size3> : fmt::formatter<std::string> {
	auto format(Size3 v, format_context& ctx) const { return formatter<std::string>::format(v.toString(), ctx); }
};

class Size3::Iterator {
public:
	inline Iterator(Size3 size3) {
		if (!size3.isValid()) {
			notDone = false;
			end = 0;
			sizeX = 0;
			sizeY = 0;
			return;
		}
		sizeX = size3.w;
		sizeY = size3.h;
		end = size3.volume() - 1;
	}
	inline bool operator==(const Iterator& other) const { return (end == other.end && cur == other.cur && sizeX == other.sizeX && sizeY == other.sizeY && notDone == other.notDone); }
	inline bool operator!=(const Iterator& other) const { return (end != other.end || cur != other.cur || sizeX != other.sizeX || sizeY != other.sizeY || notDone != other.notDone); }
	inline Iterator& operator++() {
		next();
		return *this;
	}
	inline Iterator& operator--() {
		prev();
		return *this;
	}
	inline Iterator operator++(int) {
		Iterator tmp = *this;
		next();
		return tmp;
	}
	inline Iterator operator--(int) {
		Iterator tmp = *this;
		prev();
		return tmp;
	}
	inline explicit operator bool() const { return notDone; }
	inline Vector3 operator*() const {
#ifndef DEBUG // I dont know if this works
		if (!sizeX || !sizeY) {
			logError("Reading Size3::Iterator iterating over invalid size3 not valid. Fix this!");
		}
#endif
		return Vector3(cur % sizeX, cur / sizeX % sizeY, cur / sizeX / sizeY);
	}
	// inline Vector3 operator->() const { return *(*this); }

private:
	inline void next() {
		notDone = cur != end;
		cur += notDone;
	}
	inline void prev() {
		cur -= notDone && (cur != 0);
		notDone = (bool)end;
	}
	unsigned int end;
	unsigned int cur = 0;
	coordinate_t sizeX;
	coordinate_t sizeY;
	bool notDone = true;
};

Size3::Iterator Size3::iter() const noexcept { return Iterator(*this); }

bool Vector3::widthInSize3(Size3 size3) const noexcept { return dx < size3.w && dx >= 0 && dy < size3.h && dy >= 0 && dz < size3.d && dz >= 0; }

struct FSize3 {
	inline FSize3() noexcept : w(0), h(0), d(0) { }
	inline FSize3(f_coordinate_t sideLength) noexcept : w(sideLength), h(sideLength), d(sideLength) { }
	inline FSize3(f_coordinate_t w, f_coordinate_t h, coordinate_t d) noexcept : w(w), h(h), d(d) { }
	inline Size3 snap() const noexcept;

	inline void extentToFitTartgetCell(FVector3 vector3) noexcept { extentToFitTartgetCell(vector3.snap()); }
	inline void extentToFitTartgetCell(Vector3 vector3) noexcept {
		Size3 size3 = snap();
		size3.extentToFitTartgetCell(vector3);
		*this = size3.free();
	}
	inline void extentToFitVector3(FVector3 vector3) noexcept {
		w = std::max(w, vector3.dx);
		h = std::max(h, vector3.dy);
		d = std::max(d, vector3.dz);
	}
	inline bool containsTartgetCell(FVector3 vector3) const noexcept {
		return (
			(!isNegative(vector3.dx) && !isNegative(vector3.dy) && !isNegative(vector3.dz)) &&
			(approx_lessOrEquals(std::floor(vector3.dx) + 1, w) && approx_lessOrEquals(std::floor(vector3.dy) + 1, h) && approx_lessOrEquals(std::floor(vector3.dz) + 1, d))
		);
	}
	inline bool containsVector3(FVector3 vector3) const noexcept {
		return !isNegative(vector3.dx) && !isNegative(vector3.dy) && !isNegative(vector3.dz) && approx_lessOrEquals(vector3.dx, w) && approx_lessOrEquals(vector3.dy, h) && approx_lessOrEquals(vector3.dz, d);
	}
	inline std::string toString() const noexcept { return std::to_string(w) + "x" + std::to_string(h) + "x" + std::to_string(d); }

	inline bool operator==(FSize3 other) const noexcept { return approx_equals(w, other.w) && approx_equals(h, other.h); }
	inline bool operator!=(FSize3 other) const noexcept { return !operator==(other); }

	inline bool isValid() const noexcept { return w > 0 && h > 0 && d > 0; }
	inline bool isInvalid() const noexcept { return !isValid(); }

	inline coordinate_t volume() const noexcept { return w * h * d; }
	inline coordinate_t surfaceArea() const noexcept { return w * d * 2 + h * w * 2 + d * h * 2; }

	// inline Iterator iter() const noexcept;

	f_coordinate_t w, h, d;
};

template <>
struct fmt::formatter<FSize3> : fmt::formatter<std::string> {
	auto format(FSize3 v, format_context& ctx) const { return formatter<std::string>::format(v.toString(), ctx); }
};

// conversion
inline FVector3 Vector3::free() const noexcept { return FVector3(dx, dy, dz); }
inline Vector3 FVector3::snap() const noexcept { return Vector3(std::floor(dx), std::floor(dy), std::floor(dz)); }
inline FPosition3 Position3::free() const noexcept { return FPosition3(x, y, z); }
inline Position3 FPosition3::snap() const noexcept { return Position3(std::floor(x), std::floor(y), std::floor(z)); }
inline FSize3 Size3::free() const noexcept { return FSize3(w, h, d); }
inline Size3 FSize3::snap() const noexcept { return Size3(std::floor(w), std::floor(h), std::floor(d)); }
/*
// ---- we also define block rotation here so ----
enum Rotation : std::uint8_t {
	ZERO = 0,
	NINETY = 1,
	ONE_EIGHTY = 2,
	TWO_SEVENTY = 3,
};

template <>
struct fmt::formatter<Rotation> : fmt::formatter<std::string> {
	auto format(Rotation v, format_context& ctx) const {
		switch (v) {
		case Rotation::TWO_SEVENTY: return "TWO_SEVENTY";
		case Rotation::ONE_EIGHTY: return "ONE_EIGHTY";
		case Rotation::NINETY: return "NINETY";
		default: return "ZERO";
		}
	}
};

inline Vector3 rotateVector3(Vector3 vector3, Rotation rotationAmount) noexcept {
	switch (rotationAmount) {
	case Rotation::TWO_SEVENTY: return Vector3(vector3.dy, -vector3.dx);
	case Rotation::ONE_EIGHTY: return Vector3(-vector3.dx, -vector3.dy);
	case Rotation::NINETY: return Vector3(-vector3.dy, vector3.dx);
	default: return vector3;
	}
}
inline FVector3 rotateVector3(FVector3 vector3, Rotation rotationAmount) noexcept {
	switch (rotationAmount) {
	case Rotation::TWO_SEVENTY: return FVector3(vector3.dy, -vector3.dx);
	case Rotation::ONE_EIGHTY: return FVector3(-vector3.dx, -vector3.dy);
	case Rotation::NINETY: return FVector3(-vector3.dy, vector3.dx);
	default: return vector3;
	}
}
inline Size3 rotateSize3(Rotation rotationAmount, Size3 size3) noexcept {
	if (rotationAmount & 1) return Size3(size3.h, size3.w);
	return size3;
}
inline FSize3 rotateSize3(Rotation rotationAmount, FSize3 size3) noexcept {
	if (rotationAmount & 1) return FSize3(size3.h, size3.w);
	return size3;
}
inline constexpr Rotation rotate(Rotation rotation, bool clockWise) {
	if (clockWise) {
		if (rotation == Rotation::TWO_SEVENTY) return Rotation::ZERO;
		return (Rotation)((int)rotation + 1);
	}
	if (rotation == Rotation::ZERO) return Rotation::TWO_SEVENTY;
	return (Rotation)((int)rotation - 1);
}
inline constexpr Rotation addRotations(Rotation rotationA, Rotation rotationB) {
	std::uint8_t output = rotationA + rotationB;
	if ((std::uint8_t)output > (std::uint8_t)Rotation::TWO_SEVENTY) output -= 4;
	return (Rotation)output;
}
inline constexpr Rotation rotationNeg(Rotation rotation) { return (Rotation)((4 - (std::uint8_t)rotation) & 0b11); }
inline constexpr Rotation subRotations(Rotation rotationA, Rotation rotationB) { return addRotations(rotationA, rotationNeg(rotationB)); }
inline constexpr int getDegrees(Rotation rotation) { return rotation * 90; }
inline Vector3 rotateVector3WithArea(Vector3 vector3, Size3 size3, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return Vector3(size3.h - vector3.dy - 1, vector3.dx);
	case Rotation::ONE_EIGHTY: return Vector3(size3.w - vector3.dx - 1, size3.h - vector3.dy - 1);
	case Rotation::TWO_SEVENTY: return Vector3(vector3.dy, size3.w - vector3.dx - 1);
	default: return vector3;
	}
}
inline Vector3 reverseRotateVector3WithArea(Vector3 vector3, Size3 size3, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return Vector3(vector3.dy, size3.w - vector3.dx - 1);
	case Rotation::ONE_EIGHTY: return Vector3(size3.w - vector3.dx - 1, size3.h - vector3.dy - 1);
	case Rotation::TWO_SEVENTY: return Vector3(size3.h - vector3.dy - 1, vector3.dx);
	default: return vector3;
	}
}
inline FVector3 rotateFVector3WithArea(FVector3 vector3, FSize3 size3, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return FVector3(size3.h - vector3.dy, vector3.dx);
	case Rotation::ONE_EIGHTY: return FVector3(size3.w - vector3.dx, size3.h - vector3.dy);
	case Rotation::TWO_SEVENTY: return FVector3(vector3.dy, size3.w - vector3.dx);
	default: return vector3;
	}
}
inline FVector3 reverseRotateFVector3WithArea(FVector3 vector3, FSize3 size3, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return FVector3(vector3.dy, size3.w - vector3.dx);
	case Rotation::ONE_EIGHTY: return FVector3(size3.w - vector3.dx, size3.h - vector3.dy);
	case Rotation::TWO_SEVENTY: return FVector3(size3.h - vector3.dy, vector3.dx);
	default: return vector3;
	}
}

// change to 1 byte later
// flip then rotate
struct Orientation {
	Rotation rotation = Rotation::ZERO;
	bool flipped = false;

	Orientation(int value) noexcept : rotation((Rotation)(value & 0b11)), flipped((value & 0b100) != 0) { }
	Orientation(Rotation rotation = Rotation::ZERO, bool flipped = false) noexcept : rotation(rotation), flipped(flipped) { }

	inline std::string toString() const { return "(r:" + std::to_string(rotation) + ", f:" + std::to_string(flipped) + ")"; }

	inline void nextOrientation() {
		if (rotation == Rotation::ONE_EIGHTY && !flipped) flipped = true;
		else if (rotation == Rotation::TWO_SEVENTY && flipped) flipped = false;
		else rotate(!flipped);
	}

	inline void lastOrientation() {
		if (rotation == Rotation::ONE_EIGHTY && flipped) flipped = false;
		else if (rotation == Rotation::TWO_SEVENTY && !flipped) flipped = true;
		else rotate(flipped);
	}

	inline void nextRotation() { rotate(true); }

	inline void lastRotation() { rotate(false); }

	inline Vector3 operator*(Vector3 vector3) const noexcept {
		Vector3 vec(vector3.dx, flipped ? -vector3.dy : vector3.dy);
		return rotateVector3(vec, rotation);
	}
	inline FVector3 operator*(FVector3 vector3) const noexcept {
		FVector3 vec(vector3.dx, flipped ? -vector3.dy : vector3.dy);
		return rotateVector3(vec, rotation);
	}
	inline Size3 operator*(Size3 size3) const noexcept { return rotateSize3(rotation, size3); }
	inline FSize3 operator*(FSize3 size3) const noexcept { return rotateSize3(rotation, size3); }
	inline bool operator==(Orientation other) const noexcept { return this->rotation == other.rotation && this->flipped == other.flipped; }
	inline bool operator!=(Orientation other) const noexcept { return !(*this == other); }
	inline void rotate(bool clockWise) { rotation = ::rotate(rotation, clockWise); }
	inline void flip() { flipped = !flipped; }
	inline Orientation operator*(Orientation other) const noexcept {
		return Orientation(addRotations(rotation, flipped ? rotationNeg(other.rotation) : other.rotation), other.flipped ^ flipped);
	}
	inline const Orientation& operator*=(Orientation other) noexcept {
		rotation = addRotations(rotation, flipped ? rotationNeg(other.rotation) : other.rotation);
		flipped ^= other.flipped;
		return *this;
	}
	inline Orientation inverse() const noexcept { return Orientation(flipped ? rotation : rotationNeg(rotation), flipped); }
	inline Orientation relativeTo(Orientation orientation) const noexcept { return (*this) * (orientation.inverse()); }
	inline Vector3 transformVector3WithArea(Vector3 vector3, Size3 size3) const noexcept {
		Vector3 vec(vector3.dx, flipped ? (size3.h - vector3.dy - 1) : vector3.dy);
		return rotateVector3WithArea(vec, size3, rotation);
	}
	inline Vector3 inverseTransformVector3WithArea(Vector3 vector3, Size3 size3) const noexcept {
		return inverse().transformVector3WithArea(vector3, size3);
		// Vector3 vec = reverseRotateVector3WithArea(vector3, size3, rotation);
		// return Vector3(vec.dx, flipped ? (rotateSize3(rotation, size3).h - vec.dy - 1) : vec.dy);
	}
	inline FVector3 transformFVector3WithArea(FVector3 vector3, FSize3 size3) const noexcept {
		FVector3 vec(vector3.dx, flipped ? (size3.h - vector3.dy) : vector3.dy);
		return rotateFVector3WithArea(vec, size3, rotation);
	}
	inline FVector3 inverseTransformFVector3WithArea(FVector3 vector3, FSize3 size3) const noexcept {
		return inverse().transformFVector3WithArea(vector3, size3);
		// FVector3 vec = reverseRotateVector3WithArea(vector3, size3, rotation);
		// return FVector3(vec.dx, flipped ? (rotateSize3(rotation, size3).h - vec.dy - 1) : vec.dy);
	}
};

template <>
struct fmt::formatter<Orientation> : fmt::formatter<std::string> {
	auto format(Orientation o, format_context& ctx) const { return formatter<std::string>::format(o.toString(), ctx); }
};
*/
#endif /* position3d_h */
