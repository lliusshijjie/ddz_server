import type { GatewayEnvelope } from '../protocol/types'

export function parseEnvelopeText(text: string): GatewayEnvelope {
  const raw = JSON.parse(text) as Partial<GatewayEnvelope>
  if (
    typeof raw.msg_id !== 'number' ||
    typeof raw.seq_id !== 'number' ||
    typeof raw.player_id !== 'number' ||
    typeof raw.body !== 'string'
  ) {
    throw new Error('invalid envelope payload')
  }
  return {
    msg_id: raw.msg_id,
    seq_id: raw.seq_id,
    player_id: raw.player_id,
    body: raw.body,
    h5_request_id: raw.h5_request_id,
    gateway_trace_id: raw.gateway_trace_id,
    server_trace_id: raw.server_trace_id,
  }
}

export function toEnvelopeText(envelope: GatewayEnvelope): string {
  return JSON.stringify(envelope)
}
