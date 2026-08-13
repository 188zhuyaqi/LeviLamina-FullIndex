# LeviLamina-FullIndex / 全服索引

面向 LeviLamina Bedrock Dedicated Server 的存档/运行态索引与管理面板。

## 目标

- 运行态：在线玩家、已加载实体、掉落物、已加载容器
- 存储态：离线玩家、未加载区块、BlockActor、ActorStorage
- 统一数据模型：Web 不关心数据来自 Runtime 还是 Storage
- WebSocket：插件与 Node.js 网关实时通信
- Vue 3 + Element Plus：管理面板
- GitHub Actions：构建插件 DLL、Web 静态资源和 Node 网关

## 一致性原则

1. 已加载对象以 BDS Runtime 为权威源。
2. 未加载对象以 BDS DBStorage / LevelStorage 为权威源。
3. 严格审计使用 Snapshot 模式，不把活动世界的长时间扫描当成一致性快照。
4. Node/Web 不直接写 Minecraft 存档，所有未来写操作必须回到 C++ MutationEngine。

## 当前里程碑

### M0 - 基础骨架
- [x] LeviLamina NativeMod 工程结构
- [x] RuntimeProvider / StorageProvider 抽象
- [x] WebSocket 协议与插件客户端
- [x] Node.js 双向 WS 网关
- [x] Vue 3 + Element Plus 面板骨架
- [x] Dashboard / 玩家 / 容器 / 掉落物 / 实体页面
- [x] GitHub Actions 构建骨架

### M1 - Runtime 读取
- [x] 在线玩家完整信息
- [x] 背包 / 快捷栏 / 装备 / 副手 / 末影箱
- [x] 已加载 BlockActor / 实体容器
- [x] ItemActor 掉落物按区块聚合与分页
- [x] Actor 普通生物 / 特殊实体分类
- [x] 所有 Runtime 查询切回服务器主线程执行

### M2 - Storage 读取
- [x] PlayerDataSystem 离线玩家
- [x] DBStorage 区块键只读枚举
- [x] 未加载 BlockActor 容器
- [x] ActorStorage 实体与掉落物
- [x] 嵌套 ItemStack（潜影盒 / 收纳袋）递归解析（最大 8 层）

> Storage 扫描只复用 BDS 持有的 `DBStorage / LevelStorage`，服务器运行时不会再次打开世界 LevelDB。

### M3 - 索引数据库
- [x] Node 24 内置 SQLite 初始实现（WAL）
- [x] 按需实时检索（临时结果、分批、进度、取消）
- [x] 全局物品搜索（玩家 / 容器 / 掉落物 / 嵌套物品）
- [x] 快照保留与物品数量差异比较
- [x] WS 异步索引任务（进度、分批传输、取消）

### M4 - 安全修改
- [ ] 在线对象 Mutation
- [ ] 操作审计
- [ ] 修改前备份
- [ ] 未加载数据安全写回

## 仓库结构

```text
.
├─ plugin/                  C++ LeviLamina 插件
├─ server/                  Node.js WS/API 网关
├─ web/                     Vue 3 + Element Plus
├─ shared/                  协议文档
├─ .github/workflows/       CI / Release
├─ xmake.lua
└─ manifest.json
```

## 本地开发

### 插件

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=server -y
xmake -y
```

### Web / Node

```bash
npm install
npm run dev:server
npm run dev:web
```

默认：
- Web: `http://127.0.0.1:5173`
- Node: `http://127.0.0.1:30110`
- 插件 WS: `ws://127.0.0.1:30110/ws/plugin`
- 浏览器 WS: `ws://127.0.0.1:30110/ws/browser`

面板默认只读取 Node SQLite 中最近一次完成的快照，不会在后台定时扫描或用运行态数据覆盖快照。
“重建只读索引”通过 WS 接收插件批次，所有批次接收完成后才原子写入新快照；取消或失败不会改变
当前快照。

快照中的玩家、容器、掉落物、实体和物品均按记录写入 SQLite。列表筛选、排序、计数和分页由
SQL 完成，只返回当前页摘要；玩家、容器、掉落物及实体区块详情在点击后按记录 ID 或区块条件读取，
不会为显示一页数据而把整类快照 JSON 加载到 Node 内存。

玩家、容器、掉落物、实体和全局物品搜索页在两段 WS 均在线时显示“实时检索”。开启后，查询条件
直接发送给插件，插件读取当前运行态与存档、在插件端筛选并将命中结果分批返回。实时结果仅保存在
当前浏览器页面内存中，用于客户端分页和排序，不写入 SQLite；关闭开关、离开页面或连接中断会
取消任务、清空临时结果并恢复快照模式。

实时容器查询会根据参数选择读取路径：完整 X/Z 坐标或完整区块坐标会直接读取对应的已加载区块
和 Bedrock 区块存储键，不遍历全世界；没有可用于定位区块的条件时才执行全世界扫描。已加载区块
始终以运行内存为准，磁盘中的旧区块内容不会混入结果。实时列表只传输槽位数、物品数等摘要，
点击“查看详情”后再按维度与 XYZ 即时读取完整物品数据。快照重建仍保存全部详情，离线使用不受影响。

容器类型、实体类型、掉落物类型和全局物品类型使用最新快照生成的类型目录，显示为
`原始 ID（中文名称）`。类型条件执行原始 ID 精确匹配，名称条件只模糊匹配显示名称或自定义名称；
实体名称专指命名牌等产生的自定义名称。实时模式仍使用快照类型目录辅助输入，但允许直接键入当前
世界中尚未进入快照的合法原始 ID，查询结果本身不会读取或混入快照。

### 界面国际化

Web 默认使用简体中文。协议与数据库仍保留稳定英文枚举，界面通过 `web/src/i18n/index.js`
集中翻译维度、数据来源、容器类型、实体名称、物品槽位、嵌套路径、索引阶段和常见错误；
物品 ID、实体 ID、XUID、玩家名与坐标保持原始值。Element Plus 组件也使用简体中文语言包。

### 生成发布包

```powershell
npm.cmd run build:release
```

发布结果位于 `release/`，其中服务端依赖已打包进 `main.js`，不需要携带源码、`node_modules`
或执行 `npm install`。将整个 `release/` 目录复制到目标机器（要求 Node.js 24+），执行：

```powershell
node main.js
```

## GitHub Actions

每次推送到 `main` 会同时验证：
- Windows x64 LeviLamina 插件构建，并上传 `bin/` Artifact
- Vue Web 构建与 Node 网关打包
- 推送 `v*` 标签时生成 Release 压缩包

## 安全

插件 WS 通过 `pluginToken` 做基础鉴权。生产环境应：
- 使用高强度随机 token
- Node 网关不要直接暴露到公网，或放在 HTTPS/WSS 反向代理之后
- Web 管理端增加独立管理员登录
- 未来 Mutation 操作必须二次确认并记录审计日志
