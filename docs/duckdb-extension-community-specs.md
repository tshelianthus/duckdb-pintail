# DuckDB 扩展社区规范（核心规范正文）

> 本文件是 `duckdb-pintail` 项目归档的 **DuckDB 扩展社区规范核心正文**，来源为官方
> `duckdb/duckdb-web` 仓库（`main` 分支，最新）与官方仓库 README。原始链接见文末「来源」。
> 空间函数原文镜像于本地 `docs/_raw/spatial-functions.md`（含 Jekyll front-matter，无固化时间戳）；
> 以官方 `duckdb-web` 仓库 `main` 分支最新版为准。

---

## 一、社区扩展开发规范（Community Extension Development）

来源：`duckdb/duckdb-web` → `community_extensions/development.md`

### 1. 构建（Building）

- DuckDB 提供 **C++ 版扩展模板**（Extension Template）: https://github.com/duckdb/extension-template ，开箱即用（batteries-included）。
- 模板预配置了：
  - 包管理器 vcpkg（https://vcpkg.io/）
  - 基于 SQL 的测试框架（SQLLogicTest）
  - 基于 GitHub Actions 的 CI/CD 工具链
- CI/CD 链会自动为所有受支持的 DuckDB 平台构建扩展：**Linux、macOS、Windows、Wasm**。

### 2. 发布（Publishing）

- 发布到 DuckDB 社区仓库需向 **Community Repository**（https://github.com/duckdb/community-extensions）提 PR。
- PR 中需提供 **descriptor 描述符文件**（`description.yml`），包含扩展的源仓库、版本等信息。
- 可参考已收录的社区扩展作为范例：https://github.com/duckdb/community-extensions/tree/main/extensions 。
- 社区仓库使用 DuckDB 的 **CI 工具链**（https://github.com/duckdb/extension-ci-tools）构建扩展，与扩展模板同一套工具链，因此基于模板的扩展天然可用。

### 3. 开发者文档入口

社区扩展相对较新，官方开发者文档有限，信息分布于：
- DuckDB 仓库 README：https://github.com/duckdb/duckdb
- CI 工具链：https://github.com/duckdb/extension-ci-tools
- 扩展模板：https://github.com/duckdb/extension-template
- 社区扩展仓库：https://github.com/duckdb/community-extensions

### 4. 跨 DuckDB 版本维护扩展

- 目前社区扩展目标为 **只针对最新稳定版**（latest stable）构建分发；非最新稳定版用户看到的扩展会「冻结」，不再更新。
- 临近新版本发布时，`duckdb/community-extensions` 会同时针对「最新稳定版」与「`main` 分支」测试扩展。
- 若扩展同时兼容两者则不受影响（理想情况）。
- 若**不**能同时兼容两者，推荐更新路径：
  1. 维护两条分支：一条面向最新稳定版，一条面向 `main`。
  2. 在 descriptor 中提供稳定版分支最新 commit 为 `ref`，main 分支 commit 为 `ref_next`。
  3. 这样扩展可同时针对最新稳定版与 `main` 测试。
  4. 版本 hash 确定后，社区扩展按该 hash 构建，`ref_next`（若存在）替换为 `ref`。

```yaml
# 示例（hannes/duckdb_avro）
repo:
  github: hannes/duckdb_avro
  ref: e5ed59b6ccf915c65e17eb6286b9a64f3ab09f59
  ref_next: c8941c92ec103f7825eb88207c04512f8a714b23
```

### 5. 获取帮助

- 扩展开发问题有专门的 DuckDB Discord 频道：https://discord.com/invite/tcvwpjfnZx 。
- 发现 bug/问题，在对应仓库提 issue（DuckDB 本体 / 扩展模板 / CI 工具链）。

---

## 二、扩展描述符（Descriptor / YAML）规范

来源：`community_extensions/documentation.md`

### 1. 提交要求

