# duckdb-pintail 🦆

[![DuckDB Community Extension](https://img.shields.io/badge/DuckDB-Community%20Extension-blue.svg)](https://duckdb.org/community_extensions/)
[![License: MIT/Apache-2.0](https://img.shields.io/badge/License-MIT%2FApache--2.0-green.svg)](LICENSE)

**Pintail** (`duckdb-pintail`) is a high-performance, lightweight geospatial and spatial indexing extension for DuckDB, bringing PostGIS-compatible capabilities and discrete global grid systems (Geohash, S2, Quadkey, Slippy Map Tiles) to DuckDB.

## Why C++?

DuckDB's canonical extension path is **C++** — the official `extension-template`, the CMake/vcpkg toolchain, and the whole CI matrix are C++-first. The PostGIS ecosystem (GEOS, GDAL, PROJ) is also native C/C++, so C++ gives us direct, header-level integration with zero FFI overhead. Pintail is built on the official C++ extension template.

## Quick Start

### Installation (Once Published to Community)
```sql
INSTALL pintail FROM community;
LOAD pintail;
```

### Local Build & Testing
```bash
git clone git@github.com:tshelianthus/duckdb-pintail.git
cd duckdb-pintail
git submodule update --init --recursive

# Configure & Build (targets DuckDB v1.5.5)
make debug

# Run tests (includes load smoke via test/sql/00_load.test)
make test_debug

# Manual CLI load (requires DuckDB CLI v1.5.5+; use -unsigned for local builds)
duckdb -unsigned
LOAD './build/debug/extension/pintail/pintail.duckdb_extension';
```

## Features (v0.1.0 Roadmap)
- **Geohash Encoding & Decoding**: `st_geohash`, `st_pointfromgeohash`, `st_geohash_bbox`, `st_geohash_neighbors`
- **Zero Heavy C++ Dependencies (Tier 0)**: pure bit-math C++ implementation, lightweight and fast to compile.
- **Full Vectorization**: processes spatial chunks at native DuckDB speed (batch Vector / DataChunk execution).
- **GEOS/PROJ Ready (Tier 1)**: architecture reserves a `third_party/` vendoring path for future `GEOMETRY` topology, mirroring the official `duckdb-spatial` approach.
