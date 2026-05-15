#include "server/game_server.h"

#include <atomic>
#include <chrono>
#include <optional>
#include <sstream>

#include "common/util/kv_codec.h"
#include "common/util/time_util.h"
#include "log/logger.h"
#include "network/packet_codec.h"

namespace ddz {
namespace {

std::atomic<uint64_t> g_trace_seq{1};

std::vector<int32_t> ParseCardList(const std::string& text, bool* ok) {
    std::vector<int32_t> out;
    *ok = true;
    if (text.empty()) {
        return out;
    }
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) {
            *ok = false;
            return {};
        }
        try {
            size_t idx = 0;
            const int32_t v = static_cast<int32_t>(std::stoll(item, &idx));
            if (idx != item.size()) {
                *ok = false;
                return {};
            }
            out.push_back(v);
        } catch (...) {
            *ok = false;
            return {};
        }
    }
    return out;
}

template <typename T>
std::string JoinNumberList(const std::vector<T>& values) {
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out.push_back(',');
        }
        out += std::to_string(values[i]);
    }
    return out;
}

std::string BuildCardCountList(const std::vector<std::pair<int64_t, int32_t>>& card_counts) {
    std::string out;
    for (size_t i = 0; i < card_counts.size(); ++i) {
        if (i > 0) {
            out.push_back(',');
        }
        out += std::to_string(card_counts[i].first);
        out.push_back(':');
        out += std::to_string(card_counts[i].second);
    }
    return out;
}

std::string GenerateTraceId(int64_t connection_id, uint32_t seq_id) {
    return std::to_string(NowMs()) + "-" +
           std::to_string(connection_id) + "-" +
           std::to_string(seq_id) + "-" +
           std::to_string(g_trace_seq.fetch_add(1));
}

std::string EventNameByMsgId(uint32_t msg_id) {
    switch (msg_id) {
    case GameServer::MSG_LOGIN_REQ: return "login";
    case GameServer::MSG_HEARTBEAT_REQ: return "heartbeat";
    case GameServer::MSG_MATCH_REQ: return "match";
    case GameServer::MSG_CANCEL_MATCH_REQ: return "cancel_match";
    case GameServer::MSG_RECONNECT_REQ: return "reconnect";
    case GameServer::MSG_PLAYER_ACTION_REQ: return "player_action";
    case GameServer::MSG_SETTLEMENT_REQ: return "settlement_request";
    default: return "unknown";
    }
}

