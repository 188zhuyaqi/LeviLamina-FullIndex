<script setup>
import { computed, onMounted, reactive, ref, watch } from 'vue'
import PageHeader from '../components/PageHeader.vue'
import RealtimeQueryToggle from '../components/RealtimeQueryToggle.vue'
import TablePager from '../components/TablePager.vue'
import TypeSelect from '../components/TypeSelect.vue'
import { indexStatus, searchItems } from '../api/client.js'
import { useIndexJob } from '../composables/useIndexJob.js'
import { useLiveQuery } from '../composables/useLiveQuery.js'
import { dimensionLabel, itemPathLabel, ownerLabel, sourceLabel } from '../i18n/index.js'

const snapshotLoading = ref(false)
const rows = ref([])
const total = ref(0)
const page = ref(1)
const pageSize = ref(50)
const snapshot = ref(null)
const error = ref('')
const sortField = ref('count')
const sortOrder = ref('descending')
const advancedOpen = ref([])
const defaults = {
  typeId: '', name: '', sourceType: '', dimension: '', enchanted: '', hasContainerData: '', nestedOnly: false,
  owner: '', itemPath: '', countMin: null, countMax: null,
  xMin: null, xMax: null, yMin: null, yMax: null, zMin: null, zMax: null,
  chunkXMin: null, chunkXMax: null, chunkZMin: null, chunkZMax: null
}
const filters = reactive({ ...defaults })
const { job, active, error: jobError, submitting, start, cancel } = useIndexJob()
const refreshing = computed(() => active.value || submitting.value)
const live = useLiveQuery('items')
const loading = computed(() => live.enabled.value ? live.running.value : snapshotLoading.value)
const displayError = computed(() => live.error.value || error.value)

function refreshLivePage() {
  const allowed = new Set([
    'item_id', 'display_name', 'count', 'source_type', 'owner', 'item_path', 'dimension',
    'x', 'y', 'z', 'chunk_x', 'chunk_z', 'enchanted', 'has_container_data', 'custom_name', 'damage'
  ])
  const field = allowed.has(sortField.value) ? sortField.value : 'count'
  const direction = sortOrder.value === 'ascending' ? 1 : -1
  const all = [...live.items.value].sort((left, right) => {
    const a = left[field]
    const b = right[field]
    if (a == null && b == null) return String(left.item_id).localeCompare(String(right.item_id))
    if (a == null) return 1
    if (b == null) return -1
    if (typeof a === 'number' || typeof b === 'number' || typeof a === 'boolean' || typeof b === 'boolean') {
      return (Number(a) - Number(b)) * direction
    }
    return String(a).localeCompare(String(b), 'zh-CN', { numeric: true }) * direction
  })
  total.value = all.length
  const begin = (page.value - 1) * pageSize.value
  rows.value = all.slice(begin, begin + pageSize.value)
}

async function loadStatus() {
  try {
    const response = await indexStatus()
    snapshot.value = response.snapshot
    if (snapshot.value) await search()
  } catch (e) {
    error.value = e.message
  }
}

async function rebuild() {
  error.value = ''
  try {
    await start()
  } catch (e) {
    error.value = e.message
  }
}

async function search() {
  if (live.enabled.value) {
    page.value = 1
    live.start({ ...filters })
    refreshLivePage()
    return
  }
  if (!snapshot.value) return
  snapshotLoading.value = true
  error.value = ''
  try {
    const response = await searchItems({
      ...filters,
      sortField: sortField.value,
      sortOrder: sortOrder.value,
      page: page.value,
      pageSize: pageSize.value
    })
    rows.value = response.items ?? []
    total.value = response.total ?? 0
    snapshot.value = response.snapshot
  } catch (e) {
    error.value = e.message
  } finally {
    snapshotLoading.value = false
  }
}

function submit() {
  page.value = 1
  search()
}

function resetFilters() {
  Object.assign(filters, defaults)
  sortField.value = 'count'
  sortOrder.value = 'descending'
  submit()
}

function changePage(value) {
  page.value = value
  if (live.enabled.value) refreshLivePage()
  else search()
}

function changePageSize(value) {
  pageSize.value = value
  page.value = 1
  if (live.enabled.value) refreshLivePage()
  else search()
}

function changeSort({ prop, order }) {
  sortField.value = prop ?? 'count'
  sortOrder.value = order ?? 'descending'
  page.value = 1
  if (live.enabled.value) refreshLivePage()
  else search()
}

