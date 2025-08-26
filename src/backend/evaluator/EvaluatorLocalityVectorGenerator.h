#ifndef evaluatorLocalityVectorGenerator_h
#define evaluatorLocalityVectorGenerator_h

#include "util/localityAllocator.h"

typedef LocalityVectorGenerator<unsigned int, 128, 12, 256*sizeof(unsigned int)> EvaluatorLocalityVectorGenerator;

#endif /* evaluatorLocalityVectorGenerator_h */
