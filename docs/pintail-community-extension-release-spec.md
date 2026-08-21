# DuckDB Community Extension 发布规范

## 1. 基本要求

扩展项目必须满足以下条件：

- 源代码公开托管在 GitHub。
- 使用开源许可证，例如 Apache-2.0、MIT 或 BSD。
- 能够通过 DuckDB Community Extensions 的构建系统从源码编译。
- C++ 扩展使用 CMake 构建。
- 扩展名称只能包含小写字母、数字、连字符或下划线。
- 构建过程不得依赖未声明的本地环境或私有资源。
- 发布版本必须指向固定且公开可访问的 Git commit。
- 扩展必须能够正常安装、加载和执行基本功能，不得导致 DuckDB 崩溃。

## 2. 项目仓库要求

项目仓库应至少包含：

- `CMakeLists.txt`
- `Makefile`
- `extension_config.cmake`
- `vcpkg.json`
- `LICENSE`
- `README.md`
- `.github/workflows/MainDistributionPipeline.yml`
- `src/`
- `test/sql/`
- DuckDB 与 `extension-ci-tools` 子模块配置

扩展名称必须在以下位置保持一致：

- Makefile 中的 `EXT_NAME`
- CMake 中的 `TARGET_NAME`
- `extension_config.cmake`
- C++ 扩展入口
- SQL `LOAD` 名称
- Community `description.yml`

Pintail 的名称统一为：

```text
pintail
```

加载命令为：

```sql
LOAD pintail;
```

二进制产物为：

```text
pintail.duckdb_extension
```

## 3. 构建与测试门禁

提交 Community Extension 之前，必须完成以下验证：

```bash
git submodule update --init --recursive
make configure
make debug
make test_debug
make release
make test_release
```

至少需要确认：

- Debug 构建成功。
- Release 构建成功。
- 扩展二进制成功生成。
- 扩展能够被 DuckDB 加载。
- 所有 SQLLogicTest 测试通过。
- GitHub Actions 跨平台构建矩阵全部通过。
- 没有未声明的外部依赖。
- 没有崩溃、未定义行为或未捕获异常。

基本加载测试应包含：

```sql
LOAD pintail;
```

功能冒烟测试应至少调用一个公开函数：

```sql
SELECT st_geohash(31.2304, 121.4737, 6);
```

预期结果：

```text
wtw3sj
```

## 4. 功能测试要求

每个公开 SQL 函数至少覆盖：

- 正常输入。
- 默认参数。
- 输入边界。
- 非法输入。
- NULL 传播。
- 常量向量。
- 普通扁平向量。
- 字典或切片场景。
- 多行批量执行。
- 返回 STRUCT 或 LIST 时的嵌套向量行为。

Geohash 功能应覆盖：

- 赤道和本初子午线。
- 南北极。
- 正负 180 度日期变更线。
- 最小精度和最大精度。
- 非法 Base32 字符。
- 空 Geohash。
- NaN 和无穷大。
- 邻居数量与固定顺序。
- 极区和日期变更线处的邻居行为。

邻居返回顺序必须直接验证为：

```text
[N, NE, E, SE, S, SW, W, NW]
```

不能只使用 `list_sort` 验证成员集合。

## 5. 分支与版本要求

建议采用以下发布流程：

1. 在开发分支完成实现和测试。
2. 创建合并到 `main` 的 Pull Request。
3. 等待 GitHub Actions 全部通过。
4. 合并到 `main`。
5. 在 `main` 上再次确认发布构建通过。
6. 创建符合语义化版本的 Git tag，例如：

   ```text
   v0.1.0
   ```

7. 记录该 tag 对应的完整 Git commit SHA。
8. Community 描述文件使用固定 commit SHA，不能使用会移动的开发分支作为正式发布引用。

版本号应遵循语义化版本：

```text
主版本.次版本.修订版本
```

例如：

```text
0.1.0
```

## 6. Community 描述文件

需要向以下仓库提交 Pull Request：

```text
https://github.com/duckdb/community-extensions
```

新增文件路径：

```text
extensions/pintail/description.yml
```

建议内容：

