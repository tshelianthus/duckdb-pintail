# Testing & Verification Specification

All test cases are written in standard SQLLogicTest format under `test/sql/`.

### Standing Gate: Extension Load Smoke (Post–Phase 0)

Applies to every commit on `dev`/`main` after scaffolding is complete.
Feature completeness is irrelevant; the load path must stay green.

**Pass criteria**
1. `make debug` succeeds and emits
   `build/debug/extension/pintail/pintail.duckdb_extension`
2. In `duckdb -unsigned`, the following succeeds without error:
   ```sql
   LOAD './build/debug/extension/pintail/pintail.duckdb_extension';
   ```
3. Extension identity is correct: name `pintail` (no hyphens); no segfault / panic on load.

**Out of scope for this gate**
- `INSTALL pintail FROM community;` (Phase 2 only)
- Any `st_*` function behavior (covered by the operator matrix below)

**Minimal regression test**
Keep a SQLLogicTest (e.g. `test/sql/00_load.test`) that only `require pintail` / loads the
extension, so `make test_debug` fails if the entrypoint breaks.

### Mandatory Test Matrix for Each Operator:
1. **Happy Path**: Standard coordinates (e.g., Shanghai: `31.2304, 121.4737` -> `wtw3sj`).
2. **Boundary Cases**:
   - Equator `(0, 0)`
   - North Pole `(90, 0)` / South Pole `(-90, 0)`
   - Antimeridian `(0, 180)` / `(0, -180)`
3. **Invalid Inputs**:
   - `st_geohash(100.0, 0.0, 5)` -> Throws Latitude Out of Bounds Error
   - `st_pointfromgeohash('invalid_chars!')` -> Throws Invalid Base32 Error
4. **NULL Handling**:
   - `st_geohash(NULL, 121.47, 5)` -> `NULL`
   - `st_geohash(31.23, NULL, 5)` -> `NULL`
   - `st_geohash(31.23, 121.47, NULL)` -> `NULL`
