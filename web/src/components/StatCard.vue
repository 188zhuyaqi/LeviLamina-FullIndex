<script setup>
defineProps({
  label: String,
  value: [String, Number],
  hint: String,
  loading: Boolean
})
</script>

<template>
  <div class="stat-card">
    <el-skeleton v-if="loading" :rows="1" animated />
    <template v-else>
      <span>{{ label }}</span>
      <strong>{{ value }}</strong>
      <small>{{ hint }}</small>
    </template>
  </div>
</template>
