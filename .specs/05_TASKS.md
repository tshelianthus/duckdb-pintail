# Tasks & Execution Roadmap

## [x] Phase 0: Scaffolding & Language Migration (C++)
- [x] Migrate from `extension-template-rs` (Rust) to the official C++ `extension-template`.
- [x] Remove all Rust artifacts (`Cargo.toml`, `Cargo.lock`, `src/*.rs`, `.cargo/`).
- [x] Rename extension target to `pintail` across CMake / Makefile / headers / `extension_config.cmake`.
- [x] Keep submodule initialization + `make debug` + `make test_debug` (00_load.test) green.

## Standing Rule (from Phase 0 onward)
- [ ] Every commit keeps submodule initialization + `make debug` + `make test_debug` green.
- [ ] Empty / partial feature sets are OK; a broken load entrypoint is not.

## [x] Phase 1: Geohash Core (MVP, pure C++)
- [x] Implement `st_geohash(lat, lon [, precision])` (pure bit math, no external deps).
- [x] Implement `st_pointfromgeohash(hash)`.
- [x] Implement `st_geohash_bbox(hash)`.
- [x] Implement `st_geohash_neighbors(hash)`.
- [x] Write `test/sql/geohash.test` covering the full operator matrix.

## [x] Phase 2: Initial Community Release (v0.1.0)
- [x] Create `docs/community/description.yml` (submit to `duckdb/community-extensions` after `v0.1.0` is tagged).
- [x] Verify the GitHub Actions cross-compilation matrix.
- [x] Tag `v0.1.0` at `ef5644d2450bcdf269482292e5ab78b7b24dccb3`.
- [x] Merge `dev` to `main` via PR #1.
- [x] Publish through community-extensions PR #2529 (merged).

## [x] Phase 2A: Native Geohash GEOMETRY (DuckDB Core)
- [x] Update the API contract for the breaking `st_pointfromgeohash` return-type change.
- [x] Return `GEOMETRY(EPSG:4326)` POINT/POLYGON values from direct Little-Endian WKB.
- [x] Add one- and two-argument overloads for `st_pointfromgeohash` and `st_geomfromgeohash`.
- [x] Cover type, CRS, precision, validation, NULL, batch, polar, antimeridian, and regression cases.
- [x] Keep Tier 0 independent of DuckDB Spatial, GEOS, and PROJ.

## [ ] Phase 3 (future): Tier-1 Geometry Topology
- [ ] Vendor GEOS/PROJ under `third_party/`, statically link (mirrors `duckdb-spatial`).
- [ ] Add topology and coordinate-transformation functions per a future API-contract update.
- [ ] Review future geometry/CRS APIs separately before adding them to the API contract.

## [x] Phase 2B: PostGIS Geohash parity
- [x] Record full official-doc/source difference matrix and intentional deviations.
- [x] Define GEOMETRY encoding, empty/CRS/boundary semantics and bbox precision/BOX2D alias.
- [x] Write SQLLogicTest before implementation, including 880 source-derived differential vectors.
- [x] Implement Core GEOMETRY input and vectorized nested output, without dependencies.
- [x] Pass `make debug && make test_debug` and `make release && make test_release` (5 files, 1149 assertions each).

