import { computed, onMounted, reactive, ref, watch } from 'vue'
import { indexedData } from '../api/client.js'
import { useLiveQuery } from './useLiveQuery.js'

const SORT_FIELDS = {
  players: new Set(['name', 'xuid', 'uuid', 'online', 'source', 'dimension', 'position.x', 'position.y', 'position.z']),
  containers: new Set(['kind', 'source', 'dimension', 'chunkX', 'chunkZ', 'position.x', 'position.y', 'position.z', 'occupiedSlots', 'itemCount']),
  drops: new Set(['source', 'dimension', 'chunkX', 'chunkZ', 'entityCount', 'itemCount', 'distinctItemCount']),
  entities: new Set(['typeName', 'category', 'source', 'dimension', 'chunkX', 'chunkZ', 'position.x', 'position.y', 'position.z']),
  entityChunks: new Set(['source', 'dimension', 'chunkX', 'chunkZ', 'entityCount', 'naturalCount', 'specialCount', 'distinctTypeCount'])
}

function fieldValue(record, field) {
  return field.startsWith('position.') ? record.position?.[field.slice(9)] : record[field]
}

function sortRows(items, kind, field, order) {
  if (!SORT_FIELDS[kind]?.has(field)) return items
  const direction = order === 'ascending' ? 1 : -1
  return items.sort((left, right) => {
    const a = fieldValue(left, field)
    const b = fieldValue(right, field)
    if (a == null && b == null) return 0
    if (a == null) return 1
    if (b == null) return -1
    if (typeof a === 'number' || typeof b === 'number') return (Number(a) - Number(b)) * direction
    return String(a).localeCompare(String(b), 'zh-CN', { numeric: true }) * direction
  })
}

function entityChunkRows(entities) {
  const groups = new Map()
  for (const entity of entities) {
    const key = `${entity.dimension}:${entity.chunkX}:${entity.chunkZ}`
    let group = groups.get(key)
    if (!group) {
      group = {
        source: entity.source, dimension: entity.dimension, chunkX: entity.chunkX, chunkZ: entity.chunkZ,
        entityCount: 0, naturalCount: 0, specialCount: 0, distinctTypeCount: 0, types: []
      }
      groups.set(key, group)
    }
    if (entity.source === 'runtime') group.source = 'runtime'
    group.entityCount += 1
    if (entity.category === 'NATURAL_MOB') group.naturalCount += 1
    else group.specialCount += 1
    let type = group.types.find(item => item.typeName === entity.typeName
      && item.customName === entity.customName && item.category === entity.category)
    if (!type) {
      type = { typeName: entity.typeName, customName: entity.customName ?? '', category: entity.category, count: 0, positions: [] }
      group.types.push(type)
    }
    type.count += 1
    type.positions.push(entity.position)
  }
  for (const group of groups.values()) {
    group.types.sort((a, b) => b.count - a.count || a.typeName.localeCompare(b.typeName))
    group.distinctTypeCount = group.types.length
  }
  return [...groups.values()]
}

export function useIndexedData(kind, {
  initialPageSize = 50,
  initialFilters = {},
  initialSortField = '',
  initialSortOrder = ''
} = {}) {
  const snapshotLoading = ref(true)
  const rows = ref([])
  const total = ref(0)
  const page = ref(1)
  const pageSize = ref(initialPageSize)
  const filterDefaults = { ...initialFilters }
  const filters = reactive({ ...filterDefaults })
  const sortField = ref(initialSortField)
  const sortOrder = ref(initialSortOrder)
  const snapshotError = ref('')
  const live = useLiveQuery(kind)
  const loading = computed(() => live.enabled.value ? live.running.value : snapshotLoading.value)
  const error = computed(() => live.error.value || snapshotError.value)

  function refreshLivePage() {
    let all = [...live.items.value]
    let sortKind = kind
    if (kind === 'entities' && filters.view === 'chunks') {
      all = entityChunkRows(all)
      sortKind = 'entityChunks'
    }
    sortRows(all, sortKind, sortField.value, sortOrder.value)
    total.value = all.length
    const begin = (page.value - 1) * pageSize.value
    rows.value = all.slice(begin, begin + pageSize.value)
  }

  async function load() {
    if (live.enabled.value) {
      page.value = 1
      live.start({ ...filters })
      refreshLivePage()
      return
    }
    snapshotLoading.value = true
    snapshotError.value = ''
    try {
      const response = await indexedData(kind, {
        page: page.value,
        pageSize: pageSize.value,
        ...filters,
        sortField: sortField.value,
        sortOrder: sortOrder.value
      })
      rows.value = response.items ?? []
      total.value = response.total ?? 0
    } catch (e) {
      snapshotError.value = e.message
    } finally {
      snapshotLoading.value = false
    }
  }

  function changePage(value) {
    page.value = value
    if (live.enabled.value) refreshLivePage()
    else load()
  }

  function changePageSize(value) {
    pageSize.value = value
    page.value = 1
    if (live.enabled.value) refreshLivePage()
    else load()
  }

  function applyFilters() {
    page.value = 1
    load()
  }

  function resetFilters() {
    Object.assign(filters, filterDefaults)
    page.value = 1
    load()
  }

  function changeSort({ prop, order }) {
    sortField.value = prop ?? ''
    sortOrder.value = order ?? ''
    page.value = 1
    if (live.enabled.value) refreshLivePage()
    else load()
  }

  function setRealtimeEnabled(value) {
    if (live.setEnabled(value)) {
      page.value = 1
      refreshLivePage()
    } else {
      page.value = 1
      load()
    }
  }

  watch(live.version, refreshLivePage)
  watch(live.fallbackToken, () => {
    page.value = 1
    load()
  })
  onMounted(load)

  return {
    loading, rows, total, page, pageSize, filters, sortField, sortOrder, error,
    load, changePage, changePageSize, applyFilters, resetFilters, changeSort,
    realtimeAvailable: live.available,
    realtimeEnabled: live.enabled,
    realtimeRunning: live.running,
    realtimeComplete: live.complete,
    realtimePhase: live.phase,
    realtimeProgress: live.progress,
    realtimeReceived: live.received,
    setRealtimeEnabled,
    cancelRealtime: live.requestCancel
  }
}
