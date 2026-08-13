<script setup>
import { onMounted, ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import { query } from '../api/client.js'

const loading = ref(true)
const rows = ref([])
const error = ref('')

async function load() {
  loading.value = true
  error.value = ''
  try {
    const response = await query('players.list')
    rows.value = response.data?.items ?? []
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
    <PageHeader title="玩家" description="在线玩家优先读取 Runtime，离线玩家后续由 Storage 补齐">
      <el-button @click="load" :loading="loading">刷新</el-button>
    </PageHeader>

    <el-alert v-if="error" :title="error" type="error" show-icon class="mb-16" />

    <el-card shadow="never" class="panel-card">
      <el-skeleton v-if="loading" :rows="8" animated />
      <el-table v-else :data="rows" stripe height="calc(100vh - 220px)">
        <el-table-column prop="name" label="玩家名称" min-width="160" />
        <el-table-column prop="xuid" label="XUID" min-width="220" show-overflow-tooltip />
        <el-table-column label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="row.online ? 'success' : 'info'" size="small">
              {{ row.online ? '在线' : '离线' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="dimension" label="维度" width="120" />
        <el-table-column label="坐标" min-width="220">
          <template #default="{ row }">
            <span v-if="row.position">
              {{ row.position.x?.toFixed?.(1) ?? row.position.x }},
              {{ row.position.y?.toFixed?.(1) ?? row.position.y }},
              {{ row.position.z?.toFixed?.(1) ?? row.position.z }}
            </span>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </section>
</template>
