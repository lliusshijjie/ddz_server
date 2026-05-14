#include "service/rule/ddz_rule_engine.h"

#include <algorithm>
#include <unordered_map>

namespace ddz {
namespace {

bool IsStraight(const std::vector<int32_t>& ranks) {
    if (ranks.size() < 5) {
        return false;
    }
    for (size_t i = 1; i < ranks.size(); ++i) {
        if (ranks[i] != ranks[i - 1] + 1) {
            return false;
        }
    }
    return ranks.front() >= 3 && ranks.back() <= 14;
}

}  // namespace

int32_t DdzRuleEngine::CardRank(int32_t card_id) {
    if (card_id < 0 || card_id > 53) {
        return 0;
    }
    if (card_id == 52) {
        return 16;
    }
    if (card_id == 53) {
        return 17;
    }
    return (card_id % 13) + 3;
}

CardCombo DdzRuleEngine::Analyze(const std::vector<int32_t>& cards) {
    CardCombo combo;
    combo.card_count = static_cast<int32_t>(cards.size());
    if (cards.empty()) {
        return combo;
    }

    std::unordered_map<int32_t, int32_t> cnt;
    std::vector<int32_t> ranks;
    ranks.reserve(cards.size());
    for (const int32_t c : cards) {
        const int32_t r = CardRank(c);
        if (r == 0) {
            return combo;
        }
        cnt[r] += 1;
        ranks.push_back(r);
    }
    std::sort(ranks.begin(), ranks.end());

    const int32_t n = static_cast<int32_t>(cards.size());
    if (n == 1) {
        combo.type = CardComboType::Single;
        combo.main_rank = ranks[0];
        combo.valid = true;
        return combo;
    }
    if (n == 2) {
        if (ranks[0] == 16 && ranks[1] == 17) {
            combo.type = CardComboType::Rocket;
            combo.main_rank = 17;
            combo.valid = true;
            return combo;
        }
        if (ranks[0] == ranks[1]) {
            combo.type = CardComboType::Pair;
            combo.main_rank = ranks[0];
            combo.valid = true;
            return combo;
        }
        return combo;
    }
    if (n == 3) {
        if (cnt.size() == 1) {
            combo.type = CardComboType::Triple;
            combo.main_rank = ranks[0];
            combo.valid = true;
        }
        return combo;
    }
    if (n == 4) {
        if (cnt.size() == 1) {
            combo.type = CardComboType::Bomb;
            combo.main_rank = ranks[0];
            combo.valid = true;
            return combo;
        }
        for (const auto& kv : cnt) {
            if (kv.second == 3) {
                combo.type = CardComboType::TripleOne;
                combo.main_rank = kv.first;
                combo.valid = true;
                return combo;
            }
        }
        return combo;
    }
    if (n == 5) {
        if (cnt.size() == 2) {
            int32_t triple_rank = 0;
            int32_t pair_rank = 0;
            for (const auto& kv : cnt) {
                if (kv.second == 3) triple_rank = kv.first;
                if (kv.second == 2) pair_rank = kv.first;
            }
            if (triple_rank > 0 && pair_rank > 0) {
                combo.type = CardComboType::TriplePair;
                combo.main_rank = triple_rank;
                combo.valid = true;
                return combo;
            }
        }
    }

    bool all_single = true;
    for (const auto& kv : cnt) {
        if (kv.second != 1) {
            all_single = false;
            break;
        }
    }
    if (all_single && IsStraight(ranks)) {
        combo.type = CardComboType::Straight;
        combo.main_rank = ranks.back();
        combo.valid = true;
        return combo;
    }

    return combo;
}

bool DdzRuleEngine::CanBeat(const CardCombo& challenger, const CardCombo& previous) {
    if (!challenger.valid) {
        return false;
    }
    if (!previous.valid) {
        return true;
    }
    if (challenger.type == CardComboType::Rocket) {
        return previous.type != CardComboType::Rocket;
    }
    if (previous.type == CardComboType::Rocket) {
        return false;
    }
    if (challenger.type == CardComboType::Bomb && previous.type != CardComboType::Bomb) {
        return true;
    }
    if (previous.type == CardComboType::Bomb && challenger.type != CardComboType::Bomb) {
        return false;
    }
    if (challenger.type != previous.type) {
        return false;
    }
    if (challenger.type == CardComboType::Straight && challenger.card_count != previous.card_count) {
        return false;
    }
    return challenger.main_rank > previous.main_rank;
}

}  // namespace ddz

