<script setup>
import { ref } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import RealtimeQueryToggle from '../components/RealtimeQueryToggle.vue'
import TablePager from '../components/TablePager.vue'
import TypeSelect from '../components/TypeSelect.vue'
import { useIndexedData } from '../composables/useIndexedData.js'
import { entityChunkDetail } from '../api/client.js'
import { categoryLabel, dimensionLabel, entityLabel, sourceLabel } from '../i18n/index.js'

const selectedChunk = ref(null)
const dialogVisible = ref(false)
const detailLoading = ref(false)
const {
  loading, rows, total, page, pageSize, filters, sortField, sortOrder, error, load,
  changePage, changePageSize, applyFilters, changeSort,
  realtimeAvailable, realtimeEnabled, realtimeRunning, realtimeComplete,
  realtimePhase, realtimeProgress, realtimeReceived, setRealtimeEnabled, cancelRealtime
} = useIndexedData('entities', {
  initialFilters: {
    view: 'list', typeId: '', name: '', dimension: '', source: '', category: '', chunkX: null, chunkZ: null
  },
  initialSortField: 'typeName',
  initialSortOrder: 'ascending'
})

function changeView(view) {
  filters.view = view
  if (view === 'chunks') {
    sortField.value = 'entityCount'
    sortOrder.value = 'descending'
  } else {
    sortField.value = 'typeName'
    sortOrder.value = 'ascending'
  }
  applyFilters()
}

async function showChunkDetails(row) {
  selectedChunk.value = row
  dialogVisible.value = true
  if (realtimeEnabled.value) return
  detailLoading.value = true
  try {
    const response = await entityChunkDetail({
      dimension: row.dimension, chunkX: row.chunkX, chunkZ: row.chunkZ,
      typeId: filters.typeId, name: filters.name, category: filters.category, source: filters.source
    })
    selectedChunk.value = { ...row, ...(response.item ?? {}) }
  } finally {
    detailLoading.value = false
  }
}

function resetEntityFilters() {
  Object.assign(filters, {
    typeId: '', name: '', dimension: '', source: '', category: '', chunkX: null, chunkZ: null
  })
  applyFilters()
}

function coordinate(value) {
  return Number.isFinite(Number(value)) ? Number(value).toFixed(1) : '-'
}
</script>