ErrorCode ParseCodeFromResponseBody(const std::string& body) {
    const auto kv = ParseKvBody(body);
    const auto it = kv.find("code");
    if (it == kv.end()) {
        return ErrorCode::UNKNOWN_ERROR;
    }
    try {
        return static_cast<ErrorCode>(std::stoi(it->second));
    } catch (...) {
        return ErrorCode::UNKNOWN_ERROR;
    }
}

}  // namespace

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
    if (config_.server.io_threads <= 0) {
        if (err != nullptr) {
            *err = "invalid server.io_threads, must be > 0";
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
    if (config_.observability.metrics_report_interval_ms <= 0) {
        if (err != nullptr) {
            *err = "invalid observability.metrics_report_interval_ms, must be > 0";
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

    std::string redis_err;
    if (!redis_store_.Init(config_.redis, &redis_err)) {
        if (err != nullptr) *err = redis_err;
        running_.store(false);
        Logger::Instance().Shutdown();
        return false;
    }

    auth_token_service_.Configure(config_.auth.token_secret, config_.auth.token_ttl_seconds);
    observability_.Configure(config_.observability.enable_structured_log, config_.observability.metrics_report_interval_ms);
    RegisterHandlers();
    tcp_server_ = std::make_unique<TcpServer>(
        config_.server.host, config_.server.port, config_.server.max_packet_size, config_.server.io_threads);
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
        std::optional<RoomSnapshot> snapshot;
        if (r.code == ErrorCode::OK && r.reconnect_mode && r.room_id > 0) {
            snapshot = room_manager_.BuildSnapshotByRoomId(r.room_id);
        }
        response.msg_id = MSG_LOGIN_RESP;
        response.seq_id = request.seq_id;
        response.player_id = r.player_id;
        response.body = BuildKvBody({
            {"code", std::to_string(static_cast<int32_t>(r.code))},
            {"player_id", std::to_string(r.player_id)},
            {"nickname", r.nickname},
            {"coin", std::to_string(r.coin)},
            {"room_id", std::to_string(r.room_id)},
            {"reconnect_mode", r.reconnect_mode ? "1" : "0"},
            {"snapshot_version", std::to_string(snapshot.has_value() ? snapshot->snapshot_version : 0)},
            {"token", r.token},
            {"expire_at_ms", std::to_string(r.expire_at_ms)}
        });
        if (r.old_connection_to_kick.has_value() &&
            r.old_connection_to_kick.value() != connection_id &&
            tcp_server_ != nullptr) {
            tcp_server_->CloseConnection(r.old_connection_to_kick.value());
        }
        if (r.code == ErrorCode::OK && r.reconnect_mode && snapshot.has_value()) {
            room_manager_.MarkPlayerOnline(r.player_id);
            NotifyRoomSnapshot(r.player_id, connection_id, snapshot.value());
            NotifyPrivateHandByConnection(r.player_id, connection_id, r.room_id, snapshot->snapshot_version);
            if (const auto push_v2 = room_manager_.BuildPushSnapshotV2ByRoomId(r.room_id); push_v2.has_value()) {
                NotifyRoomStatePushV2(r.room_id, push_v2.value());
            }
            NotifyPlayerReconnectInRoom(r.room_id, r.player_id);
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
        if (redis_store_.enabled()) {
            const auto player_id = session_manager_.GetPlayerIdByConnection(connection_id);
            if (player_id.has_value()) {
                const auto session = session_manager_.GetSessionByPlayer(player_id.value());
                std::string redis_err;
                const int64_t ttl_ms = config_.auth.token_ttl_seconds * 1000;
                if (session.has_value()) {
                    redis_store_.SetOnline(player_id.value(), true, ttl_ms, &redis_err);
                    redis_store_.UpsertSession(player_id.value(), session->room_id, ttl_ms, &redis_err);
                }
            }
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
            {"room_id", std::to_string(r.room_id)},
            {"snapshot_version", std::to_string(r.snapshot.has_value() ? r.snapshot->snapshot_version : 0)}
        });

        if (r.old_connection_to_kick.has_value() &&
            r.old_connection_to_kick.value() != connection_id &&
            tcp_server_ != nullptr) {
            tcp_server_->CloseConnection(r.old_connection_to_kick.value());
        }
        if ((r.code == ErrorCode::OK || r.code == ErrorCode::SNAPSHOT_VERSION_CONFLICT) && r.snapshot.has_value()) {
            room_manager_.MarkPlayerOnline(r.player_id);
            NotifyRoomSnapshot(r.player_id, connection_id, r.snapshot.value());
            NotifyPrivateHandByConnection(r.player_id, connection_id, r.snapshot->room_id, r.snapshot->snapshot_version);
            if (const auto push_v2 = room_manager_.BuildPushSnapshotV2ByRoomId(r.snapshot->room_id); push_v2.has_value()) {
                NotifyRoomStatePushV2(r.snapshot->room_id, push_v2.value());
            }
            NotifyPlayerReconnectInRoom(r.snapshot->room_id, r.player_id);
        }
        return true;
    });

    dispatcher_.Register(MSG_PLAYER_ACTION_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        response.msg_id = MSG_PLAYER_ACTION_RESP;
        response.seq_id = request.seq_id;
        response.player_id = 0;

        const auto sender_player_id = session_manager_.GetPlayerIdByConnection(connection_id);
        if (!sender_player_id.has_value()) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::NOT_LOGIN))}});
            return true;
        }
        response.player_id = sender_player_id.value();

        const auto kv = ParseKvBody(request.body);
        if (kv.find("room_id") == kv.end() || kv.find("action_type") == kv.end()) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
            return true;
        }

        int64_t room_id = 0;
        int32_t action_type = 0;
        try {
            room_id = std::stoll(kv.at("room_id"));
            action_type = static_cast<int32_t>(std::stoll(kv.at("action_type")));
        } catch (...) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
            return true;
        }
        if (room_id <= 0 || action_type <= 0) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
            return true;
        }

        PlayerActionRequest action_req;
        action_req.action_type = static_cast<PlayerActionType>(action_type);
        if (action_req.action_type == PlayerActionType::CallScore) {
            auto it = kv.find("score");
            if (it == kv.end()) {
                response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
                return true;
            }
            try {
                action_req.call_score = static_cast<int32_t>(std::stoll(it->second));
            } catch (...) {
                response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
                return true;
            }
        } else if (action_req.action_type == PlayerActionType::RobLandlord) {
            auto it = kv.find("rob");
            if (it == kv.end()) {
                response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
                return true;
            }
            try {
                action_req.rob = static_cast<int32_t>(std::stoll(it->second));
            } catch (...) {
                response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
                return true;
            }
        } else if (action_req.action_type == PlayerActionType::PlayCards) {
            auto it = kv.find("cards");
            if (it == kv.end()) {
                response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
                return true;
            }
            bool parse_ok = false;
            action_req.cards = ParseCardList(it->second, &parse_ok);
            if (!parse_ok) {
                response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
                return true;
            }
        } else if (action_req.action_type != PlayerActionType::Pass) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
            return true;
        }

        std::string action_err;
        const auto action_result = room_manager_.ApplyPlayerAction(
            room_id, sender_player_id.value(), action_req, &action_err);
        if (!action_result.has_value()) {
            response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PLAYER_STATE))}});
            return true;
        }

        const auto& snapshot = action_result->snapshot;
        ErrorCode action_code = ErrorCode::OK;
        int64_t settlement_record_id = 0;
        NotifyRoomStatePush(room_id, snapshot);
        if (const auto push_v2 = room_manager_.BuildPushSnapshotV2ByRoomId(room_id); push_v2.has_value()) {
            NotifyRoomStatePushV2(room_id, push_v2.value());
        }
        NotifyPrivateHandsInRoom(room_id, snapshot.snapshot_version);

        if (action_result->game_over) {
            const int64_t settle_begin = NowMs();
            const SettlementResult settle_result = settlement_service_.SettleByServerResult(
                ServerSettlementDecision{room_id, action_result->winner_player_id, action_result->base_coin},
                NowMs());
            observability_.Record("auto_settlement", settle_result.code, NowMs() - settle_begin);
            if (settle_result.code == ErrorCode::OK) {
                settlement_record_id = settle_result.record_id;
                NotifyGameOver(settle_result);
            } else {
                action_code = settle_result.code;
                Logger::Instance().Error(
                    "auto settlement failed room_id=" + std::to_string(room_id) +
                    " winner=" + std::to_string(action_result->winner_player_id));
            }
        }

        response.body = BuildKvBody({
            {"code", std::to_string(static_cast<int32_t>(action_code))},
            {"room_id", std::to_string(snapshot.room_id)},
            {"room_state", std::to_string(snapshot.room_state)},
            {"current_operator_player_id", std::to_string(snapshot.current_operator_player_id)},
            {"landlord_player_id", std::to_string(snapshot.landlord_player_id)},
            {"base_coin", std::to_string(snapshot.base_coin)},
            {"snapshot_version", std::to_string(snapshot.snapshot_version)},
            {"last_action_seq", std::to_string(snapshot.last_action_seq)},
            {"game_over", action_result->game_over ? "1" : "0"},
            {"winner_player_id", std::to_string(action_result->winner_player_id)},
            {"record_id", std::to_string(settlement_record_id)}
        });
        return true;
    });

    dispatcher_.Register(MSG_SETTLEMENT_REQ, [this](const Packet& request, Packet& response, int64_t connection_id) {
        response.msg_id = MSG_SETTLEMENT_RESP;
        response.seq_id = request.seq_id;
        response.player_id = request.player_id;
        response.body = BuildKvBody({{"code", std::to_string(static_cast<int32_t>(ErrorCode::INVALID_PACKET))}});
        Logger::Instance().Warn("client settlement request rejected; settlement must be server authoritative");
        return true;
    });
}