- 社区扩展必须 **公开、开源、托管于 GitHub**。
- 鼓励用户去扩展仓库反馈、查看实现、报告 issue。
- 提交方式：向 https://github.com/duckdb/community-extensions 提 PR，在 `extensions/<name_of_the_extension>` 目录放置单个 `description.yml`。

### 2. YAML 描述符字段

| 字段 | 说明 |
| --- | --- |
| `extension.name` | 扩展名。大小写不敏感，只允许小写字母、数字、`-` 或 `_` |
| `extension.description` | 简短描述 |
| `extension.version` | 语义化版本号 |
| `extension.language` | `C++`、`Rust & C++`、`C++ & SQL` 或其它组合 |
| `extension.build` | 构建系统，目前仅支持 `cmake` |
| `extension.license` | 扩展许可证（官方字段名为 `license`，美式拼写） |
| `extension.maintainers` | 维护者列表 |
| `extension.excluded_platforms` | 可选，指定不支持的平台 |
| `extension.requires_toolchains` | 可选，指定额外构建期依赖 |
| `repo.github` | GitHub 公开仓库的 `组织/仓库名` |
| `repo.ref` | 扩展对应的 git ref |

可选文档字段（仅用于自动生成的文档）：

| 字段 | 说明 |
| --- | --- |
| `docs.hello_world` | 「Hello, World!」代码片段，展示扩展能力的自包含示例 |
| `docs.extended_description` | 额外上下文、相关链接、能力导览 |

### 3. 托管文档页与自动探测

- 每个社区扩展有文档页：`https://duckdb.org/community_extensions/extensions/<extension_name>`。
- 文档页由 descriptor 字段 + 自动探测的扩展变更生成，探测逻辑大致为：

```sql
CREATE TABLE functions_pre AS SELECT ... FROM duckdb_functions();
LOAD extension_name;
CREATE TABLE functions_post AS SELECT ... FROM duckdb_functions();
SELECT * FROM functions_post EXCEPT (FROM functions_pre) ORDER BY ...;
```

- 可自动探测：新函数、函数重载、新设置（settings）、新类型（types）。
- 尚未实现：parser/optimizer 回调等变更，建议写入 `docs.extended_description`。

---

## 三、空间扩展（Spatial）规范

来源：`duckdb/duckdb-web` → `docs/current/core_extensions/spatial/overview.md` 与 `functions.md`

### 1. 概述与安装加载

- `spatial` 扩展为 DuckDB 提供地理空间数据处理能力。代码仓库：https://github.com/duckdb/duckdb-spatial 。
- 官方博客：2023-04-28 spatial 发布。

```sql
INSTALL spatial;
```

- 注意：`spatial` 扩展 **不可自动加载**（not autoloadable），使用前需显式加载：

```sql
LOAD spatial;
```

### 2. 空间函数 API（命名与签名规范）

`spatial` 扩展函数分四类：**Scalar / Aggregate / Macro / Table**。完整签名与行为见本地原文
`docs/_raw/spatial-functions.md`（约 92KB，与官方 `functions.md` 一致）。

**命名约定**（供 `pintail` 对齐 PostGIS 兼容命名）：

