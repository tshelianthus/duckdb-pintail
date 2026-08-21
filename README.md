# duckdb-pintail 🦆

[![DuckDB Community Extension](https://img.shields.io/badge/DuckDB-Community%20Extension-blue.svg)](https://duckdb.org/community_extensions/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

**Pintail** is a lightweight geospatial indexing extension for DuckDB. The v0.1.0 release provides dependency-free Geohash encoding, decoding, bounding-box extraction, and adjacent-cell operations implemented in C++17.

## Installation

Once published to DuckDB Community Extensions:

```sql
INSTALL pintail FROM community;
LOAD pintail;
```

Until then, build from source (see below) and load the local binary:

```sql
LOAD './build/release/extension/pintail/pintail.duckdb_extension';
```

## Requirements

- DuckDB **v1.5.5** (pinned by this repository's `duckdb` submodule and CI matrix)
- A C++17 compiler, CMake, and Make
- Git, for submodules

## SQL Functions

All functions are registered into the default catalog on `LOAD pintail;`. Any `NULL` argument yields `NULL`. Invalid inputs raise a DuckDB query error; they never crash the process.

| Function | Signature | Returns |
| :--- | :--- | :--- |
| `st_geohash` | `(lat DOUBLE, lon DOUBLE [, precision INTEGER])` | `VARCHAR` |
| `st_pointfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | `GEOMETRY(EPSG:4326)` |
| `st_geomfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | `GEOMETRY(EPSG:4326)` |
| `st_geohash_bbox` | `(hash VARCHAR)` | `STRUCT(min_lat DOUBLE, min_lon DOUBLE, max_lat DOUBLE, max_lon DOUBLE)` |
| `st_geohash_neighbors` | `(hash VARCHAR)` | `VARCHAR[]` |

### `st_geohash`

Encodes `(lat, lon)` into a standard Geohash Base32 string (`0123456789bcdefghjkmnpqrstuvwxyz`).

- Latitude: `[-90, 90]`
- Longitude: `[-180, 180]`
- Precision: `[1, 20]`, default **12**
- Out-of-range coordinates (including NaN and ±Infinity) raise `Out of Range Error`
- Out-of-range precision raises `Invalid Input Error`

### `st_pointfromgeohash`

Decodes a Geohash string to the center `POINT` of its cell.

### `st_geomfromgeohash`

Decodes a Geohash string to the boundary `POLYGON` of its cell. Geometry coordinates always use
`x = longitude`, `y = latitude`, with CRS `EPSG:4326`. Omitting `precision` uses the complete hash;
specifying it decodes the first `precision` characters, where `precision` must be in
`[1, hash.length]`.

### `st_geohash_bbox`

Decodes a Geohash string to its cell bounding box.

### `st_geohash_neighbors`

Returns the 8 adjacent cells at the same resolution, ordered `[N, NE, E, SE, S, SW, W, NW]`. East/west wrap at the antimeridian; north/south wrap across the poles.

Decode and neighbor functions accept Geohash lengths `[1, 20]`. Empty strings, longer strings, and characters outside the Base32 alphabet raise `Invalid Input Error`.

## Examples

```sql
SELECT st_geohash(31.2304, 121.4737, 6);
-- wtw3sj

SELECT st_astext(st_pointfromgeohash('c0w3h'));
-- POINT (-126.01318359375 48.01025390625)

SELECT st_astext(st_geomfromgeohash('c0w3h'));
-- POLYGON ((-126.03515625 47.98828125, -126.03515625 48.0322265625, -125.9912109375 48.0322265625, -125.9912109375 47.98828125, -126.03515625 47.98828125))

SELECT st_geohash_bbox('ezs42');
-- {'min_lat': 42.583984375, 'min_lon': -5.625, 'max_lat': 42.626953125, 'max_lon': -5.5810546875}

SELECT st_geohash_neighbors('ezs42');
-- [ezs48, ezs49, ezs43, ezs41, ezs40, ezefp, ezefr, ezefx]
```

## Migration from the STRUCT return type

`st_pointfromgeohash` now returns native `GEOMETRY(EPSG:4326)`. This is a breaking API change:
queries using `(st_pointfromgeohash(grid_id)).lat` or `.lon` must migrate.

```sql
-- New interface returns GEOMETRY
SELECT st_pointfromgeohash(grid_id);

-- Inspect as WKT
SELECT st_astext(st_pointfromgeohash(grid_id));

-- Export standard WKB
SELECT st_aswkb(st_pointfromgeohash(grid_id));
```

DuckDB 1.5.0 and later provide native `GEOMETRY` in Core. Pintail generates WKB directly and does
not install, load, or link DuckDB Spatial. GEOS/PROJ remain reserved for future topology operations
or coordinate transformations.

## Local Build & Testing

```bash
git clone git@github.com:tshelianthus/duckdb-pintail.git
cd duckdb-pintail
git submodule update --init --recursive

make debug
make test_debug

make release
make test_release
```

Debug artifacts:

```text
build/debug/extension/pintail/pintail.duckdb_extension
```

Manual CLI load (requires a DuckDB CLI matching v1.5.5; use `-unsigned` for local builds):

```sql
duckdb -unsigned
LOAD './build/debug/extension/pintail/pintail.duckdb_extension';
SELECT st_geohash(31.2304, 121.4737, 6);
```

## Roadmap

- **v0.1.0 (current)**: Tier 0 Geohash grids — including native Core `GEOMETRY` point and polygon decoding. No Spatial, GEOS/GDAL/PROJ.
- **Later**: S2, Quadkey, and Slippy Map tile helpers; optional Tier 1 topology via vendored GEOS/PROJ.

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Issues

Please report bugs and feature requests at [github.com/tshelianthus/duckdb-pintail/issues](https://github.com/tshelianthus/duckdb-pintail/issues).
