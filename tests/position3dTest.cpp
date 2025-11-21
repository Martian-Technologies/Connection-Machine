#include "position3dTest.h"

#include "randomGens.h"

TEST_F(Position3dTest, Vector3BasicOperations) {
	Vector3 a(5, 5, 5);

	ASSERT_TRUE(a == Vector3(5, 5, 5));
	ASSERT_FALSE(a != Vector3(5, 5, 5));
}

TEST_F(Position3dTest, Vector3Functions) {
	Vector3 a(218, 21, -2);
	ASSERT_EQ(a.toString(), "<218, 21, -2>");
	ASSERT_EQ(a.manhattenLength(), 218+21+2);
	ASSERT_EQ(a.lengthSquared(), 47969);
	ASSERT_TRUE(approx_equals(a.length(), sqrt(47969)));

}

TEST_F(Position3dTest, Vector3Comparisons) {
	Vector3 a(6, 9, 0);
	Vector3 b(3, 3, 3);

	Vector3 a1 = a;
	Vector3 a2 = a;
	Vector3 a3 = a;
	Vector3 a4 = a;
	a1 += b;
	a2 -= b;
	a3 *= 3;
	a4 /= 3;

	ASSERT_TRUE(a + b == Vector3(9, 12, 3));
	ASSERT_TRUE(a1 == Vector3(9, 12, 3));
	ASSERT_TRUE(a - b == Vector3(3, 6, -3));
	ASSERT_TRUE(a2 == Vector3(3, 6, -3));
	ASSERT_TRUE(a * 3 == Vector3(18, 27, 0));
	ASSERT_TRUE(a3 == Vector3(18, 27, 0));
	ASSERT_TRUE(a / 3 == Vector3(2, 3, 0));
	ASSERT_TRUE(a / 2 == Vector3(3, 4, 0));
	ASSERT_TRUE(a4 == Vector3(2, 3, 0));
}

TEST_F(Position3dTest, FVector3BasicOperations) {
	FVector3 a(5.1f, 5.2f, -5.9f);

	ASSERT_TRUE(a == FVector3(5.1f, 5.2f, -5.9f));
	ASSERT_FALSE(a != FVector3(5.1f, 5.2f, -5.9f));
	ASSERT_FALSE(a == FVector3(5.1f, 5.1f, -5.9f));
	ASSERT_TRUE(a != FVector3(5.1f, 5.1f, -5.9f));
	ASSERT_FALSE(a == FVector3(5.1f, 5.2f, 5.9f));
	ASSERT_TRUE(a != FVector3(5.1f, 5.2f, 5.9f));
}

TEST_F(Position3dTest, FVector3Functions) {
	FVector3 a(5.1f, 5.2f, -72);

	// Idk if we should test this. Maybe a better way is the read the string in as a new vector3 and then comapare.
	ASSERT_EQ(a.toString(), "<5.100000, 5.200000, -72.000000>");

	ASSERT_TRUE(approx_equals(a.manhattenLength(), 82.3f));
	ASSERT_TRUE(approx_equals(a.lengthSquared(), 5237.05f));
	ASSERT_TRUE(approx_equals(a.length(), sqrt(5237.05f)));
}

TEST_F(Position3dTest, FVector3Comparisons) {
	FVector3 a(6.72f, 9.45f, 0);
	FVector3 b(3.1f, 3.2f, -3.3f);

	FVector3 a1 = a;
	FVector3 a2 = a;
	FVector3 a3 = a;
	FVector3 a4 = a;
	a1 += b;
	a2 -= b;
	a3 *= 3.0f;
	a4 /= 2.1f;
	ASSERT_TRUE(a + b == FVector3(9.82f, 12.65f, -3.3f));
	ASSERT_TRUE(a1 == FVector3(9.82f, 12.65f, -3.3f));
	ASSERT_TRUE(a - b == FVector3(3.62f, 6.25f, 3.3f));
	ASSERT_TRUE(a2 == FVector3(3.62f, 6.25f, 3.3f));
	ASSERT_TRUE(a * 3.0f == FVector3(20.16f, 28.35f, 0));
	ASSERT_TRUE(a * 3.1f == FVector3(20.832f, 29.295f, 0));
	ASSERT_TRUE(a3 == FVector3(20.16f, 28.35f, 0));
	ASSERT_TRUE(a / 2.1f == FVector3(3.2f, 4.5f, 0));
	ASSERT_TRUE(a4 == FVector3(3.2f, 4.5f, 0));
}

