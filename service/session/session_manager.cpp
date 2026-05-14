#include "service/session/session_manager.h"

namespace ddz {

std::optional<int64_t> SessionManager::BindLogin(int64_t player_id, int64_t connection_id, int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    std::optional<int64_t> old_conn;

    auto it = player_to_session_.find(player_id);
    if (it != player_to_session_.end() && it->second.online) {
        old_conn = it->second.connection_id;
        conn_to_player_.erase(it->second.connection_id);
    }

    Session& s = player_to_session_[player_id];
    s.player_id = player_id;
    s.connection_id = connection_id;
    s.online = true;
    s.login_time_ms = now_ms;
    s.last_heartbeat_ms = now_ms;
    s.offline_at_ms = 0;

    conn_to_player_[connection_id] = player_id;
    return old_conn;
}

std::optional<int64_t> SessionManager::RebindForReconnect(int64_t player_id, int64_t new_connection_id, int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = player_to_session_.find(player_id);
    if (it == player_to_session_.end()) {
        return std::nullopt;
    }

    std::optional<int64_t> old_conn;
    const int64_t prev_conn = it->second.connection_id;
    if (it->second.online && prev_conn != 0 && prev_conn != new_connection_id) {
        old_conn = prev_conn;
        conn_to_player_.erase(prev_conn);
    } else if (!it->second.online && prev_conn != new_connection_id) {
        conn_to_player_.erase(prev_conn);
    }

    it->second.connection_id = new_connection_id;
    it->second.online = true;
    it->second.last_heartbeat_ms = now_ms;
    it->second.offline_at_ms = 0;
    conn_to_player_[new_connection_id] = player_id;
    return old_conn;
}

bool SessionManager::IsLoggedInConnection(int64_t connection_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return conn_to_player_.find(connection_id) != conn_to_player_.end();
}

std::optional<int64_t> SessionManager::GetPlayerIdByConnection(int64_t connection_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = conn_to_player_.find(connection_id);
    if (it == conn_to_player_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<int64_t> SessionManager::GetConnectionIdByPlayer(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = player_to_session_.find(player_id);
    if (it == player_to_session_.end() || !it->second.online) {
        return std::nullopt;
    }
    return it->second.connection_id;
}

std::optional<Session> SessionManager::GetSessionByPlayer(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = player_to_session_.find(player_id);
    if (it == player_to_session_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Session> SessionManager::GetAllSessions() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<Session> out;
    out.reserve(player_to_session_.size());
    for (const auto& item : player_to_session_) {
        out.push_back(item.second);
    }
    return out;
}

bool SessionManager::UpdateHeartbeatByConnection(int64_t connection_id, int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = conn_to_player_.find(connection_id);
    if (it == conn_to_player_.end()) {
        return false;
    }
    auto sit = player_to_session_.find(it->second);
    if (sit == player_to_session_.end()) {
        return false;
    }
    sit->second.last_heartbeat_ms = now_ms;
    return true;
}

bool SessionManager::MarkOfflineByConnection(int64_t connection_id, int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = conn_to_player_.find(connection_id);
    if (it == conn_to_player_.end()) {
        return false;
    }
    const int64_t player_id = it->second;
    conn_to_player_.erase(it);

    auto sit = player_to_session_.find(player_id);
    if (sit == player_to_session_.end()) {
        return false;
    }
    sit->second.online = false;
    sit->second.last_heartbeat_ms = now_ms;
    sit->second.offline_at_ms = now_ms;
    return true;
}

bool SessionManager::SetRoomId(int64_t player_id, int64_t room_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = player_to_session_.find(player_id);
    if (it == player_to_session_.end()) {
        return false;
    }
    it->second.room_id = room_id;
    return true;
}

bool SessionManager::ClearRoomId(int64_t player_id) {
    return SetRoomId(player_id, 0);
}

}  // namespace ddz
