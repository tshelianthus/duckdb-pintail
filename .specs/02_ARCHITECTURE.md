# Architecture Specification

## 1. Tech Stack
- **Language**: **C++17** (matches DuckDB core & official extension-template; C++11 minimum per duckdb-spatial, C++17 preferred).
- **Build System**: **CMake** (>= 3.5) driven by DuckDB's `extension-ci-tools` makefiles via `make`.
- **Extension Basis**: the official `duckdb/extension-template` (C++), vendored as the repo scaffolding.
- **Core Dependencies (Tier 0)**: **none** — pure C++ bit/string math. (Tier 1 will vendor GEOS/PROJ under `third_party/`.)

## 2. Extension Skeleton (per extension-template)
- Entry point via `DUCKDB_CPP_EXTENSION_ENTRY(pintail, loader)` (no `DUCKDB_EXTENSION_MAIN` in library builds; uses the generated `pintail_extension` + `pintail_loadable_extension` targets).
- An `Extension` subclass (`PintailExtension`) exposing `Load`, `Name`, `Version`.
- `Load` delegates to a static `LoadInternal(ExtensionLoader &)` that registers every SQL function.
- Version macro (`EXT_VERSION_PINTAIL`) plumbed through `extension_config.cmake`.

## 3. Execution & Memory Architecture
- **Vectorized First**: every scalar function binds a `ScalarFunction` and runs through `UnaryExecutor::Execute` / `TernaryExecutor` / `BinaryExecutor` — batch operation over DuckDB `DataChunk`/`Vector`. No per-row scalar iteration.
- **Validity Masks**: NULLs propagate via DuckDB's validity mask; any `NULL` argument yields `NULL` without entering the algorithm body (DuckDB executor handles this automatically for simple-type executors).
- **Zero Panic / Zero Crash**: all validation (lat ∈ [-90, 90], lon ∈ [-180, 180], precision ∈ [1, 20], Base32 charset) raises DuckDB query errors via `InvalidInputException` / `OutOfRangeException`. Never `std::abort`, never UB, never uncaught C++ exceptions.

## 4. Directory Layout
```
duckdb-pintail/
├── .github/workflows/          # DuckDB Community CI matrix (MainDistributionPipeline.yml)
├── .specs/                     # SSD Specifications (this directory)
├── extension-ci-tools/         # DuckDB CI/build submodule
├── duckdb/                     # DuckDB core submodule (pinned)
├── third_party/                # (reserved) Tier-1 vendored GEOS/PROJ — empty in Tier 0
├── src/
│   ├── include/
│   │   └── pintail_extension.hpp   # PintailExtension class declaration
│   ├── pintail_extension.cpp       # DU... entry point + LoadInternal registration
│   └── geohash/               # (reserved) Tier-0 geohash implementation units
├── test/
│   └── sql/                   # SQLLogicTest cases
├── CMakeLists.txt             # build_static_extension + build_loadable_extension
├── extension_config.cmake     # duckdb_extension_load(pintail ...), version pin
├── vcpkg.json                 # (Tier 0: empty deps) ; Tier 1: + geos, proj
├── Makefile                   # EXT_NAME=pintail, includes extension-ci-tools makefile
└── README.md
```

## 5. GEOS / PROJ Integration Point (reserved for Tier 1)
- `CMakeLists.txt` will, in Tier 1, `FetchContent`/vendor GEOS & PROJ under `third_party/`, build them statically, and `target_link_libraries` them into both the static and loadable extension targets — the exact pattern `duckdb-spatial` uses.
- `vcpkg.json` gains `geos` and `proj` dependencies at that point.
- No architectural change to the extension skeleton is required; this is purely additive.
