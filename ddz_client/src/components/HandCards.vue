<script setup lang="ts">
const props = defineProps<{
  cards: number[]
  selectedIndexes: number[]
}>()

const emit = defineEmits<{
  toggle: [card: number, index: number]
}>()
</script>

<template>
  <div class="cards">
    <button
      v-for="(card, index) in cards"
      :key="`${index}-${card}`"
      class="card"
      :class="{ selected: props.selectedIndexes.includes(index) }"
      type="button"
      @click="emit('toggle', card, index)"
    >
      {{ card }}
    </button>
  </div>
</template>

<style scoped>
.cards {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.card {
  min-width: 34px;
  border: 1px solid #d0c9e3;
  border-radius: 8px;
  padding: 6px 8px;
  background: #f8f5ff;
  cursor: pointer;
}

.card.selected {
  transform: translateY(-6px);
  border-color: #7e5bef;
  background: #e9e2ff;
}
</style>
