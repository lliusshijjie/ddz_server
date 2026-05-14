#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ddz {

enum class CardComboType {
    Invalid = 0,
    Single = 1,
    Pair = 2,
    Triple = 3,
    TripleOne = 4,
    TriplePair = 5,
    Straight = 6,
    Bomb = 7,
    Rocket = 8
};

struct CardCombo {
    CardComboType type = CardComboType::Invalid;
    int32_t main_rank = 0;
    int32_t card_count = 0;
    bool valid = false;
};

class DdzRuleEngine {
public:
    static int32_t CardRank(int32_t card_id);
    static CardCombo Analyze(const std::vector<int32_t>& cards);
    static bool CanBeat(const CardCombo& challenger, const CardCombo& previous);
};

}  // namespace ddz