<template>
  <section>
    <PageHeader title="实体" description="默认查询最近完整快照；实时检索可查看当前实体与区块负载">
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
      <el-tabs v-model="filters.view" class="view-tabs" @tab-change="changeView">
        <el-tab-pane label="实体明细" name="list" />
        <el-tab-pane label="区块负载" name="chunks" />
      </el-tabs>

      <el-form class="filter-bar" inline @submit.prevent="applyFilters">
        <el-form-item label="实体类型">
          <TypeSelect v-model="filters.typeId" kind="entities" :realtime="realtimeEnabled" />
        </el-form-item>
        <el-form-item label="自定义名称">
          <el-input v-model="filters.name" clearable placeholder="模糊搜索命名牌名称" @keyup.enter="applyFilters" />
        </el-form-item>
        <el-form-item label="分类">
          <el-select v-model="filters.category" clearable placeholder="全部分类">
            <el-option label="自然生物" value="NATURAL_MOB" />
            <el-option label="特殊实体" value="SPECIAL_ENTITY" />
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
        <el-form-item label="区块">
          <div class="range-pair compact-pair">
            <el-input-number v-model="filters.chunkX" :controls="false" placeholder="区块 X" />
            <el-input-number v-model="filters.chunkZ" :controls="false" placeholder="区块 Z" />
          </div>
        </el-form-item>
        <el-form-item class="filter-actions">
          <el-button type="primary" native-type="submit">查询</el-button>
          <el-button @click="resetEntityFilters">重置</el-button>
        </el-form-item>
      </el-form>

      <el-table
        v-if="filters.view === 'list'"
        :data="rows"
        height="100%"
        stripe
        v-loading="loading"
        @sort-change="changeSort"
      >
        <el-table-column prop="typeName" label="实体名称" min-width="140" sortable="custom">
          <template #default="{ row }">{{ entityLabel(row.typeName) }}</template>
        </el-table-column>
        <el-table-column prop="typeName" label="实体 ID" min-width="210" show-overflow-tooltip sortable="custom" />
        <el-table-column prop="customName" label="自定义名称" min-width="150" sortable="custom">
          <template #default="{ row }">{{ row.customName || '-' }}</template>
        </el-table-column>
        <el-table-column prop="category" label="分类" min-width="120" sortable="custom">
          <template #default="{ row }">
            <el-tag :type="row.category === 'NATURAL_MOB' ? 'success' : 'warning'" size="small">
              {{ categoryLabel(row.category) }}
            </el-tag>
          </template>
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
      </el-table>

      <el-table
        v-else
        :data="rows"
        height="100%"
        stripe
        v-loading="loading"
        @sort-change="changeSort"
      >
        <el-table-column prop="source" label="来源" width="100" sortable="custom">
          <template #default="{ row }">{{ sourceLabel(row.source) }}</template>
        </el-table-column>
        <el-table-column prop="dimension" label="维度" min-width="120" sortable="custom">
          <template #default="{ row }">{{ dimensionLabel(row.dimension) }}</template>
        </el-table-column>
        <el-table-column prop="chunkX" label="区块 X" min-width="110" sortable="custom" />
        <el-table-column prop="chunkZ" label="区块 Z" min-width="110" sortable="custom" />
        <el-table-column prop="entityCount" label="实体总数" min-width="115" sortable="custom" />
        <el-table-column prop="naturalCount" label="自然生物" min-width="115" sortable="custom" />
        <el-table-column prop="specialCount" label="特殊实体" min-width="115" sortable="custom" />
        <el-table-column prop="distinctTypeCount" label="实体种类" min-width="115" sortable="custom" />
        <el-table-column label="操作" fixed="right" width="90">
          <template #default="{ row }">
            <el-button link type="primary" @click="showChunkDetails(row)">查看详情</el-button>
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

    <el-dialog v-model="dialogVisible" title="区块实体详情" width="86%" top="5vh" destroy-on-close>
      <template v-if="selectedChunk" v-loading="detailLoading">
        <el-descriptions :column="4" border class="detail-summary">
          <el-descriptions-item label="来源">{{ sourceLabel(selectedChunk.source) }}</el-descriptions-item>
          <el-descriptions-item label="维度">{{ dimensionLabel(selectedChunk.dimension) }}</el-descriptions-item>
          <el-descriptions-item label="区块">{{ selectedChunk.chunkX }}, {{ selectedChunk.chunkZ }}</el-descriptions-item>
          <el-descriptions-item label="实体总数">{{ selectedChunk.entityCount }}</el-descriptions-item>
          <el-descriptions-item label="自然生物">{{ selectedChunk.naturalCount }}</el-descriptions-item>
          <el-descriptions-item label="特殊实体">{{ selectedChunk.specialCount }}</el-descriptions-item>
          <el-descriptions-item label="实体种类">{{ selectedChunk.distinctTypeCount }}</el-descriptions-item>
        </el-descriptions>
        <el-table :data="selectedChunk.types" stripe class="dialog-table">
          <el-table-column prop="typeName" label="实体名称" min-width="150" sortable>
            <template #default="{ row }">{{ entityLabel(row.typeName) }}</template>
          </el-table-column>
          <el-table-column prop="typeName" label="实体 ID" min-width="220" sortable />
          <el-table-column prop="customName" label="自定义名称" min-width="150" sortable>
            <template #default="{ row }">{{ row.customName || '-' }}</template>
          </el-table-column>
          <el-table-column prop="category" label="分类" width="130" sortable>
            <template #default="{ row }">{{ categoryLabel(row.category) }}</template>
          </el-table-column>
          <el-table-column prop="count" label="数量" width="90" sortable />
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