TEST_F(Position3dTest, Position3BasicOperations) {
	Position3 a(2, 4, 8);

	ASSERT_TRUE(a == Position3(2, 4, 8));
	ASSERT_FALSE(a != Position3(2, 4, 8));
	ASSERT_TRUE(a.withinArea(Position3(1, 1, 1), Position3(10, 10, 10)));
	ASSERT_TRUE(a.withinArea(Position3(1, 1, 1), Position3(10, 10, 7)));
}

TEST_F(Position3dTest, Position3Functions) {
	Position3 a(5, 5, 5);
	Position3 b(1, 2, 3);

	ASSERT_EQ(a.toString(), "(5, 5, 5)");
	ASSERT_EQ(a.manhattenDistanceTo(b), 9);
	ASSERT_EQ(a.manhattenDistanceToOrigin(), 15);
	ASSERT_EQ(a.distanceToSquared(b), 29);
	ASSERT_EQ(a.distanceToOriginSquared(), 75);
	ASSERT_TRUE(approx_equals(a.distanceTo(b), sqrt(29)));
	ASSERT_TRUE(approx_equals(a.distanceToOrigin(), sqrt(75)));
}

TEST_F(Position3dTest, Position3Comparisons) {
	Position3 a(6, 9, 12);
	Vector3 b(3, 3, 3);

	Position3 a1 = a;
	Position3 a2 = a;
	a1 += b;
	a2 -= b;

	ASSERT_TRUE(a + b == Position3(9, 12, 15));
	ASSERT_TRUE(a1 == Position3(9, 12, 15));
	ASSERT_TRUE(a - Position3(2, 2, 2) == Vector3(4, 7, 10));
	ASSERT_TRUE(a - b == Position3(3, 6, 9));
	ASSERT_TRUE(a2 == Position3(3, 6, 9));
}

TEST_F(Position3dTest, FPosition3BasicOperations) {
	FPosition3 a(2.1, 4.2, 8.4);

	ASSERT_TRUE(a == FPosition3(2.1, 4.2, 8.4));
	ASSERT_FALSE(a != FPosition3(2.1, 4.2, 8.4));
	ASSERT_TRUE(a.withinArea(FPosition3(1.1, 1.2, 1.4), FPosition3(9.1, 9.2, 9.3)));
	ASSERT_FALSE(a.withinArea(FPosition3(1.1, 1.2, 1.4), FPosition3(9.1, 2, 9.3)));
}

TEST_F(Position3dTest, FPosition3Functions) {
	FPosition3 a(5.5, 5.4, 5.3);
	FPosition3 b(1.1, 2.2, 3.3);

	ASSERT_TRUE(a.toString() == "(5.500000, 5.400000, 5.300000)");
	ASSERT_TRUE(approx_equals(a.manhattenDistanceTo(b), 9.6f));
	ASSERT_TRUE(approx_equals(a.manhattenDistanceToOrigin(), 16.2f));
	ASSERT_TRUE(approx_equals(a.distanceToSquared(b), 33.6f));
	ASSERT_TRUE(approx_equals(a.distanceToOriginSquared(), 87.5f));
	ASSERT_TRUE(approx_equals(a.distanceTo(b), sqrt(33.6f)));
	ASSERT_TRUE(approx_equals(a.distanceToOrigin(), sqrt(87.5f)));
}

