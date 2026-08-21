#include "pintail_wkb.hpp"

#include <cstdint>
#include <cstring>

namespace duckdb {

namespace {

void WriteU8(data_ptr_t dest, idx_t &offset, uint8_t value) {
	dest[offset++] = value;
}

void WriteU32LE(data_ptr_t dest, idx_t &offset, uint32_t value) {
	dest[offset++] = static_cast<data_t>(value);
	dest[offset++] = static_cast<data_t>(value >> 8);
	dest[offset++] = static_cast<data_t>(value >> 16);
	dest[offset++] = static_cast<data_t>(value >> 24);
}

void WriteF64LE(data_ptr_t dest, idx_t &offset, double value) {
	uint64_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	dest[offset++] = static_cast<data_t>(bits);
	dest[offset++] = static_cast<data_t>(bits >> 8);
	dest[offset++] = static_cast<data_t>(bits >> 16);
	dest[offset++] = static_cast<data_t>(bits >> 24);
	dest[offset++] = static_cast<data_t>(bits >> 32);
	dest[offset++] = static_cast<data_t>(bits >> 40);
	dest[offset++] = static_cast<data_t>(bits >> 48);
	dest[offset++] = static_cast<data_t>(bits >> 56);
}

void WriteXY(data_ptr_t dest, idx_t &offset, double x, double y) {
	WriteF64LE(dest, offset, x);
	WriteF64LE(dest, offset, y);
}

string_t FinishBlob(string_t blob, idx_t expected_size, idx_t offset) {
	if (offset != expected_size) {
		throw InvalidInputException("Internal WKB size mismatch: wrote %llu bytes, expected %llu",
		                            static_cast<unsigned long long>(offset),
		                            static_cast<unsigned long long>(expected_size));
	}
	blob.Finalize();
	return blob;
}

} // namespace

string_t EncodeWkbPoint(Vector &result, double longitude, double latitude) {
	auto blob = StringVector::EmptyString(result, PINTAIL_WKB_POINT_SIZE);
	auto dest = reinterpret_cast<data_ptr_t>(blob.GetDataWriteable());
	idx_t offset = 0;
	WriteU8(dest, offset, 1);
	WriteU32LE(dest, offset, 1);
	WriteXY(dest, offset, longitude, latitude);
	return FinishBlob(blob, PINTAIL_WKB_POINT_SIZE, offset);
}

string_t EncodeWkbPolygon(Vector &result, const GeohashBBox &bbox) {
	auto blob = StringVector::EmptyString(result, PINTAIL_WKB_POLYGON_SIZE);
	auto dest = reinterpret_cast<data_ptr_t>(blob.GetDataWriteable());
	idx_t offset = 0;
	WriteU8(dest, offset, 1);
	WriteU32LE(dest, offset, 3);
	WriteU32LE(dest, offset, 1);
	WriteU32LE(dest, offset, 5);
	WriteXY(dest, offset, bbox.min_lon, bbox.min_lat);
	WriteXY(dest, offset, bbox.min_lon, bbox.max_lat);
	WriteXY(dest, offset, bbox.max_lon, bbox.max_lat);
	WriteXY(dest, offset, bbox.max_lon, bbox.min_lat);
	WriteXY(dest, offset, bbox.min_lon, bbox.min_lat);
	return FinishBlob(blob, PINTAIL_WKB_POLYGON_SIZE, offset);
}

} // namespace duckdb
