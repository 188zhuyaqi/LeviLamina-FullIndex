<script setup>
import { ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import RealtimeQueryToggle from '../components/RealtimeQueryToggle.vue'
import TypeSelect from '../components/TypeSelect.vue'
import ItemTable from '../components/ItemTable.vue'
import TablePager from '../components/TablePager.vue'
import { useIndexedData } from '../composables/useIndexedData.js'
import { indexedDetail, query } from '../api/client.js'
import { containerLabel, dimensionLabel, sourceLabel } from '../i18n/index.js'

const selected = ref(null)
const dialogVisible = ref(false)
const detailLoading = ref(false)
const detailError = ref('')
const detailReadAt = ref(null)
const {
  loading, rows, total, page, pageSize, filters, error, load,
  changePage, changePageSize, applyFilters, resetFilters, changeSort,
  realtimeAvailable, realtimeEnabled, realtimeRunning, realtimeComplete,
  realtimePhase, realtimeProgress, realtimeReceived, setRealtimeEnabled, cancelRealtime
} = useIndexedData('containers', {
  initialFilters: {
    typeId: '', name: '', dimension: '', source: '', chunkX: null, chunkZ: null,
    x: null, y: null, z: null
  }
})

async function showDetails(row) {
  selected.value = row
  dialogVisible.value = true
  detailError.value = ''
  detailReadAt.value = null
  detailLoading.value = true
  try {
    if (realtimeEnabled.value) {
      const response = await query('containers.get', {
        dimension: row.dimension, x: row.position?.x, y: row.position?.y, z: row.position?.z
      })
      selected.value = response.data?.item ?? row
      detailReadAt.value = response.data?.readAt ?? Date.now()
    } else {
      const response = await indexedDetail('containers', row.id)
      selected.value = response.item
    }
  } catch (error) {
    detailError.value = error.message
  } finally {
    detailLoading.value = false
  }
}

function coordinate(value) {
  return Number.isFinite(Number(value)) ? Number(value).toFixed(1) : '-'
}
</script>

<template>
  <section>
    <PageHeader title="容器" description="默认查询最近完整快照；实时检索按需扫描当前容器数据">
      <template #title-extra>
        <RealtimeQueryToggle
          v-if="realtimeAvailable" :model-value="realtimeEnabled" :running="realtimeRunning"
          :complete="realtimeComplete" :phase="realtimePhase" :progress="realtimeProgress"
          :received="realtimeReceived" @update:model-value="setRealtimeEnabled" @cancel="cancelRealtime"
        />
      </template>
      <el-button @click="load" :loading="loading">刷新</el-button>
    </PageHeader>

    <el-alert v-if="error" :title="error" type="error" show-icon class="mb-16" />

    <el-card shadow="never" class="panel-card data-panel">
      <el-form class="filter-bar" inline @submit.prevent="applyFilters">
        <el-form-item label="容器类型">
          <TypeSelect v-model="filters.typeId" kind="containers" :realtime="realtimeEnabled" />
        </el-form-item>
        <el-form-item label="物品名称">
          <el-input v-model="filters.name" clearable placeholder="模糊搜索显示名称或自定义名称" @keyup.enter="applyFilters" />
        </el-form-item>
        <el-form-item label="维度">
          <el-select v-model="filters.dimension" clearable placeholder="全部维度">
            <el-option label="主世界" value="overworld" />
            <el-option label="下界" value="nether" />
            <el-option label="末地" value="the_end" />
          </el-select>
        </el-form-item>
        <el-form-item label="来源">
          <el-select v-model="filters.source" clearable placeholder="全部来源">
            <el-option label="运行态" value="runtime" />
            <el-option label="存储态" value="storage" />
          </el-select>
        </el-form-item>
        <el-form-item label="区块">
          <div class="range-pair compact-pair">
            <el-input-number v-model="filters.chunkX" :controls="false" placeholder="区块 X" />
            <el-input-number v-model="filters.chunkZ" :controls="false" placeholder="区块 Z" />
          </div>
        </el-form-item>
        <el-form-item label="坐标">
          <div class="coordinate-triplet">
            <el-input-number v-model="filters.x" :controls="false" placeholder="X" aria-label="容器 X 坐标" />
            <el-input-number v-model="filters.y" :controls="false" placeholder="Y" aria-label="容器 Y 坐标" />
            <el-input-number v-model="filters.z" :controls="false" placeholder="Z" aria-label="容器 Z 坐标" />
          </div>
        </el-form-item>
        <el-form-item class="filter-actions">
          <el-button type="primary" native-type="submit">查询</el-button>
          <el-button @click="resetFilters">重置</el-button>
        </el-form-item>
      </el-form>

      <el-table :data="rows" height="100%" stripe v-loading="loading" @sort-change="changeSort">
        <el-table-column prop="kind" label="类型" min-width="150" sortable="custom">
          <template #default="{ row }">{{ containerLabel(row.kind) }}</template>
        </el-table-column>
        <el-table-column prop="source" label="来源" width="100" sortable="custom">
          <template #default="{ row }">{{ sourceLabel(row.source) }}</template>
        </el-table-column>
        <el-table-column prop="dimension" label="维度" width="110" sortable="custom">
          <template #default="{ row }">{{ dimensionLabel(row.dimension) }}</template>
        </el-table-column>
        <el-table-column prop="chunkX" label="区块 X" width="105" sortable="custom" />
        <el-table-column prop="chunkZ" label="区块 Z" width="105" sortable="custom" />
        <el-table-column prop="position.x" label="X" width="100" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.position?.x) }}</template>
        </el-table-column>
        <el-table-column prop="position.y" label="Y" width="90" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.position?.y) }}</template>
        </el-table-column>
        <el-table-column prop="position.z" label="Z" width="100" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.position?.z) }}</template>
        </el-table-column>
        <el-table-column prop="occupiedSlots" label="已用槽位" width="110" sortable="custom" />
        <el-table-column prop="itemCount" label="物品数" width="100" sortable="custom" />
        <el-table-column label="操作" fixed="right" width="90">
          <template #default="{ row }">
            <el-button link type="primary" @click="showDetails(row)">查看详情</el-button>
          </template>
        </el-table-column>
      </el-table>

      <TablePager
        :page="page"
        :page-size="pageSize"
        :total="total"
        @page-change="changePage"
        @size-change="changePageSize"
      />
    </el-card>

    <el-dialog v-model="dialogVisible" title="容器详细数据" width="82%" top="6vh" destroy-on-close>
      <el-alert
        v-if="realtimeEnabled"
        :title="detailReadAt ? `实时读取于 ${new Date(detailReadAt).toLocaleString('zh-CN')}` : '正在读取当前容器内容；此详情不写入快照'"
        type="info" show-icon :closable="false" class="mb-16"
      />
      <el-alert v-if="detailError" :title="detailError" type="error" show-icon class="mb-16" />
      <template v-if="selected">
        <el-descriptions :column="4" border class="detail-summary">
          <el-descriptions-item label="类型">{{ containerLabel(selected.kind) }}</el-descriptions-item>
          <el-descriptions-item label="来源">{{ sourceLabel(selected.source) }}</el-descriptions-item>
          <el-descriptions-item label="维度">{{ dimensionLabel(selected.dimension) }}</el-descriptions-item>
          <el-descriptions-item label="区块">{{ selected.chunkX }}, {{ selected.chunkZ }}</el-descriptions-item>
          <el-descriptions-item label="坐标" :span="2">
            {{ coordinate(selected.position?.x) }}, {{ coordinate(selected.position?.y) }}, {{ coordinate(selected.position?.z) }}
          </el-descriptions-item>
          <el-descriptions-item label="已用槽位">{{ selected.occupiedSlots }}</el-descriptions-item>
          <el-descriptions-item label="物品总数">{{ selected.itemCount }}</el-descriptions-item>
        </el-descriptions>
        <div class="dialog-table" v-loading="detailLoading">
          <ItemTable :items="selected.items" empty-text="空容器" />
        </div>
      </template>
    </el-dialog>
  </section>
</template>
