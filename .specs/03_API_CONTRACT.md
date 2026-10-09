# API Contract Specification (Geohash parity revision)

All functions are registered into DuckDB's default catalog upon `LOAD pintail;`.
This contract is **language-agnostic**: the SQL surface below is identical regardless of
the C++ (current) implementation language. Do not invent names, parameters, or return
types not listed here.

---

### 1. `st_geohash`
- **Signature**:
  - `st_geohash(lat DOUBLE, lon DOUBLE, precision INTEGER) -> VARCHAR`
  - `st_geohash(lat DOUBLE, lon DOUBLE) -> VARCHAR` (Default precision = 12)
  - `st_geohash(geom GEOMETRY) -> VARCHAR`
  - `st_geohash(geom GEOMETRY, maxchars INTEGER) -> VARCHAR`
- **Behavior**:
  - Encodes `(lat, lon)` into a standard Geohash Base32 string (`0123456789bcdefghjkmnpqrstuvwxyz`).
  - Latitude valid range: `[-90.0, 90.0]`. Out of bounds raises `Out of Range Error`.
  - Longitude valid range: `[-180.0, 180.0]`. Out of bounds raises `Out of Range Error`.
  - Coordinate-overload precision valid range: `[1, 20]`, default 12. This is Pintail's
    encoding limit, not a universal Geohash maximum.
  - Geometry overloads return the smallest Geohash cell containing the **entire geometry**,
    within a maximum of 20 characters. A positive `maxchars` limits the result to at most that
    many characters; omitted or zero means automatic precision (up to 20). Negative or >20
    values raise `Invalid Input Error`. POINT and any coincident XY extent can reach 20 characters.
  - Supported Core types: POINT, LINESTRING, POLYGON, their MULTI variants and
    GEOMETRYCOLLECTION, including nested collections and Z/M variants. Only XY participates.
    Fully empty geometries/collections return NULL; empty members contribute no extent.
    Precision/CRS validation precedes empty detection for non-NULL inputs.
  - Geometry XY must be finite longitude/latitude within the ranges above. Z/M are ignored.
    Coordinates are always x=longitude, y=latitude. An untagged GEOMETRY asserts geographic XY.
    Tagged geometry must identify as EPSG:4326 or OGC:CRS84; other CRS tags raise
    `Invalid Input Error`. No CRS transformation or axis reordering occurs.
  - The extent is Cartesian: a line from -179 to +179 crosses most of the world. No shortest
    antimeridian arc or polar normalization is inferred. ±180 and ±90 are inclusive.
  - Cell boundaries are closed. If an extent fits two cells on a partition boundary, choose
    the upper (east/north) one. When no nonempty prefix covers it, return `''` (the world).
    Finite point coordinates choose east/north at exact partition midpoints.

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
- **Signatures**:
  - `st_geohash_bbox(hash VARCHAR) -> STRUCT(min_lat DOUBLE, min_lon DOUBLE, max_lat DOUBLE, max_lon DOUBLE)`
  - `st_geohash_bbox(hash VARCHAR, precision INTEGER) ->` the same STRUCT
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

### 6. `st_box2dfromgeohash`
- **Signatures**:
  - `st_box2dfromgeohash(hash VARCHAR) -> STRUCT(min_lat DOUBLE, min_lon DOUBLE, max_lat DOUBLE, max_lon DOUBLE)`
  - `st_box2dfromgeohash(hash VARCHAR, precision INTEGER) ->` the same STRUCT
- An alias of `st_geohash_bbox`, with identical fields and precision rules. DuckDB Core has
  no PostgreSQL BOX2D type. This compatible function name intentionally returns Pintail's
  existing STRUCT; it does not promise BOX2D casts, operators, or text formatting.

---

### Semantics shared by all functions
- **NULL propagation**: any `NULL` input ⇒ `NULL` output (standard SQL 3-valued logic).
- **Error style**: invalid inputs raise DuckDB query errors (`Invalid Input Error` / Out-of-range), never crash the process.
- **Decode inputs** (point, polygon, bbox and BOX2D alias): accept any length, including
  `''` (world). ASCII uppercase/lowercase Base32 are equivalent. Omitted or negative precision
  uses the full string; precision above the string length clamps to that length; zero decodes
  no characters and returns the world (center POINT(0 0), polygon or bbox). Only the consumed
  prefix is validated, so an invalid suffix is ignored at shorter precision. Embedded NUL and
  non-ASCII bytes in the consumed prefix are invalid. Very long strings can collapse the cell
  bounds under double rounding; no minimum nonzero cell width is promised.
- **Neighbors** retain their existing stricter input: lowercase Base32, lengths `[1, 20]`.
  Empty/world hashes have no finite-resolution neighbors. Invalid length/character raises
  `Invalid Input Error`.
- **NULL precision** deliberately propagates NULL for every Pintail overload; unlike PostGIS
  decode, it is not an alias for omitted precision.
- **Determinism**: all functions are deterministic and side-effect free.

---

## Breaking change

`st_pointfromgeohash` now returns `GEOMETRY(EPSG:4326)` instead of
`STRUCT(lat DOUBLE, lon DOUBLE)`. SQL that accesses `.lat` or `.lon` must migrate to the geometry
interface (for example `st_astext` or `st_aswkb`). No compatibility STRUCT overload is provided.


## PostGIS compatibility policy

The reference is PostGIS official documentation and source snapshot
`33904db915bb3c2ff0f23a69f047bfdb5b93fcc0` (2026-10-07).
Pintail follows the documented whole-geometry containment promise even with positive maxchars;
PostGIS's source currently encodes the bbox center at the requested positive length, which can
violate that promise. Pintail also caps encoding at 20, rejects negative geometry maxchars,
checks tagged CRS, keeps strict NULL propagation, and tags decode geometry EPSG:4326 (PostGIS
returns unknown SRID). These are deliberate differences, not full binary/API equivalence.
