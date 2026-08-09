// The two SVM enums the Voronoi kernel names, plus FLT_MAX.
//
// Upstream declares these in kernel/svm/types.h, which also carries the whole node bytecode
// layout — several thousand lines of scene representation that has nothing to do with the maths
// under test. Only the enumerator *names* matter here: the comparison selects a feature or a
// metric by name on both sides, so the underlying values never have to agree with upstream's.
//
// The order below is upstream's, for readability only. Nothing depends on it.

#pragma once

#include "util/defines.h"

#include <cfloat>

CCL_NAMESPACE_BEGIN

enum NodeVoronoiDistanceMetric {
  NODE_VORONOI_EUCLIDEAN,
  NODE_VORONOI_MANHATTAN,
  NODE_VORONOI_CHEBYCHEV,
  NODE_VORONOI_MINKOWSKI,
};

enum NodeVoronoiFeature {
  NODE_VORONOI_F1,
  NODE_VORONOI_SMOOTH_F1,
  NODE_VORONOI_F2,
  NODE_VORONOI_DISTANCE_TO_EDGE,
  NODE_VORONOI_N_SPHERE_RADIUS,
};

CCL_NAMESPACE_END
