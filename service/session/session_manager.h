#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace ddz {

struct Session {
    int64_t player_id = 0;
    int64_t connection_id = 0;
    int64_t room_id = 0;
    bool online = false;
    int64_t login_time_ms = 0;
    int64_t last_heartbeat_ms = 0;
};

class SessionManager {
public:
    // Returns old connection id if duplicated login happened.
    std::optional<int64_t> BindLogin(int64_t player_id, int64_t connection_id, int64_t now_ms);
    // Rebinds an existing player session to a new connection.
    // Returns old connection id if there is one and differs from new connection.
    std::optional<int64_t> RebindForReconnect(int64_t player_id, int64_t new_connection_id, int64_t now_ms);

    bool IsLoggedInConnection(int64_t connection_id) const;
    std::optional<int64_t> GetPlayerIdByConnection(int64_t connection_id) const;
    std::optional<int64_t> GetConnectionIdByPlayer(int64_t player_id) const;
    std::optional<Session> GetSessionByPlayer(int64_t player_id) const;

    bool UpdateHeartbeatByConnection(int64_t connection_id, int64_t now_ms);
    bool MarkOfflineByConnection(int64_t connection_id, int64_t now_ms);
    bool SetRoomId(int64_t player_id, int64_t room_id);
    bool ClearRoomId(int64_t player_id);

private:
    mutable std::mutex mu_;
    std::unordered_map<int64_t, int64_t> conn_to_player_;
    std::unordered_map<int64_t, Session> player_to_session_;
};

}  // namespace ddz
