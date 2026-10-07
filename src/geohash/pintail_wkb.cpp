#include "pintail_wkb.hpp"

#include "duckdb/common/types/geometry.hpp"
#include "duckdb/common/helper.hpp"

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

// Core extent reduction can hide a NaN vertex among finite ones. This narrow checked
// scan validates XY; Core still supplies type validation, empty handling and the extent.
void ValidateGeographicWkb(const string_t &geometry) {
	const auto data = reinterpret_cast<const_data_ptr_t>(geometry.GetData());
	const idx_t size = geometry.GetSize();
	idx_t offset = 0;
	auto reserve = [&](idx_t bytes) {
		if (bytes > size - offset) {
			throw InvalidInputException("Truncated Core geometry WKB");
		}
		auto ptr = data + offset;
		offset += bytes;
		return ptr;
	};
	auto read_u32 = [&]() { return LoadLE<uint32_t>(reserve(4)); };
	auto vertices = [&](uint32_t count, uint32_t dimensions, bool point) {
		const idx_t width = dimensions * sizeof(double);
		if (count > (size - offset) / width) {
			throw InvalidInputException("Truncated Core geometry vertices");
		}
		for (uint32_t i = 0; i < count; i++) {
			const auto ptr = reserve(width);
			const double x = LoadLE<double>(ptr);
			const double y = LoadLE<double>(ptr + sizeof(double));
			bool empty = point;
			for (uint32_t d = 0; empty && d < dimensions; d++) {
				empty = std::isnan(LoadLE<double>(ptr + d * sizeof(double)));
			}
			if (!empty) {
				ValidateGeographicXY(x, y);
			}
		}
	};
	// Core stores collections as consecutive normalized WKB headers, so no recursion is needed.
	while (offset < size) {
		if (*reserve(1) != 1) {
			throw InvalidInputException("Unsupported Core geometry WKB byte order");
		}
		const auto meta = read_u32();
		const auto type = (meta & 0xFFFF) % 1000;
		const auto flag = (meta & 0xFFFF) / 1000;
		if (type < 1 || type > 7 || flag > 3) {
			throw InvalidInputException("Unsupported Core geometry WKB type");
		}
		const uint32_t dimensions = 2 + (flag == 1 || flag == 3) + (flag == 2 || flag == 3);
		if (type == 1) {
			vertices(1, dimensions, true);
		} else if (type == 2) {
			vertices(read_u32(), dimensions, false);
		} else if (type == 3) {
			const auto rings = read_u32();
			if (rings > (size - offset) / 4) {
				throw InvalidInputException("Truncated Core geometry rings");
			}
			for (uint32_t ring = 0; ring < rings; ring++) {
				vertices(read_u32(), dimensions, false);
			}
		} else {
			read_u32(); // Core already validated collection member structure on ingestion.
		}
	}
}

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
