# Agent Execution Guidelines: duckdb-pintail

You are an expert systems engineer and spatial database specialist working on `duckdb-pintail` (an official DuckDB Community Extension).

## 0. Language & Build Stack
- **Language**: **C++17**. The extension is built on the official C++ `duckdb/extension-template` (CMake + `extension-ci-tools` makefiles). There is **no Rust** toolchain — do not introduce Cargo/`.rs` files.
- **Dependencies (Tier 0)**: none (pure C++ bit math). GEOS/PROJ are reserved for Tier 1 via `third_party/` vendoring.
- Entry point: `DUCKDB_CPP_EXTENSION_ENTRY(pintail, loader)` in `src/pintail_extension.cpp`.

## 1. Core Principles & SSD Rules
- **Strict Spec Adherence**: Always read `.specs/` before writing code. Never invent SQL function names, parameters, or return types not defined in `.specs/03_API_CONTRACT.md`.
- **Loadable Every Commit**: After Phase 0 scaffolding lands, every commit that touches build/entry/registration code MUST keep the extension load path working, even if no SQL functions are implemented yet.

  **Automatic verification (authoritative)**: `make debug && make test_debug` must pass (covers `test/sql/00_load.test`).

  Prefer a correct empty registration entrypoint over unfinished feature code. Community publish (`INSTALL … FROM community`) is NOT required for this gate.
- **Vectorized First**: All scalar functions must bind a `ScalarFunction` and run through `UnaryExecutor`/`BinaryExecutor`/`TernaryExecutor` over DuckDB Vectors/DataChunks. Never process rows via per-row scalar iteration.
- **Zero Panic / Zero Crash**: Never `std::abort`, never UB, never uncaught exceptions. Validate inputs and raise DuckDB errors (`InvalidInputException`, `OutOfRangeException`).
- **TDD Workflow**: For every function implemented, write the corresponding `.test` (SQLLogicTest) file first or simultaneously.

## 2. Project Naming Rules
- GitHub Repo: `duckdb-pintail`
- DuckDB Extension Name: `pintail` (EXT_NAME in Makefile, TARGET_NAME in CMakeLists.txt)
- SQL Load command: `LOAD pintail;` (Never use hyphens in SQL extension names!)
- Binary Artifact: `pintail.duckdb_extension`

## 3. Essential Commands
- Init submodules: `git submodule update --init --recursive`
- Build Debug: `make debug`
- Build Release: `make release`
- Run SQL Tests: `make test_debug`
- Test with CLI (optional): `duckdb -unsigned` (only when local CLI version matches the `duckdb` submodule at `v1.5.5`) -> `LOAD './build/debug/extension/pintail/pintail.duckdb_extension';`
- Load smoke in CI/local: `make test_debug` (runs `test/sql/00_load.test`)

## 4. DuckDB Releases & Two-Branch Testing
- Project CI builds the same source against DuckDB **v1.5.5, v1.5.6, and v2.0.0**. All matrices and `Release compatibility gate` must pass on the final `main` commit before community submission.
- Keep local development pinned to DuckDB `v1.5.5`; verify additional version compatibility through the actual CI matrices. DuckDB 2.0.0 is released and included in the release gate.
- Community builds use `repo.ref` for DuckDB 2.0 and `repo.variegata` for DuckDB 1.5.6. `repo.ref_next` does not select the current 2.0 build; inspect actual jobs for coverage.
- If one source cannot cover both lines, publish independently verified source refs in `repo.ref` and `repo.variegata`.
- **Version locking**: the binary is bound to the DuckDB version it was compiled against — a `v0.9.2` binary cannot be used with `v0.10.3`, and 1.5.x / 2.0 binaries are not interchangeable.
- PR body text such as `test_all_stable` / `test_all_main` is not evidence that a matrix ran; require successful workflow jobs.
- Keep local research in ignored `.docs/`; do not commit `.reasonix/` or `reasonix.toml`.
- Full specs: `.specs/04_TESTING_SPEC.md`, Phase 2C in `.specs/05_TASKS.md`.
