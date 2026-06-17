#include "positionTest.h"

#include "randomGens.h"

TEST_F(PositionTest, VectorBasicOperations) {
	Vector a(5, 5);

	ASSERT_TRUE(a == Vector(5, 5));
	ASSERT_FALSE(a != Vector(5, 5));
}

TEST_F(PositionTest, VectorFunctions) {
	Vector a(5, 5);
	ASSERT_TRUE(a.toString() == "<5, 5>");
	ASSERT_TRUE(a.manhattenLength() == 10);
	ASSERT_TRUE(approx_equals(a.lengthSquared(), 50.f));
	ASSERT_TRUE(approx_equals(a.length(), sqrt(50)));
}

TEST_F(PositionTest, VectorComparisons) {
	Vector a(6, 9);
	Vector b(3, 3);

	Vector a1 = a;
	Vector a2 = a;
	Vector a3 = a;
	Vector a4 = a;
	a1 += b;
	a2 -= b;
	a3 *= 3;
	a4 /= 3;

	ASSERT_TRUE(a + b == Vector(9, 12));
	ASSERT_TRUE(a1 == Vector(9, 12));
	ASSERT_TRUE(a - b == Vector(3, 6));
	ASSERT_TRUE(a2 == Vector(3, 6));
	ASSERT_TRUE(a * 3 == Vector(18, 27));
	ASSERT_TRUE(a3 == Vector(18, 27));
	ASSERT_TRUE(a / 3 == Vector(2, 3));
	ASSERT_TRUE(a / 2 == Vector(3, 4));
	ASSERT_TRUE(a4 == Vector(2, 3));
}

TEST_F(PositionTest, FVectorBasicOperations) {
	FVector a(5.1f, 5.2f);

	ASSERT_TRUE(a == FVector(5.1f, 5.2f));
	ASSERT_FALSE(a != FVector(5.1f, 5.2f));
}

TEST_F(PositionTest, FVectorFunctions) {
	FVector a(5.1f, 5.2f);

	// Idk if we should test this. Maybe a better way is the read the string in as a new vector and then comapare.
	ASSERT_TRUE(a.toString() == "<5.100000, 5.200000>");

	ASSERT_TRUE(approx_equals(a.manhattenLength(), 10.3f));
	ASSERT_TRUE(approx_equals(a.lengthSquared(), 53.05f));
	ASSERT_TRUE(approx_equals(a.length(), sqrt(53.05f)));
}

TEST_F(PositionTest, FVectorComparisons) {
	FVector a(6.72f, 9.45f);
	FVector b(3.1f, 3.2f);

	FVector a1 = a;
	FVector a2 = a;
	FVector a3 = a;
	FVector a4 = a;
	a1 += b;
	a2 -= b;
	a3 *= 3.0f;
	a4 /= 2.1f;
	ASSERT_TRUE(a + b == FVector(9.82f, 12.65f));
	ASSERT_TRUE(a1 == FVector(9.82f, 12.65f));
	ASSERT_TRUE(a - b == FVector(3.62f, 6.25f));
	ASSERT_TRUE(a2 == FVector(3.62f, 6.25f));
	ASSERT_TRUE(a * 3.0f == FVector(20.16f, 28.35f));
	ASSERT_TRUE(a * 3.1f == FVector(20.832f, 29.295f));
	ASSERT_TRUE(a3 == FVector(20.16f, 28.35f));
	ASSERT_TRUE(a / 2.1f == FVector(3.2f, 4.5f));
	ASSERT_TRUE(a4 == FVector(3.2f, 4.5f));
}

TEST_F(PositionTest, PositionBasicOperations) {
	Position a(2, 4);

	ASSERT_TRUE(a == Position(2, 4));
	ASSERT_FALSE(a != Position(2, 4));
	ASSERT_TRUE(a.withinArea(Position(1, 1), Position(10, 10)));
}

TEST_F(PositionTest, PositionFunctions) {
	Position a(5, 5);
	Position b(1, 2);

	ASSERT_TRUE(a.toString() == "(5, 5)");
	ASSERT_TRUE(a.manhattenDistanceTo(b) == 7);
	ASSERT_TRUE(a.manhattenDistanceToOrigin() == 10);
	ASSERT_TRUE(a.distanceToSquared(b) == 25);
	ASSERT_TRUE(a.distanceToOriginSquared() == 50);
	ASSERT_TRUE(approx_equals(a.distanceTo(b), sqrt(25)));
	ASSERT_TRUE(approx_equals(a.distanceToOrigin(), sqrt(50)));
}

