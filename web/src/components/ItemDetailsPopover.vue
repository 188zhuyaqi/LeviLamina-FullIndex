<script setup>
import { computed, onBeforeUnmount, ref } from 'vue'
import { enchantmentLabel } from '../i18n/index.js'
import MinecraftText from './MinecraftText.vue'

const props = defineProps({
  item: { type: Object, required: true }
})

const visible = ref(false)
const focused = ref(false)
let hideTimer = null

const title = computed(() => props.item.customName || props.item.displayName || props.item.id || '未知物品')
const showBaseName = computed(() => Boolean(
  props.item.customName && props.item.displayName && props.item.customName !== props.item.displayName
))
const enchantments = computed(() => Array.isArray(props.item.enchantments) ? props.item.enchantments : [])
const lore = computed(() => Array.isArray(props.item.lore) ? props.item.lore : [])

function enchantmentText(enchantment) {
  if (enchantment.gameText) return String(enchantment.gameText)
  const name = enchantmentLabel(enchantment.id)
  const level = enchantment.levelText || romanLevel(enchantment.level)
  return `${name}${level ? ` ${level}` : ''}`
}

function romanLevel(value) {
  let remaining = Number(value)
  if (!Number.isInteger(remaining) || remaining <= 0 || remaining > 3999) return String(value ?? '')
  const numerals = [
    [1000, 'M'], [900, 'CM'], [500, 'D'], [400, 'CD'], [100, 'C'], [90, 'XC'], [50, 'L'],
    [40, 'XL'], [10, 'X'], [9, 'IX'], [5, 'V'], [4, 'IV'], [1, 'I']
  ]
  let result = ''
  for (const [amount, numeral] of numerals) {
    while (remaining >= amount) {
      result += numeral
      remaining -= amount
    }
  }
  return result
}

function show() {
  if (hideTimer) clearTimeout(hideTimer)
  visible.value = true
}

function hideSoon() {
  if (focused.value) return
  if (hideTimer) clearTimeout(hideTimer)
  hideTimer = setTimeout(() => { visible.value = false }, 120)
}

function focus() {
  focused.value = true
  show()
}

function blur() {
  focused.value = false
  hideSoon()
}

function close() {
  focused.value = false
  visible.value = false
}

onBeforeUnmount(() => {
  if (hideTimer) clearTimeout(hideTimer)
})
</script>

<template>
  <el-popover
    :visible="visible"
    placement="right"
    :width="360"
    :show-arrow="false"
    :teleported="true"
    popper-class="minecraft-item-popper"
  >
    <template #reference>
      <button
        type="button"
        class="item-details-link"
        aria-label="查看物品详情"
        :aria-expanded="visible"
        @mouseenter="show"
        @mouseleave="hideSoon"
        @focus="focus"
        @blur="blur"
        @keydown.esc="close"
      >物品详情</button>
    </template>

    <section
      class="minecraft-tooltip"
      role="tooltip"
      @mouseenter="show"
      @mouseleave="hideSoon"
    >
      <MinecraftText class="minecraft-tooltip__title" :text="title" default-color="#ffffff" />
      <MinecraftText
        v-if="showBaseName"
        class="minecraft-tooltip__base-name"
        :text="item.displayName"
        default-color="#aaaaaa"
      />

      <div v-if="enchantments.length" class="minecraft-tooltip__section minecraft-tooltip__enchants">
        <MinecraftText
          v-for="(enchantment, index) in enchantments"
          :key="`${enchantment.id}-${enchantment.level}-${index}`"
          :text="enchantmentText(enchantment)"
          default-color="#aaaaff"
        />
      </div>
      <div v-else-if="item.enchanted" class="minecraft-tooltip__section minecraft-tooltip__enchants">
        <span>已附魔</span>
      </div>

      <div v-if="lore.length" class="minecraft-tooltip__section minecraft-tooltip__lore">
        <MinecraftText
          v-for="(line, index) in lore"
          :key="index"
          :text="line"
          default-color="#aaaaaa"
        />
      </div>

      <div class="minecraft-tooltip__meta">
        <span>{{ item.id || 'unknown' }}</span>
        <span>数量 {{ item.count ?? 0 }}<template v-if="item.damage"> · 损耗 {{ item.damage }}</template></span>
      </div>
    </section>
  </el-popover>
</template>

<style scoped>
.item-details-link {
  padding: 2px 0;
  border: 0;
  color: var(--el-color-primary);
  background: transparent;
  font: inherit;
  text-decoration: underline;
  text-underline-offset: 3px;
  cursor: pointer;
  transition: color 160ms ease;
}

.item-details-link:hover { color: var(--el-color-primary-dark-2); }
.item-details-link:focus-visible { outline: 2px solid var(--el-color-primary); outline-offset: 3px; border-radius: 2px; }

.minecraft-tooltip {
  display: grid;
  gap: 4px;
  max-height: min(520px, calc(100vh - 32px));
  padding: 12px 14px;
  overflow: auto;
  color: #ffffff;
  font-family: "Microsoft YaHei", "PingFang SC", system-ui, sans-serif;
  font-size: 14px;
  line-height: 1.45;
}

.minecraft-tooltip__title { font-weight: 600; }
.minecraft-tooltip__base-name { font-size: 12px; }
.minecraft-tooltip__section { display: grid; gap: 2px; margin-top: 4px; }
.minecraft-tooltip__enchants { color: #aaaaff; }
.minecraft-tooltip__meta {
  display: grid;
  gap: 1px;
  margin-top: 7px;
  padding-top: 7px;
  border-top: 1px solid rgba(170, 0, 170, .72);
  color: #777777;
  font-size: 11px;
}

@media (prefers-reduced-motion: reduce) {
  .item-details-link { transition: none; }
}
</style>

<style>
.el-popper.minecraft-item-popper {
  max-width: calc(100vw - 24px);
  padding: 0 !important;
  border: 1px solid #5000a0 !important;
  border-radius: 3px !important;
  background: rgba(16, 0, 16, .97) !important;
  box-shadow: inset 0 0 0 1px #280050, 0 8px 26px rgba(0, 0, 0, .42) !important;
}
</style>
