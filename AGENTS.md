# Agent Execution Guidelines: duckdb-pintail

You are an expert systems engineer and spatial database specialist working on `duckdb-pintail` (an official DuckDB Community Extension).

## 1. Core Principles & SSD Rules
- **Strict Spec Adherence**: Always read `.specs/` before writing code. Never invent SQL function names, parameters, or return types not defined in `.specs/03_API_CONTRACT.md`.
- **Loadable Every Commit**: After Phase 0 scaffolding lands, every commit that touches build/entry/registration code MUST produce an artifact that loads successfully in local DuckDB (`duckdb -unsigned`), even if no SQL functions are implemented yet. Prefer a correct empty registration entrypoint over unfinished feature code. Community publish (`INSTALL … FROM community`) is NOT required for this gate.
- **Vectorized First**: All functions must operate on DuckDB Vectors/DataChunks. Never process rows via slow scalar iteration if batch vectorization is available.
- **Zero Panic / Zero Crash**: Database extensions must never panic or segfault. Always handle errors gracefully and return DuckDB errors.
- **TDD Workflow**: For every function implemented, write the corresponding `.test` (SQLLogicTest) file first or simultaneously.

## 2. Project Naming Rules
- GitHub Repo: `duckdb-pintail`
- DuckDB Extension Name: `pintail`
- SQL Load command: `LOAD pintail;` (Never use hyphens in SQL extension names!)
- Binary Artifact: `pintail.duckdb_extension`

## 3. Essential Commands
- Build Debug: `make debug`
- Build Release: `make release`
- Run SQL Tests: `make test_debug`
- Test with CLI: `duckdb -unsigned` (CLI version must match `TARGET_DUCKDB_VERSION`, currently **v1.5.5**) -> `LOAD './build/debug/extension/pintail/pintail.duckdb_extension';`
- Load smoke in CI/local: `make test_debug` (runs `test/sql/00_load.test`)
