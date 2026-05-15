<script setup lang="ts">
import { computed, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth'
import { useLobbyStore } from '../stores/lobby'
import { useSessionStore } from '../stores/session'

const router = useRouter()
const authStore = useAuthStore()
const lobbyStore = useLobbyStore()
const sessionStore = useSessionStore()

const canStart = computed(() => authStore.loggedIn && !lobbyStore.matching)

watch(
  () => lobbyStore.matchedRoomId,
  async (roomId) => {
    if (roomId > 0) {
      await router.push('/room')
    }
  },
  { immediate: true },
)

watch(
  () => authStore.loggedIn,
  async (loggedIn) => {
    if (!loggedIn) {
      await router.replace('/login')
    }
  },
  { immediate: true },
)

async function startMatch() {
  try {
    await sessionStore.startMatch()
  } catch (error) {
    sessionStore.error = error instanceof Error ? error.message : '匹配失败'
  }
}

async function cancelMatch() {
  try {
    await sessionStore.cancelMatch()
  } catch (error) {
    sessionStore.error = error instanceof Error ? error.message : '取消失败'
  }
}
</script>

<template>
  <div class="page">
    <h2>大厅</h2>
    <div class="info">
      <div>玩家：{{ authStore.playerId }}</div>
      <div>昵称：{{ authStore.nickname }}</div>
      <div>金币：{{ authStore.coin }}</div>
      <div>Token 剩余：{{ authStore.tokenSecondsRemaining }}s</div>
    </div>
    <div class="actions">
      <button type="button" :disabled="!canStart || sessionStore.busy" @click="startMatch">开始匹配(mode=3)</button>
      <button type="button" :disabled="!lobbyStore.matching || sessionStore.busy" @click="cancelMatch">取消匹配</button>
    </div>
    <p v-if="lobbyStore.matching">匹配中，请等待服务端推送 2005/2006...</p>
    <p v-if="lobbyStore.timeoutNotified" class="warn">匹配超时，可重试</p>
    <p v-if="sessionStore.error" class="error">{{ sessionStore.error }}</p>
  </div>
</template>

<style scoped>
.page {
  width: min(700px, 100%);
  margin: 24px auto;
  padding: 16px;
  border: 1px solid #ddd7ec;
  border-radius: 12px;
  background: #fff;
}

.info {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px;
  margin-bottom: 12px;
}

.actions {
  display: flex;
  gap: 8px;
}

button {
  border: 1px solid #7562d3;
  background: #7562d3;
  color: #fff;
  border-radius: 8px;
  padding: 8px 12px;
  cursor: pointer;
}

button:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.warn {
  color: #8f6c11;
}

.error {
  color: #b02020;
}

@media (max-width: 720px) {
  .info {
    grid-template-columns: 1fr;
  }
}
</style>
