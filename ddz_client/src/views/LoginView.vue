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
    localError.value = 'PLAYER_ID 必须大于 0'
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
  <div class="page panel login-panel">
    <div class="login-header">
      <div class="icon">🎰</div>
      <h2>INSERT COIN</h2>
    </div>
    
    <div class="form-group">
      <label>PLAYER ID</label>
      <input v-model.number="playerId" type="number" min="1" class="arcade-input" />
    </div>
    
    <div class="form-group">
      <label>NICKNAME (OPTIONAL)</label>
      <input v-model="nickname" type="text" placeholder="player_xxx" class="arcade-input" />
    </div>
    
    <button type="button" class="btn-chunky btn-gold login-btn" :disabled="sessionStore.busy" @click="submitLogin">
      {{ sessionStore.busy ? 'LOADING...' : 'START GAME' }}
    </button>
    
    <div v-if="localError" class="error-msg">
      ⚠️ {{ localError }}
    </div>
  </div>
</template>

<style scoped>
.page {
  width: min(400px, 100%);
  margin: 40px auto;
  padding: 32px 24px;
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.login-header {
  text-align: center;
  margin-bottom: 8px;
}

.icon {
  font-size: 48px;
  margin-bottom: 8px;
  filter: drop-shadow(0 4px 8px rgba(0,0,0,0.5));
}

h2 {
  color: var(--color-accent-gold);
  font-size: 28px;
  letter-spacing: 2px;
  text-shadow: 0 4px 0 rgba(0,0,0,0.3);
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

label {
  font-size: 14px;
  font-weight: 600;
  color: var(--color-text-secondary);
  letter-spacing: 1px;
}

.arcade-input {
  background: rgba(0, 0, 0, 0.6);
  border: 2px solid var(--color-panel-border);
  border-radius: 12px;
  padding: 14px 16px;
  color: #fff;
  font-family: var(--font-main);
  font-size: 18px;
  font-weight: 600;
  transition: all 0.2s;
  box-shadow: inset 0 4px 8px rgba(0,0,0,0.5);
}

.arcade-input:focus {
  outline: none;
  border-color: var(--color-accent-gold);
  box-shadow: inset 0 4px 8px rgba(0,0,0,0.5), 0 0 12px rgba(255, 204, 0, 0.3);
}

.login-btn {
  margin-top: 12px;
  padding: 16px;
  font-size: 20px;
}

.error-msg {
  background: rgba(255, 71, 87, 0.15);
  border: 1px solid var(--color-accent-red);
  color: #ff8c96;
  padding: 12px;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 600;
  text-align: center;
  animation: shake 0.4s ease-in-out;
}

@keyframes shake {
  0%, 100% { transform: translateX(0); }
  25% { transform: translateX(-4px); }
  75% { transform: translateX(4px); }
}
</style>
