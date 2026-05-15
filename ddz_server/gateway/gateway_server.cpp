
#include "gateway/gateway_server.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include "common/util/kv_codec.h"
#include "common/util/time_util.h"
#include "gateway/json_codec.h"
#include "gateway/upstream_tcp_client.h"
#include "log/logger.h"

namespace ddz {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

constexpr uint32_t kMsgSessionRefreshReq = 6103;
constexpr uint32_t kMsgSessionRefreshResp = 6104;

std::atomic<uint64_t> g_gateway_trace_seq{1};
std::atomic<uint32_t> g_upstream_http_seq{100000};

struct RouteInfo {
    std::string path;
    std::unordered_map<std::string, std::string> query;
};

struct UpstreamSyncResult {
    bool ok = false;
    Packet response;
    int64_t latency_ms = 0;
    std::string err;
};

std::string NormalizeOrigin(std::string origin) {
    while (!origin.empty() && (origin.back() == ' ' || origin.back() == '\t')) {
        origin.pop_back();
    }
    if (!origin.empty() && origin.back() == '/') {
        origin.pop_back();
    }
    return origin;
}

std::string BuildGatewayTraceId() {
    return "gw-" + std::to_string(NowMs()) + "-" + std::to_string(g_gateway_trace_seq.fetch_add(1));
}

bool ParseI64Strict(const std::string& text, int64_t* out) {
    try {
        size_t idx = 0;
        const int64_t value = std::stoll(text, &idx);
        if (idx != text.size()) {
            return false;
        }
        *out = value;
        return true;
    } catch (...) {
        return false;
    }
}

std::string BuildGatewayErrorEnvelope(
    int32_t code,
    const std::string& reason,
    const std::string& message,
    const std::string& h5_request_id,
    const std::string& gateway_trace_id,
    const std::string& server_trace_id) {
    GatewayEnvelope envelope;
    envelope.msg_id = 0;
    envelope.seq_id = 0;
    envelope.player_id = 0;
    envelope.h5_request_id = h5_request_id;
    envelope.gateway_trace_id = gateway_trace_id;
    envelope.server_trace_id = server_trace_id;
    envelope.body = BuildKvBody({
        {"code", std::to_string(code)},
        {"reason", reason},
        {"message", message}
    });
    return BuildEnvelopeJson(envelope);
}

std::string BuildGatewayHttpErrorJson(
    int32_t code,
    const std::string& reason,
    const std::string& message,
    const std::string& gateway_trace_id) {
    std::string out =
        "{\"code\":" + std::to_string(code) +
        ",\"reason\":\"" + reason +
        "\",\"message\":\"" + message +
        "\"";
    if (!gateway_trace_id.empty()) {
        out += ",\"gateway_trace_id\":\"" + gateway_trace_id + "\"";
    }
    out += "}";
    return out;
}

http::response<http::string_body> MakeJsonResponse(
    http::status status,
    const std::string& body,
    int version,
    bool keep_alive,
    const std::string& allow_origin) {
    http::response<http::string_body> response{status, version};
    response.set(http::field::content_type, "application/json; charset=utf-8");
    if (!allow_origin.empty()) {
        response.set(http::field::access_control_allow_origin, allow_origin);
        response.set(http::field::vary, "Origin");
        response.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
        response.set(http::field::access_control_allow_headers, "Authorization, Content-Type, X-Dev-Key");
    }
    response.keep_alive(keep_alive);
    if (status == http::status::no_content || status == http::status::not_modified ||
        (status >= http::status::continue_ && status < http::status::ok)) {
        response.body().clear();
        response.content_length(0);
    } else {
        response.body() = body;
        response.prepare_payload();
    }
    return response;
}

RouteInfo ParseRouteInfo(const std::string& target) {
    RouteInfo info;
    const auto q = target.find('?');
    if (q == std::string::npos) {
        info.path = target;
        return info;
    }
    info.path = target.substr(0, q);
    const std::string query = target.substr(q + 1);
    size_t begin = 0;
    while (begin < query.size()) {
        const size_t amp = query.find('&', begin);
        const size_t end = (amp == std::string::npos) ? query.size() : amp;
        const std::string item = query.substr(begin, end - begin);
        const size_t eq = item.find('=');
        if (eq != std::string::npos) {
            info.query[item.substr(0, eq)] = item.substr(eq + 1);
        }
        begin = end + 1;
    }
    return info;
}

std::string ExtractBearerToken(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
        return "";
    }
    const std::string value(it->value());
    const std::string prefix = "Bearer ";
    if (value.size() <= prefix.size() || value.substr(0, prefix.size()) != prefix) {
        return "";
    }
    return value.substr(prefix.size());
}

