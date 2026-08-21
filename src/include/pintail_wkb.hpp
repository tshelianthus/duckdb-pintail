#pragma once

// Dependency-free Little-Endian WKB encoder for Tier-0 Core GEOMETRY values.
// Does not use GEOS, PROJ, or DuckDB Spatial.

#include "pintail_geohash.hpp"

#include "duckdb/common/types/string_type.hpp"
#include "duckdb/common/types/vector.hpp"

namespace duckdb {

static constexpr idx_t PINTAIL_WKB_POINT_SIZE = 21;
static constexpr idx_t PINTAIL_WKB_POLYGON_SIZE = 93;

//! Little-Endian WKB POINT (type 1): byte order + type + x + y. x = longitude, y = latitude.
string_t EncodeWkbPoint(Vector &result, double longitude, double latitude);

//! Little-Endian WKB POLYGON (type 3): one closed 5-vertex ring, LL → UL → UR → LR → LL.
string_t EncodeWkbPolygon(Vector &result, const GeohashBBox &bbox);

} // namespace duckdb
