import { createRouter, createWebHashHistory } from 'vue-router'
import DebugView from '../views/DebugView.vue'
import LobbyView from '../views/LobbyView.vue'
import LoginView from '../views/LoginView.vue'
import RoomView from '../views/RoomView.vue'
import SettlementView from '../views/SettlementView.vue'

export const router = createRouter({
  history: createWebHashHistory(),
  routes: [
    { path: '/', redirect: '/login' },
    { path: '/login', component: LoginView },
    { path: '/lobby', component: LobbyView },
    { path: '/room', component: RoomView },
    { path: '/settlement', component: SettlementView },
    { path: '/debug', component: DebugView },
  ],
})
