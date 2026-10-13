# Changelog

User-facing changes are recorded here. **Unreleased** changes are available from source
and have not yet been published to the DuckDB Community Extension repository.

## Unreleased — planned v0.2.0

### Added

- Geohash encoding from DuckDB Core `GEOMETRY`, referencing PostGIS documentation and
  behavior. Points, lines, polygons, multi-geometries, and collections are supported;
  the hash covers the entire geometry.
- `st_box2dfromgeohash` as an alias of `st_geohash_bbox`, returning the same bounding-box STRUCT.
- Function signatures, parameter names, descriptions, and examples discoverable through
  `duckdb_functions()` (issue #2).

### Changed

- Decode functions accept uppercase hashes and empty hashes (the world cell), with
  optional precision for bounding-box decoding. See the
  [API contract](.specs/03_API_CONTRACT.md) for the full behavior and PostGIS differences.
- Recommend GEOMETRY input for new queries to make longitude/latitude order explicit.
  The existing coordinate overload keeps latitude/longitude order (issue #3).

### Validation

- Add the Shanghai regression example: coordinate `(31.2304, 121.4737)` and geometry
  `POINT(121.4737 31.2304)` both encode to `wtw3sj` at precision 6 (issue #3).
- Add CI coverage for DuckDB v1.5.5 and v1.5.6, with both version matrices required
  by the release compatibility gate.

## v0.1.0

- Initial community release: coordinate Geohash encoding, cell-center and cell-polygon
  decoding to Core `GEOMETRY`, bounding-box decoding, and eight-direction neighbors.
