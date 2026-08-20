# Agent Execution Guidelines: duckdb-pintail

You are an expert systems engineer and spatial database specialist working on `duckdb-pintail` (an official DuckDB Community Extension).

## 1. Core Principles & SSD Rules
- **Strict Spec Adherence**: Always read `.specs/` before writing code. Never invent SQL function names, parameters, or return types not defined in `.specs/03_API_CONTRACT.md`.
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
- Test with CLI: `duckdb -unsigned` -> `LOAD './build/debug/extension/pintail/pintail.duckdb_extension';`
