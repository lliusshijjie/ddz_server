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
  <div class="seat">
    <div class="title">
      <span>玩家 {{ playerId || '-' }}</span>
      <span v-if="isCurrent()" class="badge turn">当前操作</span>
      <span v-if="isLandlord()" class="badge landlord">地主</span>
      <span v-if="!online" class="badge offline">离线</span>
      <span v-if="trustee" class="badge trustee">托管</span>
    </div>
    <div class="meta">手牌数：{{ cardCount }}</div>
  </div>
</template>

<style scoped>
.seat {
  border: 1px solid #d7d2e5;
  border-radius: 10px;
  padding: 8px;
  background: #faf8ff;
}

.title {
  display: flex;
  gap: 6px;
  align-items: center;
  flex-wrap: wrap;
}

.meta {
  margin-top: 6px;
  color: #6f6a80;
  font-size: 12px;
}

.badge {
  font-size: 11px;
  padding: 2px 6px;
  border-radius: 999px;
}

.turn {
  background: #e7f5ff;
  color: #0d5f91;
}

.landlord {
  background: #ffecd9;
  color: #9c5d09;
}

.offline {
  background: #fff3e0;
  color: #8c5100;
}

.trustee {
  background: #fde8f0;
  color: #8e255f;
}
</style>
