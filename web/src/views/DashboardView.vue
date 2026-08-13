<script setup>
import { onMounted, ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import StatCard from '../components/StatCard.vue'
import { listServers, query } from '../api/client.js'
import { useGateway } from '../composables/useGateway.js'

const loading = ref(true)
const serverOnline = ref(false)
const capabilities = ref({})
const playerCount = ref(0)
const { connected } = useGateway()

async function refresh() {
  loading.value = true
  try {
    const servers = await listServers()
    serverOnline.value = servers.items?.some(item => item.serverId === 'default') ?? false

    if (serverOnline.value) {
      const [caps, players] = await Promise.all([
        query('system.capabilities'),
        query('players.list')
      ])
      capabilities.value = caps.data ?? {}
      playerCount.value = players.data?.items?.length ?? 0
    }
  } finally {
    loading.value = false
  }
}

onMounted(refresh)
</script>

<template>
  <section>
    <PageHeader
      title="服务器概览"
      description="实时运行态与持久化索引的统一入口"
    >
      <el-button @click="refresh" :loading="loading">刷新</el-button>
    </PageHeader>

    <div class="status-line">
      <el-tag :type="serverOnline ? 'success' : 'danger'" effect="light">
        插件 {{ serverOnline ? '在线' : '离线' }}
      </el-tag>
      <el-tag :type="connected ? 'success' : 'info'" effect="light">
        面板 WS {{ connected ? '已连接' : '未连接' }}
      </el-tag>
    </div>

    <div class="stats-grid">
      <StatCard label="在线玩家" :value="playerCount" hint="Runtime" :loading="loading" />
      <StatCard label="Runtime Provider" :value="capabilities.runtime ? '可用' : '不可用'" hint="已加载对象" :loading="loading" />
      <StatCard label="Storage Provider" :value="capabilities.storage ? '可用' : '不可用'" hint="持久化入口" :loading="loading" />
      <StatCard label="协议版本" value="1" hint="WebSocket JSON" :loading="loading" />
    </div>

    <el-card class="panel-card" shadow="never">
      <template #header>
        <div class="card-title">当前开发状态</div>
      </template>
      <el-alert
        title="第一阶段先完成只读索引"
        description="未加载区块/离线玩家正在按 v26.10.11 的 DBStorage 与 ActorStorage 结构逐项验证；写存档功能不会提前开放。"
        type="info"
        show-icon
        :closable="false"
      />
    </el-card>
  </section>
</template>
