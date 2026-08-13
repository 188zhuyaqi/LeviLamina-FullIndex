# 架构说明

## 数据权威规则

### 实时查询
- 在线玩家 -> RuntimeProvider
- 已加载区块/Actor/BlockActor -> RuntimeProvider
- 离线玩家 -> StorageProvider
- 未加载区块/ActorStorage/BlockActor -> StorageProvider
- 同一对象同时存在两份数据时，Runtime 优先

### 一致性审计
资产审计、刷物品排查等需要严格时间边界的功能使用 SnapshotProvider（后续实现），而不是边运行边扫整个活动世界。

## Provider 层

`IDataProvider` 是 Web/IndexService 与 BDS 细节之间的边界。

这样做的目的：
1. Web 协议不受 BDS 版本影响
2. Runtime/Storage 可以独立测试
3. 后续 BDS 升级只修改 Provider 适配层
4. 索引数据库不关心原始来源

## Storage 原则

服务器在线时禁止插件自己 `leveldb::DB::Open(worlds/.../db)`。

优先通过：
- `ll::service::getDBStorage()`
- `LevelStorage`
- `DBChunkStorage`
- `PlayerDataSystem`

使用 BDS 当前持有的存储对象。

## Mutation

未来所有修改统一进入：

```text
Web
 -> Node Gateway
 -> C++ MutationEngine
 -> Runtime / Storage
```

Node 永远不直接操作 Minecraft LevelDB。
