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
