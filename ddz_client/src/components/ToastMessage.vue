<script setup lang="ts">
defineProps<{
  text: string
  type?: 'info' | 'error'
}>()
</script>

<template>
  <div v-if="text" class="toast" :class="type || 'info'">{{ text }}</div>
</template>

<style scoped>
.toast {
  padding: 8px 12px;
  border-radius: 8px;
  font-size: 13px;
}

.info {
  background: #eef6ff;
  color: #134b84;
}

.error {
  background: #fdecec;
  color: #8f1f1f;
}
</style>
