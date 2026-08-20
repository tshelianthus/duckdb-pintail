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
