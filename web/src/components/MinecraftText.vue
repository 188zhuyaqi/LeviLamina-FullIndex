<script setup>
import { computed } from 'vue'
import { minecraftSegmentStyle, parseMinecraftText } from '../utils/minecraftText.js'

const props = defineProps({
  text: { type: [String, Number], default: '' },
  defaultColor: { type: String, default: '' }
})

const segments = computed(() => parseMinecraftText(props.text, props.defaultColor))
</script>

<template>
  <span class="minecraft-text">
    <span
      v-for="(segment, index) in segments"
      :key="index"
      :class="{ 'minecraft-obfuscated': segment.obfuscated }"
      :style="minecraftSegmentStyle(segment)"
    >{{ segment.text }}</span>
  </span>
</template>

<style scoped>
.minecraft-text { white-space: pre-wrap; overflow-wrap: anywhere; }
.minecraft-obfuscated { text-shadow: 1px 0 currentColor; letter-spacing: 1px; }
</style>
