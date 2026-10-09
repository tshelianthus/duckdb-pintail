# Architecture Specification

## 1. Tech Stack
- **Language**: **C++17** for both Pintail targets. Set the directory-local CMake standard explicitly;
  do not rely on a cache entry inherited from Core (which can remain C++11).
- **Build System**: **CMake** (>= 3.8, for C++17 standard support) driven by DuckDB's `extension-ci-tools` makefiles via `make`.
- **Extension Basis**: the official `duckdb/extension-template` (C++), vendored as the repo scaffolding.
- **External Dependencies**: **none** — pure C++ bit/string math and direct WKB encoding.
  DuckDB 1.5.0 and later provide native `GEOMETRY` in Core, so Pintail may return
  `GEOMETRY(EPSG:4326)` without installing, loading, or linking DuckDB Spatial.

## 2. Extension Skeleton (per extension-template)
- Entry point via `DUCKDB_CPP_EXTENSION_ENTRY(pintail, loader)` (no `DUCKDB_EXTENSION_MAIN` in library builds; uses the generated `pintail_extension` + `pintail_loadable_extension` targets).
- An `Extension` subclass (`PintailExtension`) exposing `Load`, `Name`, `Version`.
- `Load` delegates to a static `LoadInternal(ExtensionLoader &)` that registers every SQL function.
- Version macro (`EXT_VERSION_PINTAIL`) plumbed through `extension_config.cmake`.

## 3. Execution & Memory Architecture
- **Vectorized First**: every scalar function binds a `ScalarFunction` and runs through `UnaryExecutor::Execute` / `TernaryExecutor` / `BinaryExecutor` — batch operation over DuckDB `DataChunk`/`Vector`. No per-row scalar iteration.
- **Validity Masks**: NULLs propagate via DuckDB's validity mask; any `NULL` argument yields `NULL` without entering the algorithm body (DuckDB executor handles this automatically for simple-type executors).
- **Zero Panic / Zero Crash**: all validation (lat ∈ [-90, 90], lon ∈ [-180, 180], encoding precision limits, consumed-prefix Base32 charset) raises DuckDB query errors via `InvalidInputException` / `OutOfRangeException`. Never `std::abort`, never UB, never uncaught C++ exceptions.

## 4. Directory Layout
```
duckdb-pintail/
├── .github/workflows/          # DuckDB Community CI matrix (MainDistributionPipeline.yml)
├── .specs/                     # SSD Specifications (this directory)
├── extension-ci-tools/         # DuckDB CI/build submodule
├── duckdb/                     # DuckDB core submodule (pinned)
├── src/
│   ├── include/
│   │   ├── pintail_extension.hpp   # PintailExtension class declaration
│   │   ├── pintail_geohash.hpp     # dependency-free Geohash algorithms
│   │   └── pintail_wkb.hpp         # dependency-free WKB encoder interface
│   ├── pintail_extension.cpp       # DU... entry point + LoadInternal registration
│   └── geohash/
│       └── pintail_wkb.cpp          # Little-Endian POINT/POLYGON WKB
├── test/
│   └── sql/                   # SQLLogicTest cases
├── CMakeLists.txt             # build_static_extension + build_loadable_extension
├── extension_config.cmake     # duckdb_extension_load(pintail ...), version pin
├── vcpkg.json                 # no external dependencies
├── Makefile                   # EXT_NAME=pintail, includes extension-ci-tools makefile
└── README.md
```

## 5. Dependency Boundary
- DuckDB Core's native `GEOMETRY` stores standard WKB and provides basic inspection functions such as
  `ST_AsText`, `ST_AsWKB`, and `ST_CRS`. Pintail writes WKB directly into DuckDB-managed result-vector
  memory and declares `LogicalType::GEOMETRY("EPSG:4326")`.
- Pintail does not vendor or link GEOS, PROJ, GDAL, or DuckDB Spatial.
- Geometry topology, coordinate transformation, and general GIS processing remain outside this
  extension's architecture and should be handled by DuckDB Spatial.

## Core geometry input and nested results

Use Core Geometry::GetType/GetExtent on normalized WKB, with a bounded byte reader solely to
validate finite XY vertices that an extent reduction might otherwise hide (NaN mixed with valid
vertices). Preserve the input CRS type at bind time and validate its identifier without PROJ. Core
ST_SetCRS can attach compact identifiers without a provider; direct SQL type casts may require
a provider or a complete CRS definition. No provider is registered or loaded by Pintail.
The coverage algorithm descends the longitude/latitude grid while the closed extent fits,
retaining complete Base32 characters up to 20. This is an implementation choice, not the SQL
contract. Every SQL row is dispatched through UnaryExecutor/BinaryExecutor/TernaryExecutor;
empty geometry sets output validity via ExecuteWithNulls. Nested STRUCT children share the
executor's constant/flat layout and validity; LIST offsets are written by UnaryExecutor.
