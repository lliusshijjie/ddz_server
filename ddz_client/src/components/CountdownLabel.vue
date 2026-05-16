<script setup lang="ts">
import { computed } from 'vue'
import { secondsRemaining } from '../utils/time'

const props = defineProps<{
  deadlineMs: number
  label?: string
}>()

const remain = computed(() => secondsRemaining(props.deadlineMs))
const isUrgent = computed(() => remain.value > 0 && remain.value <= 5)
</script>

<template>
  <div class="countdown-container">
    <span class="label">{{ label || 'TIME' }}</span>
    <span class="countdown-value" :class="{ urgent: isUrgent }">
      {{ remain > 0 ? remain : 0 }}s
    </span>
  </div>
</template>

<style scoped>
.countdown-container {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  background: rgba(0,0,0,0.6);
  padding: 4px 12px;
  border-radius: 20px;
  border: 1px solid rgba(255,255,255,0.1);
  box-shadow: inset 0 2px 4px rgba(0,0,0,0.5);
}

.label {
  font-size: 11px;
  font-weight: 700;
  color: var(--color-text-secondary);
  letter-spacing: 1px;
}

.countdown-value {
  font-size: 16px;
  font-weight: 700;
  color: var(--color-accent-gold);
  font-variant-numeric: tabular-nums;
}

.countdown-value.urgent {
  color: var(--color-accent-red);
  animation: pulse-urgent 0.5s infinite alternate;
}

@keyframes pulse-urgent {
  from { transform: scale(1); text-shadow: none; }
  to { transform: scale(1.1); text-shadow: 0 0 8px var(--color-accent-red); }
}
</style>
