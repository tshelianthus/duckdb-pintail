#include "pintail_extension.hpp"
#include "pintail_geohash.hpp"
#include "pintail_wkb.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/types/geometry.hpp"
#include "duckdb/common/types/geometry_crs.hpp"
#include "duckdb/planner/expression.hpp"
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
// Native GEOMETRY(EPSG:4326) geohash decode
// ---------------------------------------------------------------------------
static LogicalType Geometry4326() {
	return LogicalType::GEOMETRY("EPSG:4326");
}

static string_t PointFromDecodedHash(Vector &result, const string &hash, int32_t precision = -1) {
	auto bbox = GeohashDecodeBBox(hash, precision);
	return EncodeWkbPoint(result, bbox.min_lon + (bbox.max_lon - bbox.min_lon) / 2.0,
	                      bbox.min_lat + (bbox.max_lat - bbox.min_lat) / 2.0);
}

inline void PointFromGeohashFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<string_t, string_t>(args.data[0], result, args.size(), [&](string_t hash) {
		return PointFromDecodedHash(result, hash.GetString());
	});
}

inline void PointFromGeohashPrecisionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::Execute<string_t, int32_t, string_t>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t hash, int32_t precision) {
		    return PointFromDecodedHash(result, hash.GetString(), precision);
	    });
}

inline void GeomFromGeohashFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<string_t, string_t>(args.data[0], result, args.size(), [&](string_t hash) {
		return EncodeWkbPolygon(result, GeohashDecodeBBox(hash.GetString()));
	});
}

inline void GeomFromGeohashPrecisionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::Execute<string_t, int32_t, string_t>(
	    args.data[0], args.data[1], result, args.size(), [&](string_t hash, int32_t precision) {
		    return EncodeWkbPolygon(result, GeohashDecodeBBox(hash.GetString(), precision));
	    });
}

// Preserve CRS-bearing argument types instead of silently casting away the tag.
static unique_ptr<FunctionData> BindGeometryGeoHash(ClientContext &, ScalarFunction &function,
                                                  vector<unique_ptr<Expression>> &arguments) {
	if (arguments[0]->return_type.id() == LogicalTypeId::GEOMETRY) {
		function.arguments[0] = arguments[0]->return_type;
	}
	return nullptr;
}

static void ValidateGeoHashCRS(const LogicalType &type) {
	if (GeoType::HasCRS(type)) {
		const auto &crs = GeoType::GetCRS(type).GetIdentifier();
		if (crs != "EPSG:4326" && crs != "OGC:CRS84") {
			throw InvalidInputException("Geohash requires geographic XY with CRS EPSG:4326 or OGC:CRS84, got %s", crs);
		}
	}
}

static string_t EncodeGeometryGeoHash(Vector &result, const string_t &geometry, const LogicalType &type,
                                     int32_t maxchars, ValidityMask &validity, idx_t index) {
	if (maxchars < 0 || maxchars > MAX_GEOHASH_PRECISION) {
		throw InvalidInputException("Geohash geometry precision out of range: %d (must be within [0, 20])", maxchars);
	}
	ValidateGeoHashCRS(type);
	Geometry::GetType(geometry); // Validate Core's normalized root type and dimensionality.
	ValidateGeographicWkb(geometry);
	auto extent = GeometryExtent::Empty();
	if (Geometry::GetExtent(geometry, extent) == 0) {
		validity.SetInvalid(index);
		return string_t();
	}
	return StringVector::AddString(result, GeohashEncodeExtent(
	    {extent.y_min, extent.x_min, extent.y_max, extent.x_max}, maxchars));
}

inline void GeometryGeoHashFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
	    args.data[0], result, args.size(), [&](string_t geometry, ValidityMask &validity, idx_t index) {
		    return EncodeGeometryGeoHash(result, geometry, args.data[0].GetType(), 0, validity, index);
	    });
}

inline void GeometryGeoHashPrecisionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	BinaryExecutor::ExecuteWithNulls<string_t, int32_t, string_t>(
	    args.data[0], args.data[1], result, args.size(),
	    [&](string_t geometry, int32_t maxchars, ValidityMask &validity, idx_t index) {
		    return EncodeGeometryGeoHash(result, geometry, args.data[0].GetType(), maxchars, validity, index);
	    });
}

