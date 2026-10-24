#include "pintail_extension.hpp"
#include "pintail_geohash.hpp"
#include "pintail_wkb.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/types/geometry.hpp"
#include "duckdb/common/types/geometry_crs.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/common/vector_operations/binary_executor.hpp"
#include "duckdb/common/vector_operations/ternary_executor.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include <optional>

#if __has_include("duckdb/common/vector/struct_vector.hpp")
#define PINTAIL_DUCKDB_2 1
#include "duckdb/function/function_set.hpp"
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"
#include "duckdb/common/vector/string_vector.hpp"
#else
#define PINTAIL_DUCKDB_2 0
#endif

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
#if PINTAIL_DUCKDB_2
struct GeometryGeoHashBindData : FunctionData {
	explicit GeometryGeoHashBindData(LogicalType geometry_type_p) : geometry_type(std::move(geometry_type_p)) {
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<GeometryGeoHashBindData>(geometry_type);
	}
	bool Equals(const FunctionData &other) const override {
		return geometry_type == other.Cast<GeometryGeoHashBindData>().geometry_type;
	}

	LogicalType geometry_type;
};

static unique_ptr<FunctionData> BindGeometryGeoHash(BindScalarFunctionInput &input) {
	auto &arguments = input.GetArguments();
	if (arguments[0]->GetReturnType().id() == LogicalTypeId::GEOMETRY) {
		auto geometry_type = arguments[0]->GetReturnType();
		input.GetBoundFunction().GetArguments()[0] = geometry_type;
		return make_uniq<GeometryGeoHashBindData>(std::move(geometry_type));
	}
	return nullptr;
}

static const LogicalType &BoundGeometryType(ExpressionState &state, const Vector &argument) {
	auto &bound_function = state.expr.Cast<BoundFunctionExpression>();
	auto &logical_arguments = bound_function.Function().GetLogicalArguments();
	if (!logical_arguments.empty() && GeoType::HasCRS(logical_arguments[0])) {
		return logical_arguments[0];
	}
	if (bound_function.BindInfo()) {
		return bound_function.BindInfo()->Cast<GeometryGeoHashBindData>().geometry_type;
	}
	return argument.GetType();
}
#else
static unique_ptr<FunctionData> BindGeometryGeoHash(ClientContext &, ScalarFunction &function,
                                                  vector<unique_ptr<Expression>> &arguments) {
	if (arguments[0]->return_type.id() == LogicalTypeId::GEOMETRY) {
		function.arguments[0] = arguments[0]->return_type;
	}
	return nullptr;
}
#endif

static void ValidateGeoHashCRS(const LogicalType &type) {
	if (GeoType::HasCRS(type)) {
		const auto &crs = GeoType::GetCRS(type).GetIdentifier();
		if (crs != "EPSG:4326" && crs != "OGC:CRS84") {
			throw InvalidInputException("Geohash requires geographic XY with CRS EPSG:4326 or OGC:CRS84, got %s", crs);
		}
	}
}

static std::optional<string_t> EncodeGeometryGeoHash(Vector &result, const string_t &geometry, const LogicalType &type,
                                                     int32_t maxchars) {
	if (maxchars < 0 || maxchars > MAX_GEOHASH_PRECISION) {
		throw InvalidInputException("Geohash geometry precision out of range: %d (must be within [0, 20])", maxchars);
	}
	ValidateGeoHashCRS(type);
	Geometry::GetType(geometry); // Validate Core's normalized root type and dimensionality.
	ValidateGeographicWkb(geometry);
	auto extent = GeometryExtent::Empty();
	if (Geometry::GetExtent(geometry, extent) == 0) {
		return std::nullopt;
	}
	return StringVector::AddString(result, GeohashEncodeExtent(
	    {extent.y_min, extent.x_min, extent.y_max, extent.x_max}, maxchars));
}

