<script setup lang="ts">
import { computed } from 'vue'
import { useDebugStore } from '../stores/debug'

const debugStore = useDebugStore()

const traces = computed(() => [...debugStore.traces].reverse())
</script>

<template>
  <div class="page">
    <h2>调试面板</h2>
    <p>WS 状态：{{ debugStore.wsStatus }}</p>
    <p v-if="debugStore.message">{{ debugStore.message }}</p>
    <button type="button" @click="debugStore.clear">清空收发日志</button>
    <ul class="logs">
      <li v-for="(item, index) in traces" :key="index">
        <span class="tag">{{ item.direction }}</span>
        <span class="time">{{ new Date(item.atMs).toLocaleTimeString() }}</span>
        <code>{{ item.text }}</code>
      </li>
    </ul>
  </div>
</template>

<style scoped>
.page {
  width: min(980px, 100%);
  margin: 24px auto;
  padding: 16px;
  border: 1px solid #ddd7ec;
  border-radius: 12px;
  background: #fff;
}

.logs {
  list-style: none;
  padding: 0;
  margin: 12px 0 0;
  display: grid;
  gap: 8px;
}

li {
  border: 1px solid #e4deef;
  border-radius: 8px;
  padding: 8px;
  display: grid;
  gap: 4px;
}

.tag {
  width: 34px;
  text-align: center;
  border-radius: 999px;
  background: #ece7f8;
  color: #4f3f80;
  font-size: 11px;
}

.time {
  color: #7b758f;
  font-size: 12px;
}

code {
  white-space: pre-wrap;
  word-break: break-all;
}
</style>
