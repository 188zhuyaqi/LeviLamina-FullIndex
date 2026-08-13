<script setup>
import { onMounted, ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import { query } from '../api/client.js'

const loading = ref(true)
const rows = ref([])

async function load() {
  loading.value = true
  try {
    const response = await query('entities.list')
    rows.value = response.data?.items ?? []
  } finally {
    loading.value = false
  }
}

onMounted(load)
</script>

<template>
  <section>
    <PageHeader title="实体" description="普通生物与盔甲架、船、矿车等特殊实体分开统计">
      <el-button @click="load" :loading="loading">刷新</el-button>
    </PageHeader>

    <el-card shadow="never" class="panel-card">
      <el-skeleton v-if="loading" :rows="9" animated />
      <el-empty v-else-if="rows.length === 0" description="当前阶段尚无可展示数据" />
      <el-table v-else :data="rows" stripe>
        <el-table-column prop="typeName" label="实体类型" min-width="240" />
        <el-table-column prop="category" label="分类" min-width="140" />
        <el-table-column prop="dimension" label="维度" min-width="120" />
        <el-table-column prop="chunkX" label="区块 X" min-width="100" />
        <el-table-column prop="chunkZ" label="区块 Z" min-width="100" />
      </el-table>
    </el-card>
  </section>
</template>