std::string ClientIpFromXForwardedFor(const std::string& xff) {
    const auto comma = xff.find(',');
    std::string ip = (comma == std::string::npos) ? xff : xff.substr(0, comma);
    while (!ip.empty() && (ip.front() == ' ' || ip.front() == '\t')) {
        ip.erase(ip.begin());
    }
    while (!ip.empty() && (ip.back() == ' ' || ip.back() == '\t')) {
        ip.pop_back();
    }
    return ip;
}

bool IsOriginAllowed(const GatewayConfig& config, const std::string& origin) {
    if (origin.empty()) {
        return false;
    }
    const std::string normalized = NormalizeOrigin(origin);
    for (const auto& allowed : config.security.allowed_origins) {
        if (normalized == allowed) {
            return true;
        }
    }
    return false;
}

void LogRejectAudit(
    const std::string& reason,
    const std::string& client_ip,
    const std::string& origin,
    const std::string& path,
    const std::string& user_agent,
    const std::string& trace_id) {
    Logger::Instance().Warn(
        "event=gateway_reject reason=" + reason +
        " client_ip=" + client_ip +
        " origin=" + (origin.empty() ? "-" : origin) +
        " path=" + path +
        " user_agent=" + (user_agent.empty() ? "-" : user_agent) +
        " trace_id=" + (trace_id.empty() ? "-" : trace_id));
}

bool IsIpBanned(
    std::mutex& security_mu,
    std::unordered_map<std::string, int64_t>& banned_until_ms_by_ip,
    const std::string& client_ip,
    int64_t now_ms) {
    std::lock_guard<std::mutex> lock(security_mu);
    auto it = banned_until_ms_by_ip.find(client_ip);
    if (it == banned_until_ms_by_ip.end()) {
        return false;
    }
    if (it->second <= now_ms) {
        banned_until_ms_by_ip.erase(it);
        return false;
    }
    return true;
}

void BanIp(
    std::mutex& security_mu,
    std::unordered_map<std::string, int64_t>& banned_until_ms_by_ip,
    const GatewayConfig& config,
    const std::string& client_ip,
    int64_t now_ms) {
    std::lock_guard<std::mutex> lock(security_mu);
    banned_until_ms_by_ip[client_ip] = now_ms + static_cast<int64_t>(config.security.ban_seconds_on_abuse) * 1000;
}

bool CheckAndConsumeHttpQps(
    std::mutex& security_mu,
    std::unordered_map<std::string, std::deque<int64_t>>& http_rate_by_ip,
    const GatewayConfig& config,
    const std::string& client_ip,
    int64_t now_ms) {
    std::lock_guard<std::mutex> lock(security_mu);
    auto& window = http_rate_by_ip[client_ip];
    while (!window.empty() && now_ms - window.front() >= 1000) {
        window.pop_front();
    }
    if (static_cast<int32_t>(window.size()) >= config.security.max_http_qps_per_ip) {
        return false;
    }
    window.push_back(now_ms);
    return true;
}

bool TryAcquireWsSlot(
    std::mutex& security_mu,
    std::unordered_map<std::string, int32_t>& ws_conn_by_ip,
    const GatewayConfig& config,
    const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(security_mu);
    int32_t& count = ws_conn_by_ip[client_ip];
    if (count >= config.security.max_ws_connections_per_ip) {
        return false;
    }
    count += 1;
    return true;
}

void ReleaseWsSlot(
    std::mutex& security_mu,
    std::unordered_map<std::string, int32_t>& ws_conn_by_ip,
    const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(security_mu);
    auto it = ws_conn_by_ip.find(client_ip);
    if (it == ws_conn_by_ip.end()) {
        return;
    }
    it->second -= 1;
    if (it->second <= 0) {
        ws_conn_by_ip.erase(it);
    }
}

