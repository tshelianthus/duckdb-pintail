# PRD: duckdb-pintail (Pintail Spatial Extension)

## 1. Vision & Objectives
- **Target**: Build a high-performance, lightweight geospatial & spatial indexing extension for DuckDB.
- **Positioning**: Fill the critical gap in DuckDB's spatial ecosystem (Geohash, S2, Quadkey, lightweight spherical geodesy) and provide a seamless migration path for PostGIS users.
- **Tech Stack**: **C++** — DuckDB's officially recommended extension language and the same choice as the PostGIS ecosystem (GEOS / GDAL / PROJ are all C/C++).
- **Distribution**: Target official inclusion in `duckdb/community-extensions`.

## 2. Rationale: Why C++
- **Official First-Class Path**: DuckDB's canonical extension template (`duckdb/extension-template`), toolchain (CMake, vcpkg), and CI (`extension-ci-tools`) are all C++-first. C++ is the only path that calls DuckDB's internal API with zero overhead.
- **Seamless GIS Ecosystem Reuse**: GEOS, GDAL, and PROJ — the spatial stack shared with PostGIS — are native C/C++ libraries. C++ enables direct, header-level integration with no FFI boundary or cross-language copies.
- **Industry Alignment**: PostGIS, the de-facto spatial standard, is C/C++ at its core. Matching that stack minimizes impedance mismatch.

## 3. Dependency Strategy (Layered / Pay-As-You-Go)
Pintail adopts a **tiered dependency model** to avoid paying for heavy GIS libraries when the MVP needs none of them, while keeping a clear, designed-in upgrade path that mirrors the official `duckdb-spatial` approach.

| Tier | Scope | Dependencies |
| :--- | :--- | :--- |
| **Tier 0 (MVP — Geohash grids)** | Pure bit-math grid encoding/decoding (`st_geohash`, `st_pointfromgeohash`, `st_geohash_bbox`, `st_geohash_neighbors`) | **Zero external deps** (pure C++ bit/string math). No GEOS/GDAL/PROJ, no OpenSSL. |
| **Tier 1 (future — geometry topology)** | `GEOMETRY` type, polygon overlay (`st_intersection`, `st_union`, …) | Vendor GEOS (and PROJ for CRS transforms) under `third_party/` and statically link, exactly as `duckdb-spatial` does. |

- **Reference implementation**: the official `duckdb-spatial` bundles GEOS/GDAL/PROJ under `third_party/` and statically links them, so end users install nothing beyond CMake + a C++ compiler. Pintail will follow the same pattern when Tier 1 features are added.
- **No wheel reinvention**: where a mature C++ library exists (GEOS/PROJ), Pintail will reuse it rather than reimplement. Where no such library applies (pure Geohash bit math), Pintail implements directly in C++.
- **Deployment guarantee**: Tier 0 keeps the `.duckdb_extension` binary tiny, wasm-portable, and fast to cross-compile on the community CI matrix.
- **CRS / axis-order convention**: Tier 1 geometry/CRS functions reserve an optional `always_xy BOOLEAN` argument (default `false`) to toggle between CRS-defined axis order (OGC/ISO default) and explicit `(lon, lat)` order for PostGIS/GeoJSON interoperability. See `.specs/03_API_CONTRACT.md` — it does **not** apply to Tier-0 grid functions whose `lat`/`lon` arguments are already explicit.

## 4. Target Persona & Use Cases
1. **PostGIS Migrators**: Move analytical workloads from PostgreSQL/PostGIS to DuckDB with `ST_GeoHash` and spatial prefix matching.
2. **Geospatial & Mobility Engineers**: Spatial bucketing, clustering, trajectory analysis, and tile coordinate processing in DuckDB and GeoParquet.
3. **Cloud Native & WASM Pipelines**: DuckDB in browser/WASM or lightweight lambda, where Tier-0 binary size and fast compiles matter; heavy GIS comes later via Tier 1.

## 5. Non-Goals (Scope Boundaries)
- **Do NOT re-implement heavy GIS topology** in Tier 0: leave polygon union/intersection overlays to `duckdb-spatial` (GEOS) until Tier 1, at which point GEOS is vendored rather than hand-rolled.
- **Do NOT reintroduce a second/parallel language toolchain**: after the C++ migration, Rust artifacts (Cargo.toml, Cargo.lock, `src/*.rs`, `.cargo/`) are removed. One language, one build system.
