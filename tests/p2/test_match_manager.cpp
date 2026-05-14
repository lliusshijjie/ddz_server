#include <cassert>
#include <iostream>

#include "service/match/match_manager.h"

int main() {
    ddz::MatchManager mm;

    assert(mm.Join(ddz::MatchPlayer{1001, 1, 1000, 3}));
    assert(mm.Join(ddz::MatchPlayer{1002, 2, 1000, 3}));
    assert(mm.Join(ddz::MatchPlayer{1003, 3, 1000, 3}));
    assert(!mm.Join(ddz::MatchPlayer{1001, 4, 1000, 3}));

    auto matched = mm.TryPopMatchedPlayers(3);
    assert(matched.has_value());
    assert(matched->size() == 3);
    assert(!mm.IsMatching(1001));

    assert(mm.Join(ddz::MatchPlayer{2001, 1, 1000, 4}));
    assert(mm.IsMatching(2001));
    assert(mm.Cancel(2001));
    assert(!mm.IsMatching(2001));

    std::cout << "test_p2_match_manager passed" << std::endl;
    return 0;
}