TEST_F(PositionTest, PositionComparisons) {
	Position a(6, 9);
	Vector b(3, 3);

	Position a1 = a;
	Position a2 = a;
	a1 += b;
	a2 -= b;

	ASSERT_TRUE(a + b == Position(9, 12));
	ASSERT_TRUE(a1 == Position(9, 12));
	ASSERT_TRUE(a - Position(2, 2) == Vector(4, 7));
	ASSERT_TRUE(a - b == Position(3, 6));
	ASSERT_TRUE(a2 == Position(3, 6));
}

TEST_F(PositionTest, FPositionBasicOperations) {
	FPosition a(2.1, 4.2);

	ASSERT_TRUE(a == FPosition(2.1, 4.2));
	ASSERT_FALSE(a != FPosition(2.1, 4.2));
	ASSERT_TRUE(a.withinArea(FPosition(1.1, 1.2), FPosition(9.1, 9.2)));
}

TEST_F(PositionTest, FPositionFunctions) {
	FPosition a(5.5, 5.4);
	FPosition b(1.1, 2.2);

	ASSERT_TRUE(a.toString() == "(5.500000, 5.400000)");
	ASSERT_TRUE(approx_equals(a.manhattenDistanceTo(b), 7.6f));
	ASSERT_TRUE(approx_equals(a.manhattenDistanceToOrigin(), 10.9f));
	ASSERT_TRUE(approx_equals(a.distanceToSquared(b), 29.6f));
	ASSERT_TRUE(approx_equals(a.distanceToOriginSquared(), 59.41f));
	ASSERT_TRUE(approx_equals(a.distanceTo(b), sqrt(29.6)));
	ASSERT_TRUE(approx_equals(a.distanceToOrigin(), sqrt(59.41)));
}

TEST_F(PositionTest, FPositionComparisons) {
	FPosition a(6.4, 9.2);
	FVector b(3.2, 3.1);

	FPosition a1 = a;
	FPosition a2 = a;
	a1 += b;
	a2 -= b;

	ASSERT_TRUE(a + b == FPosition(9.6, 12.3));
	ASSERT_TRUE(a1 == FPosition(9.6, 12.3));
	ASSERT_TRUE(a - FPosition(2.1, 2.1) == FVector(4.3, 7.1));
	ASSERT_TRUE(a - b == FPosition(3.2, 6.1));
	ASSERT_TRUE(a2 == FPosition(3.2, 6.1));
	ASSERT_TRUE(a * 3.1 == FPosition(19.84, 28.52));
}

TEST_F(PositionTest, ZeroConstructor) {
	Position zeroPos;
	ASSERT_EQ(zeroPos.x, 0);
	ASSERT_EQ(zeroPos.y, 0);

	FPosition zeroFPos;
	ASSERT_EQ(zeroFPos.x, 0);
	ASSERT_EQ(zeroFPos.y, 0);
	ASSERT_TRUE(zeroFPos.isValid());

	Vector zeroVec;
	ASSERT_EQ(zeroVec.dx, 0);
	ASSERT_EQ(zeroVec.dy, 0);

	FVector zeroFVec;
	ASSERT_EQ(zeroFVec.dx, 0);
	ASSERT_EQ(zeroFVec.dy, 0);

	Size zeroSize;
	ASSERT_EQ(zeroSize.w, 0);
	ASSERT_EQ(zeroSize.h, 0);

	FSize zeroFSize;
	ASSERT_EQ(zeroFSize.w, 0);
	ASSERT_EQ(zeroFSize.h, 0);
	ASSERT_TRUE(zeroFSize.isValid());

	Orientation zeroOrientation;
	ASSERT_EQ(zeroOrientation.rotation, Rotation::ZERO);
	ASSERT_EQ(zeroOrientation.flipped, false);
}

