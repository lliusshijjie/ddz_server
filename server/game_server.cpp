#include "server/game_server.h"

#include <chrono>

#include "common/util/kv_codec.h"
#include "common/util/time_util.h"
#include "log/logger.h"
#include "network/packet_codec.h"

namespace ddz {

GameServer::~GameServer() {
    Stop();
}

bool GameServer::Start(const std::string& config_path, std::string* err) {
    if (running_.exchange(true)) {
        return true;
    }

    std::string config_err;
    if (!ConfigLoader::LoadFromFile(config_path, config_, &config_err)) {
        if (err != nullptr) *err = config_err;
        running_.store(false);
        return false;
    }
    if (config_.server.max_packet_size < PacketCodec::kHeaderSize) {
        if (err != nullptr) {
            *err = "invalid server.max_packet_size, must be >= " + std::to_string(PacketCodec::kHeaderSize);
        }
        running_.store(false);
        return false;
    }
    if (config_.server.heartbeat_timeout_ms <= 0) {
        if (err != nullptr) {
            *err = "invalid server.heartbeat_timeout_ms, must be > 0";
        }
        running_.store(false);
        return false;
    }
    if (config_.server.match_timeout_ms <= 0) {
        if (err != nullptr) {
            *err = "invalid server.match_timeout_ms, must be > 0";
        }
        running_.store(false);
        return false;
    }
    if (config_.auth.token_ttl_seconds <= 0) {
        if (err != nullptr) {
            *err = "invalid auth.token_ttl_seconds, must be > 0";
        }
        running_.store(false);
        return false;
    }

    std::string log_err;
    if (!Logger::Instance().Init(config_.log.level, config_.log.dir, &log_err)) {
        if (err != nullptr) *err = log_err;
        running_.store(false);
        return false;
    }

    std::string storage_err;
    if (!storage_service_.Init(config_, &storage_err)) {
        if (err != nullptr) *err = storage_err;
        running_.store(false);
        Logger::Instance().Shutdown();
        return false;
    }

    auth_token_service_.Configure(config_.auth.token_secret, config_.auth.token_ttl_seconds);
    RegisterHandlers();
    tcp_server_ = std::make_unique<TcpServer>(config_.server.host, config_.server.port, config_.server.max_packet_size);
    tcp_server_->SetMessageCallback([this](int64_t connection_id, const Packet& packet) { OnMessage(connection_id, packet); });
    tcp_server_->SetCloseCallback([this](int64_t connection_id) { OnConnectionClosed(connection_id); });

    std::string start_err;
    if (!tcp_server_->Start(&start_err)) {
        if (err != nullptr) *err = start_err;
        running_.store(false);
        return false;
    }

    timer_thread_ = std::thread(&GameServer::TimerLoop, this);
    Logger::Instance().Info(
        "game server started host=" + config_.server.host +
        " port=" + std::to_string(config_.server.port) +
        " max_packet_size=" + std::to_string(config_.server.max_packet_size) +
        " heartbeat_timeout_ms=" + std::to_string(config_.server.heartbeat_timeout_ms) +
        " match_timeout_ms=" + std::to_string(config_.server.match_timeout_ms));
    return true;
}

void GameServer::Stop() {
    const bool was_running = running_.exchange(false);
    if (!was_running) {
        return;
    }

    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
    if (tcp_server_ != nullptr) {
        tcp_server_->Stop();
        tcp_server_.reset();
    }
    Logger::Instance().Info("game server stopped");
    Logger::Instance().Shutdown();
}

void GameServer::RegisterHandlers() {
    dispatcher_.Register(MSG_LOGIN_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        const LoginResult r = login_service_.HandleLogin(connection_id, request.body, NowMs());
        response.msg_id = MSG_LOGIN_RESP;
        response.seq_id = request.seq_id;
        response.player_id = r.player_id;
        response.body = BuildKvBody({
            {"code", std::to_string(static_cast<int32_t>(r.code))},
            {"player_id", std::to_string(r.player_id)},
            {"nickname", r.nickname},
            {"coin", std::to_string(r.coin)},
            {"token", r.token},
            {"expire_at_ms", std::to_string(r.expire_at_ms)}
        });
        if (r.old_connection_to_kick.has_value() &&
            r.old_connection_to_kick.value() != connection_id &&
            tcp_server_ != nullptr) {
            tcp_server_->CloseConnection(r.old_connection_to_kick.value());
        }
        return true;
    });

