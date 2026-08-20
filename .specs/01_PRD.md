# PRD: duckdb-pintail (Pintail Spatial Extension)

## 1. Vision & Objectives
- **Target**: Build a high-performance, lightweight geospatial & spatial indexing extension for DuckDB.
- **Positioning**: Fill the critical gap in DuckDB's spatial ecosystem (lack of Geohash, S2, Quadkey, and lightweight spherical geodesy) and provide a seamless migration path for PostGIS users.
- **Distribution**: Target official inclusion in `duckdb/community-extensions`.

## 2. Target Persona & Use Cases
1. **PostGIS Migrators**: Users transitioning analytical workloads from PostgreSQL/PostGIS to DuckDB who require `ST_GeoHash` and spatial prefix matching.
2. **Geospatial & Mobility Engineers**: Engineers building spatial bucketing, geospatial clustering, trajectory analysis, and tile coordinate processing directly in DuckDB and GeoParquet.
3. **Cloud Native & WASM Pipelines**: Users running DuckDB in browser/WASM or lightweight lambda environments where heavy GIS libraries (GEOS/GDAL) are impractical.

## 3. Non-Goals (Scope Boundaries)
- **Do NOT re-implement heavy GIS geometry topology pipelines**: Leave complex polygon union/intersection overlays to `duckdb-spatial` (GEOS/GDAL).
- **Do NOT introduce heavy external C dependencies**: Keep compilation fast, portable, and Wasm-compatible.
