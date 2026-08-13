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
- [ ] 在线玩家完整信息
- [ ] 背包 / 快捷栏 / 装备 / 副手 / 末影箱
- [ ] 已加载 BlockActor 容器
- [ ] ItemActor 掉落物按区块聚合
- [ ] Actor 普通生物 / 特殊实体分类

### M2 - Storage 读取
- [ ] PlayerDataSystem 离线玩家
- [ ] DBStorage / DBChunkStorage 区块枚举
- [ ] 未加载 BlockActor
- [ ] ActorStorage
- [ ] 嵌套 ItemStack（潜影盒 / 收纳袋）递归解析

### M3 - 索引数据库
- [ ] SQLite 初始实现
- [ ] 增量更新
- [ ] 全局物品搜索
- [ ] 快照与差异比较

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
