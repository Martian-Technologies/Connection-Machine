#include "randomGens.h"

Position randPos() { return Position(rand() % 100000, rand() % 100000); }
Vector randVec() { return Vector(rand() % 100000, rand() % 100000); }
Size randSize() { return Size(rand() % 100000, rand() % 100000); }

Position3 randPos3() { return Position3(rand() % 100000, rand() % 100000, rand() % 100000); }
Vector3 randVec3() { return Vector3(rand() % 100000, rand() % 100000, rand() % 100000); }
Size3 randSize3() { return Size3(rand() % 100000, rand() % 100000, rand() % 100000); }
