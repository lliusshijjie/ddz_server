cmake_minimum_required(VERSION 3.16)
project(ddz_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ddz_core
    common/config/config_loader.cpp
    common/util/sha256.cpp
    log/logger.cpp
    network/packet_codec.cpp
    network/dispatcher.cpp
    network/tcp_connection.cpp
    network/tcp_server.cpp
    service/session/session_manager.cpp
    service/login/auth_token_service.cpp
    service/player/player_manager.cpp
    service/login/login_service.cpp
    service/reconnect/reconnect_service.cpp
    service/room/room_manager.cpp
    service/match/match_manager.cpp
    service/match/match_service.cpp
    service/storage/mysql/mysql_connection_pool.cpp
    service/storage/dao/player_dao.cpp
    service/storage/dao/game_record_dao.cpp
    service/storage/dao/coin_log_dao.cpp
    service/storage/storage_service.cpp
    service/redis/redis_optional_store.cpp
    service/settlement/settlement_service.cpp
    server/game_server.cpp
)

target_include_directories(ddz_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_libraries(ddz_core PUBLIC ws2_32)
endif()

add_executable(ddz_server
    server/main.cpp
)
target_link_libraries(ddz_server PRIVATE ddz_core)

enable_testing()

add_executable(test_p0_packet_codec
    tests/p0/test_packet_codec.cpp
)
target_link_libraries(test_p0_packet_codec PRIVATE ddz_core)
add_test(NAME p0_packet_codec COMMAND test_p0_packet_codec)

add_executable(test_p0_dispatcher_heartbeat
    tests/p0/test_dispatcher_heartbeat.cpp
)
target_link_libraries(test_p0_dispatcher_heartbeat PRIVATE ddz_core)
add_test(NAME p0_dispatcher_heartbeat COMMAND test_p0_dispatcher_heartbeat)

add_executable(test_p1_session_manager
    tests/p1/test_session_manager.cpp
)
target_link_libraries(test_p1_session_manager PRIVATE ddz_core)
add_test(NAME p1_session_manager COMMAND test_p1_session_manager)

add_executable(test_p1_player_manager
    tests/p1/test_player_manager.cpp
)
target_link_libraries(test_p1_player_manager PRIVATE ddz_core)
add_test(NAME p1_player_manager COMMAND test_p1_player_manager)

add_executable(test_p1_login_service
    tests/p1/test_login_service.cpp
)
target_link_libraries(test_p1_login_service PRIVATE ddz_core)
add_test(NAME p1_login_service COMMAND test_p1_login_service)

add_executable(test_p2_match_manager
    tests/p2/test_match_manager.cpp
)
target_link_libraries(test_p2_match_manager PRIVATE ddz_core)
add_test(NAME p2_match_manager COMMAND test_p2_match_manager)

add_executable(test_p2_room_manager
    tests/p2/test_room_manager.cpp
)
target_link_libraries(test_p2_room_manager PRIVATE ddz_core)
add_test(NAME p2_room_manager COMMAND test_p2_room_manager)

add_executable(test_p2_match_service
    tests/p2/test_match_service.cpp
)
target_link_libraries(test_p2_match_service PRIVATE ddz_core)
add_test(NAME p2_match_service COMMAND test_p2_match_service)

add_executable(test_p3_reconnect_service
    tests/p3/test_reconnect_service.cpp
)
target_link_libraries(test_p3_reconnect_service PRIVATE ddz_core)
add_test(NAME p3_reconnect_service COMMAND test_p3_reconnect_service)

add_executable(test_p3_room_snapshot
    tests/p3/test_room_snapshot.cpp
)
target_link_libraries(test_p3_room_snapshot PRIVATE ddz_core)
add_test(NAME p3_room_snapshot COMMAND test_p3_room_snapshot)

add_executable(test_p4_storage_service
    tests/p4/test_storage_service.cpp
)
target_link_libraries(test_p4_storage_service PRIVATE ddz_core)
add_test(NAME p4_storage_service COMMAND test_p4_storage_service)

add_executable(test_p4_settlement_service
    tests/p4/test_settlement_service.cpp
)
target_link_libraries(test_p4_settlement_service PRIVATE ddz_core)
add_test(NAME p4_settlement_service COMMAND test_p4_settlement_service)

add_executable(test_p5_full_flow_integration
    tests/p5/test_full_flow_integration.cpp
)
target_link_libraries(test_p5_full_flow_integration PRIVATE ddz_core)
add_test(NAME p5_full_flow_integration COMMAND test_p5_full_flow_integration)

add_executable(test_p5_match_manager_stress
    tests/p5/test_match_manager_stress.cpp
)
target_link_libraries(test_p5_match_manager_stress PRIVATE ddz_core)
add_test(NAME p5_match_manager_stress COMMAND test_p5_match_manager_stress)

add_executable(test_p6_mysql_dao
    tests/p6/test_mysql_dao.cpp
)
target_link_libraries(test_p6_mysql_dao PRIVATE ddz_core)
add_test(NAME p6_mysql_dao COMMAND test_p6_mysql_dao)

add_executable(test_p6_settlement_transaction
    tests/p6/test_settlement_transaction.cpp
)
target_link_libraries(test_p6_settlement_transaction PRIVATE ddz_core)
add_test(NAME p6_settlement_transaction COMMAND test_p6_settlement_transaction)

add_executable(test_p6_redis_optional_store
    tests/p6/test_redis_optional_store.cpp
)
target_link_libraries(test_p6_redis_optional_store PRIVATE ddz_core)
add_test(NAME p6_redis_optional_store COMMAND test_p6_redis_optional_store)
