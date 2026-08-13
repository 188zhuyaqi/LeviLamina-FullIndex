# FullIndex WebSocket Protocol v3

## 角色

- Plugin：LeviLamina C++ 插件，主动连接 Node `/ws/plugin`
- Browser：Vue 页面连接 Node `/ws/browser`
- Gateway：Node.js，负责鉴权、请求关联、浏览器推送

## Plugin 握手

HTTP headers：

```text
Authorization: Bearer <pluginToken>
X-FullIndex-Server-Id: default
```

连接成功后：

```json
{
  "type": "hello",
  "role": "plugin",
  "serverId": "default",
  "protocolVersion": 3,
  "pluginVersion": "0.1.0-dev"
}
```

## 请求

Node -> Plugin

```json
{
  "type": "request",
  "requestId": "uuid",
  "action": "players.list",
  "params": {}
}
```

## 成功响应

Plugin -> Node

```json
{
  "type": "response",
  "requestId": "uuid",
  "action": "players.list",
  "ok": true,
  "data": {
    "items": []
  }
}
```

## 错误响应

```json
{
  "type": "response",
  "requestId": "uuid",
  "action": "players.list",
  "ok": false,
  "error": "..."
}
```

## 查询 Action

- `system.capabilities`
- `players.list`
- `containers.list`
- `containers.get`（按 `dimension/x/y/z` 即时读取单个容器完整详情）
- `drops.list`
- `entities.list`

列表动作 `containers.list`、`drops.list`、`entities.list` 支持：

```json
{ "page": 1, "pageSize": 50 }
```

Node 重建 SQLite 索引时使用内部参数 `{ "all": true }` 获取完整结果。响应包含
`total/page/pageSize/pageCount/scope/snapshotConsistent`，Runtime 数据优先覆盖同一 Storage 对象。

`players.list`、`containers.list`、`drops.list`、`entities.list` 支持
`scope: "runtime" | "storage" | "runtime_and_storage"`。插件记录包含
`source: "runtime" | "storage"`。面板默认不会定时调用这些动作，也不会用运行态数据覆盖快照。

## 异步索引任务

Node 通过 `index.start` 启动任务。插件先立即返回受理响应，扫描结果随后通过同一 WS 分批推送，
避免全存档扫描占用普通请求的 30 秒超时窗口。

```json
{
  "type": "request",
  "requestId": "uuid",
  "action": "index.start",
  "params": { "jobId": "uuid", "batchSize": 100 }
}
```

任务事件：

- `index.progress`：当前数据类别、阶段、百分比和累计数量
- `index.batch`：`players/containers/drops/entities` 的一个数据批次
- `index.complete`：插件扫描与传输完成，Node 提交 SQLite 快照
- `index.failed`：任务失败
- `index.cancelled`：任务取消，已接收但未提交的批次直接丢弃

取消任务使用 `index.cancel`：

```json
{
  "type": "request",
  "requestId": "uuid",
  "action": "index.cancel",
  "params": { "jobId": "uuid" }
}
```

Node 将任务状态统一转换为 `index.job` 推送到 `/ws/browser`。扫描批次只存在于任务内存中，
直到 `index.complete` 后才原子写入新快照；浏览器不会读取未完成的中间结果。

## 按需实时检索

浏览器仅在 Browser -> Node 和 Node -> Plugin 两段 WS 都在线时显示“实时检索”。浏览器直接向
`/ws/browser` 发送：

```json
{
  "type": "live.query.start",
  "serverId": "default",
  "jobId": "uuid",
  "kind": "players",
  "filters": { "keyword": "Alex" },
  "batchSize": 200
}
```

`kind` 支持 `players/containers/drops/entities/items`。Node 将任务转换为插件动作
`live.query.start`，插件使用 `runtime_and_storage` 读取当前数据、在插件端执行筛选并分批发送：

- `live.query.status`：Node 受理、运行或取消状态
- `live.query.progress`：扫描、传输阶段与百分比
- `live.query.batch`：只发送给发起查询的浏览器
- `live.query.complete`：完整结果已经传输
- `live.query.cancelled`：任务已取消
- `live.query.failed`：任务失败或实时连接中断

浏览器通过 `live.query.cancel` 取消任务。浏览器关闭页面、关闭实时检索或 WS 断开时，Node 也会
取消插件任务。实时结果只保存在当前浏览器页面内存中，不写入 SQLite、快照或 Node 临时文件；
客户端使用已收到的结果执行分页和排序。扫描完成前的排序属于当前已接收结果的临时排序。

`containers` 实时查询使用独立查询计划，不读取 SQLite 快照。完整 `x/z` 或 `chunkX/chunkZ`
可定位到区块时，插件直接读取目标运行区块与 BlockEntity 存储键；已加载区块只采用 Runtime 数据。
`live.query.batch` 中的容器是无 `items` 的摘要行。浏览器查看详情时调用 `containers.get`，响应包含
完整 `item`、`readAt` 和 `persisted: false`。`containers.list` 用于快照重建时仍返回完整 `items`。

Node HTTP 索引接口：

- `POST /api/index/refresh`
- `GET /api/index/status`
- `GET /api/index/job`
- `POST /api/index/cancel`
- `GET /api/data/:kind`
- `GET /api/types/:kind`（最新快照中的原始类型目录）
- `GET /api/details/:kind/:id`（按行读取快照详情）
- `GET /api/details/entities/chunk`（按区块及当前筛选读取实体详情）
- `POST /api/search/items`
- `GET /api/snapshots`
- `GET /api/snapshots/diff?from=<id>&to=<id>`

后续：

- `search.items`
- `snapshot.create`
- `snapshot.diff`
- `mutation.*`
