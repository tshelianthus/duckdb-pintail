# Pintail 社区发布门禁与参数

本轮发布必须先完成 DuckDB `v1.5.5`、`v1.5.6` 和 `v2.0.0` 的项目内验证。所有矩阵未全部通过前，禁止创建社区发布 PR（包括草稿）、创建发布 tag 或发布新版。社区 CI 是后续验证，不能替代该门禁。

## 验证与发布顺序

1. 在项目 PR 中对同一源码运行 1.5.x 双版本官方跨平台构建矩阵及 DuckDB 2.0.0 官方矩阵。原生平台按官方工具链支持运行 SQLLogicTest；Wasm 等平台的构建成功不代表执行过 SQL 测试。
2. 确认所有版本的构建任务和适用的 SQL 测试通过，且 `Release compatibility gate` 成功。失败、取消或未完成均不满足门禁。
3. 经项目 PR 合并到 `main` 后，再确认合并提交的全部矩阵和兼容性门禁成功。发布引用必须是该已验证提交，不能使用不同源码的历史 CI 结果。
4. 本地完成 `git submodule update --init --recursive`、`make debug`、`make test_debug`、`make release`、`make test_release`，记录测试版本、提交和结果。本地单版本通过不能代替双版本 CI。
5. 创建语义化版本 tag，记录完整 commit SHA，更新描述文件版本与 `repo.ref`，再向社区提交只修改 `extensions/pintail/description.yml` 的 PR。
6. 等待社区构建、测试与维护者审核完成，再确认实际安装分发结果。各 DuckDB 版本使用针对各自版本构建的二进制。

建议在 GitHub 分支保护中将 `Release compatibility gate` 设置为 required status check。工作流提供该检查；新增工作流本身不会修改仓库分支保护设置。

## CI 参数

| 参数 | 当前设置 | 含义 |
| --- | --- | --- |
| `matrix.include[].duckdb_version` | `v1.5.5`、`v1.5.6` | 同一源码针对两个保留的 1.5.x 版本编译；本地子模块继续固定为 `v1.5.5` |
| `strategy.fail-fast` | `false` | 一个版本失败时，另一个版本继续验证；官方被调用工作流内部的平台调度仍由上游控制 |
| `ci_tools_version` | `v1.5-variegata` | 使用 DuckDB 官方 1.5 系列工具链 |
| `matrix.include[].vcpkg_commit` | 1.5.5: `cd61e1e26a038e82d6550a3ebbe0fbbfe7da78e3`；1.5.6: `84bab45d415d22042bd0b9081aea57f362da3f35` | 与社区对应版本的构建基线一致；Pintail 自身仍无外部运行时依赖 |
| `skip_tests` | `false` | 启用官方工具链提供的适用平台测试；不为仅编译的平台虚报测试结果 |
| `duckdb-2-build` | `v2.0.0`，`extension-ci-tools@v2.0-cyanoptera`，vcpkg `d54cbbe3df17e8e5ce68312f483b4155836112aa` | 针对 DuckDB 2.0 LTS 的完整平台构建与适用 SQL 测试 |
| `reduced_ci_mode` | `disabled` | 请求完整分发平台矩阵，不跳过可归并架构 |
| `release-gate.needs` | `duckdb-stable-build`、`duckdb-2-build` | 仅当 1.5.x 与 2.0.0 两组全部成功才放行 |

上游产物名称与编译缓存键包含 DuckDB 版本及平台，避免混用二进制。两个可复用工作流分别锁定 1.5 和 2.0 工具链；更改版本列表时，应同时核实社区当前工具链与 vcpkg 基线。

## 社区描述文件参数