    dispatcher_.Register(MSG_HEARTBEAT_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        response.msg_id = MSG_HEARTBEAT_RESP;
        response.seq_id = request.seq_id;
        response.player_id = request.player_id;
        if (!session_manager_.UpdateHeartbeatByConnection(connection_id, NowMs())) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::NOT_LOGIN))}});
            return true;
        }
        response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::OK))}});
        return true;
    });

    dispatcher_.Register(MSG_MATCH_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        const MatchResult r = match_service_.HandleMatch(connection_id, request.body, NowMs());
        response.msg_id = MSG_MATCH_RESP;
        response.seq_id = request.seq_id;
        response.player_id = r.player_id;
        response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(r.code))}});
        if (r.matched) {
            NotifyMatchSuccess(r.room_id, r.mode, r.matched_players);
        }
        return true;
    });

    dispatcher_.Register(MSG_CANCEL_MATCH_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        const CancelMatchResult r = match_service_.HandleCancelMatch(connection_id);
        response.msg_id = MSG_CANCEL_MATCH_RESP;
        response.seq_id = request.seq_id;
        response.player_id = r.player_id;
        response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(r.code))}});
        return true;
    });

    dispatcher_.Register(MSG_RECONNECT_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        const ReconnectResult r = reconnect_service_.HandleReconnect(connection_id, request.body, NowMs());
        response.msg_id = MSG_RECONNECT_RESP;
        response.seq_id = request.seq_id;
        response.player_id = r.player_id;
        response.body = BuildKvBody({
            {"code", std::to_string(static_cast<int32_t>(r.code))},
            {"player_id", std::to_string(r.player_id)},
            {"room_id", std::to_string(r.room_id)}
        });

        if (r.old_connection_to_kick.has_value() &&
            r.old_connection_to_kick.value() != connection_id &&
            tcp_server_ != nullptr) {
            tcp_server_->CloseConnection(r.old_connection_to_kick.value());
        }
        if (r.code == ErrorCode::OK && r.snapshot.has_value()) {
            NotifyRoomSnapshot(r.player_id, connection_id, r.snapshot.value());
            NotifyPlayerReconnectInRoom(r.snapshot->room_id, r.player_id);
        }
        return true;
    });

    dispatcher_.Register(MSG_SETTLEMENT_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        response.msg_id = MSG_SETTLEMENT_RESP;
        response.seq_id = request.seq_id;
        response.player_id = 0;

        const auto sender_player_id = session_manager_.GetPlayerIdByConnection(connection_id);
        if (!sender_player_id.has_value()) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::NOT_LOGIN))}});
            return true;
        }
        response.player_id = sender_player_id.value();

        const auto kv = ParseKvBody(request.body);
        if (kv.find("room_id") == kv.end() || kv.find("winner_player_id") == kv.end() || kv.find("base_coin") == kv.end()) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
            return true;
        }

        SettlementRequest settle_req;
        try {
            settle_req.room_id = std::stoll(kv.at("room_id"));
            settle_req.winner_player_id = std::stoll(kv.at("winner_player_id"));
            settle_req.base_coin = std::stoll(kv.at("base_coin"));
        } catch (...) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
            return true;
        }

        const auto sender_room_id = room_manager_.GetRoomIdByPlayer(sender_player_id.value());
        if (!sender_room_id.has_value() || sender_room_id.value() != settle_req.room_id) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PLAYER_STATE))}});
            return true;
        }

        const SettlementResult settle_result = settlement_service_.Settle(settle_req, NowMs());
        response.body = BuildKvBody({
            {"code", std::to_string(static_cast<int32_t>(settle_result.code))},
            {"room_id", std::to_string(settle_result.room_id)},
            {"record_id", std::to_string(settle_result.record_id)}
        });
        if (settle_result.code == ErrorCode::OK) {
            NotifyGameOver(settle_result);
        }
        return true;
    });
}

