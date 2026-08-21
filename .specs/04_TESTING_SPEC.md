# Testing & Verification Specification

All test cases use standard SQLLogicTest format under `test/sql/`, executed by DuckDB's
`extension-ci-tools` test runner via `make test_debug`.

## Build System (C++)
- Extension must compile with a **C++17** toolchain through CMake + `make`.
- `make configure` obtains the pinned `duckdb` + `extension-ci-tools` submodules and preps the build.
- `make debug` produces `build/debug/extension/pintail/pintail.duckdb_extension`.

## Standing Gate: Extension Load Smoke (post-Phase 0)
Applies to every commit on `dev`/`main` after scaffolding. Feature completeness is irrelevant; the load path must stay green.

**Automatic verification (authoritative)**
1. `make configure` succeeds.
2. `make debug` succeeds and emits `build/debug/extension/pintail/pintail.duckdb_extension`.
3. `make test_debug` succeeds (runs `test/sql/00_load.test` which validates `require pintail` / loadability).

**Manual verification (optional)**
If the local `duckdb` CLI version matches the pinned target, the following should also succeed:
```sql
LOAD './build/debug/extension/pintail/pintail.duckdb_extension';
```
Extension identity: name `pintail` (no hyphens); no segfault/panic on load.

**Out of scope for this gate**: `INSTALL pintail FROM community;` (Phase 2) and any `st_*` behavior (operator matrix below).

## Mandatory Test Matrix for Each Operator
1. **Happy Path**: std coords (e.g. Shanghai `31.2304, 121.4737` -> `wtw3sj`).
2. **Boundary Cases**: Equator `(0,0)`; poles `(±90, 0)`; antimeridian `(0, ±180)`.
3. **Invalid Inputs**:
   - `st_geohash(100.0, 0.0, 5)` → Lat Out of Bounds.
   - `st_pointfromgeohash('invalid_chars!')` → Invalid Base32.
4. **NULL Handling**:
   - `st_geohash(NULL, 121.47, 5)` → `NULL`
   - `st_geohash(31.23, NULL, 5)` → `NULL`
   - `st_geohash(31.23, 121.47, NULL)` → `NULL`

## Native GEOMETRY Geohash Decode Gate

`test/sql/geohash_geometry.test` must run with DuckDB Spatial not loaded and cover:

1. `duckdb_extensions()` reports `spatial.loaded = false`.
2. Both decode functions return Core `GEOMETRY` with CRS `EPSG:4326`.
3. `st_pointfromgeohash` produces the exact cell center as `(longitude, latitude)`.
4. `st_geomfromgeohash` produces a closed five-vertex ring ordered lower-left, upper-left,
   upper-right, lower-right, lower-left.
5. Full and explicit shorter precision; lengths 1 and 20; invalid characters, empty and overlong
   strings; precision 0 and precision above input length.
6. NULL propagation for every overload, non-constant batch vectors, polar cells, and antimeridian cells.
7. Canonical decode vectors compare `st_astext` of both functions against DuckDB Core's
   exact WKT format. Cover at least PostGIS `c0w3h`, Wikipedia `ezs42` / `u4pru`,
   Shanghai `wtw3sj`, equator `s0000`, polar and antimeridian cells, and the contract
   maximum-length hash. Polygon rings must match the WKT vertex order
   (lower-left → upper-left → upper-right → lower-right → close).

DuckDB 1.5.0 and later provide native `GEOMETRY` in Core. These tests must not install, load, or link
DuckDB Spatial. Tier 0 directly emits standard WKB; GEOS/PROJ remain reserved for topology operations
and coordinate transformations.