inline void GeometryGeoHashFunction(DataChunk &args, ExpressionState &state, Vector &result) {
#if PINTAIL_DUCKDB_2
	UnifiedVectorFormat input;
	args.data[0].ToUnifiedFormat(input);
	auto geometries = UnifiedVectorFormat::GetData<string_t>(input);
	auto output = FlatVector::GetDataMutable<string_t>(result);
	auto &validity = FlatVector::ValidityMutable(result);
	for (idx_t row = 0; row < args.size(); row++) {
		const auto input_index = input.sel->get_index(row);
		if (!input.validity.RowIsValid(input_index)) {
			validity.SetInvalid(row);
			continue;
		}
		auto encoded = EncodeGeometryGeoHash(result, geometries[input_index], BoundGeometryType(state, args.data[0]), 0);
		if (!encoded) {
			validity.SetInvalid(row);
			continue;
		}
		output[row] = *encoded;
	}
#else
	UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
	    args.data[0], result, args.size(), [&](string_t geometry, ValidityMask &validity, idx_t index) {
		    auto encoded = EncodeGeometryGeoHash(result, geometry, args.data[0].GetType(), 0);
		    if (!encoded) {
			    validity.SetInvalid(index);
			    return string_t();
		    }
		    return *encoded;
	    });
#endif
}

inline void GeometryGeoHashPrecisionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
#if PINTAIL_DUCKDB_2
	UnifiedVectorFormat geometry_input;
	args.data[0].ToUnifiedFormat(geometry_input);
	auto geometries = UnifiedVectorFormat::GetData<string_t>(geometry_input);
	UnifiedVectorFormat precision_input;
	args.data[1].ToUnifiedFormat(precision_input);
	auto precisions = UnifiedVectorFormat::GetData<int32_t>(precision_input);
	auto output = FlatVector::GetDataMutable<string_t>(result);
	auto &validity = FlatVector::ValidityMutable(result);
	for (idx_t row = 0; row < args.size(); row++) {
		const auto geometry_index = geometry_input.sel->get_index(row);
		const auto precision_index = precision_input.sel->get_index(row);
		if (!geometry_input.validity.RowIsValid(geometry_index) ||
		    !precision_input.validity.RowIsValid(precision_index)) {
			validity.SetInvalid(row);
			continue;
		}
		auto encoded = EncodeGeometryGeoHash(result, geometries[geometry_index], BoundGeometryType(state, args.data[0]),
		                                    precisions[precision_index]);
		if (!encoded) {
			validity.SetInvalid(row);
			continue;
		}
		output[row] = *encoded;
	}
#else
	BinaryExecutor::ExecuteWithNulls<string_t, int32_t, string_t>(
	    args.data[0], args.data[1], result, args.size(),
	    [&](string_t geometry, int32_t maxchars, ValidityMask &validity, idx_t index) {
		    auto encoded = EncodeGeometryGeoHash(result, geometry, args.data[0].GetType(), maxchars);
		    if (!encoded) {
			    validity.SetInvalid(index);
			    return string_t();
		    }
		    return *encoded;
	    });
#endif
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
#if PINTAIL_DUCKDB_2
		apply(child);
#else
		apply(*child);
#endif
	}
}