void GameServer::TimerLoop() {
    while (running_.load()) {
        if (tcp_server_ != nullptr) {
            tcp_server_->CloseIdleConnections(config_.server.heartbeat_timeout_ms);
        }
        const auto timeout_events = match_service_.HandleMatchTimeout(NowMs(), config_.server.match_timeout_ms);
        if (!timeout_events.empty()) {
            NotifyMatchTimeout(timeout_events);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void GameServer::OnMessage(int64_t connection_id, const Packet& packet) {
    Logger::Instance().Info(
        "recv conn_id=" + std::to_string(connection_id) +
        " msg_id=" + std::to_string(packet.msg_id) +
        " seq_id=" + std::to_string(packet.seq_id) +
        " player_id=" + std::to_string(packet.player_id) +
        " body_len=" + std::to_string(packet.body.size()));

    Packet response;
    const bool need_reply = dispatcher_.Dispatch(packet, response, connection_id);
    if (need_reply && tcp_server_ != nullptr) {
        Logger::Instance().Info(
            "send conn_id=" + std::to_string(connection_id) +
            " msg_id=" + std::to_string(response.msg_id) +
            " seq_id=" + std::to_string(response.seq_id) +
            " player_id=" + std::to_string(response.player_id) +
            " body_len=" + std::to_string(response.body.size()));
        if (!tcp_server_->SendPacket(connection_id, response)) {
            Logger::Instance().Warn("send response failed conn_id=" + std::to_string(connection_id));
        }
    }
}

void GameServer::OnConnectionClosed(int64_t connection_id) {
    Logger::Instance().Info("connection closed conn_id=" + std::to_string(connection_id));
    const auto player_id = session_manager_.GetPlayerIdByConnection(connection_id);
    if (!player_id.has_value()) {
        return;
    }
    if (player_manager_.GetState(player_id.value()) == PlayerState::Matching) {
        match_manager_.Cancel(player_id.value());
    }
    session_manager_.MarkOfflineByConnection(connection_id, NowMs());
    player_manager_.ForceState(player_id.value(), PlayerState::Offline);
}

void GameServer::NotifyMatchSuccess(int64_t room_id, int32_t mode, const std::vector<int64_t>& players) {
    if (tcp_server_ == nullptr) {
        return;
    }
    std::string players_joined;
    for (size_t i = 0; i < players.size(); ++i) {
        if (i > 0) {
            players_joined.push_back(',');
        }
        players_joined += std::to_string(players[i]);
    }

    for (const auto player_id : players) {
        const auto conn_id = session_manager_.GetConnectionIdByPlayer(player_id);
        if (!conn_id.has_value()) {
            continue;
        }
        Packet notify;
        notify.msg_id = MSG_MATCH_SUCCESS_NOTIFY;
        notify.seq_id = 0;
        notify.player_id = player_id;
        notify.body = BuildKvBody({
            {"room_id", std::to_string(room_id)},
            {"mode", std::to_string(mode)},
            {"players", players_joined}
        });
        tcp_server_->SendPacket(conn_id.value(), notify);
    }
}

void GameServer::NotifyMatchTimeout(const std::vector<MatchTimeoutEvent>& timeout_players) {
    if (tcp_server_ == nullptr) {
        return;
    }
    for (const auto& event : timeout_players) {
        const auto conn_id = session_manager_.GetConnectionIdByPlayer(event.player_id);
        if (!conn_id.has_value()) {
            continue;
        }
        Packet notify;
        notify.msg_id = MSG_MATCH_TIMEOUT_NOTIFY;
        notify.seq_id = 0;
        notify.player_id = event.player_id;
        notify.body = BuildKvBody({
            {"code", std::to_string(static_cast<int32_t>(ErrorCode::MATCH_TIMEOUT))},
            {"mode", std::to_string(event.mode)}
        });
        tcp_server_->SendPacket(conn_id.value(), notify);
    }
}

void GameServer::NotifyRoomSnapshot(int64_t player_id, int64_t connection_id, const RoomSnapshot& snapshot) {
    if (tcp_server_ == nullptr) {
        return;
    }
    std::string players_joined;
    for (size_t i = 0; i < snapshot.players.size(); ++i) {
        if (i > 0) {
            players_joined.push_back(',');
        }
        players_joined += std::to_string(snapshot.players[i]);
    }

    Packet notify;
    notify.msg_id = MSG_ROOM_SNAPSHOT_NOTIFY;
    notify.seq_id = 0;
    notify.player_id = player_id;
    notify.body = BuildKvBody({
        {"room_id", std::to_string(snapshot.room_id)},
        {"mode", std::to_string(snapshot.mode)},
        {"room_state", std::to_string(snapshot.room_state)},
        {"players", players_joined},
        {"current_operator_player_id", std::to_string(snapshot.current_operator_player_id)},
        {"remain_seconds", std::to_string(snapshot.remain_seconds)}
    });
    tcp_server_->SendPacket(connection_id, notify);
}

void GameServer::NotifyPlayerReconnectInRoom(int64_t room_id, int64_t reconnect_player_id) {
    if (tcp_server_ == nullptr) {
        return;
    }
    const auto room = room_manager_.GetRoomById(room_id);
    if (!room.has_value()) {
        return;
    }
    for (const auto player_id : room->players) {
        if (player_id == reconnect_player_id) {
            continue;
        }
        const auto conn_id = session_manager_.GetConnectionIdByPlayer(player_id);
        if (!conn_id.has_value()) {
            continue;
        }
        Packet notify;
        notify.msg_id = MSG_PLAYER_RECONNECT_NOTIFY;
        notify.seq_id = 0;
        notify.player_id = player_id;
        notify.body = BuildKvBody({
            {"room_id", std::to_string(room_id)},
            {"player_id", std::to_string(reconnect_player_id)}
        });
        tcp_server_->SendPacket(conn_id.value(), notify);
    }
}

void GameServer::NotifyGameOver(const SettlementResult& result) {
    if (tcp_server_ == nullptr) {
        return;
    }
    for (const auto& p : result.players) {
        const auto conn_id = session_manager_.GetConnectionIdByPlayer(p.player_id);
        if (!conn_id.has_value()) {
            continue;
        }
        Packet notify;
        notify.msg_id = MSG_GAME_OVER_NOTIFY;
        notify.seq_id = 0;
        notify.player_id = p.player_id;
        notify.body = BuildKvBody({
            {"room_id", std::to_string(result.room_id)},
            {"winner_player_id", std::to_string(result.winner_player_id)},
            {"coin_change", std::to_string(p.change_amount)},
            {"after_coin", std::to_string(p.after_coin)}
        });
        tcp_server_->SendPacket(conn_id.value(), notify);
    }
}

}  // namespace ddz
