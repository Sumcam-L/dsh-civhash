# dsh-civhash

把 [CivNexus6](https://github.com/deliverator23/CivNexus6) `ConsoleApplication1` 里的 hash 逻辑提取成一个独立的命令行工具
（`civhash.exe`），并把它注册成 DSH 里 AI 可直接调用的工具。

Civ VI 的引擎里，几乎所有资源名最终都变成一个 32 位 hash：地形、资源、单位、骨骼、动画轨道……
本工具能正向算这个 hash，也能在已知前缀的前提下把后缀**反推**出来——排查「这串 hash 原本是哪个名字」时很有用。

## 能做什么

| 能力 | 说明 |
|---|---|
| Civ 风格 CRC32 | 逐字复刻 `ComputeHash`（返回原始状态、**无**最终异或），同时输出标准 CRC32 便于对照 |
| 引擎 FNV-1a | `Platform::strhash` / `StaticHash_FNV1A` 语义，支持自定义 seed 与大小写折叠变体（`--fold`） |
| 骨骼链 hash | 沿父链自叶向根累加骨骼名，再累加轨道名，最后与 `0xFFFFFFFF` 异或 |
| 反推后缀 | 已知前缀 + 目标 hash，在可控字符集内暴力搜索后缀（CRC 与 FNV 都支持） |

## 目录结构

```
dsh-civhash/
├── cli/
│   ├── civhash.cpp          # CLI 源码；CRC32 表 / ComputeHash 逻辑逐字取自原工程
│   ├── build.cmd            # MinGW g++ 编译（Windows）
│   └── build.sh             # 同上，bash 版
├── bin/
│   └── civhash.exe          # 随包分发的构建产物（由 cli/build 生成）
├── src/
│   └── index.ts             # DSH 插件入口：注册 _dsh_external_dsh_civhash_hash
├── scripts/
│   ├── build.sh             # 编译 TypeScript（src → lib）
│   └── test.mjs             # 对 bin/civhash.exe 跑已知向量
├── cordis.patch.yml         # DSH bundle patch
├── LICENSE                  # BSD-3-Clause
├── NOTICE                   # 上游 CivNexus6 署名
└── package.json
```

## civhash.exe 用法

```bat
civhash <text>                                      :: Civ 风格 CRC32（raw + 标准）
civhash <text> --fnv [--seed N] [--fold]            :: 引擎 FNV-1a
civhash <track> --bone Root/BoneA/BoneB             :: 骨骼链 hash（路径自根到叶，末位为目标骨骼）
civhash --reverse <target>[,<target>...] [opts]     :: 反推后缀（逗号分隔可批量）

  --prefix <P>     已知字段前缀（默认空）
  --max <N>        后缀最大长度 1..6（默认 5；大字符集下 max=6 需数十秒）
  --charset <S>    搜索字符集（默认 A-Z 0-9 _）

数值格式：默认 hex；--int 有符号 int32；--uint 无符号 uint32（reverse 下忽略）
通用：--json 输出 JSON；-h / --help 帮助
```

反推时必须用 **raw** 值作为目标——`raw` 才是 `ComputeHash` 的原语义，标准 CRC32 是它再异或 `0xFFFFFFFF` 的结果。

示例：

```console
$ civhash 123456789 --json
{"ok":true,"mode":"hash","algo":"crc","input":"123456789","fmt":"hex",
 "raw":"0x340BC6D9","std":"0xCBF43926","rawValue":873187033,"stdValue":3421780262}

$ civhash Idle --bone Root/Spine/Head --json
{"ok":true,"mode":"bone","algo":"crc","track":"Idle","bone":"Root/Spine/Head",
 "order":["Head","Spine","Root"],"hash":"0xFF0361A1","hashValue":4278411681}

$ civhash --reverse 0x16D22538 --prefix TERRA --max 2
algo     : CRC32 (raw 语义)
prefix   : "TERRA"  prefix_state=0xBEEA7DE8
charset  : 37 字符, maxlen=2, 搜索空间 ~1407
target[0] : 0x16D22538  found 1
  7   ->  TERRA7  verify=0x16D22538
```

## 作为 AI 工具使用

插件把 CLI 注册为 `_dsh_external_dsh_civhash_hash`，参数与 CLI 一一对应：

| 参数 | 类型 | 说明 |
|---|---|---|
| `text` | string | 要哈希的文本（如 `TERRAIN_COAST`）；reverse 模式可省 |
| `bone` | string | 骨骼链 `Root/A/B`（自根到叶）；提供时 `text` 作为轨道名 |
| `reverse` | string | 目标值（`0x` 十六进制或十进制，raw 语义）；提供时进入反推模式 |
| `prefix` | string | 反推：已知字段前缀 |
| `max` | number | 反推：后缀最大长度 1-6（默认 5） |
| `charset` | string | 反推：搜索字符集（默认 `A-Z 0-9 _`） |
| `fnv` | boolean | 改用引擎 FNV-1a；reverse / bone 同步切换 |
| `seed` | string | FNV 种子（默认 `0x811C9DC5`，仅 `fnv` 生效） |
| `fold` | boolean | FNV 大小写折叠变体（仅 `fnv` 生效） |
| `int` / `uint` | boolean | 输出 int32 / uint32 十进制（默认 hex；reverse 下无效） |

## 安装

```bash
dsh plugin --profile web add link:/path/to/dsh-civhash
# 然后把 "@dsh-external/dsh-civhash" 加进 profile package.json 的 dsh.profile.bundles
```

插件默认使用包内自带的 `bin/civhash.exe`；要指向别处的可执行文件，用 `config.exePath` 覆盖。

## 构建

```bash
# TypeScript 插件（src → lib）
bash scripts/build.sh

# CLI（产出 cli/civhash.exe；Windows 下也可用 cli\build.cmd）
bash cli/build.sh
cp cli/civhash.exe bin/civhash.exe
```

> `node_modules` 由 `scripts/build.sh` 自动建立 junction 链接，指向本机已有的
> `dsh-super-injector` / npm 版 dsh 依赖树；仓库本身不提交 `node_modules`。

## 测试

```bash
node scripts/test.mjs      # 或 npm test
```

对包内 `bin/civhash.exe` 跑一组已知向量（标准 CRC32 / Civ raw / FNV / 折叠变体 / 骨骼链 /
反推往返 / int·uint 格式 / 退出码）。重编 exe 后若 hash 语义有变，这里会直接失败。
用 `CIVHASH_EXE` 环境变量可指定其他可执行文件。

## 验证记录

- 标准向量：`std("123456789") = 0xCBF43926` ✓
- 与原工程旧 exe 对照：`NOTIFICATION_ADDED = 0xF26F3ACF` ✓
- 重编的 `cli/civhash.exe` 与原有 `bin/civhash.exe` 对 12 组用例输出逐字节一致 ✓
- 骨骼 hash 与 node `zlib` 交叉验证（叶→根名字 + 轨道名的标准 CRC32）✓
- 反推往返：`raw("TERRA7") = 0x16D22538`，prefix `TERRA` → 精确找回 `7` ✓
- FNV 反推往返：`fnv("UNIT_ARCHER") = 0x3C250997`，prefix `UNIT_ARCHER` → 空后缀命中 ✓

## 致谢与许可

hash 逻辑（CRC32 表、`ComputeHash`、骨骼层级 hash）提取自
[deliverator23/CivNexus6](https://github.com/deliverator23/CivNexus6)，原作者 deliverator23。
本仓库仅包含这一小部分逻辑及其 DSH 封装。

以 BSD-3-Clause 许可发布，见 [LICENSE](LICENSE)。本工具与 Firaxis / 2K 无关，
不包含任何游戏本体资源。
