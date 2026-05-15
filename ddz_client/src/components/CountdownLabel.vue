<script setup lang="ts">
import { computed } from 'vue'
import { secondsRemaining } from '../utils/time'

const props = defineProps<{
  deadlineMs: number
  label?: string
}>()

const remain = computed(() => secondsRemaining(props.deadlineMs))
</script>

<template>
  <span class="countdown">{{ label || '剩余' }} {{ remain }}s</span>
</template>

<style scoped>
.countdown {
  font-size: 12px;
  color: #625c73;
}
</style>
