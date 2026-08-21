#pragma once

// Pure C++ geohash encoding/decoding — no external dependencies (Tier 0).
// Algorithm follows the standard geohash grid (32-cell Base32, lat interleaved with lon),
// identical to PostGIS ST_GeoHash / ST_PointFromGeoHash semantics and the Wikipedia/Geohash.org
// reference. Validated against PostGIS official documentation examples.
//
//   Base32 alphabet: "0123456789bcdefghjkmnpqrstuvwxyz"   (a,i,l,o excluded)
//   Valid coordinates: lat in [-90, 90], lon in [-180, 180]
//   Valid precision: [1, 20]

#include <cstdint>
#include <string>

#include "duckdb/common/exception.hpp"

namespace duckdb {

//! The standard geohash Base32 alphabet (a, i, l, o are excluded).
static constexpr const char *GEOHASH_BASE32 = "0123456789bcdefghjkmnpqrstuvwxyz";

//! Reverse lookup: Base32 character -> 0..31 index, or -1 if invalid.
inline int32_t GeohashBase32Index(char c) {
	for (int32_t i = 0; i < 32; i++) {
		if (GEOHASH_BASE32[i] == c) {
			return i;
		}
	}
	return -1;
}

//! Throws if c is not a valid Base32 geohash character.
inline void GeohashValidateChar(char c) {
	if (GeohashBase32Index(c) < 0) {
		throw InvalidInputException("Invalid Base32 character in geohash: %s", string(1, c));
	}
}

//! Encodes (lat, lon) into a geohash string of the given precision.
inline std::string GeohashEncode(double lat, double lon, int32_t precision) {
	if (lat < -90.0 || lat > 90.0) {
		throw OutOfRangeException("Latitude out of range: %f (must be within [-90, 90])", lat);
	}
	if (lon < -180.0 || lon > 180.0) {
		throw OutOfRangeException("Longitude out of range: %f (must be within [-180, 180])", lon);
	}
	if (precision < 1 || precision > 20) {
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

//! Computes the 8 adjacent cells (N, NE, E, SE, S, SW, W, NW) at the same resolution.
//! Returns a caller-provided array of 8 strings.
inline void GeohashNeighbors(const std::string &geohash, std::string out[8]) {
	if (geohash.empty()) {
		throw InvalidInputException("Geohash string must not be empty");
	}

	GeohashBBox bbox = GeohashDecodeBBox(geohash);
	double center_lat = (bbox.min_lat + bbox.max_lat) / 2.0;
	double center_lon = (bbox.min_lon + bbox.max_lon) / 2.0;

	double dlat = bbox.max_lat - bbox.min_lat;   // height of the cell
	double dlon = bbox.max_lon - bbox.min_lon;   // width of the cell

	int32_t precision = (int32_t)geohash.size();

	// Neighbor centers in (dlat, dlon) offsets, ordered [N, NE, E, SE, S, SW, W, NW].
	const double lat_off[8] = {1.0, 1.0, 0.0, -1.0, -1.0, -1.0, 0.0, 1.0};
	const double lon_off[8] = {0.0, 1.0, 1.0, 1.0, 0.0, -1.0, -1.0, -1.0};

	for (int32_t i = 0; i < 8; i++) {
		double nlat = center_lat + lat_off[i] * dlat;
		double nlon = center_lon + lon_off[i] * dlon;
		// Clamp to valid coordinate range for cells along the poles / antimeridian edges.
		if (nlat > 90.0) {
			nlat = 90.0;
		} else if (nlat < -90.0) {
			nlat = -90.0;
		}
		if (nlon > 180.0) {
			nlon = 180.0;
		} else if (nlon < -180.0) {
			nlon = -180.0;
		}
		out[i] = GeohashEncode(nlat, nlon, precision);
	}
}

} // namespace duckdb
