<script setup>
import { computed } from 'vue'

const props = defineProps({
  modelValue: Boolean,
  running: Boolean,
  complete: Boolean,
  phase: String,
  progress: Number,
  received: Number
})

defineEmits(['update:modelValue', 'cancel'])

const runningLabel = computed(() => {
  if (props.phase === 'cancelling') return '正在取消'
  if (props.phase === 'streaming') return `接收中 ${props.received} 条 · ${props.progress}%`
  if (props.phase === 'scanning_players') return `正在扫描玩家 · 已接收 ${props.received} 条`
  if (props.phase === 'scanning_containers') return `正在扫描容器 · 已接收 ${props.received} 条`
  if (props.phase === 'reading_container_point') return '正在直接读取坐标所在区块'
  if (props.phase === 'reading_container_chunk') return '正在直接读取指定区块'
  if (props.phase === 'scanning_drops') return `正在扫描掉落物 · 已接收 ${props.received} 条`
  if (props.phase === 'scanning_entities') return '正在扫描实体'
  return '正在扫描当前数据'
})
</script>

<template>
  <div class="realtime-query-control">
    <el-switch
      :model-value="modelValue"
      active-text="实时检索"
      aria-label="切换实时检索"
      @update:model-value="$emit('update:modelValue', $event)"
    />
    <el-tag v-if="modelValue && running" size="small" type="warning" effect="plain">
      {{ runningLabel }}
    </el-tag>
    <el-tag v-else-if="modelValue && complete" size="small" type="success" effect="plain">
      已完成 · {{ received }} 条
    </el-tag>
    <el-tag v-else-if="modelValue" size="small" type="info" effect="plain">等待查询</el-tag>
    <el-button v-if="modelValue && running" link type="danger" @click="$emit('cancel')">取消</el-button>
  </div>
</template>
