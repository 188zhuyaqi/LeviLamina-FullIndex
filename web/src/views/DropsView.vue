<script setup>
import { ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import RealtimeQueryToggle from '../components/RealtimeQueryToggle.vue'
import TablePager from '../components/TablePager.vue'
import TypeSelect from '../components/TypeSelect.vue'
import { useIndexedData } from '../composables/useIndexedData.js'
import { indexedDetail } from '../api/client.js'
import { dimensionLabel, sourceLabel } from '../i18n/index.js'

const selected = ref(null)
const dialogVisible = ref(false)
const detailLoading = ref(false)
const {
  loading, rows, total, page, pageSize, filters, error, load,
  changePage, changePageSize, applyFilters, resetFilters, changeSort,
  realtimeAvailable, realtimeEnabled, realtimeRunning, realtimeComplete,
  realtimePhase, realtimeProgress, realtimeReceived, setRealtimeEnabled, cancelRealtime
} = useIndexedData('drops', {
  initialFilters: { typeId: '', name: '', dimension: '', source: '', chunkX: null, chunkZ: null },
  initialSortField: 'itemCount',
  initialSortOrder: 'descending'
})

async function showDetails(row) {
  selected.value = row
  dialogVisible.value = true
  if (realtimeEnabled.value) return
  detailLoading.value = true
  try {
    selected.value = (await indexedDetail('drops', row.id)).item
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
    <PageHeader title="掉落物" description="默认查询最近完整快照；实时检索按需读取当前掉落物">
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
        <el-form-item label="物品类型">
          <TypeSelect v-model="filters.typeId" kind="drops" :realtime="realtimeEnabled" />
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
        <el-form-item class="filter-actions">
          <el-button type="primary" native-type="submit">查询</el-button>
          <el-button @click="resetFilters">重置</el-button>
        </el-form-item>
      </el-form>

      <el-table :data="rows" height="100%" stripe v-loading="loading" @sort-change="changeSort">
        <el-table-column prop="source" label="来源" width="100" sortable="custom">
          <template #default="{ row }">{{ sourceLabel(row.source) }}</template>
        </el-table-column>
        <el-table-column prop="dimension" label="维度" min-width="120" sortable="custom">
          <template #default="{ row }">{{ dimensionLabel(row.dimension) }}</template>
        </el-table-column>
        <el-table-column prop="chunkX" label="区块 X" min-width="110" sortable="custom" />
        <el-table-column prop="chunkZ" label="区块 Z" min-width="110" sortable="custom" />
        <el-table-column prop="entityCount" label="掉落实体" min-width="115" sortable="custom" />
        <el-table-column prop="itemCount" label="物品总数" min-width="115" sortable="custom" />
        <el-table-column prop="distinctItemCount" label="物品种类" min-width="110" sortable="custom" />
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

    <el-dialog v-model="dialogVisible" title="区块掉落物详情" width="86%" top="5vh" destroy-on-close>
      <template v-if="selected" v-loading="detailLoading">
        <el-descriptions :column="4" border class="detail-summary">
          <el-descriptions-item label="来源">{{ sourceLabel(selected.source) }}</el-descriptions-item>
          <el-descriptions-item label="维度">{{ dimensionLabel(selected.dimension) }}</el-descriptions-item>
          <el-descriptions-item label="区块">{{ selected.chunkX }}, {{ selected.chunkZ }}</el-descriptions-item>
          <el-descriptions-item label="物品种类">{{ selected.distinctItemCount }}</el-descriptions-item>
          <el-descriptions-item label="掉落实体">{{ selected.entityCount }}</el-descriptions-item>
          <el-descriptions-item label="物品总数">{{ selected.itemCount }}</el-descriptions-item>
        </el-descriptions>
        <el-table :data="selected.items" stripe class="dialog-table">
          <el-table-column prop="itemId" label="物品 ID" min-width="220" sortable />
          <el-table-column prop="displayName" label="名称" min-width="150" sortable />
          <el-table-column prop="stackCount" label="物品数" width="100" sortable />
          <el-table-column prop="entityCount" label="实体数" width="100" sortable />
          <el-table-column label="坐标" min-width="320">
            <template #default="{ row }">
              <div class="position-list">
                <span v-for="(position, index) in row.positions" :key="index">
                  {{ coordinate(position.x) }}, {{ coordinate(position.y) }}, {{ coordinate(position.z) }}
                </span>
              </div>
            </template>
          </el-table-column>
        </el-table>
      </template>
    </el-dialog>
  </section>
</template>