- 前缀 `ST_`：绝大多数空间函数（对标 PostGIS）。
- 库版本函数：`DuckDB_PROJ_Compiled_Version` / `DuckDB_Proj_Version`。
- 坐标访问：`ST_X` / `ST_Y` / `ST_Z` / `ST_M` 及 `ST_XMin/XMax/YMin/YMax/ZMin/ZMax/MMin/MMax`。
- 构造：`ST_Point`、`ST_Point2D/3D/4D`、`ST_MakePoint`、`ST_MakeLine`、`ST_MakePolygon`、`ST_MakeEnvelope`、`ST_MakeBox2D`。
- 序列化：`ST_AsText`（WKT）、`ST_AsWKB`、`ST_AsHEXWKB`、`ST_AsGeoJSON`、`ST_AsSVG`、`ST_AsMVTGeom`。
- 反序列化：`ST_GeomFromText`、`ST_GeomFromWKB`、`ST_GeomFromHEXWKB`（与 `ST_GeomFromHEXEWKB` 为别名）、`ST_GeomFromGeoJSON`。
- 注：`ST_AsHEXEWKB` 不存在于官方文档（序列化侧仅 `ST_AsHEXWKB`）；`ST_AsMVT` 为聚合函数（见下方「聚合」），序列化标量仅为 `ST_AsMVTGeom`。
- 谓词：`ST_Contains`、`ST_Within`、`ST_Intersects`、`ST_Equals`、`ST_Crosses`、`ST_Overlaps`、`ST_Touches`、`ST_Disjoint` 等。
- 拓扑运算：`ST_Intersection`、`ST_Union`、`ST_Difference`、`ST_Buffer`、`ST_ConvexHull`、`ST_Simplify`、`ST_Transform`。
- 距离/度量：`ST_Distance`、`ST_Area`、`ST_Length`、`ST_Perimeter` 及 `_Spheroid`/`_Sphere`/`_GEOS` 变体。
- 网格/索引相关（与 pintail 定位重叠）：`ST_Hilbert`、`ST_QuadKey`、`ST_TileEnvelope`。
- 聚合：`ST_AsMVT`（Mapbox Vector Tile）、`ST_Extent_Agg`、`ST_Union_Agg`、`ST_Intersection_Agg`、`ST_Envelope_Agg`、`ST_MemUnion_Agg`、`ST_Coverage*_Agg`。（`ST_Extent` 与 `ST_Extent_Approx` 为对应标量，返回 `BOX_2D`/`BOX_2DF`）
- 表函数：`ST_Read`（GDAL）、`ST_ReadOSM`、`ST_ReadSHP`、`ST_Read_Meta`、`ST_Drivers`、`ST_GeneratePoints`。

**类型**：`GEOMETRY`、`GEOMETRY_TYPE`（enum：POINT/LINESTRING/POLYGON/MULTIPOINT/MULTILINESTRING/MULTIPOLYGON/GEOMETRYCOLLECTION）、`BOX_2D`、`BOX_2DF`（float 版，如 `ST_Extent_Approx` 返回）、`WKB_BLOB`、`POINT_2D`/`POINT_3D`/`POINT_4D`、`LINESTRING_2D`、`POLYGON_2D`。

### 3. 相关 GeoParquet / 格式规范

- GeoParquet 输出与写入讨论：https://github.com/duckdb/duckdb/discussions/14274
- duckdb-skills 空间参考：https://github.com/duckdb/duckdb-skills/blob/main/skills/spatial/references/functions.md

---

## 四、来源链接（汇总）

### 官方规范和总入口
- 社区扩展开发：https://duckdb.org/community_extensions/development
- 社区扩展文档：https://duckdb.org/community_extensions/documentation
- 社区扩展 FAQ：https://duckdb.org/community_extensions/faq
- 社区扩展列表：https://duckdb.org/community_extensions/list_of_extensions
- 社区扩展仓库：https://github.com/duckdb/community-extensions
- 社区扩展宣布博客：https://duckdb.org/2024/07/05/community-extensions

### 扩展模板与工具链
- C++ 扩展模板：https://github.com/duckdb/extension-template
- 依赖管理：https://duckdb.org/2024/03/22/dependency-management.html
- CI 工具链：https://github.com/duckdb/extension-ci-tools

### 空间扩展
- 仓库：https://github.com/duckdb/duckdb-spatial
- 总览（原文件）：https://raw.githubusercontent.com/duckdb/duckdb-web/refs/heads/main/docs/current/core_extensions/spatial/overview.md
- 函数参考（原文件）：https://raw.githubusercontent.com/duckdb/duckdb-web/refs/heads/main/docs/current/core_extensions/spatial/functions.md

### 贡献规范范例
- https://raw.githubusercontent.com/duckdb/community-extensions/main/README.md
- https://github.com/ekkuleivonen/ducklake-cdc-extension/blob/main/CONTRIBUTING.md
- https://github.com/xqlsystems/duckdb-zarr/blob/main/CONTRIBUTING.md
