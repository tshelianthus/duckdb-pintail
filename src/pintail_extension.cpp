#define DUCKDB_EXTENSION_MAIN

#include "pintail_extension.hpp"
#include "pintail_geohash.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/common/vector_operations/binary_executor.hpp"
#include "duckdb/common/vector_operations/ternary_executor.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

namespace duckdb {

// ---------------------------------------------------------------------------
// Placeholder scalar (kept from Phase 0 as a load-path canary)
// ---------------------------------------------------------------------------
inline void PintailScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "pintail 🦆 " + name.GetString());
	});
}

// ---------------------------------------------------------------------------
// st_geohash(lat [, lon [, precision]])
//   st_geohash(lat, lon, precision) -> VARCHAR
//   st_geohash(lat, lon)            -> VARCHAR   (precision defaults to 12)
// ---------------------------------------------------------------------------
inline void StGeoHash3Fun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &lat = args.data[0];
	auto &lon = args.data[1];
	auto &precision = args.data[2];
	TernaryExecutor::Execute<double, double, int32_t, string_t>(
	    lat, lon, precision, result, args.size(), [&](double lat_v, double lon_v, int32_t prec) {
		    return StringVector::AddString(result, GeohashEncode(lat_v, lon_v, prec));
	    });
}

inline void StGeoHash2Fun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &lat = args.data[0];
	auto &lon = args.data[1];
	BinaryExecutor::Execute<double, double, string_t>(lat, lon, result, args.size(), [&](double lat_v, double lon_v) {
		return StringVector::AddString(result, GeohashEncode(lat_v, lon_v, 12));
	});
}

// ---------------------------------------------------------------------------
// st_pointfromgeohash(hash) -> STRUCT(lat DOUBLE, lon DOUBLE)
// ---------------------------------------------------------------------------
inline void StPointFromGeohashFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &hash = args.data[0];
	auto constant = hash.GetVectorType() == VectorType::CONSTANT_VECTOR;
	UnifiedVectorFormat hdata;
	hash.ToUnifiedFormat(args.size(), hdata);
	auto hash_ptr = hdata.GetData<string_t>();

	auto &entries = StructVector::GetEntries(result);
	auto &lat_child = *entries[0];
	auto &lon_child = *entries[1];
	auto lat_data = FlatVector::GetData<double>(lat_child);
	auto lon_data = FlatVector::GetData<double>(lon_child);

	idx_t count = constant ? 1 : args.size();
	for (idx_t i = 0; i < count; i++) {
		auto idx = hdata.sel->get_index(i);
		if (!hdata.validity.RowIsValid(idx)) {
			FlatVector::SetNull(result, i, true);
			continue;
		}
		auto bbox = GeohashDecodeBBox(hash_ptr[idx].GetString());
		lat_data[i] = (bbox.min_lat + bbox.max_lat) / 2.0;
		lon_data[i] = (bbox.min_lon + bbox.max_lon) / 2.0;
	}
	if (constant) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

// ---------------------------------------------------------------------------
// st_geohash_bbox(hash) -> STRUCT(min_lat, min_lon, max_lat, max_lon)
// ---------------------------------------------------------------------------
inline void StGeohashBBoxFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &hash = args.data[0];
	auto constant = hash.GetVectorType() == VectorType::CONSTANT_VECTOR;
	UnifiedVectorFormat hdata;
	hash.ToUnifiedFormat(args.size(), hdata);
	auto hash_ptr = hdata.GetData<string_t>();

	auto &entries = StructVector::GetEntries(result);
	auto min_lat_data = FlatVector::GetData<double>(*entries[0]);
	auto min_lon_data = FlatVector::GetData<double>(*entries[1]);
	auto max_lat_data = FlatVector::GetData<double>(*entries[2]);
	auto max_lon_data = FlatVector::GetData<double>(*entries[3]);

	idx_t count = constant ? 1 : args.size();
	for (idx_t i = 0; i < count; i++) {
		auto idx = hdata.sel->get_index(i);
		if (!hdata.validity.RowIsValid(idx)) {
			FlatVector::SetNull(result, i, true);
			continue;
		}
		auto bbox = GeohashDecodeBBox(hash_ptr[idx].GetString());
		min_lat_data[i] = bbox.min_lat;
		min_lon_data[i] = bbox.min_lon;
		max_lat_data[i] = bbox.max_lat;
		max_lon_data[i] = bbox.max_lon;
	}
	if (constant) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