TEST_F(PositionTest, FPositionInvalid) {
	FPosition zeroVec = FPosition::getInvalid();
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec = FPosition(1, 2);
	ASSERT_EQ(zeroVec.x, 1);
	ASSERT_EQ(zeroVec.y, 2);
	ASSERT_FALSE(zeroVec.isInvalid());
	ASSERT_TRUE(zeroVec.isValid());
	zeroVec.setInvalid();
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec.x = 0;
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec.y = 0;
	ASSERT_EQ(zeroVec.x, 0);
	ASSERT_EQ(zeroVec.y, 0);
	ASSERT_FALSE(zeroVec.isInvalid());
	ASSERT_TRUE(zeroVec.isValid());
	zeroVec.setInvalid();
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec.y = 0;
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
}

TEST_F(PositionTest, toString) {
	Position pos;
	ASSERT_EQ(pos.toString(), "(0, 0)");
	pos.x = 10;
	ASSERT_EQ(pos.toString(), "(10, 0)");
	pos.y = -51;
	ASSERT_EQ(pos.toString(), "(10, -51)");
	pos.x = -1161231;
	ASSERT_EQ(pos.toString(), "(-1161231, -51)");

	Vector vec;
	ASSERT_EQ(vec.toString(), "<0, 0>");
	vec.dx = 10;
	ASSERT_EQ(vec.toString(), "<10, 0>");
	vec.dy = -51;
	ASSERT_EQ(vec.toString(), "<10, -51>");
	vec.dx = -1161231;
	ASSERT_EQ(vec.toString(), "<-1161231, -51>");

	Size size;
	ASSERT_EQ(size.toString(), "0x0");
	size.w = 10;
	ASSERT_EQ(size.toString(), "10x0");
	size.h = 14371;
	ASSERT_EQ(size.toString(), "10x14371");
	size.h = -51;
	ASSERT_EQ(size.toString(), "10x-51");
	size.w = -1161231;
	ASSERT_EQ(size.toString(), "-1161231x-51");

	Orientation orientation;

	ASSERT_EQ(orientation.toString(), "(r:0, f:0)");
}

TEST_F(PositionTest, vectorIter) {
	for (unsigned int i = 0; i < 50; i++) {
		Vector v = randVec();
		v.dx %= 1000; // takes too long otherwise
		v.dy %= 1000;

		unsigned long long area = 0;
		for (Vector::Iterator iter = v.iter(); iter; iter++) {
			Vector::Iterator curIter = iter;
			Vector::Iterator nextIter = (++iter)--;
			ASSERT_EQ(iter++, curIter);
			ASSERT_EQ(iter--, nextIter);
			ASSERT_EQ(++iter, nextIter);
			ASSERT_EQ(--iter, curIter);
			area++;
		}
		ASSERT_EQ(area, (v.dx + 1) * (v.dy + 1));
	}
}

TEST_F(PositionTest, sizeIter) {
	for (unsigned int i = 0; i < 50; i++) {
		Size s = randSize();
		s.w = abs(s.w) % 1000 + 1; // takes too long otherwise, move than 1 w&h
		s.h = abs(s.h) % 1000 + 1;
		ASSERT_TRUE(s.isValid());

		unsigned long long area = 0;
		for (Size::Iterator iter = s.iter(); iter; iter++) {
			Size::Iterator curIter = iter;
			Size::Iterator nextIter = (++iter)--;
			ASSERT_EQ(iter++, curIter);
			ASSERT_EQ(iter--, nextIter);
			ASSERT_EQ(++iter, nextIter);
			ASSERT_EQ(--iter, curIter);
			area++;
		}
		ASSERT_EQ(area, s.w * s.h);
		ASSERT_EQ(area, s.area());
	}
}

TEST_F(PositionTest, positionIter) {
	for (unsigned int i = 0; i < 50; i++) {
		Position p1 = randPos();
		Position p2 = randPos();
		p1.x %= 1000; // takes too long otherwise
		p1.y %= 1000; // takes too long otherwise
		p2.x %= 1000; // takes too long otherwise
		p2.y %= 1000; // takes too long otherwise

		unsigned long long area = 0;
		// ASSERT_EQ(*(p1.iterTo(p2)), p1); // not guaranteed. Should it be?
		for (Position::Iterator iter = p1.iterTo(p2); iter; iter++) {
			Position::Iterator curIter = iter;
			Position::Iterator nextIter = (++iter)--;
			ASSERT_EQ(iter++, curIter);
			ASSERT_EQ(iter--, nextIter);
			ASSERT_EQ(++iter, nextIter);
			ASSERT_EQ(--iter, curIter);
			area++;
		}
		ASSERT_EQ(area, (abs(p1.x - p2.x) + 1) * (abs(p1.y - p2.y) + 1));
	}
}

