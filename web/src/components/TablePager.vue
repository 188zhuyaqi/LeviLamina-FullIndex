<script setup>
defineProps({
  page: { type: Number, default: 1 },
  pageSize: { type: Number, default: 50 },
  total: { type: Number, default: 0 }
})

defineEmits(['page-change', 'size-change'])
</script>

<template>
  <el-pagination
    class="data-pager"
    background
    layout="total, sizes, prev, pager, next, jumper"
    :current-page="page"
    :page-size="pageSize"
    :page-sizes="[20, 50, 100, 200]"
    :total="total"
    @current-change="$emit('page-change', $event)"
    @size-change="$emit('size-change', $event)"
  />
</template>
