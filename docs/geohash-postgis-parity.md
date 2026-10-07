# Geohash: PostGIS reference and Pintail decisions

Reference checked 2026-10-07. This report concerns only Geohash. Earlier general GIS gap
research is unchanged and is not an approved expansion of the Pintail API.

## Primary evidence

- Official [ST_GeoHash](https://postgis.net/docs/ST_GeoHash.html),
  [ST_PointFromGeoHash](https://postgis.net/docs/ST_PointFromGeoHash.html),
  [ST_GeomFromGeoHash](https://postgis.net/docs/ST_GeomFromGeoHash.html),
  [ST_Box2dFromGeoHash](https://postgis.net/docs/ST_Box2dFromGeoHash.html).
- Exact upstream snapshot `33904db915bb3c2ff0f23a69f047bfdb5b93fcc0`:
  [algorithms](https://github.com/postgis/postgis/blob/33904db915bb3c2ff0f23a69f047bfdb5b93fcc0/liblwgeom/lwalgorithm.c)
  (`geohash_point`, `decode_geohash_bbox`, `lwgeom_geohash_precision`, `lwgeom_geohash`),
  [decode wrappers](https://github.com/postgis/postgis/blob/33904db915bb3c2ff0f23a69f047bfdb5b93fcc0/postgis/lwgeom_in_geohash.c),
  [SQL declarations](https://github.com/postgis/postgis/blob/33904db915bb3c2ff0f23a69f047bfdb5b93fcc0/postgis/postgis.sql.in).
- Local DuckDB v1.5.5 Core `geometry.hpp`, `geometry.cpp`, `geometry_crs.hpp` and
  `geometry_functions.cpp` establish normalized WKB, GetType/GetExtent and type-level CRS.
  Core ST_SetCRS can attach compact CRS metadata without loading a CRS provider; SQL
  GEOMETRY type casts using compact identifiers can require such a provider.
  Core has no PostgreSQL BOX2D type. Spatial's separate BOX_2D alias is not a Core dependency.

Upstream source is inspected as evidence; no GPL implementation is copied into Pintail.
The reference runner is temporary and separate from the extension. Public behavior comes
from `.specs/03_API_CONTRACT.md`, not from an internal precision-selection algorithm.

## Function surface

| Function | PostGIS | Pintail before this work | Revised Pintail |
| --- | --- | --- | --- |
| ST_GeoHash | geometry, optional integer maxchars; text | lat, lon, optional precision; VARCHAR | Both coordinate and Core GEOMETRY overloads; VARCHAR |
| ST_PointFromGeoHash | text, optional precision; geometry POINT | VARCHAR, optional precision; Core POINT | Same overloads, expanded decode semantics |
| ST_GeomFromGeoHash | text, optional precision; geometry POLYGON | VARCHAR, optional precision; Core POLYGON | Same overloads, expanded decode semantics |
| ST_Box2dFromGeoHash | text, optional precision; BOX2D | Absent; st_geohash_bbox(hash) STRUCT | Compatible name, existing lat/lon STRUCT, plus bbox precision |
| st_geohash_neighbors | No equivalent in this family | Eight cells, cylindrical wrap | Preserved Pintail extension |

## Boundary and migration matrix

| Aspect | PostGIS reference behavior | Revised Pintail / deliberate difference |
| --- | --- | --- |
| POINT default | 20 characters | Geometry default 20; coordinate default remains 12 |
| Encoding maximum | Automatic up to 20; explicit positive value is passed through, even >20 | All encoding capped at 20; larger request is an error |
| Geometry maxchars=0 | Automatic extent-dependent precision | Same |
| Geometry maxchars<0 | Automatic in source | Error in Pintail; omitted/zero is explicit automatic mode |
| Non POINT default | Documented as smallest cell covering geometry | Same public guarantee, with 20-character cap |
| Non POINT positive maxchars | Docs promise coverage; source instead encodes bbox center at requested length | Always preserve whole-feature coverage, then apply cap; may return fewer characters |
| Degenerate XY extent | Automatic precision treats any coincident extent as a point | Up to 20, including duplicate-point lines and multis |
| Decode precision omitted / negative | Full input | Same |
| Decode precision > length | Clamp to input length | Same; previously an error |
| Decode precision=0 | Global bbox, center (0,0) | Same; previously an error |
| Empty hash | Global cell in decoder | Same; also permits roundtrip of global geometry hash |
| Long hash | No 20-character decoder limit; bounds may collapse by rounding | Same; previously rejected |
| Character validation | ASCII uppercase accepted; consumed prefix only | Same; previously lowercase/full-string only |
| Invalid consumed bytes | Error; C-string implementation has NUL caveats | Error, including embedded NUL and non-ASCII; no locale dependency |
| NULL geometry/hash | NULL | Same |
| NULL encoding maxchars | NULL (STRICT) | Same |
| NULL decode precision | Use full hash (wrappers handle NULL as omitted) | NULL under existing Pintail all-argument propagation policy |
| Empty geometry / collections | Extent failure returns NULL | NULL; empty members ignored in mixed collections |
| Input CRS | Requires geographic XY by documentation; encoder checks bounds, not SRID | Untagged geographic XY or EPSG:4326/OGC:CRS84 identifiers only; reject other tags |
| Decode CRS | Unknown SRID in point wrapper and BOX2D cast | Core GEOMETRY(EPSG:4326) retained; output XY always lon/lat |
| Z/M | Hash uses XY | Same for supported linear Core types; Z/M ignored |
| Curves | PostGIS supports curved geometries | Core supports seven linear geometry types; unsupported curves fail in Core input parsing |
| Pole coordinates | Inclusive ±90 | Same; no latitude normalization |
| Antimeridian | Cartesian extent, no shortest-arc interpretation | Same; ±180 distinct coordinates, inclusive |
| Exact cell boundary | Point chooses east/north; extent precision uses closed bounds | Closed coverage and east/north tie choice; does not blindly encode center on an outer edge |
| Nonfinite XY | Not a consistently safe public upstream contract | Validate every nonempty vertex; NaN/Infinity produce DuckDB errors |
| BOX2D result | PostgreSQL custom type / casts / operators | Plain existing STRUCT; no BOX2D binary, casts, operators, or BOX text emulation |
| Neighbors | Not provided | Lowercase hash length 1..20, order N/NE/E/SE/S/SW/W/NW; east/west and pole wrap |

At very high precision, binary64 cell bounds can become equal. Polygon output may consequently
be degenerate, as in the official long-hash example. This does not imply topological validation.
Geometry with any actual nonfinite XY is rejected even when other vertices would mask it in an
extent reduction. Fully NaN POINT vertices are Core's representation of empty points.

## Verification provenance

SQLLogicTest includes the documented `POINT(-126 48)`, line `(-126 48,-126.1 48.1)` and
`9qqj7nmxncgyy4d0dbxqz0` examples, plus deterministic point/extent/decode differential vectors.
The vectors are evaluated using the exact upstream C algorithms compiled in a temporary isolated
reference runner, with only allocator/error/GBOX stubs. They are source-level comparisons, not
PostgreSQL integration tests. Random seed and source digest are recorded in the vector test; the generator refuses a
source digest that differs from the pinned snapshot. The fixture contains 880 comparisons.

Coverage and maximality properties are checked independently: the returned bbox contains every
extent corner, and (unless at the requested cap) none of its 32 next-character children contains
the whole extent. This protects the public promise where the upstream positive-maxchars path or
boundary-center choice differs. Error/NULL cases, type-level CRS and SQL registration are verified
in DuckDB SQLLogicTest. No external runtime dependency is introduced.

## Local validation record

- TDD red run: new overloads and revised decoder rules failed before implementation.
- Debug: `make debug` and `make test_debug` passed all 5 SQLLogicTest files,
  1149 assertions, with Core's AddressSanitizer/UndefinedBehaviorSanitizer enabled.
- Both Pintail targets compile with `-std=c++17`; Core's inherited C++11 cache no longer
  overrides the extension directory.
- Direct unsigned load of the built debug artifact passed; Spatial remained unloaded.
- Fixture regeneration was byte-for-byte identical to the checked-in test vectors.
- Optimized: `make release` and `make test_release` passed the same 5 files / 1149 assertions.
- Final debug/release build-and-test chains passed after the CMake minimum was updated to 3.8.

Remaining verification limits: no live PostgreSQL server, no Community CI cross-platform/main
matrix in this local task. The upstream snapshot is current source, not a promise that every
released PostGIS version shares all edge behavior. Long-hash degenerate cells and deliberate
API differences in the matrix are contractual limits rather than unresolved implementation bugs.
