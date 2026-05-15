#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "common/config/config_loader.h"
#include "network/dispatcher.h"
#include "network/tcp_server.h"
#include "service/login/login_service.h"
#include "service/match/match_service.h"
#include "service/match/match_manager.h"
#include "service/observability/observability_service.h"
#include "service/player/player_manager.h"
#include "service/redis/redis_optional_store.h"
#include "service/reconnect/reconnect_service.h"
#include "service/room/room_manager.h"
#include "service/settlement/settlement_service.h"
#include "service/session/session_manager.h"
#include "service/storage/storage_service.h"

namespace ddz {

class GameServer {
public:
    static constexpr uint32_t MSG_LOGIN_REQ = 1001;
    static constexpr uint32_t MSG_LOGIN_RESP = 1002;
    static constexpr uint32_t MSG_HEARTBEAT_REQ = 1101;
    static constexpr uint32_t MSG_HEARTBEAT_RESP = 1102;
    static constexpr uint32_t MSG_MATCH_REQ = 2001;
    static constexpr uint32_t MSG_MATCH_RESP = 2002;
    static constexpr uint32_t MSG_CANCEL_MATCH_REQ = 2003;
    static constexpr uint32_t MSG_CANCEL_MATCH_RESP = 2004;
    static constexpr uint32_t MSG_MATCH_SUCCESS_NOTIFY = 2005;
    static constexpr uint32_t MSG_MATCH_TIMEOUT_NOTIFY = 2006;
    static constexpr uint32_t MSG_ROOM_SNAPSHOT_NOTIFY = 3001;
    static constexpr uint32_t MSG_PLAYER_RECONNECT_NOTIFY = 3003;
    static constexpr uint32_t MSG_PLAYER_ACTION_REQ = 4001;
    static constexpr uint32_t MSG_PLAYER_ACTION_RESP = 4002;
    static constexpr uint32_t MSG_ROOM_STATE_PUSH = 4003;
    static constexpr uint32_t MSG_GAME_RESULT_PUSH = 4004;
    static constexpr uint32_t MSG_GAME_OVER_NOTIFY = 5001;
    static constexpr uint32_t MSG_SETTLEMENT_REQ = 5002;
    static constexpr uint32_t MSG_SETTLEMENT_RESP = 5003;
    static constexpr uint32_t MSG_RECONNECT_REQ = 6001;
    static constexpr uint32_t MSG_RECONNECT_RESP = 6002;

    GameServer() = default;
    ~GameServer();

    bool Start(const std::string& config_path, std::string* err);
    void Stop();

private:
    void RegisterHandlers();
    void TimerLoop();
    void OnMessage(int64_t connection_id, const Packet& packet);
    void OnConnectionClosed(int64_t connection_id);
    void NotifyMatchSuccess(int64_t room_id, int32_t mode, const std::vector<int64_t>& players);
    void NotifyMatchTimeout(const std::vector<MatchTimeoutEvent>& timeout_players);
    void NotifyRoomSnapshot(int64_t player_id, int64_t connection_id, const RoomSnapshot& snapshot);
    void NotifyPlayerReconnectInRoom(int64_t room_id, int64_t reconnect_player_id);
    void NotifyRoomStatePush(int64_t room_id, const RoomSnapshot& snapshot);
    void NotifyGameOver(const SettlementResult& result);

private:
    AppConfig config_;
    Dispatcher dispatcher_;
    std::unique_ptr<TcpServer> tcp_server_;
    SessionManager session_manager_;
    PlayerManager player_manager_;
    MatchManager match_manager_;
    RoomManager room_manager_;
    StorageService storage_service_;
    RedisOptionalStore redis_store_;
    ObservabilityService observability_;
    AuthTokenService auth_token_service_;
    LoginService login_service_{session_manager_, player_manager_, auth_token_service_, &redis_store_};
    MatchService match_service_{session_manager_, player_manager_, match_manager_, room_manager_};
    ReconnectService reconnect_service_{session_manager_, auth_token_service_, player_manager_, room_manager_, &redis_store_};
    SettlementService settlement_service_{session_manager_, player_manager_, room_manager_, storage_service_};
    std::atomic<bool> running_{false};
    std::thread timer_thread_;
};

}  // namespace ddz
