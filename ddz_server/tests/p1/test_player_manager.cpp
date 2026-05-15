#include <cassert>
#include <iostream>

#include "service/player/player_manager.h"

int main() {
    ddz::PlayerManager pm;
    pm.UpsertPlayer(1001, "alice", 1000);
    pm.ForceState(1001, ddz::PlayerState::Lobby);

    assert(pm.SetState(1001, ddz::PlayerState::Matching));
    assert(pm.SetState(1001, ddz::PlayerState::InRoom));
    assert(pm.SetState(1001, ddz::PlayerState::Playing));
    assert(pm.SetState(1001, ddz::PlayerState::Settlement));
    assert(pm.SetState(1001, ddz::PlayerState::Lobby));

    // Illegal direct transition: Lobby -> Playing
    assert(!pm.SetState(1001, ddz::PlayerState::Playing));

    std::cout << "test_p1_player_manager passed" << std::endl;
    return 0;
}

