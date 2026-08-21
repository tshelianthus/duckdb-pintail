#pragma once

// Pure C++ geohash encoding/decoding — no external dependencies (Tier 0).
// Algorithm follows the standard geohash grid (32-cell Base32, lat interleaved with lon),
// identical to PostGIS ST_GeoHash / ST_PointFromGeoHash semantics and the Wikipedia/Geohash.org
// reference. Validated against PostGIS official documentation examples.
//
//   Base32 alphabet: "0123456789bcdefghjkmnpqrstuvwxyz"   (a,i,l,o excluded)
//   Valid coordinates: lat in [-90, 90], lon in [-180, 180]
//   Valid precision: [1, 20]

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "duckdb/common/exception.hpp"

namespace duckdb {

//! The standard geohash Base32 alphabet (a, i, l, o are excluded).
inline constexpr const char *GEOHASH_BASE32 = "0123456789bcdefghjkmnpqrstuvwxyz";

//! Default geohash precision when the caller omits it (per API contract).
inline constexpr int32_t DEFAULT_GEOHASH_PRECISION = 12;

//! Maximum geohash precision (full double-precision resolution).
inline constexpr int32_t MAX_GEOHASH_PRECISION = 20;

//! Reverse lookup: Base32 character -> 0..31 index, or -1 if invalid.
inline int32_t GeohashBase32Index(char c) {
	for (int32_t i = 0; i < 32; i++) {
		if (GEOHASH_BASE32[i] == c) {
			return i;
		}
	}
	return -1;
}

//! Encodes (lat, lon) into a geohash string of the given precision.
inline std::string GeohashEncode(double lat, double lon, int32_t precision) {
	if (!std::isfinite(lat) || lat < -90.0 || lat > 90.0) {
		throw OutOfRangeException("Latitude out of range: %f (must be within [-90, 90])", lat);
	}
	if (!std::isfinite(lon) || lon < -180.0 || lon > 180.0) {
		throw OutOfRangeException("Longitude out of range: %f (must be within [-180, 180])", lon);
	}
	if (precision < 1 || precision > MAX_GEOHASH_PRECISION) {
		throw InvalidInputException("Geohash precision out of range: %d (must be within [1, 20])", precision);
	}

	double lat_lo = -90.0, lat_hi = 90.0;
	double lon_lo = -180.0, lon_hi = 180.0;

	std::string result;
	result.reserve(precision);

	int32_t bit = 0;
	int32_t ch = 0;
	bool even_bit = true;

	while ((int32_t)result.size() < precision) {
		if (even_bit) {
			double mid = (lon_lo + lon_hi) / 2.0;
			if (lon >= mid) {
				ch = (ch << 1) | 1;
				lon_lo = mid;
			} else {
				ch = ch << 1;
				lon_hi = mid;
			}
		} else {
			double mid = (lat_lo + lat_hi) / 2.0;
			if (lat >= mid) {
				ch = (ch << 1) | 1;
				lat_lo = mid;
			} else {
				ch = ch << 1;
				lat_hi = mid;
			}
		}
		even_bit = !even_bit;

		bit++;
		if (bit == 5) {
			result.push_back(GEOHASH_BASE32[ch]);
			bit = 0;
			ch = 0;
		}
	}
	return result;
}

struct GeohashBBox {
	double min_lat;
	double min_lon;
	double max_lat;
	double max_lon;
};

//! Decodes a geohash string into its bounding box (and validates all characters).
inline GeohashBBox GeohashDecodeBBox(const std::string &geohash) {
	if (geohash.empty()) {
		throw InvalidInputException("Geohash string must not be empty");
	}

	double lat_lo = -90.0, lat_hi = 90.0;
	double lon_lo = -180.0, lon_hi = 180.0;
	bool even_bit = true;

	for (char c : geohash) {
		int32_t idx = GeohashBase32Index(c);
		if (idx < 0) {
			throw InvalidInputException("Invalid Base32 character in geohash: %s", string(1, c));
		}
		for (int32_t i = 4; i >= 0; i--) {
			int32_t bit = (idx >> i) & 1;
			if (even_bit) {
				double mid = (lon_lo + lon_hi) / 2.0;
				if (bit) {
					lon_lo = mid;
				} else {
					lon_hi = mid;
				}
			} else {
				double mid = (lat_lo + lat_hi) / 2.0;
				if (bit) {
					lat_lo = mid;
				} else {
					lat_hi = mid;
				}
			}
			even_bit = !even_bit;
		}
	}
	return GeohashBBox {lat_lo, lon_lo, lat_hi, lon_hi};
}

//! Adjacent-cell direction indices (matches the public ORDER: N, NE, E, SE, S, SW, W, NW).
enum GeohashDirection : int32_t {
	GEOHASH_N = 0,
	GEOHASH_NE = 1,
	GEOHASH_E = 2,
	GEOHASH_SE = 3,
	GEOHASH_S = 4,
	GEOHASH_SW = 5,
	GEOHASH_W = 6,
	GEOHASH_NW = 7,
};

//! Cardinal-direction code for the base adjacent() step: 0=N, 1=S, 2=E, 3=W.
enum GeohashCardinal : int32_t { GEOHASH_CARD_N = 0, GEOHASH_CARD_S = 1, GEOHASH_CARD_E = 2, GEOHASH_CARD_W = 3 };

namespace {

//! Boundary tables (davetroy/geohash-js reference): for each cardinal direction and
//! parity (even/odd length), the set of trailing Base32 chars that sit on the world edge.
inline constexpr const char *GEOHASH_BORDER[4][2] = {
    {"prxz", "bcfguvyz"},     // N
    {"028b", "0145hjnp"},     // S
    {"bcfguvyz", "prxz"},     // E
    {"0145hjnp", "028b"},     // W
};

//! Neighbor translation tables: new last char for each cardinal direction and parity.
inline constexpr const char *GEOHASH_NEIGHBOR[4][2] = {
    {"p0r21436x8zb9dcf5h7kjnmqesgutwvy", "bc01fg45238967deuvhjyznpkmstqrwx"}, // N
    {"14365h7k9dcfesgujnmqp0r2twvyx8zb", "238967debc01fg45kmstqrwxuvhjyznp"}, // S
    {"bc01fg45238967deuvhjyznpkmstqrwx", "p0r21436x8zb9dcf5h7kjnmqesgutwvy"}, // E
    {"238967debc01fg45kmstqrwxuvhjyznp", "14365h7k9dcfesgujnmqp0r2twvyx8zb"}, // W
};

} // namespace

//! Computes the adjacent cell one step in a given cardinal direction (N/S/E/W).
//! This operates directly on the Base32 cell grid (no lat/lon round-trip), so it is
//! exact at the poles and antimeridian where a center-offset approach degenerates.
inline std::string GeohashAdjacent(const std::string &geohash, GeohashCardinal dir) {
	if (geohash.empty()) {
		throw InvalidInputException("Geohash string must not be empty");
	}

	char last = geohash.back();
	std::string parent = geohash.substr(0, geohash.size() - 1);
	bool odd = (geohash.size() % 2) == 1; // odd length -> use "odd" parity table

	const char *border = GEOHASH_BORDER[dir][odd ? 1 : 0];
	const char *neighbor = GEOHASH_NEIGHBOR[dir][odd ? 1 : 0];

	// If the last char sits on the world edge for this direction, move up a level first.
	if (strchr(border, last) && !parent.empty()) {
		parent = GeohashAdjacent(parent, dir);
	}

	// The neighbor table is a permutation of the Base32 alphabet: the new last char is
	// GEOHASH_BASE32[i] where i is the position of `last` within that permutation.
	const char *found = strchr(neighbor, last);
	if (!found) {
		throw InvalidInputException("Invalid Base32 character in geohash: %s", string(1, last));
	}
	int32_t idx = static_cast<int32_t>(found - neighbor);
	return parent + GEOHASH_BASE32[idx];
}

//! Computes the 8 adjacent cells, ordered [N, NE, E, SE, S, SW, W, NW].
inline void GeohashNeighbors(const std::string &geohash, std::string out[8]) {
	if (geohash.empty()) {
		throw InvalidInputException("Geohash string must not be empty");
	}

	out[GEOHASH_N] = GeohashAdjacent(geohash, GEOHASH_CARD_N);
	out[GEOHASH_E] = GeohashAdjacent(geohash, GEOHASH_CARD_E);
	out[GEOHASH_S] = GeohashAdjacent(geohash, GEOHASH_CARD_S);
	out[GEOHASH_W] = GeohashAdjacent(geohash, GEOHASH_CARD_W);

	// Diagonals via composition of cardinal steps.
	out[GEOHASH_NE] = GeohashAdjacent(out[GEOHASH_N], GEOHASH_CARD_E);
	out[GEOHASH_SE] = GeohashAdjacent(out[GEOHASH_S], GEOHASH_CARD_E);
	out[GEOHASH_SW] = GeohashAdjacent(out[GEOHASH_S], GEOHASH_CARD_W);
	out[GEOHASH_NW] = GeohashAdjacent(out[GEOHASH_N], GEOHASH_CARD_W);
}

} // namespace duckdb