## [ ] v0.2.0 Community Release & Installation Acceptance
- [x] Document the recommended GEOMETRY coordinate order and retain existing scalar calls.
- [x] Add issue #3 Shanghai regression checks; Debug and Release suites pass 6 files / 1223 assertions.
- [x] Prepare README, changelog, and the v0.2.0 candidate descriptor.
- [x] Move local research into ignored `.docs/` and exclude local tool state.
- [x] Pass both DuckDB version matrices and the compatibility gate for the final candidate.
- [x] Merge through [project PR #7](https://github.com/tshelianthus/duckdb-pintail/pull/7); final `main` commit `a91b8018c00a9e889f8215e968e42f0c6eb964dc` passed the [dual-version workflow and compatibility gate](https://github.com/tshelianthus/duckdb-pintail/actions/runs/38342840952).
- [x] Push `v0.2.0` for that verified source and finalize the community descriptor `repo.ref`.
- [x] Submit [community update PR #3061](https://github.com/duckdb/community-extensions/pull/3061), changing only `extensions/pintail/description.yml`.
- [x] Complete community maintainer review and merge PR #3061 on 2026-10-15
      at 19:35 Beijing time (UTC+8), as reported by the maintainer-facing release summary.
      @lghavami approved the work and pointed to the DuckDB 2.0 migration notice #3081.
- [x] Deploy v0.2.0 for DuckDB v1.5.6: all nine platform deployments succeeded in
      [the post-merge release workflow](https://github.com/duckdb/community-extensions/actions/runs/38728484740),
      per the supplied release summary. Earlier automatically skipped checks did not
      prevent maintainer review or merge; skipped checks are not counted as passed tests.
- [ ] Run distributed-installation acceptance with a matching DuckDB v1.5.6 runtime:
      `INSTALL pintail FROM community; LOAD pintail;`. Verify the installed extension
      version, Shanghai regression, GEOMETRY encode/decode, BOX2D alias, and catalog
      metadata. Record runtime/platform and actual results; deployment success alone
      does not complete installation acceptance.

## [ ] Community function catalog documentation

Observed on 2026-10-16: the [Pintail community page](https://duckdb.org/community_extensions/extensions/pintail#added-functions)
shows NULL descriptions/comments and empty examples. Local registration already supplies
per-overload descriptions/examples, but does not set the catalog comment.
DuckDB exposes these as separate fields: `description` is registered function documentation;
`comment` is a catalog annotation (also exposed through `COMMENT ON`).

- [ ] Add a concise English catalog comment for each registered Pintail function name,
      using the C++ registration metadata (`CreateScalarFunctionInfo::comment`), while
      preserving per-overload descriptions, parameter names, and executable examples.
      Comments are shared by a function's overloads; describe common purpose and key
      usage constraints rather than copying one overload's signature.
- [ ] Review wording against `.specs/03_API_CONTRACT.md`: distinguish scalar lat/lon
      from geometry XY order, EPSG:4326 decode output, encoding/decode precision rules,
      the STRUCT-returning BOX2D alias, and neighbor order/wrapping. Keep detailed
      overload semantics in descriptions/docs. Example draft for `st_pointfromgeohash`:
      "Returns the center of a Geohash cell as an EPSG:4326 POINT (x=longitude, y=latitude)."
- [ ] Extend `test/sql/function_metadata.test` with nonempty-comment coverage for all
      registered names and comment consistency across overloads; retain existing
      description/example checks. Run `make debug && make test_debug`, then the
      v1.5.5/v1.5.6 release compatibility gate before publishing changed registration.
- [ ] Verify community `repo.ref`, distributed binary metadata from `duckdb_functions()`,
      and the website metadata-generation/refresh path. After the verified release is
      distributed, confirm Added Functions shows comments, descriptions, and examples,
      including `st_box2dfromgeohash`; do not assume a local edit refreshes the website.

References: [DuckDB function metadata](https://duckdb.org/docs/current/sql/meta/duckdb_table_functions#duckdb_functions),
[AI extension](https://duckdb.org/community_extensions/extensions/ai#added-functions)
(descriptions/examples with NULL comments), and
[DuckFlight](https://duckdb.org/community_extensions/extensions/duckflight#added-functions)
(comments describing compatibility purpose and helper behavior).

## [x] Phase 2C: DuckDB 2.0 and 1.5.x Compatibility

- [x] Verify community's effective build policy: `repo.ref` targets DuckDB 2.0 and
      `repo.variegata` targets DuckDB 1.5.6; `repo.ref_next` does not select the
      current 2.0 build (see migration notice #3081 and current upstream workflow).
- [x] Extend the project compatibility gate to DuckDB v2.0.0 while retaining
      v1.5.5 / v1.5.6. The same source passed all three version matrices and the
      release gate in [CI run 40117563542](https://github.com/tshelianthus/duckdb-pintail/actions/runs/40117563542).
- [x] Run local Debug and Release builds plus SQLLogicTest on the pinned DuckDB
      v1.5.5 submodule; both configurations passed 1175 assertions in 5 cases.
- [x] Update release instructions and compatibility docs to match DuckDB 2.0 GA
      and the effective community descriptor mapping.
- [ ] Merge the release PR, verify the final `main` compatibility gate, publish the
      semantic version tag and matching descriptor refs, then submit the Community
      Extensions PR and verify its build, merge, deployment, and installed package.
