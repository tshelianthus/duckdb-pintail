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
- A C++17 compiler, CMake >= 3.8, and Make
- Git, for submodules

## SQL Functions

All functions are registered into the default catalog on `LOAD pintail;`. Any `NULL` argument yields `NULL`. Invalid inputs raise a DuckDB query error; they never crash the process.

| Function | Signature | Returns |
| :--- | :--- | :--- |
| `st_geohash` | `(lat DOUBLE, lon DOUBLE [, precision INTEGER])` or `(geom GEOMETRY [, maxchars INTEGER])` | `VARCHAR` |
| `st_pointfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | `GEOMETRY(EPSG:4326)` |
| `st_geomfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | `GEOMETRY(EPSG:4326)` |
| `st_geohash_bbox` | `(hash VARCHAR [, precision INTEGER])` | `STRUCT(min_lat DOUBLE, min_lon DOUBLE, max_lat DOUBLE, max_lon DOUBLE)` |
| `st_box2dfromgeohash` | `(hash VARCHAR [, precision INTEGER])` | Same bbox STRUCT (Core has no PostgreSQL BOX2D) |
| `st_geohash_neighbors` | `(hash VARCHAR)` | `VARCHAR[]` |

### `st_geohash`

Encodes `(lat, lon)` into a standard Geohash Base32 string (`0123456789bcdefghjkmnpqrstuvwxyz`).

- Latitude: `[-90, 90]`
- Longitude: `[-180, 180]`
- Precision: `[1, 20]`, default **12**
- Out-of-range coordinates (including NaN and ±Infinity) raise `Out of Range Error`
- Out-of-range precision raises `Invalid Input Error`

Geometry input returns the smallest cell containing the whole geometry, capped at 20 characters.
Omitted/zero `maxchars` selects automatic precision; positive values `[1,20]` cap the length.
Empty geometry returns NULL; global extents can return `''`. Core POINT/LINESTRING/POLYGON,
MULTI variants and collections are supported, including Z/M (XY only). Input uses x=longitude,
y=latitude; untagged geographic XY, EPSG:4326 and OGC:CRS84 are accepted. Other CRS tags are
rejected. No transformation or antimeridian reinterpretation occurs. Use Core `st_setcrs(g,
'EPSG:4326')` to attach known metadata when compact CRS type casts cannot be resolved without
a CRS provider; setting metadata assumes the coordinates already use that CRS.

### `st_pointfromgeohash`

Decodes a Geohash string to the center `POINT` of its cell.

### `st_geomfromgeohash`

Decodes a Geohash string to the boundary `POLYGON` of its cell. Geometry coordinates always use
`x = longitude`, `y = latitude`, with CRS `EPSG:4326`. Omitting `precision` uses the complete hash;
negative precision uses the full hash, positive precision is clamped to the hash length,
and zero selects the world cell. NULL precision returns NULL.

### `st_geohash_bbox`

Decodes a Geohash string to its cell bounding box.

### `st_geohash_neighbors`

Returns the 8 adjacent cells at the same resolution, ordered `[N, NE, E, SE, S, SW, W, NW]`. East/west wrap at the antimeridian; north/south wrap across the poles.

Decode functions accept empty (world), long and ASCII uppercase hashes; only the consumed
prefix must be valid Base32. Neighbors retain lowercase lengths `[1,20]`.
See [PostGIS parity and migration notes](docs/geohash-postgis-parity.md) for deliberate differences,
including NULL precision, encoding limits, CRS tags and whole-geometry containment.

## Examples

```sql
SELECT st_geohash(31.2304, 121.4737, 6);
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

DuckDB 1.5.0 and later provide native `GEOMETRY` in Core. Pintail generates WKB directly.

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

## DuckDB Release Maintenance

DuckDB community extensions are built for the latest stable DuckDB release only. When the next
release is near (see the [release calendar](https://duckdb.org/release_calendar.html)),
`duckdb/community-extensions` CI tests extensions against **both** the latest stable release and
the current `main` branch.

- **Compatible with both** → the new release has no impact (the common case).
- **Not compatible with both** → maintain two branches (stable / `main`) and record the commit
  hashes in `docs/community/description.yml` as `repo.ref` (stable) and `repo.ref_next` (`main`).
  Once the new version ships, `ref_next` is automatically promoted to `ref`.
- **Trigger**: any change to `description.yml` makes CI run against the latest stable and `main`.
- **Force on a PR**: add `test_all_stable: true` and `test_all_main: true` to the PR description.
- **Version locking**: extension binaries are bound to the DuckDB version they were compiled
  against (a `v0.9.2` binary cannot be used with `v0.10.3`).

Full details: `.specs/04_TESTING_SPEC.md` · [Official docs](https://duckdb.org/community_extensions/development)

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Issues

Please report bugs and feature requests at [github.com/tshelianthus/duckdb-pintail/issues](https://github.com/tshelianthus/duckdb-pintail/issues).
