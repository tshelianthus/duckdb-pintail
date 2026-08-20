#define DUCKDB_EXTENSION_MAIN

#include "pintail_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

namespace duckdb {

inline void PintailScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "pintail 🦆 " + name.GetString());
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	// Register a scalar function
	auto pintail_scalar_function =
	    ScalarFunction("pintail", {LogicalType::VARCHAR}, LogicalType::VARCHAR, PintailScalarFun);

	loader.RegisterFunction(pintail_scalar_function);
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