TEST_F(PositionTest, VectorHash) {
	std::unordered_map<Vector, int> map;
	map[Vector(1, 2)] = 10;
	map[Vector(3, 4)] = 20;
	EXPECT_EQ(map[Vector(1, 2)], 10);
	EXPECT_EQ(map[Vector(3, 4)], 20);
	EXPECT_EQ(map.count(Vector(5, 6)), 0u);
}

TEST_F(PositionTest, PositionPairHash) {
	std::unordered_map<std::pair<Position, Position>, int> map;
	auto key = std::make_pair(Position(1, 2), Position(3, 4));
	map[key] = 42;
	EXPECT_EQ(map[key], 42);
	EXPECT_EQ(map.count(std::make_pair(Position(0, 0), Position(1, 1))), 0u);
}

TEST_F(PositionTest, FVectorSingleArgConstructor) {
	FVector v(3.5f);
	EXPECT_FLOAT_EQ(v.dx, 3.5f);
	EXPECT_FLOAT_EQ(v.dy, 3.5f);
}

TEST_F(PositionTest, FVectorDotProduct) {
	FVector a(2.0f, 3.0f);
	FVector b(4.0f, 5.0f);
	EXPECT_FLOAT_EQ(a * b, 23.0f);
}

TEST_F(PositionTest, FVectorLengthAlongProject) {
	FVector a(3.0f, 0.0f);
	FVector axis(1.0f, 0.0f);
	EXPECT_FLOAT_EQ(a.lengthAlongProjectToVec(axis), 3.0f);
}

TEST_F(PositionTest, VectorWidthInSize) {
	Size s(5, 5);
	EXPECT_TRUE(Vector(2, 2).widthInSize(s));
	EXPECT_FALSE(Vector(5, 2).widthInSize(s));
	EXPECT_FALSE(Vector(-1, 2).widthInSize(s));
	EXPECT_FALSE(Vector(2, 5).widthInSize(s));
	EXPECT_FALSE(Vector(2, -1).widthInSize(s));
}

TEST_F(PositionTest, FreeConversions) {
	Vector v(3, 7);
	FVector fv = v.free();
	EXPECT_FLOAT_EQ(fv.dx, 3.0f);
	EXPECT_FLOAT_EQ(fv.dy, 7.0f);

	Size s(4, 6);
	FSize fs = s.free();
	EXPECT_FLOAT_EQ(fs.w, 4.0f);
	EXPECT_FLOAT_EQ(fs.h, 6.0f);

	Position p(5, 9);
	FPosition fp = p.free();
	EXPECT_FLOAT_EQ(fp.x, 5.0f);
	EXPECT_FLOAT_EQ(fp.y, 9.0f);
}

TEST_F(PositionTest, FSizeTwoArgConstructor) {
	FSize fs(3.0f, 7.0f);
	EXPECT_FLOAT_EQ(fs.w, 3.0f);
	EXPECT_FLOAT_EQ(fs.h, 7.0f);
}

TEST_F(PositionTest, FSizeIsValid) {
	EXPECT_TRUE(FSize(0.0f, 5.0f).isValid());
	EXPECT_FALSE(FSize(3.0f, 5.0f).isValid());
	EXPECT_TRUE(FSize(0.0f, 0.0f).isValid());
}

TEST_F(PositionTest, FPositionLengthAlongProject) {
	FPosition p(3.0f, 0.0f);
	FVector axis(1.0f, 0.0f);
	EXPECT_FLOAT_EQ(p.lengthAlongProjectToVec(FPosition(0.0f, 0.0f), axis), 3.0f);
}

TEST_F(PositionTest, VectorIteratorZeroVector) {
	Vector::Iterator it = Vector(0, 0).iter();
	unsigned long long count = 0;
	for (; it; it++) count++;
	EXPECT_EQ(count, 1);
}
