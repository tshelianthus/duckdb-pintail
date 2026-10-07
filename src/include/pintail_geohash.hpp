#pragma once

// Pure C++ geohash encoding/decoding — no external dependencies (Tier 0).
// Algorithm follows the standard geohash grid (32-cell Base32, lat interleaved with lon),
// with the Pintail coverage/precision contract. See docs/geohash-postgis-parity.md
// for intentional differences from PostGIS.
//
//   Base32 alphabet: "0123456789bcdefghjkmnpqrstuvwxyz"   (a,i,l,o excluded)
//   Valid coordinates: lat in [-90, 90], lon in [-180, 180]
//   Coordinate encoding precision: [1, 20]; geometry maxchars: [0, 20] (0=auto)

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "duckdb/common/exception.hpp"
#include "duckdb/common/helper.hpp"

namespace duckdb {

//! The standard geohash Base32 alphabet (a, i, l, o are excluded).
static constexpr const char *GEOHASH_BASE32 = "0123456789bcdefghjkmnpqrstuvwxyz";

//! Default geohash precision when the caller omits it (per API contract).
static constexpr int32_t DEFAULT_GEOHASH_PRECISION = 12;

//! Pintail encoding/neighbor limit; decode strings may be longer.
static constexpr int32_t MAX_GEOHASH_PRECISION = 20;

//! Reverse lookup: Base32 character -> 0..31 index, or -1 if invalid.
inline int32_t GeohashBase32Index(char c) {
	for (int32_t i = 0; i < 32; i++) {
		if (GEOHASH_BASE32[i] == c) {
			return i;
		}
	}
	return -1;
}

//! Validates the stricter lowercase/length input contract retained by neighbors.
inline void ValidateGeohash(const std::string &geohash) {
	if (geohash.empty() || geohash.size() > static_cast<size_t>(MAX_GEOHASH_PRECISION)) {
		throw InvalidInputException("Geohash length out of range: %llu (must be within [1, 20])",
		                            static_cast<unsigned long long>(geohash.size()));
	}
	for (char c : geohash) {
		if (GeohashBase32Index(c) < 0) {
			throw InvalidInputException("Invalid Base32 character in geohash: %s", string(1, c));
		}
	}
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

//! Decode the consumed prefix. Empty input/precision zero denotes the world.
//! Negative precision means full length; a larger positive precision clamps to length.
inline GeohashBBox GeohashDecodeBBox(const std::string &geohash, int32_t precision = -1) {
	const size_t length = precision < 0 ? geohash.size() :
	    MinValue(geohash.size(), static_cast<size_t>(precision));
	GeohashBBox box {-90.0, -180.0, 90.0, 180.0};
	bool longitude = true;
	for (size_t pos = 0; pos < length; pos++) {
		char c = geohash[pos];
		if (c >= 'A' && c <= 'Z') {
			c = static_cast<char>(c + ('a' - 'A'));
		}
		const int32_t digit = GeohashBase32Index(c);
		if (digit < 0) {
			throw InvalidInputException("Invalid Base32 character at geohash byte %llu",
			                            static_cast<unsigned long long>(pos));
		}
		for (int32_t bit = 4; bit >= 0; bit--) {
			auto &low = longitude ? box.min_lon : box.min_lat;
			auto &high = longitude ? box.max_lon : box.max_lat;
			const double mid = (low + high) / 2.0;
			if ((digit >> bit) & 1) {
				low = mid;
			} else {
				high = mid;
			}
			longitude = !longitude;
		}
	}
	return box;
}

//! Validate geographic XY separately from any precision choice.
inline void ValidateGeographicXY(double lon, double lat) {
	if (!std::isfinite(lat) || lat < -90 || lat > 90) {
		throw OutOfRangeException("Latitude out of range: %f (must be finite within [-90, 90])", lat);
	}
	if (!std::isfinite(lon) || lon < -180 || lon > 180) {
		throw OutOfRangeException("Longitude out of range: %f (must be finite within [-180, 180])", lon);
	}
}

//! Descend closed cells while the entire extent fits. Only complete characters survive.
//! Independent implementation of the public coverage promise, not a bbox-center approximation.
inline std::string GeohashEncodeExtent(const GeohashBBox &extent, int32_t maxchars) {
	if (maxchars < 0 || maxchars > MAX_GEOHASH_PRECISION) {
		throw InvalidInputException("Geohash geometry precision out of range: %d (must be within [0, 20])", maxchars);
	}
	ValidateGeographicXY(extent.min_lon, extent.min_lat);
	ValidateGeographicXY(extent.max_lon, extent.max_lat);
	const int32_t limit = maxchars == 0 ? MAX_GEOHASH_PRECISION : maxchars;
	GeohashBBox cell {-90, -180, 90, 180};
	std::string hash;
	hash.reserve(limit);
	for (int32_t bit = 0, digit = 0; bit < limit * 5; bit++) {
		const bool longitude = bit % 2 == 0;
		auto &low = longitude ? cell.min_lon : cell.min_lat;
		auto &high = longitude ? cell.max_lon : cell.max_lat;
		const double min = longitude ? extent.min_lon : extent.min_lat;
		const double max = longitude ? extent.max_lon : extent.max_lat;
		const double mid = (low + high) / 2;
		digit <<= 1;
		if (min >= mid) {
			low = mid;
			digit |= 1;
		} else if (max <= mid) {
			high = mid;
		} else {
			break;
		}
		if (bit % 5 == 4) {
			hash.push_back(GEOHASH_BASE32[digit]);
			digit = 0;
		}
	}
	return hash;
}

//! Cardinal-direction code for the base adjacent() step: 0=N, 1=S, 2=E, 3=W.
//! Distinct from GeohashNeighborSlot, which indexes the 8-cell public output array.
enum GeohashCardinal : int32_t { GEOHASH_CARD_N = 0, GEOHASH_CARD_S = 1, GEOHASH_CARD_E = 2, GEOHASH_CARD_W = 3 };

//! Output slot indices for GeohashNeighbors, in the public order N, NE, E, SE, S, SW, W, NW.
enum GeohashNeighborSlot : int32_t {
	GEOHASH_SLOT_N = 0,
	GEOHASH_SLOT_NE = 1,
	GEOHASH_SLOT_E = 2,
	GEOHASH_SLOT_SE = 3,
	GEOHASH_SLOT_S = 4,
	GEOHASH_SLOT_SW = 5,
	GEOHASH_SLOT_W = 6,
	GEOHASH_SLOT_NW = 7,
};

namespace {

//! Boundary tables (davetroy/geohash-js reference): for each cardinal direction and
//! parity (even/odd length), the set of trailing Base32 chars that sit on the world edge.
constexpr const char *GEOHASH_BORDER[4][2] = {
    {"prxz", "bcfguvyz"},     // N
    {"028b", "0145hjnp"},     // S
    {"bcfguvyz", "prxz"},     // E
    {"0145hjnp", "028b"},     // W
};

//! Neighbor translation tables: new last char for each cardinal direction and parity.
constexpr const char *GEOHASH_NEIGHBOR[4][2] = {
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
	ValidateGeohash(geohash);
	std::string result = geohash;

	// Propagate an edge crossing toward the root, then translate each affected digit.
	// This is equivalent to the geohash-js recursive algorithm, but its stack use is
	// constant and therefore cannot grow with attacker-controlled input.
	for (size_t remaining = geohash.size(); remaining > 0; remaining--) {
		const size_t pos = remaining - 1;
		const char current = geohash[pos];
		const bool odd = (remaining % 2) == 1;
		const char *border = GEOHASH_BORDER[dir][odd ? 1 : 0];
		const char *neighbor = GEOHASH_NEIGHBOR[dir][odd ? 1 : 0];
		const char *found = strchr(neighbor, current);
		if (!found) {
			throw InvalidInputException("Invalid Base32 character in geohash: %s", string(1, current));
		}
		const auto translated = static_cast<size_t>(found - neighbor);
		result[pos] = GEOHASH_BASE32[translated];
		if (!strchr(border, current)) {
			break;
		}
	}
	return result;
}

//! Computes the 8 adjacent cells, ordered [N, NE, E, SE, S, SW, W, NW].
inline void GeohashNeighbors(const std::string &geohash, std::string out[8]) {
	ValidateGeohash(geohash);

	out[GEOHASH_SLOT_N] = GeohashAdjacent(geohash, GEOHASH_CARD_N);
	out[GEOHASH_SLOT_E] = GeohashAdjacent(geohash, GEOHASH_CARD_E);
	out[GEOHASH_SLOT_S] = GeohashAdjacent(geohash, GEOHASH_CARD_S);
	out[GEOHASH_SLOT_W] = GeohashAdjacent(geohash, GEOHASH_CARD_W);

	// Diagonals via composition of cardinal steps.
	out[GEOHASH_SLOT_NE] = GeohashAdjacent(out[GEOHASH_SLOT_N], GEOHASH_CARD_E);
	out[GEOHASH_SLOT_SE] = GeohashAdjacent(out[GEOHASH_SLOT_S], GEOHASH_CARD_E);
	out[GEOHASH_SLOT_SW] = GeohashAdjacent(out[GEOHASH_SLOT_S], GEOHASH_CARD_W);
	out[GEOHASH_SLOT_NW] = GeohashAdjacent(out[GEOHASH_SLOT_N], GEOHASH_CARD_W);
}

} // namespace duckdb
