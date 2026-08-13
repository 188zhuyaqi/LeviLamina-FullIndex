<script setup>
import {
  Box,
  DataAnalysis,
  Goods,
  Monitor,
  User
} from '@element-plus/icons-vue'
import { useRoute } from 'vue-router'

const route = useRoute()

const menus = [
  { path: '/', label: '概览', icon: DataAnalysis },
  { path: '/players', label: '玩家', icon: User },
  { path: '/containers', label: '容器', icon: Box },
  { path: '/drops', label: '掉落物', icon: Goods },
  { path: '/entities', label: '实体', icon: Monitor }
]
</script>

<template>
  <div class="shell">
    <aside class="sidebar">
      <div class="brand">
        <div class="brand-mark">FI</div>
        <div>
          <strong>全服索引</strong>
          <small>FullIndex</small>
        </div>
      </div>

      <nav class="nav">
        <RouterLink
          v-for="item in menus"
          :key="item.path"
          :to="item.path"
          class="nav-item"
          :class="{ active: route.path === item.path }"
        >
          <el-icon><component :is="item.icon" /></el-icon>
          <span>{{ item.label }}</span>
        </RouterLink>
      </nav>
    </aside>

    <main class="main">
      <RouterView v-slot="{ Component }">
        <Transition name="page" mode="out-in">
          <component :is="Component" />
        </Transition>
      </RouterView>
    </main>
  </div>
</template>
