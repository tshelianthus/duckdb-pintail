# Architecture Specification

## 1. Tech Stack
- **Language**: Rust (2021/2024 edition) via DuckDB C Extension API (`extension-template-rs` / `quack-rs`).
- **Core Dependencies**:
  - `geohash`: Pure-math bitwise geohash encoding/decoding.
  - `geo-types` / `geo`: Lightweight geospatial geometry representations.

## 2. Execution & Memory Architecture
- **Vectorized Execution**: Functions bind directly to DuckDB Vector API. Flat Vectors and Dictionary Vectors must be supported with proper validity masks.
- **NULL Propagation**: Standard SQL 3-valued logic. Any `NULL` argument immediately outputs `NULL` without entering algorithmic logic.
- **Error Handling**: Functions validate inputs (lat in [-90, 90], lon in [-180, 180]) and raise DuckDB query errors on invalid inputs rather than panicking.

## 3. Directory Layout
```
duckdb-pintail/
├── .github/workflows/         # DuckDB Community CI matrix
├── .specs/                    # SSD Specifications
├── src/                       # Rust source code
│   ├── lib.rs                 # Extension entry point & registration
│   ├── geohash/               # Geohash scalar & table functions
│   └── common/                # Vector helpers & error conversion
├── test/sql/                  # SQLLogicTest test cases
├── Cargo.toml                 # Cargo dependencies
└── Makefile                   # Build automation
```
