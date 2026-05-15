import type { LoginTicketResponse } from '../protocol/types'

export class AuthService {
  private readonly httpBase: string
  private readonly devKey: string

  public constructor(httpBase: string, devKey: string) {
    this.httpBase = httpBase.replace(/\/$/, '')
    this.devKey = devKey
  }

  public async fetchLoginTicket(playerId: number, nickname: string): Promise<LoginTicketResponse> {
    const response = await fetch(`${this.httpBase}/api/auth/login-ticket`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-Dev-Key': this.devKey,
      },
      body: JSON.stringify({
        player_id: playerId,
        nickname,
      }),
    })
    const json = (await response.json()) as LoginTicketResponse
    return json
  }
}