// ---------------------------------------------------------------------------
// st_geohash_neighbors(hash) -> VARCHAR[]  (LIST of 8: N, NE, E, SE, S, SW, W, NW)
// ---------------------------------------------------------------------------
inline void StGeohashNeighborsFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &hash = args.data[0];
	auto constant = hash.GetVectorType() == VectorType::CONSTANT_VECTOR;
	UnifiedVectorFormat hdata;
	hash.ToUnifiedFormat(args.size(), hdata);
	auto hash_ptr = hdata.GetData<string_t>();

	auto list_entries = ListVector::GetData(result);

	idx_t count = constant ? 1 : args.size();
	for (idx_t i = 0; i < count; i++) {
		auto idx = hdata.sel->get_index(i);
		if (!hdata.validity.RowIsValid(idx)) {
			FlatVector::SetNull(result, i, true);
			continue;
		}
		std::string neighbors[8];
		GeohashNeighbors(hash_ptr[idx].GetString(), neighbors);
		list_entries[i].offset = ListVector::GetListSize(result);
		list_entries[i].length = 8;
		for (int32_t j = 0; j < 8; j++) {
			ListVector::PushBack(result, Value(neighbors[j]));
		}
	}
	if (constant) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
static LogicalType MakePointStructType() {
	child_list_t<LogicalType> children;
	children.emplace_back("lat", LogicalType::DOUBLE);
	children.emplace_back("lon", LogicalType::DOUBLE);
	return LogicalType::STRUCT(children);
}

static LogicalType MakeBBoxStructType() {
	child_list_t<LogicalType> children;
	children.emplace_back("min_lat", LogicalType::DOUBLE);
	children.emplace_back("min_lon", LogicalType::DOUBLE);
	children.emplace_back("max_lat", LogicalType::DOUBLE);
	children.emplace_back("max_lon", LogicalType::DOUBLE);
	return LogicalType::STRUCT(children);
}

static void LoadInternal(ExtensionLoader &loader) {
	// Phase 0 canary
	loader.RegisterFunction(
	    ScalarFunction("pintail", {LogicalType::VARCHAR}, LogicalType::VARCHAR, PintailScalarFun));

	// st_geohash
	loader.RegisterFunction(
	    ScalarFunction("st_geohash", {LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::INTEGER},
	                   LogicalType::VARCHAR, StGeoHash3Fun));
	loader.RegisterFunction(ScalarFunction("st_geohash", {LogicalType::DOUBLE, LogicalType::DOUBLE},
	                                       LogicalType::VARCHAR, StGeoHash2Fun));

	// st_pointfromgeohash
	loader.RegisterFunction(ScalarFunction("st_pointfromgeohash", {LogicalType::VARCHAR}, MakePointStructType(),
	                                      StPointFromGeohashFun));

	// st_geohash_bbox
	loader.RegisterFunction(ScalarFunction("st_geohash_bbox", {LogicalType::VARCHAR}, MakeBBoxStructType(),
	                                      StGeohashBBoxFun));

	// st_geohash_neighbors
	loader.RegisterFunction(ScalarFunction("st_geohash_neighbors", {LogicalType::VARCHAR},
	                                      LogicalType::LIST(LogicalType::VARCHAR), StGeohashNeighborsFun));
}

void PintailExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string PintailExtension::Name() {
	return "pintail";
}

std::string PintailExtension::Version() const {
#ifdef EXT_VERSION_PINTAIL
	return EXT_VERSION_PINTAIL;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(pintail, loader) {
	duckdb::LoadInternal(loader);
}
}