```yaml
extension:
  name: pintail
  description: Lightweight geospatial indexing functions for DuckDB, including Geohash encoding, decoding, bounding boxes, and neighbors.
  version: 0.1.0
  language: C++
  build: cmake
  license: Apache-2.0
  maintainers:
    - tshelianthus

repo:
  github: tshelianthus/duckdb-pintail
  ref: REPLACE_WITH_FULL_GIT_COMMIT_SHA

docs:
  hello_world: |
    INSTALL pintail FROM community;
    LOAD pintail;

    SELECT st_geohash(31.2304, 121.4737, 6);

  extended_description: |
    Pintail is a lightweight geospatial and spatial-indexing extension for DuckDB.
    The initial release provides dependency-free Geohash encoding, decoding,
    bounding-box extraction, and adjacent-cell operations implemented in C++17.
```

提交前必须将：

```text
REPLACE_WITH_FULL_GIT_COMMIT_SHA
```

替换为正式版本对应的完整 Git commit SHA。

如果某些平台确实无法构建，可以在 `extension` 下声明：

```yaml
excluded_platforms: "platform_a;platform_b"
```

但纯 C++、无外部依赖的扩展应优先修复跨平台构建，不应无理由排除平台。

如果需要额外构建工具，应声明：

```yaml
requires_toolchains: "toolchain_a;toolchain_b"
```

Pintail Tier 0 为纯 C++ 实现，正常情况下不应需要额外工具链。

## 7. 文档要求

README 至少需要说明：

- 扩展用途。
- 支持的 DuckDB 版本。
- 本地构建方法。
- 测试方法。
- 本地加载方法。
- Community 安装方法。
- 所有公开 SQL 函数及签名。
- 参数范围。
- NULL 行为。
- 错误行为。
- 返回类型。
- 使用示例。
- 项目许可证。
- 问题反馈地址。

正式发布后，安装说明应为：

```sql
INSTALL pintail FROM community;
LOAD pintail;
```

README 中的许可证名称必须与仓库实际许可证文件一致。

## 8. Community PR 提交要求

Community PR 应只包含 Pintail 的描述文件：

```text
extensions/pintail/description.yml
```

PR 描述建议包含：

- 项目简介。
- 首次发布版本。
- 源代码仓库地址。
- 固定 Git commit SHA。
- 本地测试结果。
- GitHub Actions 跨平台测试结果。
- 支持或排除的平台。
- 外部依赖情况。
- 许可证信息。
- 一个最小 SQL 使用示例。

Community CI 将从指定 Git ref 获取源码，完成编译、测试、签名和分发。所有检查通过并由维护者批准后，用户即可执行：

```sql
INSTALL pintail FROM community;
LOAD pintail;
```

## 9. Pintail 发布前检查清单

- [x] Geohash 五个公开函数全部完成（含 `st_geomfromgeohash` 与 GEOMETRY 返回类型）。
- [x] SQL API 与 API Contract 一致。
- [x] 邻居固定顺序有直接测试。
- [x] 南北极编码测试完成。
- [x] 正负日期变更线编码测试完成。
- [x] NULL、NaN、无穷大和非法字符测试完成。
- [x] STRUCT 和 LIST 常量向量测试完成。
- [x] `make debug` 通过。
- [x] `make test_debug` 通过。
- [x] `make release` 通过。
- [x] `make test_release` 通过。
- [ ] GitHub Actions 跨平台矩阵全部通过。
- [ ] `dev` 已通过 PR 合并到 `main`。
- [x] README 与实际功能一致。
- [x] README 许可证描述与 LICENSE 一致。
- [x] 项目路线图状态已更新。
- [ ] 已创建 `v0.1.0` tag。
- [ ] 已记录 tag 对应的完整 commit SHA。
- [x] 已创建 `docs/community/description.yml`（提交社区仓库前将 `ref` 替换为 tag SHA）。
- [ ] `description.yml` 中的版本、许可证、维护者和 Git ref 正确（`ref` 待 tag）。
- [ ] 已向 `duckdb/community-extensions` 提交 PR。
- [ ] Community CI 全部通过。
- [ ] Community PR 已获维护者批准。
