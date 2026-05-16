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
    <header class="header panel">
      <div class="brand">♠ DOU DIZHU ♣</div>
      <nav class="nav">
        <RouterLink to="/login" class="nav-link">登录 LOGIN</RouterLink>
        <RouterLink to="/lobby" class="nav-link">大厅 LOBBY</RouterLink>
        <RouterLink to="/room" class="nav-link">房间 ROOM</RouterLink>
        <RouterLink to="/settlement" class="nav-link">结算 STATS</RouterLink>
        <RouterLink to="/debug" class="nav-link">调试 DEBUG</RouterLink>
      </nav>
      <div class="right">
        <span class="status-badge" :class="debugStore.wsStatus === 'OPEN' ? 'online' : 'offline'">
          <span class="dot"></span> WS
        </span>
        <span v-if="authStore.loggedIn" class="player-badge">
          ID: <strong>{{ authStore.playerId }}</strong>
        </span>
        <button type="button" class="btn-chunky btn-red logout-btn" @click="logout">退出 EXT</button>
      </div>
    </header>
    <main class="main-content">
      <RouterView />
      <ToastMessage :text="debugStore.message" type="info" />
      <ToastMessage :text="sessionStore.error" type="error" />
    </main>
  </div>
</template>

<style scoped>
.layout {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

.header {
  margin: 16px;
  border-radius: 16px;
  position: sticky;
  top: 16px;
  z-index: 100;
  display: flex;
  align-items: center;
  gap: 16px;
  justify-content: space-between;
  padding: 12px 24px;
}

.brand {
  font-weight: 700;
  font-size: 20px;
  color: var(--color-accent-gold);
  text-shadow: 0 2px 4px rgba(0,0,0,0.5);
  letter-spacing: 1px;
}

.nav {
  display: flex;
  gap: 12px;
}

.nav-link {
  color: var(--color-text-secondary);
  text-decoration: none;
  font-size: 13px;
  font-weight: 600;
  padding: 6px 12px;
  border-radius: 8px;
  transition: all 0.2s;
}

.nav-link:hover {
  color: #fff;
  background: rgba(255, 255, 255, 0.1);
}

.nav-link.router-link-active {
  color: var(--color-accent-gold);
  background: rgba(255, 204, 0, 0.15);
}

.right {
  display: flex;
  align-items: center;
  gap: 12px;
}

.status-badge {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  font-weight: 600;
  background: rgba(0, 0, 0, 0.5);
  padding: 4px 10px;
  border-radius: 20px;
  border: 1px solid var(--color-panel-border);
}

.status-badge .dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #ff4757;
  box-shadow: 0 0 8px #ff4757;
}

.status-badge.online .dot {
  background: #2ed573;
  box-shadow: 0 0 8px #2ed573;
}

.player-badge {
  font-size: 13px;
  background: rgba(59, 130, 246, 0.2);
  border: 1px solid rgba(59, 130, 246, 0.5);
  color: #a4c8ff;
  padding: 4px 12px;
  border-radius: 20px;
}

.player-badge strong {
  color: #fff;
}

.logout-btn {
  padding: 4px 12px;
  font-size: 12px;
  border-radius: 8px;
}

.main-content {
  flex: 1;
  padding: 0 16px 32px;
  display: flex;
  flex-direction: column;
}

@media (max-width: 960px) {
  .header {
    flex-wrap: wrap;
  }
  .nav {
    order: 3;
    width: 100%;
    justify-content: center;
    margin-top: 8px;
  }
}
</style>