UpstreamSyncResult QueryUpstreamPacket(const GatewayConfig& config, const Packet& req, int64_t timeout_ms) {
    UpstreamSyncResult result;
    UpstreamTcpClient upstream;
    std::mutex mu;
    std::condition_variable cv;
    std::deque<Packet> queue;

    std::string connect_err;
    if (!upstream.Connect(
            config.upstream_host,
            config.upstream_port,
            config.max_packet_size,
            [&](const Packet& packet) {
                std::lock_guard<std::mutex> lock(mu);
                queue.push_back(packet);
                cv.notify_all();
            },
            &connect_err)) {
        result.err = connect_err;
        return result;
    }

    std::string send_err;
    const int64_t begin_ms = NowMs();
    if (!upstream.SendPacket(req, &send_err)) {
        upstream.Close();
        result.err = send_err;
        return result;
    }

    const int64_t deadline_ms = begin_ms + timeout_ms;
    bool matched = false;
    {
        std::unique_lock<std::mutex> lock(mu);
        while (NowMs() < deadline_ms) {
            for (auto it = queue.begin(); it != queue.end(); ++it) {
                if (it->seq_id == req.seq_id) {
                    result.ok = true;
                    result.response = *it;
                    result.latency_ms = NowMs() - begin_ms;
                    matched = true;
                    break;
                }
            }
            if (matched) {
                break;
            }
            const auto now_ms = NowMs();
            const auto wait_ms = std::max<int64_t>(1, deadline_ms - now_ms);
            cv.wait_for(lock, std::chrono::milliseconds(wait_ms));
        }
    }

    upstream.Close();
    if (!matched) {
        result.err = "upstream timeout";
    }
    return result;
}

http::response<http::string_body> HandleLoginTicketRequest(
    const http::request<http::string_body>& req,
    const GatewayConfig& config,
    AuthTokenService* auth_token_service,
    const std::string& allow_origin,
    const std::string& gateway_trace_id,
    GatewayObservabilityService* observability) {
    auto it = req.find("X-Dev-Key");
    if (it == req.end() || it->value() != config.dev_key) {
        observability->RecordHttpRequest("login-ticket", false);
        return MakeJsonResponse(
            http::status::unauthorized,
            BuildGatewayHttpErrorJson(401, "invalid_dev_key", "invalid X-Dev-Key", gateway_trace_id),
            req.version(),
            req.keep_alive(),
            allow_origin);
    }
    int64_t player_id = 0;
    std::string nickname;
    std::string parse_err;
    if (!ParseLoginTicketRequestJson(req.body(), &player_id, &nickname, &parse_err)) {
        observability->RecordHttpRequest("login-ticket", false);
        return MakeJsonResponse(
            http::status::bad_request,
            BuildGatewayHttpErrorJson(400, "invalid_request", parse_err, gateway_trace_id),
            req.version(),
            req.keep_alive(),
            allow_origin);
    }
    const auto issue = auth_token_service->IssueLoginTicket(player_id, NowMs());
    if (issue.token.empty()) {
        observability->RecordHttpRequest("login-ticket", false);
        return MakeJsonResponse(
            http::status::internal_server_error,
            BuildGatewayHttpErrorJson(500, "issue_ticket_failed", "issue login ticket failed", gateway_trace_id),
            req.version(),
            req.keep_alive(),
            allow_origin);
    }
    observability->RecordHttpRequest("login-ticket", true);
    return MakeJsonResponse(
        http::status::ok,
        BuildLoginTicketResponseJson(0, player_id, issue.token, issue.expire_at_ms, "ok"),
        req.version(),
        req.keep_alive(),
        allow_origin);
}

