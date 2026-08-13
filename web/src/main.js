import { createApp } from 'vue'
import ElementPlus from 'element-plus'
import zhCn from 'element-plus/es/locale/lang/zh-cn'
import 'element-plus/dist/index.css'
import 'element-plus/theme-chalk/dark/css-vars.css'
import './styles/index.css'

import App from './App.vue'
import router from './router/index.js'
import { locale } from './i18n/index.js'

const darkMode = window.matchMedia('(prefers-color-scheme: dark)')
const applyTheme = event => document.documentElement.classList.toggle('dark', event.matches)
applyTheme(darkMode)
darkMode.addEventListener('change', applyTheme)

createApp(App)
  .use(router)
  .use(ElementPlus, { locale: locale.value === 'zh-CN' ? zhCn : undefined })
  .mount('#app')
