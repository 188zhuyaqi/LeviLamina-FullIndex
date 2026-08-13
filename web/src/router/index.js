import { createRouter, createWebHistory } from 'vue-router'

import DashboardView from '../views/DashboardView.vue'
import PlayersView from '../views/PlayersView.vue'
import ContainersView from '../views/ContainersView.vue'
import DropsView from '../views/DropsView.vue'
import EntitiesView from '../views/EntitiesView.vue'

export default createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', component: DashboardView },
    { path: '/players', component: PlayersView },
    { path: '/containers', component: ContainersView },
    { path: '/drops', component: DropsView },
    { path: '/entities', component: EntitiesView }
  ]
})
