#define DUCKDB_EXTENSION_MAIN

#include "pintail_extension.hpp"
#include "pintail_geohash.hpp"
#include "pintail_wkb.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/value.hpp"
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
		return StringVector::AddString(result, GeohashEncode(lat_v, lon_v, DEFAULT_GEOHASH_PRECISION));
	});
}

// ---------------------------------------------------------------------------
// Nested constant results
// DuckDB Flatten/Verify requires STRUCT children of a CONSTANT parent to also
// be CONSTANT_VECTOR. Parent-only SetVectorType leaves FLAT children, which
// misaligns after dictionary/slice reuse. Vector::Reference(Value) builds the
// nested CONSTANT layout correctly (including typed NULL).
// ---------------------------------------------------------------------------
static void ReferenceConstantVarcharResult(Vector &hash, Vector &result,
                                           Value (*make)(const LogicalType &, const string &)) {
	D_ASSERT(hash.GetVectorType() == VectorType::CONSTANT_VECTOR);
	if (ConstantVector::IsNull(hash)) {
		result.Reference(Value(result.GetType()));
		return;
	}
	auto s = ConstantVector::GetData<string_t>(hash)[0].GetString();
	result.Reference(make(result.GetType(), s));
}

static Value MakeBBoxFromGeohashValue(const LogicalType &type, const string &hash) {
	auto bbox = GeohashDecodeBBox(hash);
	return Value::STRUCT(type, {Value::DOUBLE(bbox.min_lat), Value::DOUBLE(bbox.min_lon), Value::DOUBLE(bbox.max_lat),
	                            Value::DOUBLE(bbox.max_lon)});
}

static Value MakeNeighborsFromGeohashValue(const LogicalType &type, const string &hash) {
	std::string neighbors[8];
	GeohashNeighbors(hash, neighbors);
	vector<Value> items;
	items.reserve(8);
	for (int32_t i = 0; i < 8; i++) {
		items.emplace_back(neighbors[i]);
	}
	return Value::LIST(ListType::GetChildType(type), std::move(items));
}

// ---------------------------------------------------------------------------
// Native GEOMETRY(EPSG:4326) geohash decode
// ---------------------------------------------------------------------------
static LogicalType Geometry4326() {
	return LogicalType::GEOMETRY("EPSG:4326");
}

static string_t PointFromDecodedHash(Vector &result, const string &hash) {
	auto bbox = GeohashDecodeBBox(hash);
	return EncodeWkbPoint(result, (bbox.min_lon + bbox.max_lon) / 2.0, (bbox.min_lat + bbox.max_lat) / 2.0);
}

static string_t PolygonFromDecodedHash(Vector &result, const string &hash) {
	return EncodeWkbPolygon(result, GeohashDecodeBBox(hash));
}

inline void PointFromGeohashFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<string_t, string_t>(args.data[0], result, args.size(), [&](string_t hash) {
		return PointFromDecodedHash(result, hash.GetString());
	});
}

inline void PointFromGeohashPrecisionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::Execute<string_t, int32_t, string_t>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t hash, int32_t precision) {
		    return PointFromDecodedHash(result, GeohashPrefix(hash.GetString(), precision));
	    });
}

inline void GeomFromGeohashFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<string_t, string_t>(args.data[0], result, args.size(), [&](string_t hash) {
		return PolygonFromDecodedHash(result, hash.GetString());
	});
}

inline void GeomFromGeohashPrecisionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::Execute<string_t, int32_t, string_t>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t hash, int32_t precision) {
		    return PolygonFromDecodedHash(result, GeohashPrefix(hash.GetString(), precision));
	    });
}

