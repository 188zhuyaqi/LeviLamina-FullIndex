<script setup>
import { onMounted, ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import { query } from '../api/client.js'

const loading = ref(true)
const rows = ref([])

async function load() {
  loading.value = true
  try {
    const response = await query('drops.list')
    rows.value = response.data?.items ?? []
  } finally {
    loading.value = false
  }
}

onMounted(load)
</script>

<template>
  <section>
    <PageHeader title="掉落物" description="按维度/区块聚合并支持数量降序与分页">
      <el-button @click="load" :loading="loading">刷新</el-button>
    </PageHeader>

    <el-card shadow="never" class="panel-card">
      <el-skeleton v-if="loading" :rows="9" animated />
      <el-empty v-else-if="rows.length === 0" description="当前阶段尚无可展示数据" />
      <el-table v-else :data="rows" stripe>
        <el-table-column prop="itemId" label="物品" min-width="240" />
        <el-table-column prop="stackCount" label="数量" min-width="100" />
        <el-table-column prop="dimension" label="维度" min-width="120" />
        <el-table-column prop="chunkX" label="区块 X" min-width="100" />
        <el-table-column prop="chunkZ" label="区块 Z" min-width="100" />
      </el-table>
    </el-card>
  </section>
</template>
