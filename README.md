# duckdb-pintail 🦆

[![DuckDB Community Extension](https://img.shields.io/badge/DuckDB-Community%20Extension-blue.svg)](https://duckdb.org/community_extensions/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

**Pintail** provides focused Geohash utilities for DuckDB: coordinate and geometry encoding,
cell-center and cell-polygon decoding, bounding boxes, and eight-direction neighbors.
It is implemented in C++17 without external runtime dependencies. Geometry functions use
DuckDB Core `GEOMETRY` and do not require DuckDB Spatial, GEOS, PROJ, or PostGIS.

## Release Status

The published community release is **v0.1.0**. This README describes the development
API, including additions being prepared for the next community release. These additions
require a source build until that release is published; see the [changelog](CHANGELOG.md).

The new Geohash support references PostGIS documentation and behavior. Pintail follows its own API
contract and does not promise full PostGIS compatibility.

## Installation

Install the currently published community release:

```sql
INSTALL pintail FROM community;
LOAD pintail;
```

To use the development API documented here, build the `dev` branch from source as described
below. Start the matching local CLI with unsigned-extension loading enabled:

```bash
./build/release/duckdb -unsigned
```

Then load the binary:

```sql
LOAD './build/release/extension/pintail/pintail.duckdb_extension';
```

Extension binaries must match the DuckDB version used to build them. A successful build for
one DuckDB version does not make its binary interchangeable with another version.

## Requirements

Community installation requires only DuckDB. Source builds additionally require:

- A C++17 compiler, CMake >= 3.8, Make, and Git.
- The pinned submodules; the local DuckDB target is **v1.5.5**.

## Coordinate Order

**Prefer GEOMETRY input for new queries**: it follows the standard **x = longitude,
y = latitude** convention and helps avoid ambiguity between two numeric arguments.
The coordinate overload keeps its existing **latitude, longitude** order.

Both examples encode the same Shanghai point:

```sql
-- Recommended: GEOMETRY uses longitude, latitude.
SELECT st_geohash('POINT(121.4737 31.2304)'::GEOMETRY, 6);
-- wtw3sj

-- Coordinate overload: latitude, longitude.
SELECT st_geohash(31.2304, 121.4737, 6);
-- wtw3sj
```

GEOMETRY encoding requires a development build until the next community release.
It uses DuckDB Core and needs only `LOAD pintail`.

## SQL Functions

Load the functions with `LOAD pintail;`. Any `NULL` argument returns `NULL`; invalid inputs raise a query error.

| Function | Signature | Returns |
| :--- | :--- | :--- |
| `st_geohash` | `(lat DOUBLE, lon DOUBLE [, precision INTEGER])` or `(geom GEOMETRY [, maxchars INTEGER])` | `VARCHAR` |
| `st_pointfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | `GEOMETRY(EPSG:4326)` |
| `st_geomfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | `GEOMETRY(EPSG:4326)` |
| `st_geohash_bbox` | `(hash VARCHAR [, precision INTEGER])` | `STRUCT(min_lat DOUBLE, min_lon DOUBLE, max_lat DOUBLE, max_lon DOUBLE)` |
| `st_box2dfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | Same bbox STRUCT (Core has no PostgreSQL BOX2D) |
| `st_geohash_neighbors` | `(hash VARCHAR)` | `VARCHAR[]` |

### `st_geohash`

Encodes a point or geometry as a Geohash string.

- Coordinates: latitude `[-90, 90]`, longitude `[-180, 180]`, in degrees.
- Coordinate `precision`: 1–20 characters; default **12**.
- Geometry `maxchars`: omitted or `0` selects automatic precision (up to 20);
  1–20 caps the output length. For lines and polygons, the result covers the whole
  geometry and may be shorter than the requested maximum.

Geometry input supports points, lines, polygons, multi-geometries, and collections.
Use geographic longitude/latitude coordinates; projected coordinates must be transformed
before calling Pintail. Empty geometry returns `NULL`.

### `st_pointfromgeohash`

Decodes a Geohash to its cell-center `POINT`, with CRS `EPSG:4326`.

### `st_geomfromgeohash`

Decodes a Geohash to its cell-boundary `POLYGON`, with CRS `EPSG:4326`.

### `st_geohash_bbox` / `st_box2dfromgeohash`

Decodes a Geohash to a bounding box with `min_lat`, `min_lon`, `max_lat`, and `max_lon`.
Both names return the same STRUCT.

For all four decode functions, omit `precision` to use the full hash, or supply a
positive value to decode a shorter prefix. Geometry output uses **longitude, latitude**.

### `st_geohash_neighbors`

Returns the 8 adjacent cells at the same resolution, ordered `[N, NE, E, SE, S, SW, W, NW]`.
East/west wrap at the antimeridian; north/south wrap across the poles.

For complete input rules and differences from PostGIS, see the
[API contract](.specs/03_API_CONTRACT.md).

## Examples

```sql
SELECT st_geohash('POINT(121.4737 31.2304)'::GEOMETRY, 6);
-- wtw3sj

SELECT st_geohash('LINESTRING(-126 48, -126.1 48.1)'::GEOMETRY);
-- c0w3

SELECT st_astext(st_pointfromgeohash('c0w3h'));
-- POINT (-126.01318359375 48.01025390625)

SELECT st_astext(st_geomfromgeohash('c0w3h'));
-- POLYGON ((-126.03515625 47.98828125, -126.03515625 48.0322265625, -125.9912109375 48.0322265625, -125.9912109375 47.98828125, -126.03515625 47.98828125))

SELECT st_geohash_bbox('ezs42');
-- {'min_lat': 42.583984375, 'min_lon': -5.625, 'max_lat': 42.626953125, 'max_lon': -5.5810546875}

SELECT st_geohash_neighbors('ezs42');
-- [ezs48, ezs49, ezs43, ezs41, ezs40, ezefp, ezefr, ezefx]
```

## Discover Functions from SQL

In development builds, inspect function signatures, descriptions, and examples directly
from SQL:

```sql
SELECT function_name, parameters, parameter_types, return_type,
       description, examples
FROM duckdb_functions()
WHERE function_name = 'st_geohash'
ORDER BY len(parameter_types), parameter_types::VARCHAR;
```

## Migration from Early STRUCT Builds

Early development builds returned a latitude/longitude STRUCT from `st_pointfromgeohash`.
The function returns native `GEOMETRY(EPSG:4326)` in v0.1.0 and current development builds.
Queries written against the earlier STRUCT interface must migrate from `.lat` / `.lon`:

```sql
-- New interface returns GEOMETRY
SELECT st_pointfromgeohash(grid_id);

-- Inspect as WKT
SELECT st_astext(st_pointfromgeohash(grid_id));

-- Export standard WKB
SELECT st_aswkb(st_pointfromgeohash(grid_id));
```

## Local Build & Testing

```bash
git clone --branch dev https://github.com/tshelianthus/duckdb-pintail.git
cd duckdb-pintail
git submodule update --init --recursive
make configure

make debug
make test_debug

make release
make test_release
```

Debug artifacts:

```text
build/debug/extension/pintail/pintail.duckdb_extension
```

Manual load using the matching CLI built alongside the extension:

```bash
./build/debug/duckdb -unsigned
```

```sql
LOAD './build/debug/extension/pintail/pintail.duckdb_extension';
SELECT st_geohash(31.2304, 121.4737, 6);
-- wtw3sj
```

## DuckDB Compatibility

The current development version is tested with **DuckDB v1.5.5 and v1.5.6**.
The same Pintail SQL API works on both versions, so you can use either version
without changing your queries.

Existing `st_geohash(latitude, longitude [, precision])` calls keep their argument
order and default precision. The new GEOMETRY overloads provide an additional way
to encode coordinates and geometries.

Use an extension binary built for your DuckDB version. See the
[changelog](CHANGELOG.md) for changes and supported versions in each release.

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Issues

Please report bugs and feature requests at [github.com/tshelianthus/duckdb-pintail/issues](https://github.com/tshelianthus/duckdb-pintail/issues).
