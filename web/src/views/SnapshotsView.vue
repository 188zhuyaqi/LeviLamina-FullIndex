<script setup>
import { onMounted, ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import { diffSnapshots, listSnapshots } from '../api/client.js'

const loading = ref(true)
const snapshots = ref([])
const fromId = ref(null)
const toId = ref(null)
const changes = ref([])
const error = ref('')

function dateText(timestamp) {
  return new Date(timestamp).toLocaleString('zh-CN', { hour12: false })
}

async function load() {
  loading.value = true
  error.value = ''
  try {
    const response = await listSnapshots()
    snapshots.value = response.items ?? []
    if (snapshots.value.length >= 2) {
      toId.value = snapshots.value[0].id
      fromId.value = snapshots.value[1].id
    }
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
}

async function compare() {
  if (!fromId.value || !toId.value) return
  loading.value = true
  error.value = ''
  try {
    const response = await diffSnapshots(fromId.value, toId.value)
    changes.value = response.items ?? []
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
}

onMounted(load)
</script>

<template>
  <section>
    <PageHeader title="快照与差异" description="比较两次严格分离的索引结果，不把活动世界扫描伪装成事务快照">
      <el-button :loading="loading" @click="load">刷新列表</el-button>
    </PageHeader>

    <el-alert v-if="error" :title="error" type="error" show-icon class="mb-16" />

    <el-card shadow="never" class="panel-card mb-16">
      <div class="compare-bar">
        <el-select v-model="fromId" placeholder="起始快照">
          <el-option v-for="item in snapshots" :key="item.id" :label="`#${item.id} · ${dateText(item.created_at)}`" :value="item.id" />
        </el-select>
        <span>→</span>
        <el-select v-model="toId" placeholder="目标快照">
          <el-option v-for="item in snapshots" :key="item.id" :label="`#${item.id} · ${dateText(item.created_at)}`" :value="item.id" />
        </el-select>
        <el-button type="primary" :disabled="!fromId || !toId" @click="compare">比较物品变化</el-button>
      </div>
    </el-card>

    <el-card shadow="never" class="panel-card data-panel">
      <el-empty v-if="!loading && changes.length === 0" description="选择两个快照进行比较" />
      <el-table v-else :data="changes" height="100%" stripe v-loading="loading">
        <el-table-column prop="itemId" label="物品 ID" min-width="220" />
        <el-table-column prop="displayName" label="名称" min-width="160" />
        <el-table-column prop="before" label="之前" width="100" />
        <el-table-column prop="after" label="之后" width="100" />
        <el-table-column label="变化" width="110">
          <template #default="{ row }">
            <el-tag :type="row.delta > 0 ? 'success' : 'danger'">{{ row.delta > 0 ? '+' : '' }}{{ row.delta }}</el-tag>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </section>
</template>

<style scoped>
.compare-bar { display: grid; grid-template-columns: minmax(240px, 1fr) auto minmax(240px, 1fr) auto; align-items: center; gap: 12px; }
</style>
