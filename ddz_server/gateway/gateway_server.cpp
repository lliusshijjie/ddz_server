#include "gateway/gateway_server.h"

#include <chrono>
#include <deque>
#include <memory>
#include <utility>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

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

std::string BuildGatewayErrorEnvelope(const std::string& message) {
    GatewayEnvelope envelope;
    envelope.msg_id = 0;
    envelope.seq_id = 0;
    envelope.player_id = 0;
    envelope.body = "code=1;message=" + message;
    return BuildEnvelopeJson(envelope);
}

http::response<http::string_body> MakeJsonResponse(
    http::status status, const std::string& body, int version, bool keep_alive) {
    http::response<http::string_body> response{status, version};
    response.set(http::field::content_type, "application/json; charset=utf-8");
    response.keep_alive(keep_alive);
    response.body() = body;
    response.prepare_payload();
    return response;
}

http::response<http::string_body> HandleLoginTicketRequest(
    const http::request<http::string_body>& req, const GatewayConfig& config, AuthTokenService* auth_token_service) {
    auto it = req.find("X-Dev-Key");
    if (it == req.end() || it->value() != config.dev_key) {
        return MakeJsonResponse(
            http::status::unauthorized,
            BuildLoginTicketResponseJson(401, 0, "", 0, "invalid X-Dev-Key"),
            req.version(),
            req.keep_alive());
    }
    int64_t player_id = 0;
    std::string nickname;
    std::string parse_err;
    if (!ParseLoginTicketRequestJson(req.body(), &player_id, &nickname, &parse_err)) {
        return MakeJsonResponse(
            http::status::bad_request,
            BuildLoginTicketResponseJson(400, 0, "", 0, parse_err),
            req.version(),
            req.keep_alive());
    }
    const auto issue = auth_token_service->IssueLoginTicket(player_id, NowMs());
    if (issue.token.empty()) {
        return MakeJsonResponse(
            http::status::internal_server_error,
            BuildLoginTicketResponseJson(500, player_id, "", 0, "issue login ticket failed"),
            req.version(),
            req.keep_alive());
    }
    return MakeJsonResponse(
        http::status::ok,
        BuildLoginTicketResponseJson(0, player_id, issue.token, issue.expire_at_ms, "ok"),
        req.version(),
        req.keep_alive());
}

void RunWebSocketProxy(
    tcp::socket socket, const GatewayConfig& config, const http::request<http::string_body>& request) {
    beast::error_code ec;
    websocket::stream<tcp::socket> ws(std::move(socket));
    ws.accept(request, ec);
    if (ec) {
        return;
    }
    ws.text(true);
    ws.next_layer().non_blocking(true, ec);
    if (ec) {
        return;
    }

    std::mutex out_mu;
    std::deque<std::string> outbound;
    auto push_outbound = [&](std::string msg) {
        std::lock_guard<std::mutex> lock(out_mu);
        outbound.push_back(std::move(msg));
    };

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
                push_outbound(BuildEnvelopeJson(envelope));
            },
            &connect_err)) {
        push_outbound(BuildGatewayErrorEnvelope("upstream_connect_failed:" + connect_err));
    }

    bool upstream_disconnect_notified = false;
    while (true) {
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
                upstream.Close();
                return;
            }
        }

        beast::flat_buffer buffer;
        ws.read(buffer, ec);
        if (ec == asio::error::would_block || ec == asio::error::try_again) {
            if (!upstream.running()) {
                if (!upstream_disconnect_notified) {
                    push_outbound(BuildGatewayErrorEnvelope("upstream_disconnected"));
                    upstream_disconnect_notified = true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (ec == websocket::error::closed) {
            break;
        }
        if (ec) {
            break;
        }

        const std::string payload = beast::buffers_to_string(buffer.data());
        GatewayEnvelope envelope;
        std::string parse_err;
        if (!ParseEnvelopeJson(payload, &envelope, &parse_err)) {
            push_outbound(BuildGatewayErrorEnvelope("bad_request:" + parse_err));
            continue;
        }
        if (!upstream.running()) {
            push_outbound(BuildGatewayErrorEnvelope("upstream_not_ready"));
            continue;
        }
        Packet packet;
        packet.msg_id = envelope.msg_id;
        packet.seq_id = envelope.seq_id;
        packet.player_id = envelope.player_id;
        packet.body = envelope.body;
        std::string send_err;
        if (!upstream.SendPacket(packet, &send_err)) {
            push_outbound(BuildGatewayErrorEnvelope("upstream_send_failed:" + send_err));
        }
    }
    upstream.Close();
    ws.close(websocket::close_code::normal, ec);
}

void HandleGatewayConnection(tcp::socket socket, const GatewayConfig& config, AuthTokenService* auth_token_service) {
    beast::error_code ec;
    socket.non_blocking(false, ec);
    if (ec) {
        return;
    }

    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    http::read(socket, buffer, request, ec);
    if (ec) {
        return;
    }

    if (websocket::is_upgrade(request) && request.target() == "/ws/game") {
        RunWebSocketProxy(std::move(socket), config, request);
        return;
    }

    if (request.method() == http::verb::post && request.target() == "/api/auth/login-ticket") {
        auto response = HandleLoginTicketRequest(request, config, auth_token_service);
        http::write(socket, response, ec);
    } else {
        auto response = MakeJsonResponse(
            http::status::not_found,
            BuildLoginTicketResponseJson(404, 0, "", 0, "not found"),
            request.version(),
            request.keep_alive());
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

    std::string log_err;
    if (!Logger::Instance().Init(config_.log_level, config_.log_dir, &log_err)) {
        if (err != nullptr) *err = log_err;
        running_.store(false);
        return false;
    }

    accept_thread_ = std::thread(&GatewayServer::AcceptLoop, this);
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
            HandleGatewayConnection(std::move(s), config_, &auth_token_service_);
        });
    }
}

}  // namespace ddz
