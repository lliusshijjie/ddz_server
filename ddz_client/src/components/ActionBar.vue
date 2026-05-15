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
  <div class="actions">
    <button type="button" :disabled="loading" @click="emit('callScore', 1)">叫1分</button>
    <button type="button" :disabled="loading" @click="emit('callScore', 2)">叫2分</button>
    <button type="button" :disabled="loading" @click="emit('callScore', 3)">叫3分</button>
    <button type="button" :disabled="loading" @click="emit('rob', true)">抢地主</button>
    <button type="button" :disabled="loading" @click="emit('rob', false)">不抢</button>
    <button type="button" :disabled="loading" @click="emit('play')">出牌</button>
    <button type="button" :disabled="loading" @click="emit('pass')">过</button>
  </div>
</template>

<style scoped>
.actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

button {
  border: 1px solid #c5bdd9;
  background: #fff;
  border-radius: 8px;
  padding: 6px 10px;
  cursor: pointer;
}

button:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}
</style>