TEST_F(Position3dTest, FPosition3Comparisons) {
	FPosition3 a(6.4, 9.2, 12);
	FVector3 b(3.2, 3.1, 3);

	FPosition3 a1 = a;
	FPosition3 a2 = a;
	a1 += b;
	a2 -= b;

	ASSERT_TRUE(a + b == FPosition3(9.6, 12.3, 15));
	ASSERT_TRUE(a1 == FPosition3(9.6, 12.3, 15));
	ASSERT_TRUE(a - FPosition3(2.1, 2.1, 2) == FVector3(4.3, 7.1, 10));
	ASSERT_TRUE(a - b == FPosition3(3.2, 6.1, 9));
	ASSERT_TRUE(a2 == FPosition3(3.2, 6.1, 9));
	ASSERT_TRUE(a * 3.1 == FPosition3(19.84, 28.52, 37.2));
}

TEST_F(Position3dTest, ZeroConstructor) {
	Position3 zeroPos;
	ASSERT_EQ(zeroPos.x, 0);
	ASSERT_EQ(zeroPos.y, 0);
	ASSERT_EQ(zeroPos.z, 0);

	FPosition3 zeroFPos;
	ASSERT_EQ(zeroFPos.x, 0);
	ASSERT_EQ(zeroFPos.y, 0);
	ASSERT_EQ(zeroFPos.z, 0);
	ASSERT_TRUE(zeroFPos.isValid());

	Vector3 zeroVec;
	ASSERT_EQ(zeroVec.dx, 0);
	ASSERT_EQ(zeroVec.dy, 0);
	ASSERT_EQ(zeroVec.dz, 0);

	FVector3 zeroFVec;
	ASSERT_EQ(zeroFVec.dx, 0);
	ASSERT_EQ(zeroFVec.dy, 0);
	ASSERT_EQ(zeroFVec.dz, 0);

	Size3 zeroSize3;
	ASSERT_EQ(zeroSize3.w, 0);
	ASSERT_EQ(zeroSize3.h, 0);
	ASSERT_EQ(zeroSize3.d, 0);

	FSize3 zeroFSize3;
	ASSERT_EQ(zeroFSize3.w, 0);
	ASSERT_EQ(zeroFSize3.h, 0);
	ASSERT_EQ(zeroFSize3.d, 0);
	ASSERT_FALSE(zeroFSize3.isValid());
	ASSERT_TRUE(zeroFSize3.isInvalid());

	Orientation zeroOrientation;
	ASSERT_EQ(zeroOrientation.rotation, Rotation::ZERO);
	ASSERT_EQ(zeroOrientation.flipped, false);
}

TEST_F(Position3dTest, FPosition3Invalid) {
	FPosition3 zeroVec = FPosition3::getInvalid();
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec = FPosition3(1, 2, 3);
	ASSERT_EQ(zeroVec.x, 1);
	ASSERT_EQ(zeroVec.y, 2);
	ASSERT_EQ(zeroVec.z, 3);
	ASSERT_FALSE(zeroVec.isInvalid());
	ASSERT_TRUE(zeroVec.isValid());
	zeroVec.setInvalid();
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec.x = 0;
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec.y = 0;
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec.z = 0;
	ASSERT_EQ(zeroVec.x, 0);
	ASSERT_EQ(zeroVec.y, 0);
	ASSERT_EQ(zeroVec.z, 0);
	ASSERT_FALSE(zeroVec.isInvalid());
	ASSERT_TRUE(zeroVec.isValid());
	zeroVec.setInvalid();
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
	zeroVec.y = 0;
	ASSERT_TRUE(zeroVec.isInvalid());
	ASSERT_FALSE(zeroVec.isValid());
}