| 参数 | 用途与规则 |
| --- | --- |
| `extension.name` | 固定为 `pintail`，与构建目标、SQL 加载名一致 |
| `extension.version` | 本次扩展语义化版本；兼容性修复使用 patch 版本，正式门禁完成后再确定 tag |
| `extension.description` | 简短、准确描述已实现功能 |
| `extension.language` / `build` | `C++` / `cmake` |
| `extension.license` / `maintainers` | `Apache-2.0` / `tshelianthus` |
| `repo.github` | `tshelianthus/duckdb-pintail` |
| `repo.ref` | 通过发布门禁的公开、固定完整 commit SHA；当前用于 DuckDB 2.0 构建 |
| `repo.variegata` | 通过发布门禁的公开、固定完整 commit SHA；社区构建 DuckDB 1.5.6 时使用 |
| `extension.excluded_platforms` | 可选的分号分隔平台列表；只有明确记录不支持原因时才使用 |
| `extension.requires_toolchains` | 可选的额外工具链；Pintail 当前不需要 |
| `docs.hello_world` | 可独立执行的安装、加载和 SQL 示例 |
| `docs.extended_description` | 功能、参数边界及行为差异说明 |

社区构建策略已切换：`repo.ref` 选择 DuckDB 2.0 源码，`repo.variegata` 选择 DuckDB 1.5.6 源码；`repo.ref_next` 不再用于选择当前 2.0 构建。不能凭 PR 中的 `test_all_stable` / `test_all_main` 文本声称完成测试，应以实际工作流任务为准。

## DuckDB 2.0 与 1.5.x 兼容

DuckDB 2.0.0 已于 2026-10-21 发布。项目工作流在同一提交运行 1.5.5 / 1.5.6 矩阵（`extension-ci-tools@v1.5-variegata`）及 2.0.0 完整构建测试（`extension-ci-tools@v2.0-cyanoptera`）。`Release compatibility gate` 要求两条工具链的任务都成功。2.0.0 目前使用已验证可获取的 vcpkg 基线 `84bab45d415d22042bd0b9081aea57f362da3f35`，与 1.5.6 共用；1.5.5 使用其对应基线。Pintail 无 vcpkg 依赖。

保留 1.5.5 / 1.5.6 项目矩阵，直到明确调整支持策略，并在替换版本全部通过后再移除旧目标。本地 DuckDB 子模块暂时保持 v1.5.5。检查 CI 结果时，需确认 2.0.0 原生 SQLLogicTest 任务实际运行成功；仅构建平台不算 SQL 测试证据。扩展二进制按 DuckDB 版本锁定，不能跨版本加载。

## SQL 参数与发布措辞

本轮功能说明应为“参考 PostGIS，扩展了 Pintail 的 Geohash 功能”。发布说明依据 [API 合同](../../.specs/03_API_CONTRACT.md) ，不宣称接入 PostGIS 或完全兼容。

| 参数 | 范围与行为 |
| --- | --- |
| `lat` / `lon` | 纬度 `[-90,90]`、经度 `[-180,180]`；必须有限；坐标编码顺序为纬度、经度 |
| 坐标编码 `precision` | 整数 `[1,20]`，默认 `12` |
| `geom` | Core GEOMETRY，XY 为经度、纬度；接受无 CRS 的地理坐标或 EPSG:4326/OGC:CRS84；忽略 Z/M；空几何返回 NULL |
| 几何编码 `maxchars` | 省略或 `0` 自动选择，`[1,20]` 限制最长字符数；保持整个几何的覆盖，可能返回更短前缀或世界格 `''` |
| 解码 `hash` | 接受空串、长串和 ASCII 大写；只验证消耗的 Base32 前缀 |
| 解码 `precision` | 省略或负值使用完整字符串；超长截至字符串长度；`0` 解码世界格 |
| 邻居 `hash` | 小写 Base32，长度 `[1,20]`；返回顺序 `[N,NE,E,SE,S,SW,W,NW]` |
| 任意 NULL 参数 | 返回 NULL，包括显式 NULL 精度 |

## 官方依据

- [DuckDB 社区描述文件](https://duckdb.org/community_extensions/documentation)
- [DuckDB 社区发布与下一版本适配](https://duckdb.org/community_extensions/development)
- [社区构建目标及构建基线](https://github.com/duckdb/community-extensions/blob/main/.github/workflows/build.yml)
- [GitHub Actions 矩阵与失败控制](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/run-job-variations)