function setRealtimeEnabled(value) {
  const enabled = live.setEnabled(value)
  page.value = 1
  if (enabled) refreshLivePage()
  else search()
}

function coordinate(value) {
  return Number.isFinite(Number(value)) ? Number(value).toFixed(1) : '-'
}

onMounted(loadStatus)
watch(() => job.value?.status, status => {
  if (status === 'complete' && !live.enabled.value) {
    page.value = 1
    loadStatus()
  }
})
watch(live.version, refreshLivePage)
watch(live.fallbackToken, () => {
  page.value = 1
  search()
})
</script>

<template>
  <section>
    <PageHeader title="全局物品搜索" description="默认检索最近完整快照；实时检索按需读取当前物品数据">
      <template #title-extra>
        <RealtimeQueryToggle
          v-if="live.available.value" :model-value="live.enabled.value" :running="live.running.value"
          :complete="live.complete.value" :phase="live.phase.value" :progress="live.progress.value"
          :received="live.received.value" @update:model-value="setRealtimeEnabled" @cancel="live.requestCancel"
        />
      </template>
      <el-button v-if="active" type="danger" plain :loading="submitting" @click="cancel">取消索引</el-button>
      <el-button v-else type="primary" :loading="refreshing" @click="rebuild">重建只读索引</el-button>
    </PageHeader>

    <el-alert v-if="displayError || jobError" :title="displayError || jobError" type="error" show-icon class="mb-16" />
    <el-alert
      v-if="!snapshot && !live.enabled.value"
      title="尚无索引快照"
      description="点击“重建只读索引”从插件读取运行态与存储态数据。"
      type="info"
      show-icon
      :closable="false"
      class="mb-16"
    />

    <el-card shadow="never" class="panel-card data-panel">
      <el-form class="filter-bar search-primary" inline @submit.prevent="submit">
        <el-form-item label="物品类型">
          <TypeSelect v-model="filters.typeId" kind="items" :realtime="live.enabled.value" />
        </el-form-item>
        <el-form-item label="物品名称">
          <el-input v-model="filters.name" clearable placeholder="模糊搜索显示名称或自定义名称" @keyup.enter="submit" />
        </el-form-item>
        <el-form-item label="来源">
          <el-select v-model="filters.sourceType" clearable placeholder="全部位置">
            <el-option label="玩家" value="player" />
            <el-option label="容器" value="container" />
            <el-option label="掉落物" value="drop" />
          </el-select>
        </el-form-item>
        <el-form-item label="维度">
          <el-select v-model="filters.dimension" clearable placeholder="全部维度">
            <el-option label="主世界" value="overworld" />
            <el-option label="下界" value="nether" />
            <el-option label="末地" value="the_end" />
          </el-select>
        </el-form-item>
        <el-form-item label="附魔">
          <el-select v-model="filters.enchanted" clearable placeholder="全部">
            <el-option label="已附魔" value="true" />
            <el-option label="未附魔" value="false" />
          </el-select>
        </el-form-item>
        <el-form-item class="filter-actions">
          <el-button type="primary" native-type="submit" :loading="loading">搜索</el-button>
          <el-button @click="resetFilters">重置</el-button>
        </el-form-item>
      </el-form>

      <el-collapse v-model="advancedOpen" class="advanced-filters">
        <el-collapse-item title="更多筛选条件" name="advanced">
          <el-form label-position="top" class="advanced-filter-grid">
            <el-form-item label="持有者/位置">
              <el-input v-model="filters.owner" clearable placeholder="玩家名、区块或容器位置" />
            </el-form-item>
            <el-form-item label="嵌套路径">
              <el-input v-model="filters.itemPath" clearable placeholder="背包、末影箱或容器路径" />
            </el-form-item>
            <el-form-item label="数量范围">
              <div class="range-pair">
                <el-input-number v-model="filters.countMin" :controls="false" :min="0" placeholder="最小" />
                <span>至</span>
                <el-input-number v-model="filters.countMax" :controls="false" :min="0" placeholder="最大" />
              </div>
            </el-form-item>
            <el-form-item label="容器属性">
              <div class="inline-options">
                <el-select v-model="filters.hasContainerData" clearable placeholder="全部物品">
                  <el-option label="包含容器数据" value="true" />
                  <el-option label="不含容器数据" value="false" />
                </el-select>
                <el-checkbox v-model="filters.nestedOnly">仅嵌套物品</el-checkbox>
              </div>
            </el-form-item>
            <el-form-item label="X 坐标范围">
              <div class="range-pair">
                <el-input-number v-model="filters.xMin" :controls="false" placeholder="最小 X" />
                <span>至</span>
                <el-input-number v-model="filters.xMax" :controls="false" placeholder="最大 X" />
              </div>
            </el-form-item>
            <el-form-item label="Y 坐标范围">
              <div class="range-pair">
                <el-input-number v-model="filters.yMin" :controls="false" placeholder="最小 Y" />
                <span>至</span>
                <el-input-number v-model="filters.yMax" :controls="false" placeholder="最大 Y" />
              </div>
            </el-form-item>
            <el-form-item label="Z 坐标范围">
              <div class="range-pair">
                <el-input-number v-model="filters.zMin" :controls="false" placeholder="最小 Z" />
                <span>至</span>
                <el-input-number v-model="filters.zMax" :controls="false" placeholder="最大 Z" />
              </div>
            </el-form-item>
            <el-form-item label="区块 X 范围">
              <div class="range-pair">
                <el-input-number v-model="filters.chunkXMin" :controls="false" placeholder="最小" />
                <span>至</span>
                <el-input-number v-model="filters.chunkXMax" :controls="false" placeholder="最大" />
              </div>
            </el-form-item>
            <el-form-item label="区块 Z 范围">
              <div class="range-pair">
                <el-input-number v-model="filters.chunkZMin" :controls="false" placeholder="最小" />
                <span>至</span>
                <el-input-number v-model="filters.chunkZMax" :controls="false" placeholder="最大" />
              </div>
            </el-form-item>
          </el-form>
          <div class="advanced-actions">
            <el-button type="primary" @click="submit">应用筛选</el-button>
          </div>
        </el-collapse-item>
      </el-collapse>

      <el-table :data="rows" height="100%" stripe v-loading="loading" @sort-change="changeSort">
        <el-table-column prop="item_id" label="物品 ID" min-width="210" sortable="custom" />
        <el-table-column prop="display_name" label="名称" min-width="150" sortable="custom" />
        <el-table-column prop="custom_name" label="自定义名称" min-width="140" sortable="custom" />
        <el-table-column prop="count" label="数量" width="90" sortable="custom" />
        <el-table-column prop="damage" label="损耗" width="90" sortable="custom" />
        <el-table-column prop="enchanted" label="附魔" width="90" sortable="custom">
          <template #default="{ row }">
            <el-tag v-if="row.enchanted" type="warning" size="small">是</el-tag>
            <span v-else>否</span>
          </template>
        </el-table-column>
        <el-table-column prop="has_container_data" label="含容器" width="105" sortable="custom">
          <template #default="{ row }">{{ row.has_container_data ? '是' : '否' }}</template>
        </el-table-column>
        <el-table-column prop="source_type" label="来源" width="100" sortable="custom">
          <template #default="{ row }">{{ sourceLabel(row.source_type) }}</template>
        </el-table-column>
        <el-table-column prop="owner" label="持有者/位置" min-width="200" show-overflow-tooltip sortable="custom">
          <template #default="{ row }">{{ ownerLabel(row.owner, row.source_type) }}</template>
        </el-table-column>
        <el-table-column prop="item_path" label="嵌套路径" min-width="190" show-overflow-tooltip sortable="custom">
          <template #default="{ row }">{{ itemPathLabel(row.item_path) }}</template>
        </el-table-column>
        <el-table-column prop="dimension" label="维度" width="110" sortable="custom">
          <template #default="{ row }">{{ dimensionLabel(row.dimension) }}</template>
        </el-table-column>
        <el-table-column prop="x" label="X" width="95" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.x) }}</template>
        </el-table-column>
        <el-table-column prop="y" label="Y" width="90" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.y) }}</template>
        </el-table-column>
        <el-table-column prop="z" label="Z" width="95" sortable="custom">
          <template #default="{ row }">{{ coordinate(row.z) }}</template>
        </el-table-column>
        <el-table-column prop="chunk_x" label="区块 X" width="105" sortable="custom" />
        <el-table-column prop="chunk_z" label="区块 Z" width="105" sortable="custom" />
      </el-table>

      <TablePager
        :page="page"
        :page-size="pageSize"
        :total="total"
        @page-change="changePage"
        @size-change="changePageSize"
      />
    </el-card>
  </section>
</template>
