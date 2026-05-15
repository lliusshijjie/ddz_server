<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth'
import { useSessionStore } from '../stores/session'

const router = useRouter()
const authStore = useAuthStore()
const sessionStore = useSessionStore()

const playerId = ref(authStore.playerId > 0 ? authStore.playerId : 10001)
const nickname = ref(authStore.nickname || '')
const localError = ref('')

onMounted(async () => {
  if (authStore.hasRecoverableSession && !authStore.loggedIn) {
    try {
      await sessionStore.login(authStore.playerId, authStore.nickname || `player_${authStore.playerId}`)
      if (authStore.roomId > 0) {
        await router.replace('/room')
      } else {
        await router.replace('/lobby')
      }
    } catch {
      localError.value = sessionStore.error || '自动恢复失败，请手动登录'
    }
  }
})

async function submitLogin() {
  localError.value = ''
  if (playerId.value <= 0) {
    localError.value = 'player_id 必须大于 0'
    return
  }
  try {
    await sessionStore.login(playerId.value, nickname.value)
    if (authStore.roomId > 0) {
      await router.push('/room')
    } else {
      await router.push('/lobby')
    }
  } catch (error) {
    localError.value = error instanceof Error ? error.message : '登录失败'
  }
}
</script>

<template>
  <div class="page">
    <h2>DDZ H5 登录</h2>
    <label>
      player_id
      <input v-model.number="playerId" type="number" min="1" />
    </label>
    <label>
      nickname
      <input v-model="nickname" type="text" placeholder="可选，默认 player_xxx" />
    </label>
    <button type="button" :disabled="sessionStore.busy" @click="submitLogin">
      {{ sessionStore.busy ? '登录中...' : '登录并进入' }}
    </button>
    <p v-if="localError" class="error">{{ localError }}</p>
  </div>
</template>

<style scoped>
.page {
  width: min(460px, 100%);
  margin: 24px auto;
  padding: 16px;
  border: 1px solid #ddd7ec;
  border-radius: 12px;
  background: #fff;
  display: grid;
  gap: 12px;
}

label {
  display: grid;
  gap: 6px;
  font-size: 13px;
}

input {
  border: 1px solid #c9c1db;
  border-radius: 8px;
  padding: 8px;
}

button {
  border: 1px solid #7562d3;
  background: #7562d3;
  color: #fff;
  border-radius: 8px;
  padding: 8px;
  cursor: pointer;
}

.error {
  color: #b02020;
  margin: 0;
}
</style>
