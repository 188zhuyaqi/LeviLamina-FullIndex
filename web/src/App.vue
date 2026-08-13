<script setup>
import {
  Box,
  DataAnalysis,
  Goods,
  Monitor,
  Search,
  Clock,
  User
} from '@element-plus/icons-vue'
import { useRoute } from 'vue-router'
import { useGateway } from './composables/useGateway.js'
import { useIndexJob } from './composables/useIndexJob.js'
import { datasetLabel, errorLabel, phaseLabel } from './i18n/index.js'

const route = useRoute()
const { connected } = useGateway()
const { job, active, submitting, cancel } = useIndexJob()

const menus = [
  { path: '/', label: '概览', icon: DataAnalysis },
  { path: '/players', label: '玩家', icon: User },
  { path: '/containers', label: '容器', icon: Box },
  { path: '/drops', label: '掉落物', icon: Goods },
  { path: '/entities', label: '实体', icon: Monitor },
  { path: '/search', label: '物品搜索', icon: Search },
  { path: '/snapshots', label: '快照对比', icon: Clock }
]
</script>

<template>
  <div class="shell">
    <aside class="sidebar">
      <div class="brand">
        <div class="brand-mark">FI</div>
        <div>
          <strong>全服索引</strong>
          <small>FullIndex</small>
        </div>
      </div>

      <nav class="nav">
        <RouterLink
          v-for="item in menus"
          :key="item.path"
          :to="item.path"
          class="nav-item"
          :class="{ active: route.path === item.path }"
        >
          <el-icon><component :is="item.icon" /></el-icon>
          <span>{{ item.label }}</span>
        </RouterLink>
      </nav>
    </aside>

    <main class="main">
      <div v-if="active" class="index-job-bar">
        <div class="index-job-copy">
          <strong>正在构建索引 · {{ job?.kind ? datasetLabel(job.kind) : '准备中' }}</strong>
          <span>{{ phaseLabel(job?.phase, '处理中') }}</span>
        </div>
        <el-progress :percentage="job?.percent ?? 0" :stroke-width="8" />
        <el-button size="small" type="danger" plain :loading="submitting" @click="cancel">取消</el-button>
      </div>
      <el-alert
        v-else-if="job?.status === 'failed'"
        :title="`索引任务失败：${errorLabel(job.error)}`"
        type="error"
        show-icon
        class="job-alert"
      />
      <div class="gateway-state" :class="{ online: connected }">
        {{ connected ? '实时通道已连接' : '实时通道重连中' }}
      </div>
      <RouterView v-slot="{ Component }">
        <Transition name="page" mode="out-in">
          <component :is="Component" class="route-page" />
        </Transition>
      </RouterView>
    </main>
  </div>
</template>

<style scoped>
.index-job-bar { display: grid; grid-template-columns: minmax(210px, auto) minmax(220px, 1fr) auto; gap: 16px; align-items: center; margin-bottom: 16px; padding: 12px 16px; border: 1px solid #bfdbfe; border-radius: 10px; background: #eff6ff; }
.index-job-copy { display: grid; gap: 2px; }
.index-job-copy span { color: #64748b; font-size: 12px; }
.gateway-state { position: fixed; right: 18px; bottom: 16px; z-index: 20; padding: 5px 9px; border-radius: 999px; color: #64748b; background: rgba(255,255,255,.9); box-shadow: 0 2px 10px rgba(15,23,42,.12); font-size: 12px; }
.gateway-state::before { content: ''; display: inline-block; width: 7px; height: 7px; margin-right: 6px; border-radius: 50%; background: #f59e0b; }
.gateway-state.online::before { background: #22c55e; }
.job-alert { margin-bottom: 16px; }
@media (max-width: 800px) { .index-job-bar { grid-template-columns: 1fr auto; } .index-job-bar :deep(.el-progress) { grid-column: 1 / -1; grid-row: 2; } }
</style>