TEST_F(Position3dTest, toString) {
	Position3 pos;
	ASSERT_EQ(pos.toString(), "(0, 0, 0)");
	pos.x = 10;
	ASSERT_EQ(pos.toString(), "(10, 0, 0)");
	pos.y = -51;
	ASSERT_EQ(pos.toString(), "(10, -51, 0)");
	pos.x = -1161231;
	ASSERT_EQ(pos.toString(), "(-1161231, -51, 0)");
	pos.z = -41;
	ASSERT_EQ(pos.toString(), "(-1161231, -51, -41)");

	Vector3 vec;
	ASSERT_EQ(vec.toString(), "<0, 0, 0>");
	vec.dx = 10;
	ASSERT_EQ(vec.toString(), "<10, 0, 0>");
	vec.dy = -51;
	ASSERT_EQ(vec.toString(), "<10, -51, 0>");
	vec.dx = -1161231;
	ASSERT_EQ(vec.toString(), "<-1161231, -51, 0>");
	vec.dz = -41;
	ASSERT_EQ(vec.toString(), "<-1161231, -51, -41>");

	Size3 size3;
	ASSERT_EQ(size3.toString(), "0x0x0");
	size3.w = 10;
	ASSERT_EQ(size3.toString(), "10x0x0");
	size3.h = 14371;
	ASSERT_EQ(size3.toString(), "10x14371x0");
	size3.h = -51;
	ASSERT_EQ(size3.toString(), "10x-51x0");
	size3.w = -1161231;
	ASSERT_EQ(size3.toString(), "-1161231x-51x0");
	size3.d = -41;
	ASSERT_EQ(size3.toString(), "-1161231x-51x-41");

	// Orientation orientation;

	// ASSERT_EQ(orientation.toString(), "(r:0, f:0)");
}

TEST_F(Position3dTest, vector3Iter) {
	for (unsigned int i = 0; i < 50; i++) {
		Vector3 v = randVec3();
		v.dx %= 100; // takes too long otherwise
		v.dy %= 100;
		v.dz %= 100;

		unsigned long long volume = 0;
		for (Vector3::Iterator iter = v.iter(); iter; iter++) {
			Vector3::Iterator curIter = iter;
			Vector3::Iterator nextIter = (++iter)--;
			ASSERT_EQ(iter++, curIter);
			ASSERT_EQ(iter--, nextIter);
			ASSERT_EQ(++iter, nextIter);
			ASSERT_EQ(--iter, curIter);
			volume++;
		}
		ASSERT_EQ(volume, (v.dx+1)*(v.dy+1)*(v.dz+1));
	}
}

TEST_F(Position3dTest, size3Iter) {
	for (unsigned int i = 0; i < 50; i++) {
		Size3 s = randSize3();
		s.w = abs(s.w) % 100 + 1; // takes too long otherwise, move than 1 w&h
		s.h = abs(s.h) % 100 + 1;
		s.d = abs(s.d) % 100 + 1;
		ASSERT_TRUE(s.isValid());

		unsigned long long volume = 0;
		for (Size3::Iterator iter = s.iter(); iter; iter++) {
			Size3::Iterator curIter = iter;
			Size3::Iterator nextIter = (++iter)--;
			ASSERT_EQ(iter++, curIter);
			ASSERT_EQ(iter--, nextIter);
			ASSERT_EQ(++iter, nextIter);
			ASSERT_EQ(--iter, curIter);
			volume++;
		}
		ASSERT_EQ(volume, s.w*s.h*s.d);
		ASSERT_EQ(volume, s.volume());
	}
}

TEST_F(Position3dTest, position3Iter) {
	for (unsigned int i = 0; i < 50; i++) {
		Position3 p1 = randPos3();
		Position3 p2 = randPos3();
		p1.x %= 100; // takes too long otherwise
		p1.y %= 100; // takes too long otherwise
		p1.z %= 100; // takes too long otherwise
		p2.x %= 100; // takes too long otherwise
		p2.y %= 100; // takes too long otherwise
		p2.z %= 100; // takes too long otherwise

		unsigned long long volume = 0;
		// ASSERT_EQ(*(p1.iterTo(p2)), p1); // not guaranteed. Should it be?
		for (Position3::Iterator iter = p1.iterTo(p2); iter; iter++) {
			Position3::Iterator curIter = iter;
			Position3::Iterator nextIter = (++iter)--;
			ASSERT_EQ(iter++, curIter);
			ASSERT_EQ(iter--, nextIter);
			ASSERT_EQ(++iter, nextIter);
			ASSERT_EQ(--iter, curIter);
			volume++;
		}
		ASSERT_EQ(volume, (abs(p1.x - p2.x)+1) * (abs(p1.y - p2.y)+1) * (abs(p1.z - p2.z)+1));
	}
}
