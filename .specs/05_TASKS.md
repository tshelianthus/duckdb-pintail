# Tasks & Execution Roadmap

## [x] Phase 0: Scaffolding & Language Migration (C++)
- [x] Migrate from `extension-template-rs` (Rust) to the official C++ `extension-template`.
- [x] Remove all Rust artifacts (`Cargo.toml`, `Cargo.lock`, `src/*.rs`, `.cargo/`).
- [x] Rename extension target to `pintail` across CMake / Makefile / headers / `extension_config.cmake`.
- [x] Keep `make configure` + `make debug` + `make test_debug` (00_load.test) green.

## Standing Rule (from Phase 0 onward)
- [ ] Every commit keeps `make configure` + `make debug` + `make test_debug` green.
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

## [ ] v0.2.0 Release Candidate
- [x] Document the recommended GEOMETRY coordinate order and retain existing scalar calls.
- [x] Add issue #3 Shanghai regression checks; Debug and Release suites pass 6 files / 1223 assertions.
- [x] Prepare README, changelog, and the v0.2.0 candidate descriptor.
- [x] Move local research into ignored `.docs/` and exclude local tool state.
- [ ] Pass both DuckDB version matrices and the compatibility gate for the final candidate.
- [ ] Merge through a project PR and pass the same checks on the final `main` commit.
- [ ] Tag the verified release source and finalize the community descriptor `repo.ref`.
- [ ] Submit the community update, pass Community CI/review, and verify installation.
