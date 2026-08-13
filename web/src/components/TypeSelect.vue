<script setup>
import { computed, onMounted, ref } from 'vue'
import { typeOptions } from '../api/client.js'
import { containerLabel, entityLabel } from '../i18n/index.js'

const props = defineProps({
  modelValue: { type: String, default: '' },
  kind: { type: String, required: true },
  realtime: Boolean
})
const emit = defineEmits(['update:modelValue'])
const options = ref([])
const loading = ref(false)

function optionLabel(option) {
  let translated = option.displayName || ''
  if (props.kind === 'entities') translated = entityLabel(option.value)
  if (props.kind === 'containers') translated = containerLabel(option.value)
  if (!translated || translated === option.value || translated === '未收录名称') return option.value
  return `${option.value}（${translated}）`
}

const placeholder = computed(() => props.realtime
  ? '快照类型目录，也可输入原始 ID'
  : '从最新快照选择类型')

onMounted(async () => {
  loading.value = true
  try {
    const response = await typeOptions(props.kind)
    options.value = response.items ?? []
  } finally {
    loading.value = false
  }
})
</script>

<template>
  <el-select
    :model-value="modelValue"
    filterable clearable default-first-option
    :allow-create="realtime"
    :reserve-keyword="false"
    :loading="loading"
    :placeholder="placeholder"
    @update:model-value="emit('update:modelValue', $event ?? '')"
  >
    <el-option
      v-for="option in options"
      :key="option.value"
      :label="optionLabel(option)"
      :value="option.value"
    />
  </el-select>
</template>