template <bool HAS_PRECISION>
static void GeoHashBBoxFunction(DataChunk &args, ExpressionState &state, Vector &result) {
#if PINTAIL_DUCKDB_2
	auto &children = StructVector::GetEntries(result);
	auto min_lat = FlatVector::GetDataMutable<double>(children[0]);
	auto min_lon = FlatVector::GetDataMutable<double>(children[1]);
	auto max_lat = FlatVector::GetDataMutable<double>(children[2]);
	auto max_lon = FlatVector::GetDataMutable<double>(children[3]);
	UnifiedVectorFormat hash_format;
	args.data[0].ToUnifiedFormat(hash_format);
	auto hashes = UnifiedVectorFormat::GetData<string_t>(hash_format);
	UnifiedVectorFormat precision_format;
	const int32_t *precisions = nullptr;
	if constexpr (HAS_PRECISION) {
		args.data[1].ToUnifiedFormat(precision_format);
		precisions = UnifiedVectorFormat::GetData<int32_t>(precision_format);
	}
	auto &validity = FlatVector::ValidityMutable(result);
	for (idx_t row = 0; row < args.size(); row++) {
		const auto hash_index = hash_format.sel->get_index(row);
		if (!hash_format.validity.RowIsValid(hash_index)) {
			validity.SetInvalid(row);
			for (auto &child : children) FlatVector::ValidityMutable(child).SetInvalid(row);
			continue;
		}
		int32_t precision = -1;
		if constexpr (HAS_PRECISION) {
			const auto precision_index = precision_format.sel->get_index(row);
			if (!precision_format.validity.RowIsValid(precision_index)) {
				validity.SetInvalid(row);
				for (auto &child : children) FlatVector::ValidityMutable(child).SetInvalid(row);
				continue;
			}
			precision = precisions[precision_index];
		}
		const auto bbox = GeohashDecodeBBox(hashes[hash_index].GetString(), precision);
		min_lat[row] = bbox.min_lat;
		min_lon[row] = bbox.min_lon;
		max_lat[row] = bbox.max_lat;
		max_lon[row] = bbox.max_lon;
	}
	result.SetVectorType(VectorType::FLAT_VECTOR);
#else
	auto &children = StructVector::GetEntries(result);
	auto min_lat = FlatVector::GetData<double>(*children[0]);
	auto min_lon = FlatVector::GetData<double>(*children[1]);
	auto max_lat = FlatVector::GetData<double>(*children[2]);
	auto max_lon = FlatVector::GetData<double>(*children[3]);
	Vector indices {LogicalType(LogicalTypeId::UBIGINT)};
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
#endif
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
// Core is built as C++11 while the extension is C++17. Constructing from the
// enum IDs avoids ODR-using Core's static constexpr LogicalType aliases, which
// causes duplicate definitions when MinGW links the loadable extension.
static LogicalType MakeBBoxStructType() {
	child_list_t<LogicalType> children;
	children.emplace_back("min_lat", LogicalType(LogicalTypeId::DOUBLE));
	children.emplace_back("min_lon", LogicalType(LogicalTypeId::DOUBLE));
	children.emplace_back("max_lat", LogicalType(LogicalTypeId::DOUBLE));
	children.emplace_back("max_lon", LogicalType(LogicalTypeId::DOUBLE));
	return LogicalType::STRUCT(children);
}

struct DocumentedScalarOverload {
	ScalarFunction function;
	vector<string> parameter_names;
	const char *description;
	vector<string> examples;
};

static LogicalType DocumentedParameterType(const string &name) {
	if (name == "lat" || name == "lon") {
		return LogicalType(LogicalTypeId::DOUBLE);
	}
	if (name == "precision" || name == "maxchars") {
		return LogicalType(LogicalTypeId::INTEGER);
	}
	if (name == "geom") {
		return LogicalType::GEOMETRY();
	}
	return LogicalType(LogicalTypeId::VARCHAR);
}

static void RegisterDocumentedScalarSet(ExtensionLoader &loader, const string &name,
                                        vector<DocumentedScalarOverload> overloads,
                                        std::initializer_list<const char *> categories = {"geospatial", "geohash"}) {
#if PINTAIL_DUCKDB_2
	ScalarFunctionSet set{Identifier(name)};
#else
	ScalarFunctionSet set(name);
#endif
	CreateScalarFunctionInfo info(std::move(set));
	for (auto &overload : overloads) {
		FunctionDescription description;
		description.parameter_names = overload.parameter_names;
		for (auto &parameter_name : overload.parameter_names) {
			description.parameter_types.push_back(DocumentedParameterType(parameter_name));
		}
		description.description = overload.description;
		description.examples = overload.examples;
		for (auto category : categories) {
			description.categories.emplace_back(category);
		}
		info.functions.AddFunction(std::move(overload.function));
		info.descriptions.push_back(std::move(description));
	}
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	loader.RegisterFunction(std::move(info));
}

static void LoadInternal(ExtensionLoader &loader) {
	// Phase 0 canary
	RegisterDocumentedScalarSet(
	    loader, "pintail",
	    {{ScalarFunction("pintail", {LogicalType(LogicalTypeId::VARCHAR)},
	                     LogicalType(LogicalTypeId::VARCHAR), PintailScalarFun),
	      {"arg"},
	      "Internal load-path smoke function; returns a Pintail diagnostic string.",
	      {"pintail('load-smoke')"}}},
	    {"utility"});

	// st_geohash
	ScalarFunction geohash3("st_geohash", {LogicalType(LogicalTypeId::DOUBLE), LogicalType(LogicalTypeId::DOUBLE),
	                                    LogicalType(LogicalTypeId::INTEGER)},
	                        LogicalType(LogicalTypeId::VARCHAR), StGeoHash3Fun);
	geohash3.SetFallible();
	ScalarFunction geohash2("st_geohash", {LogicalType(LogicalTypeId::DOUBLE), LogicalType(LogicalTypeId::DOUBLE)},
	                        LogicalType(LogicalTypeId::VARCHAR),
	                        StGeoHash2Fun);
	geohash2.SetFallible();
	ScalarFunction geohash_geom("st_geohash", {LogicalType::GEOMETRY()}, LogicalType(LogicalTypeId::VARCHAR),
	                            GeometryGeoHashFunction, BindGeometryGeoHash);
	geohash_geom.SetFallible();
	ScalarFunction geohash_geom_prec("st_geohash", {LogicalType::GEOMETRY(),
	                                                 LogicalType(LogicalTypeId::INTEGER)},
	                                 LogicalType(LogicalTypeId::VARCHAR), GeometryGeoHashPrecisionFunction, BindGeometryGeoHash);
	geohash_geom_prec.SetFallible();
	RegisterDocumentedScalarSet(loader, "st_geohash",
	                            {{std::move(geohash3),
	                              {"lat", "lon", "precision"},
	                              "Encode latitude and longitude as a standard Geohash Base32 string.",
	                              {"st_geohash(31.2304, 121.4737, 6)"}},
	                             {std::move(geohash2),
	                              {"lat", "lon"},
	                              "Encode latitude and longitude using the default Geohash precision.",
	                              {"st_geohash(31.2304, 121.4737)"}},
	                             {std::move(geohash_geom),
	                              {"geom"},
	                              "Encode the smallest Geohash cell containing a geometry's complete XY extent.",
	                              {"st_geohash('LINESTRING(-126 48, -126.1 48.1)'::GEOMETRY)"}},
	                             {std::move(geohash_geom_prec),
	                              {"geom", "maxchars"},
	                              "Encode a geometry extent as a Geohash with an optional maximum character count.",
	                              {"st_geohash('POINT(121.4737 31.2304)'::GEOMETRY, 6)"}}});

	// st_pointfromgeohash
	ScalarFunction point_from_geohash("st_pointfromgeohash", {LogicalType(LogicalTypeId::VARCHAR)}, Geometry4326(),
	                                  PointFromGeohashFunction);
	point_from_geohash.SetFallible();
	ScalarFunction point_from_geohash_prec("st_pointfromgeohash", {LogicalType(LogicalTypeId::VARCHAR),
	                                                              LogicalType(LogicalTypeId::INTEGER)},
	                                       Geometry4326(), PointFromGeohashPrecisionFunction);
	point_from_geohash_prec.SetFallible();
	RegisterDocumentedScalarSet(loader, "st_pointfromgeohash",
	                            {{std::move(point_from_geohash),
	                              {"hash"},
	                              "Decode a Geohash to the center point of its cell.",
	                              {"st_pointfromgeohash('wtw3sj')"}},
	                             {std::move(point_from_geohash_prec),
	                              {"hash", "precision"},
	                              "Decode a Geohash prefix to the center point of the selected cell.",
	                              {"st_pointfromgeohash('wtw3sj-extra', 6)"}}});

	// st_geomfromgeohash
	ScalarFunction geom_from_geohash("st_geomfromgeohash", {LogicalType(LogicalTypeId::VARCHAR)}, Geometry4326(),
	                                 GeomFromGeohashFunction);
	geom_from_geohash.SetFallible();
	ScalarFunction geom_from_geohash_prec("st_geomfromgeohash", {LogicalType(LogicalTypeId::VARCHAR),
	                                                             LogicalType(LogicalTypeId::INTEGER)},
	                                      Geometry4326(), GeomFromGeohashPrecisionFunction);
	geom_from_geohash_prec.SetFallible();
	RegisterDocumentedScalarSet(loader, "st_geomfromgeohash",
	                            {{std::move(geom_from_geohash),
	                              {"hash"},
	                              "Decode a Geohash to the polygon boundary of its cell.",
	                              {"st_geomfromgeohash('c0w3h')"}},
	                             {std::move(geom_from_geohash_prec),
	                              {"hash", "precision"},
	                              "Decode a Geohash prefix to the polygon boundary of its cell.",
	                              {"st_geomfromgeohash('c0w3h-extra', 5)"}}});

	// Core has no PostgreSQL BOX2D; the compatible name aliases the existing STRUCT.
	for (const auto *name : {"st_geohash_bbox", "st_box2dfromgeohash"}) {
		ScalarFunction bbox(name, {LogicalType(LogicalTypeId::VARCHAR)}, MakeBBoxStructType(), GeoHashBBoxFunction<false>);
		bbox.SetFallible();
		const bool is_box2d_alias = string(name) == "st_box2dfromgeohash";
		ScalarFunction bbox_prec(name, {LogicalType(LogicalTypeId::VARCHAR),
		                               LogicalType(LogicalTypeId::INTEGER)},
		                         MakeBBoxStructType(),
		                         GeoHashBBoxFunction<true>);
		bbox_prec.SetFallible();
		RegisterDocumentedScalarSet(
		    loader, name,
		    {{std::move(bbox),
		      {"hash"},
		      is_box2d_alias ? "Decode a Geohash to a BOX2D-compatible latitude/longitude bounding box."
		                     : "Decode a Geohash to its latitude/longitude bounding box.",
		      {is_box2d_alias ? "st_box2dfromgeohash('ezs42')" : "st_geohash_bbox('ezs42')"}},
		     {std::move(bbox_prec),
		      {"hash", "precision"},
		      is_box2d_alias ? "Decode a Geohash prefix to a BOX2D-compatible latitude/longitude bounding box."
		                     : "Decode a Geohash prefix to its latitude/longitude bounding box.",
		      {is_box2d_alias ? "st_box2dfromgeohash('ezs42-extra', 5)" : "st_geohash_bbox('ezs42-extra', 5)"}}});
	}

	// st_geohash_neighbors
	ScalarFunction geohash_neighbors("st_geohash_neighbors", {LogicalType(LogicalTypeId::VARCHAR)},
	                                 LogicalType::LIST(LogicalType(LogicalTypeId::VARCHAR)), StGeohashNeighborsFun);
	geohash_neighbors.SetFallible();
	RegisterDocumentedScalarSet(
	    loader, "st_geohash_neighbors",
	    {{std::move(geohash_neighbors),
	      {"hash"},
	      "Return the eight adjacent Geohash cells in [N, NE, E, SE, S, SW, W, NW] order.",
	      {"st_geohash_neighbors('ezs42')"}}});
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
