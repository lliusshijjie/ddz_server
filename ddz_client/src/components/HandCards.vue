<script setup lang="ts">
import { computed } from 'vue'
const props = defineProps<{
  cards: number[]
  selectedIndexes: number[]
}>()

const emit = defineEmits<{
  toggle: [card: number, index: number]
}>()

// Helper to determine color based on suit logic (if we had real cards, but since it's just numbers right now, let's fake it for aesthetics based on modulo or just make them look cool)
const isRed = (card: number) => {
  // DDZ cards usually don't have suits sent down in simple MVPs, it's 3-17 usually, but let's just make half of them red for the casino vibe.
  return card % 2 === 0
}

const getDisplayValue = (card: number) => {
  // In DDZ, numbers usually map to: 11=J, 12=Q, 13=K, 14=A, 15=2, 16=Small Joker, 17=Big Joker
  if (card === 11) return 'J'
  if (card === 12) return 'Q'
  if (card === 13) return 'K'
  if (card === 14) return 'A'
  if (card === 15) return '2'
  if (card === 16) return 'SJ'
  if (card === 17) return 'BJ'
  return card
}
</script>

<template>
  <div class="cards-container">
    <div class="cards-inner">
      <div
        v-for="(card, index) in cards"
        :key="`${index}-${card}`"
        class="playing-card"
        :class="{ 
          'selected': props.selectedIndexes.includes(index),
          'red-suit': isRed(card),
          'black-suit': !isRed(card),
          'joker': card >= 16
        }"
        @click="emit('toggle', card, index)"
      >
        <div class="card-corner top-left">{{ getDisplayValue(card) }}</div>
        <div class="card-center">{{ getDisplayValue(card) }}</div>
        <div class="card-corner bottom-right">{{ getDisplayValue(card) }}</div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.cards-container {
  padding: 20px 0;
  overflow-x: auto;
  overflow-y: visible;
  /* hide scrollbar for horizontal scroll if needed */
  scrollbar-width: none; 
}
.cards-container::-webkit-scrollbar {
  display: none;
}

.cards-inner {
  display: flex;
  justify-content: center;
  min-width: min-content;
  padding: 20px 10px;
}

/* Overlapping effect */
.playing-card {
  position: relative;
  width: 70px;
  height: 100px;
  background: #fff;
  border-radius: 8px;
  box-shadow: -2px 2px 8px rgba(0,0,0,0.3), inset 0 0 0 1px rgba(0,0,0,0.1);
  margin-left: -35px;
  cursor: pointer;
  transition: all 0.2s cubic-bezier(0.175, 0.885, 0.32, 1.275);
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  user-select: none;
}

/* First card doesn't need negative margin */
.playing-card:first-child {
  margin-left: 0;
}

.playing-card:hover {
  transform: translateY(-10px);
  z-index: 10;
  box-shadow: 0 10px 20px rgba(0,0,0,0.4), inset 0 0 0 2px var(--color-accent-gold);
}

.playing-card.selected {
  transform: translateY(-20px);
  z-index: 5;
  box-shadow: 0 15px 25px rgba(0,0,0,0.5), inset 0 0 0 3px var(--color-accent-gold);
}

.playing-card.selected:hover {
  transform: translateY(-25px);
}

/* Aesthetics */
.red-suit {
  color: #ff3b30;
}

.black-suit {
  color: #1e272e;
}

.joker {
  color: #ff9f43;
}

.card-corner {
  font-family: system-ui, sans-serif;
  font-size: 16px;
  font-weight: 800;
  padding: 4px 6px;
  line-height: 1;
}

.top-left {
  text-align: left;
}

.bottom-right {
  text-align: right;
  transform: rotate(180deg);
}

.card-center {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  font-size: 28px;
  font-weight: 900;
  opacity: 0.2;
}

@media (min-width: 768px) {
  .playing-card {
    width: 90px;
    height: 130px;
    margin-left: -45px;
    border-radius: 10px;
  }
  .card-corner {
    font-size: 20px;
    padding: 6px 8px;
  }
  .card-center {
    font-size: 40px;
  }
}
</style>
