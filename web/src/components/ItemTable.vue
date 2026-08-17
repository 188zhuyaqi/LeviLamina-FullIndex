<script setup>
import { computed } from 'vue'
import { slotLabel } from '../i18n/index.js'
import ItemDetailsPopover from './ItemDetailsPopover.vue'
import MinecraftText from './MinecraftText.vue'

const props = defineProps({
  items: { type: Array, default: () => [] },
  emptyText: { type: String, default: '没有物品' }
})

const rows = computed(() => {
  const result = []
  const walk = (items, depth, parentPath) => {
    for (const item of items ?? []) {
      const slot = slotLabel(item.slotName || item.slot)
      const path = parentPath ? `${parentPath} / ${slot}` : String(slot)
      result.push({ ...item, depth, path })
      walk(item.children, depth + 1, `${path} (${item.id})`)
    }
  }
  walk(props.items, 0, '')
  return result
})
</script>

<template>
  <el-empty v-if="rows.length === 0" :description="emptyText" :image-size="48" />
  <el-table v-else :data="rows" size="small">
    <el-table-column label="槽位/路径" min-width="190">
      <template #default="{ row }">
        <span :style="{ paddingLeft: `${row.depth * 18}px` }">
          <span v-if="row.depth">↳ </span>{{ row.path }}
        </span>
      </template>
    </el-table-column>
    <el-table-column prop="id" label="物品 ID" min-width="210" />
    <el-table-column label="名称" min-width="150">
      <template #default="{ row }">
        <MinecraftText :text="row.displayName" />
      </template>
    </el-table-column>
    <el-table-column prop="count" label="数量" width="78" />
    <el-table-column prop="damage" label="损耗" width="78" />
    <el-table-column label="更多信息" width="110">
      <template #default="{ row }">
        <ItemDetailsPopover :item="row" />
      </template>
    </el-table-column>
  </el-table>
</template>
