<script setup>
defineProps({
  title: String,
  description: String
})
</script>

<template>
  <header class="page-header">
    <div>
      <div class="page-title-row">
        <h1>{{ title }}</h1>
        <slot name="title-extra" />
      </div>
      <p>{{ description }}</p>
    </div>
    <slot />
  </header>
</template>
