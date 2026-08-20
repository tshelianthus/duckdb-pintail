# duckdb-pintail 🦆

[![DuckDB Community Extension](https://img.shields.io/badge/DuckDB-Community%20Extension-blue.svg)](https://duckdb.org/community_extensions/)
[![License: MIT/Apache-2.0](https://img.shields.io/badge/License-MIT%2FApache--2.0-green.svg)](LICENSE)

**Pintail** (`duckdb-pintail`) is a high-performance, lightweight geospatial and spatial indexing extension for DuckDB, designed to bring PostGIS-compatible capabilities and discrete global grid systems (Geohash, S2, Quadkey, Slippy Map Tiles) to DuckDB without heavy C/C++ GIS dependencies.

## Quick Start

### Installation (Once Published to Community)
```sql
INSTALL pintail FROM community;
LOAD pintail;
```

### Local Build & Testing
```bash
# Clone the repository
git clone https://github.com/your-org/duckdb-pintail.git
cd duckdb-pintail

# Configure & Build
make configure
make release

# Run tests
make test_release
```

## Features (v0.1.0 Roadmap)
- **Geohash Encoding & Decoding**: `st_geohash`, `st_pointfromgeohash`, `st_geohash_bbox`, `st_geohash_neighbors`
- **Zero Heavy C Dependencies**: Pure-math / Rust implementation, lightweight and fast to compile.
- **Full Vectorization**: Processes spatial chunks at native DuckDB speed.
