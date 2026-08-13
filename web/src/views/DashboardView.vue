<script setup>
import { onMounted, ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import StatCard from '../components/StatCard.vue'
import { indexStatus, indexedData, listServers, query } from '../api/client.js'
import { useGateway } from '../composables/useGateway.js'

const loading = ref(true)
const serverOnline = ref(false)
const capabilities = ref({})
const playerCount = ref(0)
const snapshot = ref(null)
const { connected } = useGateway()

async function refresh() {
  loading.value = true
  try {
    const servers = await listServers()
    serverOnline.value = servers.items?.some(item => item.serverId === 'default') ?? false

    if (serverOnline.value) {
      const [caps, players, storedIndex] = await Promise.all([
        query('system.capabilities'),
        indexedData('players', { pageSize: 500 }),
        indexStatus()
      ])
      capabilities.value = caps.data ?? {}
      playerCount.value = players.items?.filter(player => player.online).length ?? 0
      snapshot.value = storedIndex.snapshot
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
        面板实时连接 {{ connected ? '已连接' : '未连接' }}
      </el-tag>
    </div>

    <div class="stats-grid">
      <StatCard label="在线玩家" :value="playerCount" hint="运行态" :loading="loading" />
      <StatCard label="运行态数据源" :value="capabilities.runtime ? '可用' : '不可用'" hint="已加载对象" :loading="loading" />
      <StatCard label="存储态数据源" :value="capabilities.storage ? '可用' : '不可用'" hint="持久化入口" :loading="loading" />
      <StatCard label="索引物品记录" :value="snapshot?.item_record_count ?? 0" hint="最新数据库快照" :loading="loading" />
    </div>

    <el-card class="panel-card" shadow="never">
      <template #header>
        <div class="card-title">当前开发状态</div>
      </template>
      <el-alert
        title="运行态与存储态只读索引已接通"
        description="已加载对象由运行态数据覆盖，离线玩家与未加载区块来自服务器持有的存储数据库；索引快照用于搜索和差异比较。"
        type="success"
        show-icon
        :closable="false"
      />
    </el-card>
  </section>
</template>
