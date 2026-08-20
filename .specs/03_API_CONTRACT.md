# API Contract Specification (v0.1.0 MVP)

All functions reside in DuckDB's default catalog upon `LOAD pintail;`.

---

### 1. `st_geohash`
- **Signature**:
  - `st_geohash(lat DOUBLE, lon DOUBLE, precision INTEGER) -> VARCHAR`
  - `st_geohash(lat DOUBLE, lon DOUBLE) -> VARCHAR` (Default precision = 12)
- **Behavior**:
  - Encodes `(lat, lon)` into a standard Geohash Base32 string.
  - Latitude valid range: `[-90.0, 90.0]`. Out of bounds throws `Invalid Input Error`.
  - Longitude valid range: `[-180.0, 180.0]`. Out of bounds throws `Invalid Input Error`.
  - Precision valid range: `[1, 12]`. Default is 12.

---

### 2. `st_pointfromgeohash`
- **Signature**: `st_pointfromgeohash(hash VARCHAR) -> STRUCT(lat DOUBLE, lon DOUBLE)`
- **Behavior**:
  - Decodes a Geohash string and returns the center coordinate struct `{lat: DOUBLE, lon: DOUBLE}`.
  - Invalid Base32 characters throw `Invalid Input Error`.

---

### 3. `st_geohash_bbox`
- **Signature**: `st_geohash_bbox(hash VARCHAR) -> STRUCT(min_lat DOUBLE, min_lon DOUBLE, max_lat DOUBLE, max_lon DOUBLE)`
- **Behavior**:
  - Decodes a Geohash string and returns its bounding box boundaries.

---

### 4. `st_geohash_neighbors`
- **Signature**: `st_geohash_neighbors(hash VARCHAR) -> VARCHAR[]`
- **Behavior**:
  - Returns a LIST of 8 adjacent Geohash cells at the same resolution: `[N, NE, E, SE, S, SW, W, NW]`.
