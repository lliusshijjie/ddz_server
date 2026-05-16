<script setup lang="ts">
const props = defineProps<{
  playerId: number
  currentOperatorPlayerId: number
  landlordPlayerId: number
  online: boolean
  trustee: boolean
  cardCount: number
}>()

const isCurrent = () => props.playerId > 0 && props.currentOperatorPlayerId === props.playerId
const isLandlord = () => props.playerId > 0 && props.landlordPlayerId === props.playerId
</script>

<template>
  <div class="seat panel" :class="{ 'is-turn': isCurrent() }">
    <div class="avatar" :class="{ 'is-landlord': isLandlord() }">
      {{ isLandlord() ? '👹' : '🤖' }}
      <div v-if="!online" class="offline-overlay">OFF</div>
    </div>
    <div class="info">
      <div class="name-row">
        <span class="player-name">P{{ playerId || '---' }}</span>
        <span v-if="trustee" class="badge trustee">BOT</span>
      </div>
      <div class="cards-count">
        🃏 <span class="count-num" :class="{ 'warning': cardCount <= 2 }">{{ cardCount }}</span>
      </div>
    </div>
  </div>
</template>

<style scoped>
.seat {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px;
  border-radius: 16px;
  transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  position: relative;
  overflow: hidden;
}

.seat.is-turn {
  border-color: var(--color-accent-gold);
  box-shadow: 0 0 15px rgba(255, 204, 0, 0.3), inset 0 0 20px rgba(255, 204, 0, 0.1);
  transform: translateY(-4px);
}

.seat.is-turn::before {
  content: '';
  position: absolute;
  top: 0; left: 0; right: 0; bottom: 0;
  border: 2px solid var(--color-accent-gold);
  border-radius: 16px;
  animation: border-pulse 1.5s infinite;
}

@keyframes border-pulse {
  0% { opacity: 1; }
  50% { opacity: 0.3; }
  100% { opacity: 1; }
}

.avatar {
  font-size: 32px;
  width: 56px;
  height: 56px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(0,0,0,0.5);
  border: 3px solid rgba(255,255,255,0.2);
  border-radius: 12px;
  position: relative;
}

.avatar.is-landlord {
  border-color: var(--color-accent-red);
  background: rgba(255, 71, 87, 0.2);
}

.offline-overlay {
  position: absolute;
  inset: 0;
  background: rgba(0,0,0,0.8);
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  font-weight: 800;
  color: var(--color-accent-red);
  letter-spacing: 1px;
}

.info {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.name-row {
  display: flex;
  align-items: center;
  gap: 8px;
}

.player-name {
  font-size: 16px;
  font-weight: 700;
  color: #fff;
}

.badge {
  font-size: 10px;
  font-weight: 800;
  padding: 2px 6px;
  border-radius: 6px;
  letter-spacing: 1px;
}

.trustee {
  background: var(--color-accent-blue);
  color: #fff;
}

.cards-count {
  font-size: 14px;
  color: var(--color-text-secondary);
  display: flex;
  align-items: center;
  gap: 6px;
}

.count-num {
  font-size: 18px;
  font-weight: 700;
  color: #fff;
}

.count-num.warning {
  color: var(--color-accent-red);
  animation: pulse-urgent 0.5s infinite alternate;
}
</style>
