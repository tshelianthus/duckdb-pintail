#pragma once

#include "duckdb.hpp"
// DuckDB 2.0 split the vector helpers out of vector.hpp.
#include "duckdb/common/identifier.hpp"
#include "duckdb/common/vector/constant_vector.hpp"
#include "duckdb/common/vector/flat_vector.hpp"
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/string_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"

namespace duckdb {

template <class T>
inline T *PintailMutableFlatData(Vector &vector) {
	return FlatVector::GetDataMutable<T>(vector);
}

inline Vector &PintailStructChild(Vector &vector, idx_t index) {
	return StructVector::GetEntries(vector)[index];
}

template <class FN>
inline void PintailForEachStructChild(Vector &vector, FN &&fn) {
	for (auto &child : StructVector::GetEntries(vector)) {
		fn(child);
	}
}

} // namespace duckdb
