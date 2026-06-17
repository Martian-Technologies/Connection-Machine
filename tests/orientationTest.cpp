#include "orientationTest.h"

#include "backend/position/position.h"

// Utility to generate all orientations (D4 group)
static std::vector<Orientation> allOrientations() {
	std::vector<Orientation> out;
	for (int r = 0; r < 4; r++) {
		for (int f = 0; f < 2; f++) {
			out.emplace_back((Rotation)r, (bool)f);
		}
	}
	return out;
}

TEST_F(OrientationTest, Identity) {
	Vector v(3, 4);
	Orientation id(Rotation::ZERO, false);

	EXPECT_EQ(id * v, v);
	EXPECT_EQ(id * Orientation(), id);
}

TEST_F(OrientationTest, PureRotations) {
	Vector v(1, 0);

	// r90 * (1,0) => (0,1)
	Orientation r90(Rotation::NINETY, false);
	EXPECT_EQ(r90 * v, Vector(0, 1));

	// r180 * (1,0) => (-1,0)
	Orientation r180(Rotation::ONE_EIGHTY, false);
	EXPECT_EQ(r180 * v, Vector(-1, 0));

	// r270 * (1,0) => (0,-1)
	Orientation r270(Rotation::TWO_SEVENTY, false);
	EXPECT_EQ(r270 * v, Vector(0, -1));

	// r90 * r90 = r180
	EXPECT_EQ(r90 * r90, r180);
	// r180 * r180 = identity
	EXPECT_EQ(r180 * r180, Orientation());
}

TEST_F(OrientationTest, FlipInvolution) {
	Orientation f(Rotation::ZERO, true);
	Vector v(2, 3);

	// flip twice = identity
	EXPECT_EQ(f * f, Orientation());
	EXPECT_EQ((f * (f * v)), v);
}

TEST_F(OrientationTest, RotationAndFlipNonCommutativity) {
	Orientation f(Rotation::ZERO, true);
	Orientation r90(Rotation::NINETY, false);

	// Flip then rotate should not equal rotate then flip
	EXPECT_NE(f * r90, r90 * f);
}

TEST_F(OrientationTest, Inverses) {
	Vector v(5, 7);

	for (auto o : allOrientations()) {
		Orientation inv = o.inverse();
		EXPECT_EQ(inv * (o * v), v) << "Failed for orientation " << o.toString();
	}
}

TEST_F(OrientationTest, VectorWithAreaRoundTrip) {
	Size s(10, 20);
	Vector v(2, 3);

	for (auto o : allOrientations()) {
		Vector t = o.transformVectorWithArea(v, s);
		Vector back = o.inverseTransformVectorWithArea(t, o * s);
		EXPECT_EQ(back, v) << "Failed roundtrip for orientation " << o.toString();
	}
}

TEST_F(OrientationTest, GroupClosure) {
	auto group = allOrientations();
	for (auto a : group) {
		for (auto b : group) {
			Orientation c = a * b;
			// Must still be a valid orientation
			auto it = std::find(group.begin(), group.end(), c);
			EXPECT_NE(it, group.end()) << "Group closure failed for " << a.toString() << " * " << b.toString();
		}
	}
}

TEST_F(OrientationTest, RotateFunction) {
	EXPECT_EQ(rotate(Rotation::ZERO, true), Rotation::NINETY);
	EXPECT_EQ(rotate(Rotation::NINETY, true), Rotation::ONE_EIGHTY);
	EXPECT_EQ(rotate(Rotation::ONE_EIGHTY, true), Rotation::TWO_SEVENTY);
	EXPECT_EQ(rotate(Rotation::TWO_SEVENTY, true), Rotation::ZERO);
	EXPECT_EQ(rotate(Rotation::ZERO, false), Rotation::TWO_SEVENTY);
	EXPECT_EQ(rotate(Rotation::TWO_SEVENTY, false), Rotation::ONE_EIGHTY);
	EXPECT_EQ(rotate(Rotation::ONE_EIGHTY, false), Rotation::NINETY);
	EXPECT_EQ(rotate(Rotation::NINETY, false), Rotation::ZERO);
}

TEST_F(OrientationTest, OrientationRotateAndFlipMethods) {
	Orientation o(Rotation::ZERO, false);
	o.rotate(true);
	EXPECT_EQ(o.rotation, Rotation::NINETY);
	o.rotate(false);
	EXPECT_EQ(o.rotation, Rotation::ZERO);
	o.flip();
	EXPECT_TRUE(o.flipped);
	o.flip();
	EXPECT_FALSE(o.flipped);
}

TEST_F(OrientationTest, OrientationNextAndLastRotation) {
	Orientation o(Rotation::ZERO, false);
	o.nextRotation();
	EXPECT_EQ(o.rotation, Rotation::NINETY);
	o.lastRotation();
	EXPECT_EQ(o.rotation, Rotation::ZERO);
}

TEST_F(OrientationTest, OrientationCompoundAssign) {
	Orientation a(Rotation::NINETY, false);
	Orientation b(Rotation::NINETY, false);
	a *= b;
	EXPECT_EQ(a.rotation, Rotation::ONE_EIGHTY);
	EXPECT_FALSE(a.flipped);
}

TEST_F(OrientationTest, TransformFVectorWithArea) {
	FSize size(10.0f, 20.0f);
	FVector v(2.0f, 3.0f);
	for (auto o : allOrientations()) {
		FVector t = o.transformFVectorWithArea(v, size);
		FVector back = o.inverseTransformFVectorWithArea(t, o * size);
		EXPECT_TRUE(approx_equals(back.dx, v.dx) && approx_equals(back.dy, v.dy)) << "FVector roundtrip failed for " << o.toString();
	}
}
