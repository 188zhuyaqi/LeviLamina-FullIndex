# FullIndex WebSocket Protocol v1

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
  "protocolVersion": 1,
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

## v1 Action

- `system.capabilities`
- `players.list`
- `containers.list`
- `drops.list`
- `entities.list`

后续：

- `index.start`
- `index.progress`
- `index.cancel`
- `search.items`
- `snapshot.create`
- `snapshot.diff`
- `mutation.*`
