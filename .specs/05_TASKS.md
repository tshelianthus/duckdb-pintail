# Tasks & Execution Roadmap

## [x] Phase 0: Scaffolding & Language Migration (C++)
- [x] Migrate from `extension-template-rs` (Rust) to the official C++ `extension-template`.
- [x] Remove all Rust artifacts (`Cargo.toml`, `Cargo.lock`, `src/*.rs`, `.cargo/`).
- [x] Rename extension target to `pintail` across CMake / Makefile / headers / `extension_config.cmake`.
- [x] Keep `make configure` + `make debug` + `make test_debug` (00_load.test) green.

## Standing Rule (from Phase 0 onward)
- [ ] Every commit keeps `make configure` + `make debug` + `make test_debug` green.
- [ ] Empty / partial feature sets are OK; a broken load entrypoint is not.

## [ ] Phase 1: Geohash Core (MVP, pure C++)
- [ ] Implement `st_geohash(lat, lon [, precision])` (pure bit math, no external deps).
- [ ] Implement `st_pointfromgeohash(hash)`.
- [ ] Implement `st_geohash_bbox(hash)`.
- [ ] Implement `st_geohash_neighbors(hash)`.
- [ ] Write `test/sql/geohash.test` covering the full operator matrix.

## [ ] Phase 2: Community Submission
- [ ] Create `description.yml` for `duckdb/community-extensions`.
- [ ] Verify the GitHub Actions cross-compilation matrix.

## [ ] Phase 3 (future): Tier-1 Geometry Topology
- [ ] Vendor GEOS/PROJ under `third_party/`, statically link (mirrors `duckdb-spatial`).
- [ ] Add `GEOMETRY` type + overlay functions per a future API-contract update.
- [ ] Apply the reserved `always_xy BOOLEAN` axis-order convention to CRS-aware functions (see `.specs/03_API_CONTRACT.md`).
