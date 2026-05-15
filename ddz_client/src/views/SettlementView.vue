<script setup lang="ts">
import { useRouter } from 'vue-router'
import { useSettlementStore } from '../stores/settlement'

const router = useRouter()
const settlementStore = useSettlementStore()

async function backLobby() {
  settlementStore.clear()
  await router.push('/lobby')
}
</script>

<template>
  <div class="page">
    <h2>结算</h2>
    <div v-if="settlementStore.latest" class="card">
      <div>房间：{{ settlementStore.latest.roomId }}</div>
      <div>赢家：{{ settlementStore.latest.winnerPlayerId }}</div>
      <div>本局增减：{{ settlementStore.latest.coinChange }}</div>
      <div>当前金币：{{ settlementStore.latest.afterCoin }}</div>
    </div>
    <p v-else>暂无结算数据</p>
    <button type="button" @click="backLobby">返回大厅</button>
  </div>
</template>

<style scoped>
.page {
  width: min(560px, 100%);
  margin: 24px auto;
  padding: 16px;
  border: 1px solid #ddd7ec;
  border-radius: 12px;
  background: #fff;
  display: grid;
  gap: 12px;
}

.card {
  border: 1px dashed #bcb3d3;
  border-radius: 10px;
  padding: 10px;
  display: grid;
  gap: 6px;
}

button {
  border: 1px solid #7562d3;
  background: #7562d3;
  color: #fff;
  border-radius: 8px;
  padding: 8px 12px;
  cursor: pointer;
}
</style>
