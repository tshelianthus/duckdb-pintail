# PRD: duckdb-pintail (Pintail Spatial Extension)

## 1. Vision & Objectives
- **Target**: Build a high-performance, lightweight Geohash extension for DuckDB.
- **Positioning**: Provide dependency-free Geohash encoding, decoding, bounding-box, adjacency, and native DuckDB Core `GEOMETRY` output.
- **Tech Stack**: **C++17**, using DuckDB's official extension toolchain.
- **Distribution**: Target official inclusion in `duckdb/community-extensions`.

## 2. Rationale: Why C++
- **Official First-Class Path**: DuckDB's canonical extension template (`duckdb/extension-template`), toolchain (CMake), and CI (`extension-ci-tools`) are C++-first.
- **Minimal Runtime Surface**: Geohash bit math and WKB encoding require no external runtime libraries or cross-language boundary.

## 3. Dependency Strategy
Pintail has **zero external dependencies**. Its public surface is implemented with pure C++ bit/string
math and direct WKB encoding. DuckDB 1.5.0 and later provide native `GEOMETRY` in Core, so Pintail
returns `GEOMETRY(EPSG:4326)` without installing, loading, or linking DuckDB Spatial.

This constraint keeps the extension small, portable, and straightforward to cross-compile on the
DuckDB Community Extensions CI matrix.

## 4. Target Persona & Use Cases
1. **PostGIS Migrators**: Move analytical workloads from PostgreSQL/PostGIS to DuckDB with `ST_GeoHash` and spatial prefix matching.
2. **Geospatial & Mobility Engineers**: Geohash bucketing and neighborhood analysis in DuckDB and GeoParquet.
3. **Cloud Native & WASM Pipelines**: DuckDB deployments where small binaries and fast cross-compilation matter.

## 5. Non-Goals (Scope Boundaries)
- **Do NOT implement geometry topology, CRS transformation, or general GIS operations**; use DuckDB Spatial for those capabilities.
- **Do NOT commit unapproved index systems or spatial-statistics ideas to the public roadmap**. They require a separate product and API review before entering this specification.
- **Do NOT reintroduce a second/parallel language toolchain**: after the C++ migration, Rust artifacts (Cargo.toml, Cargo.lock, `src/*.rs`, `.cargo/`) are removed. One language, one build system.

## Geohash parity acceptance

Cover the complete PostGIS Geohash family, including native Core GEOMETRY encoding for points,
lines, polygons, multis and collections. Return the smallest cell containing the entire geometry,
subject to the caller's maximum characters. Keep coordinate encoding and neighbors as Pintail
extensions. Bbox precision and a documented STRUCT-returning ST_Box2dFromGeoHash alias complete
the SQL surface without a PostgreSQL type emulation layer. See the API contract for intentional limits and differences.