http::response<http::string_body> HandleSessionRefreshRequest(
    const http::request<http::string_body>& req,
    const GatewayConfig& config,
    const std::string& allow_origin,
    const std::string& gateway_trace_id,
    GatewayObservabilityService* observability) {
    const RouteInfo route = ParseRouteInfo(std::string(req.target()));
    auto it_player = route.query.find("player_id");
    auto it_room = route.query.find("room_id");
    const std::string bearer_token = ExtractBearerToken(req);

    int64_t player_id = 0;
    int64_t room_id = 0;
    if (it_player == route.query.end() || it_room == route.query.end() || bearer_token.empty() ||
        !ParseI64Strict(it_player->second, &player_id) ||
        !ParseI64Strict(it_room->second, &room_id) ||
        player_id <= 0 || room_id < 0) {
        observability->RecordHttpRequest("session-refresh", false);
        observability->RecordRefreshResult(false);
        return MakeJsonResponse(
            http::status::bad_request,
            BuildGatewayHttpErrorJson(400, "invalid_request", "invalid auth or query", gateway_trace_id),
            req.version(),
            req.keep_alive(),
            allow_origin);
    }

    Packet upstream_req;
    upstream_req.msg_id = kMsgSessionRefreshReq;
    upstream_req.seq_id = g_upstream_http_seq.fetch_add(1);
    upstream_req.player_id = player_id;
    upstream_req.body = BuildKvBody({
        {"player_id", std::to_string(player_id)},
        {"token", bearer_token},
        {"room_id", std::to_string(room_id)},
        {"gateway_trace_id", gateway_trace_id}
    });

    UpstreamSyncResult upstream_result = QueryUpstreamPacket(config, upstream_req, 3000);
    if (!upstream_result.ok) {
        observability->RecordHttpRequest("session-refresh", false);
        observability->RecordRefreshResult(false);
        return MakeJsonResponse(
            http::status::bad_gateway,
            BuildGatewayHttpErrorJson(502, "upstream_error", upstream_result.err, gateway_trace_id),
            req.version(),
            req.keep_alive(),
            allow_origin);
    }

    observability->RecordUpstreamRttMs(upstream_result.latency_ms);
    const auto kv = ParseKvBody(upstream_result.response.body);

    int32_t code = 1;
    int64_t resp_player_id = 0;
    int64_t resp_room_id = 0;
    int64_t expire_at_ms = 0;
    std::string token;
    std::string reason = "unknown_error";
    std::string server_trace_id;
    std::string gateway_trace_rsp = gateway_trace_id;

    auto it_code = kv.find("code");
    if (it_code != kv.end()) {
        try {
            code = std::stoi(it_code->second);
        } catch (...) {
            code = 1;
        }
    }
    auto it_pid = kv.find("player_id");
    if (it_pid != kv.end()) {
        ParseI64Strict(it_pid->second, &resp_player_id);
    }
    auto it_rid = kv.find("room_id");
    if (it_rid != kv.end()) {
        ParseI64Strict(it_rid->second, &resp_room_id);
    }
    auto it_token = kv.find("token");
    if (it_token != kv.end()) {
        token = it_token->second;
    }
    auto it_expire = kv.find("expire_at_ms");
    if (it_expire != kv.end()) {
        ParseI64Strict(it_expire->second, &expire_at_ms);
    }
    auto it_reason = kv.find("reason");
    if (it_reason != kv.end() && !it_reason->second.empty()) {
        reason = it_reason->second;
    }
    auto it_server_trace = kv.find("server_trace_id");
    if (it_server_trace != kv.end()) {
        server_trace_id = it_server_trace->second;
    }
    auto it_gateway_trace = kv.find("gateway_trace_id");
    if (it_gateway_trace != kv.end() && !it_gateway_trace->second.empty()) {
        gateway_trace_rsp = it_gateway_trace->second;
    }

    const bool success = (upstream_result.response.msg_id == kMsgSessionRefreshResp && code == 0);
    observability->RecordHttpRequest("session-refresh", success);
    observability->RecordRefreshResult(success);

    return MakeJsonResponse(
        http::status::ok,
        BuildSessionRefreshResponseJson(
            code,
            resp_player_id,
            resp_room_id,
            token,
            expire_at_ms,
            reason,
            gateway_trace_rsp,
            server_trace_id),
        req.version(),
        req.keep_alive(),
        allow_origin);
}
void RunWebSocketProxy(
    tcp::socket socket,
    const GatewayConfig& config,
    const http::request<http::string_body>& request,
    const std::string& client_ip,
    std::mutex& security_mu,
    std::unordered_map<std::string, int64_t>& banned_until_ms_by_ip,
    GatewayObservabilityService* observability) {
    std::string stage = "init";
    try {
    beast::error_code ec;
    stage = "create_ws_stream";
    websocket::stream<tcp::socket> ws(std::move(socket));
    stage = "accept_upgrade";
    ws.accept(request, ec);
    if (ec) {
        return;
    }
    stage = "set_ws_options";
    websocket::stream_base::timeout timeout_opt =
        websocket::stream_base::timeout::suggested(beast::role_type::server);
    timeout_opt.handshake_timeout = websocket::stream_base::none();
    timeout_opt.idle_timeout = websocket::stream_base::none();
    timeout_opt.keep_alive_pings = false;
    ws.set_option(timeout_opt);
    ws.text(true);

    observability->RecordWsConnectionOpened();

    std::mutex out_mu;
    std::deque<std::string> outbound;
    auto push_outbound = [&](std::string msg) {
        std::lock_guard<std::mutex> lock(out_mu);
        outbound.push_back(std::move(msg));
    };

    push_outbound(
        "{\"type\":\"gateway_welcome\",\"heartbeat_interval_ms\":" +
        std::to_string(config.heartbeat_timeout_ms / 3) +
        ",\"reconnect_backoff_ms\":" +
        std::to_string(config.reconnect_backoff_ms) +
        "}");

    std::mutex pending_mu;
    struct PendingRequestTrace {
        int64_t send_ms = 0;
        std::string h5_request_id;
        std::string gateway_trace_id;
    };
    std::unordered_map<uint32_t, PendingRequestTrace> pending_by_seq;

    UpstreamTcpClient upstream;
    std::string connect_err;
    if (!upstream.Connect(
            config.upstream_host,
            config.upstream_port,
            config.max_packet_size,
            [&](const Packet& packet) {
                GatewayEnvelope envelope;
                envelope.msg_id = packet.msg_id;
                envelope.seq_id = packet.seq_id;
                envelope.player_id = packet.player_id;
                envelope.body = packet.body;

                const auto kv = ParseKvBody(packet.body);
                auto it_h5 = kv.find("h5_request_id");
                if (it_h5 != kv.end()) {
                    envelope.h5_request_id = it_h5->second;
                }
                auto it_gateway = kv.find("gateway_trace_id");
                if (it_gateway != kv.end()) {
                    envelope.gateway_trace_id = it_gateway->second;
                }
                auto it_server = kv.find("server_trace_id");
                if (it_server != kv.end()) {
                    envelope.server_trace_id = it_server->second;
                }

                {
                    std::lock_guard<std::mutex> lock(pending_mu);
                    auto it = pending_by_seq.find(packet.seq_id);
                    if (it != pending_by_seq.end()) {
                        const int64_t rtt_ms = NowMs() - it->second.send_ms;
                        observability->RecordUpstreamRttMs(rtt_ms);
                        if (envelope.h5_request_id.empty()) {
                            envelope.h5_request_id = it->second.h5_request_id;
                        }
                        if (envelope.gateway_trace_id.empty()) {
                            envelope.gateway_trace_id = it->second.gateway_trace_id;
                        }
                        pending_by_seq.erase(it);
                    }
                }

                observability->RecordWsOut();
                push_outbound(BuildEnvelopeJson(envelope));
            },
            &connect_err)) {
        push_outbound(BuildGatewayErrorEnvelope(1, "upstream_connect_failed", connect_err, "", "", ""));
    }

    bool upstream_disconnect_notified = false;
    std::deque<int64_t> inbound_window_ms;
    while (true) {
        stage = "flush_outbound";
        for (;;) {
            std::string next_msg;
            {
                std::lock_guard<std::mutex> lock(out_mu);
                if (outbound.empty()) {
                    break;
                }
                next_msg = std::move(outbound.front());
                outbound.pop_front();
            }
            ws.write(asio::buffer(next_msg), ec);
            if (ec) {
                Logger::Instance().Warn("ws write failed client_ip=" + client_ip + " err=" + ec.message());
                upstream.Close();
                observability->RecordWsConnectionClosed();
                return;
            }
        }

        stage = "poll_ws";
        beast::error_code avail_ec;
        const size_t available_bytes = ws.next_layer().available(avail_ec);
        if (avail_ec) {
            Logger::Instance().Warn("ws poll failed client_ip=" + client_ip + " err=" + avail_ec.message());
            break;
        }
        if (available_bytes == 0) {
            if (!upstream.running()) {
                if (!upstream_disconnect_notified) {
                    push_outbound(BuildGatewayErrorEnvelope(1, "upstream_disconnected", "upstream disconnected", "", "", ""));
                    upstream_disconnect_notified = true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        stage = "read_ws";
        beast::flat_buffer buffer;
        ws.read(buffer, ec);
        if (ec == websocket::error::closed) {
            Logger::Instance().Info("ws closed by peer client_ip=" + client_ip);
            break;
        }
        if (ec) {
            Logger::Instance().Warn("ws read failed client_ip=" + client_ip + " err=" + ec.message());
            break;
        }

        if (IsIpBanned(security_mu, banned_until_ms_by_ip, client_ip, NowMs())) {
            push_outbound(BuildGatewayErrorEnvelope(429, "ip_banned", "too many requests", "", "", ""));
            break;
        }

        const int64_t now_ms = NowMs();
        while (!inbound_window_ms.empty() && now_ms - inbound_window_ms.front() >= 1000) {
            inbound_window_ms.pop_front();
        }
        if (static_cast<int32_t>(inbound_window_ms.size()) >= config.security.max_ws_messages_per_sec) {
            observability->RecordRateLimited();
            BanIp(security_mu, banned_until_ms_by_ip, config, client_ip, now_ms);
            push_outbound(BuildGatewayErrorEnvelope(429, "ws_rate_limited", "message rate exceeded", "", "", ""));
            break;
        }
        inbound_window_ms.push_back(now_ms);

        stage = "parse_ws_payload";
        const std::string payload = beast::buffers_to_string(buffer.data());
        GatewayEnvelope envelope;
        std::string parse_err;
        if (!ParseEnvelopeJson(payload, &envelope, &parse_err)) {
            push_outbound(BuildGatewayErrorEnvelope(400, "invalid_request", parse_err, "", "", ""));
            continue;
        }
        if (!upstream.running()) {
            push_outbound(BuildGatewayErrorEnvelope(503, "upstream_not_ready", "upstream not ready", envelope.h5_request_id, "", ""));
            continue;
        }

        const std::string gateway_trace_id = BuildGatewayTraceId();
        std::string upstream_body = envelope.body;
        if (!envelope.h5_request_id.empty()) {
            if (!upstream_body.empty()) {
                upstream_body.push_back(';');
            }
            upstream_body += "h5_request_id=" + envelope.h5_request_id;
        }
        if (!upstream_body.empty()) {
            upstream_body.push_back(';');
        }
        upstream_body += "gateway_trace_id=" + gateway_trace_id;

        Packet packet;
        packet.msg_id = envelope.msg_id;
        packet.seq_id = envelope.seq_id;
        packet.player_id = envelope.player_id;
        packet.body = upstream_body;

        stage = "send_upstream";
        std::string send_err;
        if (!upstream.SendPacket(packet, &send_err)) {
            push_outbound(BuildGatewayErrorEnvelope(
                502,
                "upstream_send_failed",
                send_err,
                envelope.h5_request_id,
                gateway_trace_id,
                ""));
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(pending_mu);
            pending_by_seq[packet.seq_id] = PendingRequestTrace{now_ms, envelope.h5_request_id, gateway_trace_id};
        }
        observability->RecordWsIn();
    }

    upstream.Close();
    ws.close(websocket::close_code::normal, ec);
    observability->RecordWsConnectionClosed();
    } catch (const std::exception& ex) {
        Logger::Instance().Error("RunWebSocketProxy exception stage=" + stage + " err=" + ex.what());
        observability->RecordWsConnectionClosed();
        throw;
    } catch (...) {
        Logger::Instance().Error("RunWebSocketProxy unknown exception stage=" + stage);
        observability->RecordWsConnectionClosed();
        throw;
    }
}

void HandleGatewayConnection(
    tcp::socket socket,
    const GatewayConfig& config,
    AuthTokenService* auth_token_service,
    GatewayObservabilityService* observability,
    std::mutex& security_mu,
    std::unordered_map<std::string, int32_t>& ws_conn_by_ip,
    std::unordered_map<std::string, std::deque<int64_t>>& http_rate_by_ip,
    std::unordered_map<std::string, int64_t>& banned_until_ms_by_ip) {
    beast::error_code ec;
    socket.non_blocking(false, ec);
    if (ec) {
        return;
    }

    std::string client_ip = "unknown";
    try {
        client_ip = socket.remote_endpoint().address().to_string();
    } catch (...) {
    }

    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    http::read(socket, buffer, request, ec);
    if (ec) {
        return;
    }

    const std::string user_agent(request[http::field::user_agent]);
    const std::string origin(request[http::field::origin]);
    const RouteInfo route = ParseRouteInfo(std::string(request.target()));

    if (config.security.trust_x_forwarded_for) {
        const std::string xff(request["X-Forwarded-For"]);
        const std::string xff_ip = ClientIpFromXForwardedFor(xff);
        if (!xff_ip.empty()) {
            client_ip = xff_ip;
        }
    }

    const std::string gateway_trace_id = BuildGatewayTraceId();

    const bool has_origin = !origin.empty();
    const bool origin_allowed = has_origin ? IsOriginAllowed(config, origin) : false;
    const std::string allow_origin = origin_allowed ? NormalizeOrigin(origin) : "";

    if (has_origin && !origin_allowed) {
        LogRejectAudit("origin_not_allowed", client_ip, origin, route.path, user_agent, gateway_trace_id);
        auto response = MakeJsonResponse(
            http::status::forbidden,
            BuildGatewayHttpErrorJson(403, "origin_not_allowed", "origin rejected", gateway_trace_id),
            request.version(),
            request.keep_alive(),
            "");
        http::write(socket, response, ec);
        socket.shutdown(tcp::socket::shutdown_both, ec);
        return;
    }

    if (IsIpBanned(security_mu, banned_until_ms_by_ip, client_ip, NowMs())) {
        observability->RecordRateLimited();
        LogRejectAudit("ip_banned", client_ip, origin, route.path, user_agent, gateway_trace_id);
        auto response = MakeJsonResponse(
            http::status::too_many_requests,
            BuildGatewayHttpErrorJson(429, "ip_banned", "too many requests", gateway_trace_id),
            request.version(),
            request.keep_alive(),
            allow_origin);
        http::write(socket, response, ec);
        socket.shutdown(tcp::socket::shutdown_both, ec);
        return;
    }

    if (request.method() == http::verb::options) {
        auto response = MakeJsonResponse(
            http::status::no_content,
            "{}",
            request.version(),
            request.keep_alive(),
            allow_origin);
        http::write(socket, response, ec);
        socket.shutdown(tcp::socket::shutdown_both, ec);
        return;
    }

    if (websocket::is_upgrade(request) && route.path == "/ws/game") {
        if (!has_origin || !origin_allowed) {
            LogRejectAudit("ws_origin_invalid", client_ip, origin, route.path, user_agent, gateway_trace_id);
            auto response = MakeJsonResponse(
                http::status::forbidden,
                BuildGatewayHttpErrorJson(403, "origin_not_allowed", "origin required", gateway_trace_id),
                request.version(),
                request.keep_alive(),
                "");
            http::write(socket, response, ec);
            socket.shutdown(tcp::socket::shutdown_both, ec);
            return;
        }

        if (!TryAcquireWsSlot(security_mu, ws_conn_by_ip, config, client_ip)) {
            observability->RecordRateLimited();
            BanIp(security_mu, banned_until_ms_by_ip, config, client_ip, NowMs());
            LogRejectAudit("ws_conn_limit", client_ip, origin, route.path, user_agent, gateway_trace_id);
            auto response = MakeJsonResponse(
                http::status::too_many_requests,
                BuildGatewayHttpErrorJson(429, "ws_conn_limited", "too many websocket connections", gateway_trace_id),
                request.version(),
                request.keep_alive(),
                allow_origin);
            http::write(socket, response, ec);
            socket.shutdown(tcp::socket::shutdown_both, ec);
            return;
        }

        RunWebSocketProxy(
            std::move(socket),
            config,
            request,
            client_ip,
            security_mu,
            banned_until_ms_by_ip,
            observability);
        ReleaseWsSlot(security_mu, ws_conn_by_ip, client_ip);
        return;
    }

    if (!CheckAndConsumeHttpQps(security_mu, http_rate_by_ip, config, client_ip, NowMs())) {
        observability->RecordRateLimited();
        BanIp(security_mu, banned_until_ms_by_ip, config, client_ip, NowMs());
        LogRejectAudit("http_qps_limited", client_ip, origin, route.path, user_agent, gateway_trace_id);
        auto response = MakeJsonResponse(
            http::status::too_many_requests,
            BuildGatewayHttpErrorJson(429, "http_qps_limited", "qps exceeded", gateway_trace_id),
            request.version(),
            request.keep_alive(),
            allow_origin);
        http::write(socket, response, ec);
        socket.shutdown(tcp::socket::shutdown_both, ec);
        return;
    }

    if (request.method() == http::verb::post && route.path == "/api/auth/login-ticket") {
        auto response = HandleLoginTicketRequest(request, config, auth_token_service, allow_origin, gateway_trace_id, observability);
        http::write(socket, response, ec);
    } else if (request.method() == http::verb::get && route.path == "/api/session/refresh") {
        auto response = HandleSessionRefreshRequest(request, config, allow_origin, gateway_trace_id, observability);
        http::write(socket, response, ec);
    } else {
        observability->RecordHttpRequest("unknown", false);
        auto response = MakeJsonResponse(
            http::status::not_found,
            BuildGatewayHttpErrorJson(404, "not_found", "not found", gateway_trace_id),
            request.version(),
            request.keep_alive(),
            allow_origin);
        http::write(socket, response, ec);
    }
    socket.shutdown(tcp::socket::shutdown_both, ec);
}

}  // namespace

GatewayServer::~GatewayServer() {
    Stop();
}

bool GatewayServer::Start(const std::string& config_path, std::string* err) {
    if (running_.exchange(true)) {
        return true;
    }

    std::string config_err;
    if (!GatewayConfigLoader::LoadFromFile(config_path, config_, &config_err)) {
        if (err != nullptr) *err = config_err;
        running_.store(false);
        return false;
    }
    auth_token_service_.Configure(config_.token_secret, config_.token_ttl_seconds);
    observability_.Configure(
        config_.observability.enable_structured_log,
        config_.observability.metrics_report_interval_ms);

    std::string log_err;
    if (!Logger::Instance().Init(config_.log_level, config_.log_dir, &log_err)) {
        if (err != nullptr) *err = log_err;
        running_.store(false);
        return false;
    }

    accept_thread_ = std::thread([this]() {
        try {
            AcceptLoop();
        } catch (const std::exception& ex) {
            Logger::Instance().Error(std::string("gateway accept loop exception: ") + ex.what());
            running_.store(false);
        } catch (...) {
            Logger::Instance().Error("gateway accept loop unknown exception");
            running_.store(false);
        }
    });
    metrics_thread_ = std::thread([this]() {
        try {
            MetricsLoop();
        } catch (const std::exception& ex) {
            Logger::Instance().Error(std::string("gateway metrics loop exception: ") + ex.what());
            running_.store(false);
        } catch (...) {
            Logger::Instance().Error("gateway metrics loop unknown exception");
            running_.store(false);
        }
    });
    Logger::Instance().Info(
        "gateway started listen=" + config_.listen_host + ":" + std::to_string(config_.listen_port) +
        " upstream=" + config_.upstream_host + ":" + std::to_string(config_.upstream_port));
    return true;
}

void GatewayServer::Stop() {
    const bool was_running = running_.exchange(false);
    if (!was_running) {
        return;
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }
    std::vector<std::thread> sessions;
    {
        std::lock_guard<std::mutex> lock(session_mu_);
        sessions.swap(session_threads_);
    }
    for (auto& thread : sessions) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    Logger::Instance().Info("gateway stopped");
    Logger::Instance().Shutdown();
}

void GatewayServer::AcceptLoop() {
    asio::io_context io_context;
    beast::error_code ec;
    tcp::acceptor acceptor(io_context);
    tcp::endpoint endpoint(asio::ip::make_address(config_.listen_host, ec), config_.listen_port);
    if (ec) {
        Logger::Instance().Error("invalid listen_host: " + config_.listen_host + " err=" + ec.message());
        return;
    }
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        Logger::Instance().Error("acceptor open failed: " + ec.message());
        return;
    }
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    acceptor.bind(endpoint, ec);
    if (ec) {
        Logger::Instance().Error("acceptor bind failed: " + ec.message());
        return;
    }
    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        Logger::Instance().Error("acceptor listen failed: " + ec.message());
        return;
    }
    acceptor.non_blocking(true, ec);
    if (ec) {
        Logger::Instance().Error("acceptor non_blocking failed: " + ec.message());
        return;
    }

    while (running_.load()) {
        tcp::socket socket(io_context);
        acceptor.accept(socket, ec);
        if (ec == asio::error::would_block || ec == asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (ec) {
            Logger::Instance().Warn("accept failed: " + ec.message());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::lock_guard<std::mutex> lock(session_mu_);
        session_threads_.emplace_back([this, s = std::move(socket)]() mutable {
            try {
                HandleGatewayConnection(
                    std::move(s),
                    config_,
                    &auth_token_service_,
                    &observability_,
                    security_mu_,
                    ws_conn_by_ip_,
                    http_rate_by_ip_,
                    banned_until_ms_by_ip_);
            } catch (const std::exception& ex) {
                Logger::Instance().Error(std::string("gateway session exception: ") + ex.what());
            } catch (...) {
                Logger::Instance().Error("gateway session unknown exception");
            }
        });
    }
}

void GatewayServer::MetricsLoop() {
    while (running_.load()) {
        if (const auto snapshot = observability_.TryBuildMetricsSnapshot(NowMs()); snapshot.has_value()) {
            Logger::Instance().Info(snapshot.value());
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

}  // namespace ddz
