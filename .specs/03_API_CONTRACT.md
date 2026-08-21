# API Contract Specification (v0.1.0 MVP)

All functions are registered into DuckDB's default catalog upon `LOAD pintail;`.
This contract is **language-agnostic**: the SQL surface below is identical regardless of
the C++ (current) implementation language. Do not invent names, parameters, or return
types not listed here.

---

### 1. `st_geohash`
- **Signature**:
  - `st_geohash(lat DOUBLE, lon DOUBLE, precision INTEGER) -> VARCHAR`
  - `st_geohash(lat DOUBLE, lon DOUBLE) -> VARCHAR` (Default precision = 12)
- **Behavior**:
  - Encodes `(lat, lon)` into a standard Geohash Base32 string (`0123456789bcdefghjkmnpqrstuvwxyz`).
  - Latitude valid range: `[-90.0, 90.0]`. Out of bounds raises `Out of Range Error`.
  - Longitude valid range: `[-180.0, 180.0]`. Out of bounds raises `Out of Range Error`.
  - Precision valid range: `[1, 20]` (standard Geohash maximum). Default is 12.

---

### 2. `st_pointfromgeohash`
- **Signatures**:
  - `st_pointfromgeohash(hash VARCHAR) -> GEOMETRY(EPSG:4326)`
  - `st_pointfromgeohash(hash VARCHAR, precision INTEGER) -> GEOMETRY(EPSG:4326)`
- **Behavior**:
  - Decodes a Geohash string to its cell-center `POINT`.
  - The coordinate order is `x = longitude`, `y = latitude`; CRS is `EPSG:4326`.
  - Invalid Base32 characters raise `Invalid Input Error`.

---

### 3. `st_geomfromgeohash`
- **Signatures**:
  - `st_geomfromgeohash(hash VARCHAR) -> GEOMETRY(EPSG:4326)`
  - `st_geomfromgeohash(hash VARCHAR, precision INTEGER) -> GEOMETRY(EPSG:4326)`
- **Behavior**:
  - Decodes a Geohash string to its cell-boundary `POLYGON`.
  - The closed exterior ring contains five vertices in this order: lower-left, upper-left,
    upper-right, lower-right, lower-left.
  - The coordinate order is `x = longitude`, `y = latitude`; CRS is `EPSG:4326`.

---

### 4. `st_geohash_bbox`
- **Signature**: `st_geohash_bbox(hash VARCHAR) -> STRUCT(min_lat DOUBLE, min_lon DOUBLE, max_lat DOUBLE, max_lon DOUBLE)`
- **Behavior**:
  - Decodes a Geohash string and returns its bounding-box boundaries.

---

### 5. `st_geohash_neighbors`
- **Signature**: `st_geohash_neighbors(hash VARCHAR) -> VARCHAR[]`
- **Behavior**:
  - Returns a `LIST` of 8 adjacent Geohash cells at the same resolution, ordered `[N, NE, E, SE, S, SW, W, NW]`.
  - Adjacency follows the geohash-js cylindrical grid convention: east/west wrap at the antimeridian,
    and north/south wrap across the poles (for example, the north neighbor of `upb` is `h00`).
  - Invalid Base32 characters raise `Invalid Input Error`.

---

### Semantics shared by all functions
- **NULL propagation**: any `NULL` input ⇒ `NULL` output (standard SQL 3-valued logic).
- **Error style**: invalid inputs raise DuckDB query errors (`Invalid Input Error` / Out-of-range), never crash the process.
- **Geohash input length**: decoding and neighbor functions accept lengths `[1, 20]`; other lengths raise
  `Invalid Input Error`.
- **Decode precision**: when omitted, `st_pointfromgeohash` and `st_geomfromgeohash` use the complete
  input Geohash. When specified, only the first `precision` characters are decoded. The valid range is
  `[1, hash.length]`; values outside that range raise `Invalid Input Error`. The input itself must still
  satisfy the shared Geohash length and Base32 validation rules.
- **Determinism**: all functions are deterministic and side-effect free.

---

## Breaking change

`st_pointfromgeohash` now returns `GEOMETRY(EPSG:4326)` instead of
`STRUCT(lat DOUBLE, lon DOUBLE)`. SQL that accesses `.lat` or `.lon` must migrate to the geometry
interface (for example `st_astext` or `st_aswkb`). No compatibility STRUCT overload is provided.

---

## Axis-Order Convention (`always_xy`) — reserved for Tier 1

### Purpose
`always_xy` is an **optional** `BOOLEAN` parameter that explicitly declares the coordinate-axis
order of input geometries. It is a reserved cross-cutting convention for the Tier-1 geometry / CRS
surface (such as coordinate transforms and WKT/GeoJSON (de)serialization); it is **not** applicable
to the Tier-0 grid functions above. Tier-0 Geohash geometry outputs have the fixed, explicit order
`x = longitude`, `y = latitude`.

### Contract
- **Signature form** (future functions may add this trailing optional argument):
  `some_func(geom …, always_xy BOOLEAN)` with default `always_xy = false`.
- **`always_xy = TRUE`**: treat all input coordinates as `(longitude, latitude)` order, i.e.
  `(x, y)` / `(lon, lat)` order.
- **`always_xy = FALSE`** (default): follow the axis order defined by the coordinate reference
  system (CRS), e.g. EPSG:4326 defaults to latitude-first, consistent with the OGC Simple Features
  / ISO 19125 standard.
- **Design intent**: maximize interoperability with `(lon, lat)`-native systems (e.g. PostGIS,
  GeoJSON) without breaking the standards-first principle at the core of the DuckDB Spatial
  extension.

### Non-goals (Tier 0)
- The five grid functions (`st_geohash`, `st_pointfromgeohash`, `st_geomfromgeohash`, `st_geohash_bbox`,
  `st_geohash_neighbors`) do **not** accept `always_xy`; adding it there would be semantically
  redundant and disruptive. Apply it only when Tier 1 introduces CRS-aware `GEOMETRY` functions.
