# Testing & Verification Specification

All test cases are written in standard SQLLogicTest format under `test/sql/`.

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
