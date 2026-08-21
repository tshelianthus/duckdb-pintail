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

## [ ] Phase 2: Community Submission
- [x] Create `docs/community/description.yml` (submit to `duckdb/community-extensions` after `v0.1.0` is tagged).
- [ ] Verify the GitHub Actions cross-compilation matrix.
- [ ] Merge `dev` to `main`, tag `v0.1.0`, and open the community-extensions PR.

## [x] Phase 2A: Native Geohash GEOMETRY (DuckDB Core)
- [x] Update the API contract for the breaking `st_pointfromgeohash` return-type change.
- [x] Return `GEOMETRY(EPSG:4326)` POINT/POLYGON values from direct Little-Endian WKB.
- [x] Add one- and two-argument overloads for `st_pointfromgeohash` and `st_geomfromgeohash`.
- [x] Cover type, CRS, precision, validation, NULL, batch, polar, antimeridian, and regression cases.
- [x] Keep Tier 0 independent of DuckDB Spatial, GEOS, and PROJ.

## [ ] Phase 3 (future): Tier-1 Geometry Topology
- [ ] Vendor GEOS/PROJ under `third_party/`, statically link (mirrors `duckdb-spatial`).
- [ ] Add topology and coordinate-transformation functions per a future API-contract update.
- [ ] Apply the reserved `always_xy BOOLEAN` axis-order convention to CRS-aware functions (see `.specs/03_API_CONTRACT.md`).
