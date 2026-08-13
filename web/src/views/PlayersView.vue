<script setup>
import { ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import RealtimeQueryToggle from '../components/RealtimeQueryToggle.vue'
import ItemTable from '../components/ItemTable.vue'
import TablePager from '../components/TablePager.vue'
import { useIndexedData } from '../composables/useIndexedData.js'
import { indexedDetail } from '../api/client.js'
import { dimensionLabel, sourceLabel } from '../i18n/index.js'

const selected = ref(null)
const dialogVisible = ref(false)
const detailTab = ref('inventory')
const detailLoading = ref(false)
const {
  loading, rows, total, page, pageSize, filters, error, load,
  changePage, changePageSize, applyFilters, resetFilters, changeSort,
  realtimeAvailable, realtimeEnabled, realtimeRunning, realtimeComplete,
  realtimePhase, realtimeProgress, realtimeReceived, setRealtimeEnabled, cancelRealtime
} = useIndexedData('players', {
  initialFilters: { keyword: '', online: '', dimension: '', source: '' },
  initialSortField: 'online',
  initialSortOrder: 'descending'
})

async function showDetails(row) {
  selected.value = row
  detailTab.value = 'inventory'
  dialogVisible.value = true
  if (realtimeEnabled.value) return
  detailLoading.value = true
  try {
    selected.value = (await indexedDetail('players', row.id)).item
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
    <PageHeader title="玩家" description="默认查询最近完整快照；实时检索按需读取当前运行态与存档">
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
        <el-form-item label="搜索">
          <el-input v-model="filters.keyword" clearable placeholder="名称、XUID、UUID 或存档标识" @keyup.enter="applyFilters" />
        </el-form-item>
        <el-form-item label="状态">
          <el-select v-model="filters.online" clearable placeholder="全部状态">
            <el-option label="在线" value="true" />
            <el-option label="离线" value="false" />
          </el-select>
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
        <el-form-item class="filter-actions">
          <el-button type="primary" native-type="submit">查询</el-button>
          <el-button @click="resetFilters">重置</el-button>
        </el-form-item>
      </el-form>

      <el-table
        :data="rows"
        height="100%"
        stripe
        v-loading="loading"
        @sort-change="changeSort"
      >
        <el-table-column prop="name" label="玩家标识" min-width="190" show-overflow-tooltip sortable="custom" />
        <el-table-column prop="xuid" label="XUID" min-width="190" show-overflow-tooltip sortable="custom" />
        <el-table-column prop="uuid" label="UUID" min-width="270" show-overflow-tooltip sortable="custom" />
        <el-table-column prop="online" label="状态" width="100" sortable="custom">
          <template #default="{ row }">
            <el-tag :type="row.online ? 'success' : 'info'" size="small">
              {{ row.online ? '在线' : '离线' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="source" label="来源" width="100" sortable="custom">
          <template #default="{ row }">{{ sourceLabel(row.source) }}</template>
        </el-table-column>
        <el-table-column prop="dimension" label="维度" width="110" sortable="custom">
          <template #default="{ row }">{{ dimensionLabel(row.dimension) }}</template>
        </el-table-column>
        <el-table-column prop="position.x" label="X" width="100" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.position?.x) }}</template>
        </el-table-column>
        <el-table-column prop="position.y" label="Y" width="90" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.position?.y) }}</template>
        </el-table-column>
        <el-table-column prop="position.z" label="Z" width="100" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.position?.z) }}</template>
        </el-table-column>
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

    <el-dialog v-model="dialogVisible" title="玩家详细数据" width="86%" top="5vh" destroy-on-close>
      <template v-if="selected" v-loading="detailLoading">
        <el-descriptions :column="3" border class="detail-summary">
          <el-descriptions-item label="玩家标识">{{ selected.name }}</el-descriptions-item>
          <el-descriptions-item label="状态">{{ selected.online ? '在线' : '离线' }}</el-descriptions-item>
          <el-descriptions-item label="来源">{{ sourceLabel(selected.source) }}</el-descriptions-item>
          <el-descriptions-item label="XUID">{{ selected.xuid || '-' }}</el-descriptions-item>
          <el-descriptions-item label="UUID" :span="2">{{ selected.uuid || '-' }}</el-descriptions-item>
          <el-descriptions-item label="维度">{{ dimensionLabel(selected.dimension) }}</el-descriptions-item>
          <el-descriptions-item label="坐标" :span="2">
            {{ coordinate(selected.position?.x) }}, {{ coordinate(selected.position?.y) }}, {{ coordinate(selected.position?.z) }}
          </el-descriptions-item>
          <el-descriptions-item label="存档标识" :span="3">
            <div class="tag-list">
              <el-tag v-for="id in selected.storageIds" :key="id" type="info" effect="plain">{{ id }}</el-tag>
            </div>
          </el-descriptions-item>
        </el-descriptions>

        <el-tabs v-model="detailTab" class="detail-tabs">
          <el-tab-pane label="背包与快捷栏" name="inventory">
            <ItemTable :items="selected.inventory" empty-text="背包为空" />
          </el-tab-pane>
          <el-tab-pane label="盔甲" name="armor">
            <ItemTable :items="selected.armor" empty-text="未穿戴盔甲" />
          </el-tab-pane>
          <el-tab-pane label="副手" name="offhand">
            <ItemTable :items="selected.offhand ? [selected.offhand] : []" empty-text="副手为空" />
          </el-tab-pane>
          <el-tab-pane label="末影箱" name="enderChest">
            <ItemTable :items="selected.enderChest" empty-text="末影箱为空" />
          </el-tab-pane>
        </el-tabs>
      </template>
    </el-dialog>
  </section>
</template>