// STRUCT is a nested type rather than an executor physical scalar. Dispatch via an
// index vector, writing all four children in the executor callback, then share its
// layout/validity. CONSTANT parents and children must agree for slice/reuse safety.
static void ApplyNestedLayout(Vector &result, Vector &indices) {
	const bool constant = indices.GetVectorType() == VectorType::CONSTANT_VECTOR;
	result.SetVectorType(indices.GetVectorType());
	auto apply = [&](Vector &vector) {
		vector.SetVectorType(indices.GetVectorType());
		if (constant) {
			ConstantVector::SetNull(vector, ConstantVector::IsNull(indices));
		} else {
			FlatVector::SetValidity(vector, FlatVector::Validity(indices));
		}
	};
	apply(result);
	for (auto &child : StructVector::GetEntries(result)) {
		apply(*child);
	}
}

template <bool HAS_PRECISION>
static void GeoHashBBoxFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &children = StructVector::GetEntries(result);
	auto min_lat = FlatVector::GetData<double>(*children[0]);
	auto min_lon = FlatVector::GetData<double>(*children[1]);
	auto max_lat = FlatVector::GetData<double>(*children[2]);
	auto max_lon = FlatVector::GetData<double>(*children[3]);
	Vector indices(LogicalType::UBIGINT);
	auto emit = [&](string_t hash, int32_t precision, ValidityMask &, idx_t row) -> uint64_t {
		const auto bbox = GeohashDecodeBBox(hash.GetString(), precision);
		min_lat[row] = bbox.min_lat;
		min_lon[row] = bbox.min_lon;
		max_lat[row] = bbox.max_lat;
		max_lon[row] = bbox.max_lon;
		return row;
	};
	if constexpr (HAS_PRECISION) {
		BinaryExecutor::ExecuteWithNulls<string_t, int32_t, uint64_t>(args.data[0], args.data[1], indices,
		                                                           args.size(), emit);
	} else {
		UnaryExecutor::ExecuteWithNulls<string_t, uint64_t>(
		    args.data[0], indices, args.size(), [&](string_t hash, ValidityMask &mask, idx_t row) {
			    return emit(hash, -1, mask, row);
		    });
	}
	ApplyNestedLayout(result, indices);
}

inline void StGeohashNeighborsFun(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<string_t, list_entry_t>(args.data[0], result, args.size(), [&](string_t hash) {
		std::string neighbors[8];
		GeohashNeighbors(hash.GetString(), neighbors);
		list_entry_t entry {ListVector::GetListSize(result), 8};
		for (int32_t j = 0; j < 8; j++) {
			ListVector::PushBack(result, Value(neighbors[j]));
		}
		return entry;
	});
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

	ScalarFunction geohash_geom("st_geohash", {LogicalType::GEOMETRY()}, LogicalType::VARCHAR,
	                            GeometryGeoHashFunction, BindGeometryGeoHash);
	geohash_geom.SetFallible();
	loader.RegisterFunction(geohash_geom);
	ScalarFunction geohash_geom_prec("st_geohash", {LogicalType::GEOMETRY(), LogicalType::INTEGER},
	                                 LogicalType::VARCHAR, GeometryGeoHashPrecisionFunction, BindGeometryGeoHash);
	geohash_geom_prec.SetFallible();
	loader.RegisterFunction(geohash_geom_prec);

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

	// Core has no PostgreSQL BOX2D; the compatible name aliases the existing STRUCT.
	for (const auto *name : {"st_geohash_bbox", "st_box2dfromgeohash"}) {
		ScalarFunction bbox(name, {LogicalType::VARCHAR}, MakeBBoxStructType(), GeoHashBBoxFunction<false>);
		bbox.SetFallible();
		loader.RegisterFunction(bbox);
		ScalarFunction bbox_prec(name, {LogicalType::VARCHAR, LogicalType::INTEGER}, MakeBBoxStructType(),
		                         GeoHashBBoxFunction<true>);
		bbox_prec.SetFallible();
		loader.RegisterFunction(bbox_prec);
	}

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
