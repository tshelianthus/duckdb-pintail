# Tasks & Execution Roadmap

- [ ] **Phase 0: Scaffolding**
  - [ ] Initialize repository with `extension-template-rs`
  - [ ] Rename extension target to `pintail`
  - [ ] Verify local `make debug` and `duckdb -unsigned` load

### Standing Rule (from Phase 0 onward)
- [ ] Every commit keeps `make debug` + local `LOAD` green
- [ ] Empty / partial feature sets are OK; a broken load entrypoint is not

- [ ] **Phase 1: Geohash Core (MVP)**
  - [ ] Add `geohash` crate to `Cargo.toml`
  - [ ] Implement `st_geohash(lat, lon, precision)`
  - [ ] Implement `st_pointfromgeohash(hash)`
  - [ ] Implement `st_geohash_bbox(hash)`
  - [ ] Implement `st_geohash_neighbors(hash)`
  - [ ] Write `test/sql/geohash.test` covering all test cases
- [ ] **Phase 2: Community Submission**
  - [ ] Create `description.yml` for `duckdb/community-extensions`
  - [ ] Verify GitHub Actions cross-compilation matrix
