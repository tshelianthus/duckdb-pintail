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
5. Full/shorter/clamped/negative/zero precision; empty and long strings; consumed-prefix
   validation and ASCII case-insensitive decode. Neighbors retain strict length/case rules.
6. NULL propagation for every overload, non-constant batch vectors, polar cells, and antimeridian cells.
7. Canonical decode vectors compare `st_astext` of both functions against DuckDB Core's
   exact WKT format. Cover at least PostGIS `c0w3h`, Wikipedia `ezs42` / `u4pru`,
   Shanghai `wtw3sj`, equator `s0000`, polar and antimeridian cells, and the contract
   maximum-length hash. Polygon rings must match the WKT vertex order
   (lower-left → upper-left → upper-right → lower-right → close).

DuckDB 1.5.0 and later provide native `GEOMETRY` in Core. These tests must not install, load, or link
DuckDB Spatial. Tier 0 directly emits standard WKB; GEOS/PROJ remain reserved for topology operations
and coordinate transformations.


## Release Compatibility Gate

The project workflow builds the same source against **DuckDB v1.5.5 and v1.5.6**.
Both version matrices and `Release compatibility gate` must succeed before community
submission. Repeat the checks on the final `main` commit after merging; the community
`repo.ref` must identify that verified release source.

Native SQL tests run where supported by the official toolchain. Build-only platform
jobs, including Wasm, must not be reported as successful SQL test runs. Local Debug
and Release suites supplement these checks and do not replace the dual-version CI.

See [community release gates](../docs/community/RELEASE.md) for the procedure.

## Two-Branch Testing (DuckDB Version Releases)

The project v1.5.5/v1.5.6 matrix uses one source revision. It does not require a
separate `repo.ref_next`.

If compatibility with an upcoming DuckDB release requires different source, maintain
separate stable-targeting and next-release branches. Set `repo.ref` to the stable
source and `repo.ref_next` to the next-release source, using fixed commit hashes.
The community workflow controls when the next target is built and the reference is
promoted; inspect the actual upstream jobs rather than assuming both targets ran.
PR body text such as `test_all_stable` / `test_all_main` is not a substitute for that
verification. A current `main` build does not guarantee compatibility with a later
stable release.

Extension binaries are specific to the DuckDB version they were compiled against.
The local descriptor is a v0.2.0 candidate; replace its development reference with
the final verified `main` commit before community submission.

## Whole-geometry encoding and differential gate

`geohash_postgis.test` covers every new overload without Spatial, empty and mixed/nested
collections, all seven Core types, Z/M, NULLs, CRS validation, invalid/nonfinite XY, precision
limits, global/polar/antimeridian and degenerate/boundary extents. It checks cell containment,
maximality at the next character and nonconstant/selected vectors crossing chunk boundaries.
`geohash_postgis_vectors.test` records official documentation examples and deterministic
vectors produced by the exact PostGIS source snapshot named in the API contract. No GPL
source is vendored; the temporary reference runner executes upstream algorithms separately.
These are source-level differential vectors, not a claim of running a PostgreSQL server.
Debug build + full SQLLogicTest suite is authoritative; release build/test also validates optimized
registration and execution. Neither substitutes for the future Community CI platform matrix.
