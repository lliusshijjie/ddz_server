<script setup lang="ts">
import { onMounted } from 'vue'
import { RouterLink, RouterView } from 'vue-router'
import ToastMessage from './components/ToastMessage.vue'
import { useAuthStore } from './stores/auth'
import { useDebugStore } from './stores/debug'
import { useSessionStore } from './stores/session'

const authStore = useAuthStore()
const debugStore = useDebugStore()
const sessionStore = useSessionStore()

onMounted(() => {
  sessionStore.init()
})

function logout() {
  sessionStore.logout()
}
</script>

<template>
  <div class="layout">
    <header class="header">
      <div class="brand">ddz_client MVP</div>
      <nav class="nav">
        <RouterLink to="/login">登录</RouterLink>
        <RouterLink to="/lobby">大厅</RouterLink>
        <RouterLink to="/room">房间</RouterLink>
        <RouterLink to="/settlement">结算</RouterLink>
        <RouterLink to="/debug">调试</RouterLink>
      </nav>
      <div class="right">
        <span>WS: {{ debugStore.wsStatus }}</span>
        <span v-if="authStore.loggedIn">PID: {{ authStore.playerId }}</span>
        <button type="button" @click="logout">退出</button>
      </div>
    </header>
    <main>
      <RouterView />
      <ToastMessage :text="debugStore.message" type="info" />
      <ToastMessage :text="sessionStore.error" type="error" />
    </main>
  </div>
</template>

<style scoped>
.layout {
  min-height: 100vh;
  background: #f4f2f9;
}

.header {
  position: sticky;
  top: 0;
  z-index: 1;
  display: flex;
  align-items: center;
  gap: 12px;
  justify-content: space-between;
  background: #fff;
  border-bottom: 1px solid #ddd7ec;
  padding: 12px 16px;
}

.brand {
  font-weight: 700;
  color: #4d3c86;
}

.nav {
  display: flex;
  gap: 10px;
  font-size: 13px;
}

.nav a {
  color: #54408e;
  text-decoration: none;
}

.right {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
}

button {
  border: 1px solid #bbb2d1;
  border-radius: 6px;
  background: #fff;
  padding: 4px 10px;
  cursor: pointer;
}

main {
  padding: 0 12px 20px;
}

@media (max-width: 860px) {
  .header {
    flex-wrap: wrap;
  }
}
</style>
