<script setup>
defineProps({
  title: String,
  description: String
})
</script>

<template>
  <header class="page-header">
    <div>
      <h1>{{ title }}</h1>
      <p>{{ description }}</p>
    </div>
    <slot />
  </header>
</template>