// ---------------------------------------------------------------------------
// st_geohash_bbox(hash) -> STRUCT(min_lat, min_lon, max_lat, max_lon)
// ---------------------------------------------------------------------------
inline void StGeohashBBoxFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &hash = args.data[0];
	if (hash.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		ReferenceConstantVarcharResult(hash, result, MakeBBoxFromGeohashValue);
		return;
	}

	UnifiedVectorFormat hdata;
	hash.ToUnifiedFormat(args.size(), hdata);
	auto hash_ptr = hdata.GetData<string_t>();

	auto &entries = StructVector::GetEntries(result);
	auto min_lat_data = FlatVector::GetData<double>(*entries[0]);
	auto min_lon_data = FlatVector::GetData<double>(*entries[1]);
	auto max_lat_data = FlatVector::GetData<double>(*entries[2]);
	auto max_lon_data = FlatVector::GetData<double>(*entries[3]);

	for (idx_t i = 0; i < args.size(); i++) {
		auto idx = hdata.sel->get_index(i);
		if (!hdata.validity.RowIsValid(idx)) {
			min_lat_data[i] = 0.0;
			min_lon_data[i] = 0.0;
			max_lat_data[i] = 0.0;
			max_lon_data[i] = 0.0;
			FlatVector::SetNull(result, i, true);
			continue;
		}
		auto bbox = GeohashDecodeBBox(hash_ptr[idx].GetString());
		min_lat_data[i] = bbox.min_lat;
		min_lon_data[i] = bbox.min_lon;
		max_lat_data[i] = bbox.max_lat;
		max_lon_data[i] = bbox.max_lon;
	}
}

// ---------------------------------------------------------------------------
// st_geohash_neighbors(hash) -> VARCHAR[]  (LIST of 8: N, NE, E, SE, S, SW, W, NW)
// ---------------------------------------------------------------------------
inline void StGeohashNeighborsFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &hash = args.data[0];
	if (hash.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		ReferenceConstantVarcharResult(hash, result, MakeNeighborsFromGeohashValue);
		return;
	}

	UnifiedVectorFormat hdata;
	hash.ToUnifiedFormat(args.size(), hdata);
	auto hash_ptr = hdata.GetData<string_t>();

	auto list_entries = ListVector::GetData(result);

	for (idx_t i = 0; i < args.size(); i++) {
		auto idx = hdata.sel->get_index(i);
		if (!hdata.validity.RowIsValid(idx)) {
			list_entries[i].offset = 0;
			list_entries[i].length = 0;
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
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
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
	ScalarFunction geohash3("st_geohash", {LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::INTEGER},
	                        LogicalType::VARCHAR, StGeoHash3Fun);
	geohash3.SetFallible();
	loader.RegisterFunction(geohash3);
	ScalarFunction geohash2("st_geohash", {LogicalType::DOUBLE, LogicalType::DOUBLE}, LogicalType::VARCHAR,
	                        StGeoHash2Fun);
	geohash2.SetFallible();
	loader.RegisterFunction(geohash2);

	// st_pointfromgeohash
	ScalarFunction point_from_geohash("st_pointfromgeohash", {LogicalType::VARCHAR}, Geometry4326(),
	                                  PointFromGeohashFunction);
	point_from_geohash.SetFallible();
	loader.RegisterFunction(point_from_geohash);
	ScalarFunction point_from_geohash_prec("st_pointfromgeohash", {LogicalType::VARCHAR, LogicalType::INTEGER},
	                                       Geometry4326(), PointFromGeohashPrecisionFunction);
	point_from_geohash_prec.SetFallible();
	loader.RegisterFunction(point_from_geohash_prec);

	// st_geomfromgeohash
	ScalarFunction geom_from_geohash("st_geomfromgeohash", {LogicalType::VARCHAR}, Geometry4326(),
	                                 GeomFromGeohashFunction);
	geom_from_geohash.SetFallible();
	loader.RegisterFunction(geom_from_geohash);
	ScalarFunction geom_from_geohash_prec("st_geomfromgeohash", {LogicalType::VARCHAR, LogicalType::INTEGER},
	                                      Geometry4326(), GeomFromGeohashPrecisionFunction);
	geom_from_geohash_prec.SetFallible();
	loader.RegisterFunction(geom_from_geohash_prec);

	// st_geohash_bbox
	ScalarFunction geohash_bbox("st_geohash_bbox", {LogicalType::VARCHAR}, MakeBBoxStructType(), StGeohashBBoxFun);
	geohash_bbox.SetFallible();
	loader.RegisterFunction(geohash_bbox);

	// st_geohash_neighbors
	ScalarFunction geohash_neighbors("st_geohash_neighbors", {LogicalType::VARCHAR},
	                                 LogicalType::LIST(LogicalType::VARCHAR), StGeohashNeighborsFun);
	geohash_neighbors.SetFallible();
	loader.RegisterFunction(geohash_neighbors);
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