void GameServer::TimerLoop() {
    static constexpr int64_t kTrusteeEnterMs = 30000;
    static constexpr int64_t kOfflineLoseMs = 120000;
    while (running_.load()) {
        const int64_t now_ms = NowMs();
        if (tcp_server_ != nullptr) {
            tcp_server_->CloseIdleConnections(config_.server.heartbeat_timeout_ms);
        }
        const auto timeout_events = match_service_.HandleMatchTimeout(now_ms, config_.server.match_timeout_ms);
        if (!timeout_events.empty()) {
            NotifyMatchTimeout(timeout_events);
        }

        for (const auto& session : session_manager_.GetAllSessions()) {
            if (session.room_id <= 0) {
                continue;
            }
            if (session.online) {
                room_manager_.MarkPlayerOnline(session.player_id);
                continue;
            }
            if (session.offline_at_ms <= 0) {
                continue;
            }
            room_manager_.MarkPlayerOffline(session.player_id, session.offline_at_ms);
            const int64_t offline_dur_ms = now_ms - session.offline_at_ms;
            if (offline_dur_ms >= kTrusteeEnterMs) {
                room_manager_.MarkPlayerTrustee(session.player_id, true);
                const auto room = room_manager_.GetRoomById(session.room_id);
                if (room.has_value() && room->current_operator_player_id == session.player_id) {
                    std::string trustee_err;
                    const auto auto_action = room_manager_.ApplyTrusteeAction(session.room_id, session.player_id, &trustee_err);
                    if (auto_action.has_value()) {
                        NotifyRoomStatePush(session.room_id, auto_action->snapshot);
                        if (const auto push_v2 = room_manager_.BuildPushSnapshotV2ByRoomId(session.room_id); push_v2.has_value()) {
                            NotifyRoomStatePushV2(session.room_id, push_v2.value());
                        }
                        NotifyPrivateHandsInRoom(session.room_id, auto_action->snapshot.snapshot_version);
                        if (auto_action->game_over) {
                            const int64_t settle_begin = NowMs();
                            const SettlementResult settle_result = settlement_service_.SettleByServerResult(
                                ServerSettlementDecision{
                                    session.room_id,
                                    auto_action->winner_player_id,
                                    auto_action->base_coin
                                },
                                NowMs());
                            observability_.Record("trustee_auto_settlement", settle_result.code, NowMs() - settle_begin);
                            if (settle_result.code == ErrorCode::OK) {
                                NotifyGameOver(settle_result);
                            }
                        }
                    }
                }
            }

            if (offline_dur_ms >= kOfflineLoseMs) {
                const auto winner = room_manager_.PickWinnerForOfflineLose(session.room_id, session.player_id);
                if (winner.has_value()) {
                    const int64_t settle_begin = NowMs();
                    const SettlementResult settle_result = settlement_service_.SettleByServerResult(
                        ServerSettlementDecision{
                            session.room_id,
                            winner.value(),
                            1
                        },
                        NowMs());
                    observability_.Record("offline_timeout_settlement", settle_result.code, NowMs() - settle_begin);
                    if (settle_result.code == ErrorCode::OK) {
                        NotifyGameOver(settle_result);
                    }
                }
            }
        }

        if (const auto snapshot = observability_.TryBuildMetricsSnapshot(now_ms); snapshot.has_value()) {
            Logger::Instance().Info(snapshot.value());
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void GameServer::OnMessage(int64_t connection_id, const Packet& packet) {
    const int64_t begin_ms = NowMs();
    const std::string trace_id = GenerateTraceId(connection_id, packet.seq_id);
    ObservabilityService::SetCurrentTraceId(trace_id);
    if (observability_.structured_log_enabled()) {
        Logger::Instance().Info(
            "event=request trace_id=" + trace_id +
            " conn_id=" + std::to_string(connection_id) +
            " msg_id=" + std::to_string(packet.msg_id) +
            " seq_id=" + std::to_string(packet.seq_id) +
            " player_id=" + std::to_string(packet.player_id) +
            " body_len=" + std::to_string(packet.body.size()));
    } else {
        Logger::Instance().Info(
            "recv conn_id=" + std::to_string(connection_id) +
            " msg_id=" + std::to_string(packet.msg_id) +
            " seq_id=" + std::to_string(packet.seq_id) +
            " player_id=" + std::to_string(packet.player_id) +
            " body_len=" + std::to_string(packet.body.size()));
    }

    Packet response;
    const bool need_reply = dispatcher_.Dispatch(packet, response, connection_id);
    if (need_reply && tcp_server_ != nullptr) {
        if (observability_.structured_log_enabled()) {
            Logger::Instance().Info(
                "event=response trace_id=" + trace_id +
                " conn_id=" + std::to_string(connection_id) +
                " msg_id=" + std::to_string(response.msg_id) +
                " seq_id=" + std::to_string(response.seq_id) +
                " player_id=" + std::to_string(response.player_id) +
                " body_len=" + std::to_string(response.body.size()));
        } else {
            Logger::Instance().Info(
                "send conn_id=" + std::to_string(connection_id) +
                " msg_id=" + std::to_string(response.msg_id) +
                " seq_id=" + std::to_string(response.seq_id) +
                " player_id=" + std::to_string(response.player_id) +
                " body_len=" + std::to_string(response.body.size()));
        }
        if (!tcp_server_->SendPacket(connection_id, response)) {
            Logger::Instance().Warn("send response failed conn_id=" + std::to_string(connection_id));
        }
        const ErrorCode code = ParseCodeFromResponseBody(response.body);
        observability_.Record(EventNameByMsgId(packet.msg_id), code, NowMs() - begin_ms);
    } else {
        observability_.Record(EventNameByMsgId(packet.msg_id), ErrorCode::UNKNOWN_ERROR, NowMs() - begin_ms);
    }
    ObservabilityService::SetCurrentTraceId("");
}

void GameServer::OnConnectionClosed(int64_t connection_id) {
    Logger::Instance().Info("connection closed conn_id=" + std::to_string(connection_id));
    const auto player_id = session_manager_.GetPlayerIdByConnection(connection_id);
    if (!player_id.has_value()) {
        return;
    }
    const auto session = session_manager_.GetSessionByPlayer(player_id.value());
    if (player_manager_.GetState(player_id.value()) == PlayerState::Matching) {
        match_manager_.Cancel(player_id.value());
    }
    session_manager_.MarkOfflineByConnection(connection_id, NowMs());
    player_manager_.ForceState(player_id.value(), PlayerState::Offline);
    const int64_t offline_at_ms = NowMs();
    room_manager_.MarkPlayerOffline(player_id.value(), offline_at_ms);
    if (redis_store_.enabled()) {
        std::string redis_err;
        const int64_t ttl_ms = config_.auth.token_ttl_seconds * 1000;
        redis_store_.SetOnline(player_id.value(), false, ttl_ms, &redis_err);
        redis_store_.UpsertSession(player_id.value(), session.has_value() ? session->room_id : 0, ttl_ms, &redis_err);
    }
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
        room_manager_.MarkPlayerOnline(player_id);
        if (redis_store_.enabled()) {
            std::string redis_err;
            redis_store_.UpsertSession(player_id, room_id, config_.auth.token_ttl_seconds * 1000, &redis_err);
        }
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
    const std::string players_joined = JoinNumberList(snapshot.players);

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
        {"remain_seconds", std::to_string(snapshot.remain_seconds)},
        {"landlord_player_id", std::to_string(snapshot.landlord_player_id)},
        {"base_coin", std::to_string(snapshot.base_coin)},
        {"snapshot_version", std::to_string(snapshot.snapshot_version)},
        {"last_action_seq", std::to_string(snapshot.last_action_seq)},
        {"players_online_bitmap", snapshot.players_online_bitmap},
        {"trustee_players", snapshot.trustee_players},
        {"nearest_offline_deadline_ms", std::to_string(snapshot.nearest_offline_deadline_ms)}
    });
    tcp_server_->SendPacket(connection_id, notify);
}

void GameServer::NotifyPrivateHandByConnection(
    int64_t player_id, int64_t connection_id, int64_t room_id, int64_t snapshot_version) {
    if (tcp_server_ == nullptr || room_id <= 0) {
        return;
    }
    const auto hand_opt = room_manager_.GetPlayerHand(room_id, player_id);
    if (!hand_opt.has_value()) {
        return;
    }
    Packet notify;
    notify.msg_id = MSG_PRIVATE_HAND_NOTIFY;
    notify.seq_id = 0;
    notify.player_id = player_id;
    notify.body = BuildKvBody({
        {"room_id", std::to_string(room_id)},
        {"player_id", std::to_string(player_id)},
        {"cards", JoinNumberList(hand_opt.value())},
        {"card_count", std::to_string(static_cast<int32_t>(hand_opt->size()))},
        {"snapshot_version", std::to_string(snapshot_version)}
    });
    tcp_server_->SendPacket(connection_id, notify);
}

void GameServer::NotifyPrivateHandsInRoom(int64_t room_id, int64_t snapshot_version) {
    if (tcp_server_ == nullptr || room_id <= 0) {
        return;
    }
    const auto room = room_manager_.GetRoomById(room_id);
    if (!room.has_value() || !room->dealt) {
        return;
    }
    for (const auto player_id : room->players) {
        const auto conn_id = session_manager_.GetConnectionIdByPlayer(player_id);
        if (!conn_id.has_value()) {
            continue;
        }
        NotifyPrivateHandByConnection(player_id, conn_id.value(), room_id, snapshot_version);
    }
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

void GameServer::NotifyRoomStatePush(int64_t room_id, const RoomSnapshot& snapshot) {
    if (tcp_server_ == nullptr) {
        return;
    }
    const auto room = room_manager_.GetRoomById(room_id);
    if (!room.has_value()) {
        return;
    }
    const std::string players_joined = JoinNumberList(snapshot.players);
    for (const auto player_id : room->players) {
        const auto conn_id = session_manager_.GetConnectionIdByPlayer(player_id);
        if (!conn_id.has_value()) {
            continue;
        }
        Packet notify;
        notify.msg_id = MSG_ROOM_STATE_PUSH;
        notify.seq_id = 0;
        notify.player_id = player_id;
        notify.body = BuildKvBody({
            {"room_id", std::to_string(snapshot.room_id)},
            {"room_state", std::to_string(snapshot.room_state)},
            {"players", players_joined},
            {"current_operator_player_id", std::to_string(snapshot.current_operator_player_id)},
            {"landlord_player_id", std::to_string(snapshot.landlord_player_id)},
            {"base_coin", std::to_string(snapshot.base_coin)},
            {"snapshot_version", std::to_string(snapshot.snapshot_version)},
            {"last_action_seq", std::to_string(snapshot.last_action_seq)},
            {"players_online_bitmap", snapshot.players_online_bitmap},
            {"trustee_players", snapshot.trustee_players},
            {"nearest_offline_deadline_ms", std::to_string(snapshot.nearest_offline_deadline_ms)}
        });
        tcp_server_->SendPacket(conn_id.value(), notify);
    }
}

void GameServer::NotifyRoomStatePushV2(int64_t room_id, const RoomPushSnapshotV2& snapshot) {
    if (tcp_server_ == nullptr) {
        return;
    }
    const auto room = room_manager_.GetRoomById(room_id);
    if (!room.has_value()) {
        return;
    }
    const std::string players_joined = JoinNumberList(snapshot.base.players);
    const std::string last_play_cards = JoinNumberList(snapshot.last_play_cards);
    const std::string landlord_bottom_cards = JoinNumberList(snapshot.landlord_bottom_cards);
    const std::string player_card_counts = BuildCardCountList(snapshot.player_card_counts);

    for (const auto player_id : room->players) {
        const auto conn_id = session_manager_.GetConnectionIdByPlayer(player_id);
        if (!conn_id.has_value()) {
            continue;
        }
        Packet notify;
        notify.msg_id = MSG_ROOM_STATE_PUSH_V2;
        notify.seq_id = 0;
        notify.player_id = player_id;
        notify.body = BuildKvBody({
            {"room_id", std::to_string(snapshot.base.room_id)},
            {"room_state", std::to_string(snapshot.base.room_state)},
            {"players", players_joined},
            {"current_operator_player_id", std::to_string(snapshot.base.current_operator_player_id)},
            {"landlord_player_id", std::to_string(snapshot.base.landlord_player_id)},
            {"base_coin", std::to_string(snapshot.base.base_coin)},
            {"snapshot_version", std::to_string(snapshot.base.snapshot_version)},
            {"last_action_seq", std::to_string(snapshot.base.last_action_seq)},
            {"players_online_bitmap", snapshot.base.players_online_bitmap},
            {"trustee_players", snapshot.base.trustee_players},
            {"nearest_offline_deadline_ms", std::to_string(snapshot.base.nearest_offline_deadline_ms)},
            {"last_play_player_id", std::to_string(snapshot.last_play_player_id)},
            {"last_play_cards", last_play_cards},
            {"player_card_counts", player_card_counts},
            {"landlord_bottom_cards", landlord_bottom_cards},
            {"action_deadline_ms", std::to_string(snapshot.action_deadline_ms)}
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
        Packet notify_v2 = notify;
        notify_v2.msg_id = MSG_GAME_RESULT_PUSH;
        tcp_server_->SendPacket(conn_id.value(), notify_v2);
        if (redis_store_.enabled()) {
            std::string redis_err;
            redis_store_.UpsertSession(p.player_id, 0, config_.auth.token_ttl_seconds * 1000, &redis_err);
        }
    }
}

}  // namespace ddz
