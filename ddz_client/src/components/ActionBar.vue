<script setup lang="ts">
const props = defineProps<{
  loading: boolean
}>()

const emit = defineEmits<{
  callScore: [score: number]
  rob: [rob: boolean]
  play: []
  pass: []
}>()
</script>

<template>
  <div class="action-bar panel">
    <div class="action-group call-group">
      <button type="button" class="btn-chunky btn-blue" :disabled="loading" @click="emit('callScore', 1)">1 PT</button>
      <button type="button" class="btn-chunky btn-blue" :disabled="loading" @click="emit('callScore', 2)">2 PT</button>
      <button type="button" class="btn-chunky btn-blue" :disabled="loading" @click="emit('callScore', 3)">3 PT</button>
    </div>
    
    <div class="divider"></div>
    
    <div class="action-group rob-group">
      <button type="button" class="btn-chunky btn-red" :disabled="loading" @click="emit('rob', true)">ROB 👹</button>
      <button type="button" class="btn-chunky btn-blue" :disabled="loading" @click="emit('rob', false)">PASS</button>
    </div>
    
    <div class="divider"></div>
    
    <div class="action-group play-group">
      <button type="button" class="btn-chunky btn-blue" :disabled="loading" @click="emit('pass')">PASS 🛑</button>
      <button type="button" class="btn-chunky btn-gold play-btn" :disabled="loading" @click="emit('play')">PLAY CARDS 🃏</button>
    </div>
  </div>
</template>

<style scoped>
.action-bar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: center;
  gap: 16px;
  padding: 16px;
  margin-top: 24px;
}

.action-group {
  display: flex;
  gap: 12px;
}

.divider {
  width: 2px;
  height: 32px;
  background: rgba(255,255,255,0.1);
  border-radius: 1px;
}

.play-btn {
  padding-left: 32px;
  padding-right: 32px;
  font-size: 16px;
  box-shadow: 0 0 15px rgba(255, 204, 0, 0.4), var(--shadow-chunky);
}

@media (max-width: 768px) {
  .action-bar {
    flex-direction: column;
    gap: 20px;
  }
  .divider {
    width: 80%;
    height: 2px;
  }
  .action-group {
    width: 100%;
    justify-content: center;
  }
  .play-btn {
    flex: 1;
  }
}
</style>
