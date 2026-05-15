#include "service/rule/ddz_rule_engine.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

namespace ddz {
namespace {

bool IsConsecutive(const std::vector<int32_t>& ranks) {
    if (ranks.empty()) {
        return false;
    }
    for (size_t i = 1; i < ranks.size(); ++i) {
        if (ranks[i] != ranks[i - 1] + 1) {
            return false;
        }
    }
    return true;
}

bool IsValidSequenceRanks(const std::vector<int32_t>& ranks) {
    if (ranks.empty()) {
        return false;
    }
    return ranks.front() >= 3 && ranks.back() <= 14;
}

std::vector<int32_t> CollectRanksByCount(const std::map<int32_t, int32_t>& cnt, int32_t count) {
    std::vector<int32_t> out;
    for (const auto& kv : cnt) {
        if (kv.second == count) {
            out.push_back(kv.first);
        }
    }
    return out;
}

int32_t MaxRank(const std::vector<int32_t>& ranks) {
    if (ranks.empty()) {
        return 0;
    }
    return *std::max_element(ranks.begin(), ranks.end());
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

    std::unordered_map<int32_t, int32_t> cnt_um;
    std::vector<int32_t> ranks;
    ranks.reserve(cards.size());
    for (const int32_t c : cards) {
        const int32_t r = CardRank(c);
        if (r == 0) {
            return combo;
        }
        cnt_um[r] += 1;
        ranks.push_back(r);
    }
    std::sort(ranks.begin(), ranks.end());
    std::map<int32_t, int32_t> cnt(cnt_um.begin(), cnt_um.end());

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
    if (all_single && n >= 5 && IsConsecutive(ranks) && IsValidSequenceRanks(ranks)) {
        combo.type = CardComboType::Straight;
        combo.main_rank = ranks.back();
        combo.valid = true;
        return combo;
    }

    bool all_pair = (n >= 6 && n % 2 == 0);
    if (all_pair) {
        std::vector<int32_t> pair_ranks;
        for (const auto& kv : cnt) {
            if (kv.second != 2) {
                all_pair = false;
                break;
            }
            pair_ranks.push_back(kv.first);
        }
        if (all_pair && static_cast<int32_t>(pair_ranks.size()) == n / 2 &&
            IsConsecutive(pair_ranks) && IsValidSequenceRanks(pair_ranks)) {
            combo.type = CardComboType::PairStraight;
            combo.main_rank = pair_ranks.back();
            combo.valid = true;
            return combo;
        }
    }

    if (n >= 6 && n % 3 == 0) {
        bool all_triple = true;
        std::vector<int32_t> triple_ranks;
        for (const auto& kv : cnt) {
            if (kv.second != 3) {
                all_triple = false;
                break;
            }
            triple_ranks.push_back(kv.first);
        }
        if (all_triple && static_cast<int32_t>(triple_ranks.size()) >= 2 &&
            IsConsecutive(triple_ranks) && IsValidSequenceRanks(triple_ranks)) {
            combo.type = CardComboType::Plane;
            combo.main_rank = triple_ranks.back();
            combo.valid = true;
            return combo;
        }
    }

    if (n >= 8 && n % 4 == 0) {
        const int32_t plane_width = n / 4;
        const auto triple_ranks = CollectRanksByCount(cnt, 3);
        if (static_cast<int32_t>(triple_ranks.size()) == plane_width &&
            IsConsecutive(triple_ranks) && IsValidSequenceRanks(triple_ranks)) {
            int32_t single_count = 0;
            bool valid_wings = true;
            std::set<int32_t> triple_rank_set(triple_ranks.begin(), triple_ranks.end());
            for (const auto& kv : cnt) {
                if (kv.second == 3) {
                    continue;
                }
                if (kv.second != 1 || triple_rank_set.find(kv.first) != triple_rank_set.end()) {
                    valid_wings = false;
                    break;
                }
                single_count += 1;
            }
            if (valid_wings && single_count == plane_width) {
                combo.type = CardComboType::PlaneSingle;
                combo.main_rank = MaxRank(triple_ranks);
                combo.valid = true;
                return combo;
            }
        }
    }

    if (n >= 10 && n % 5 == 0) {
        const int32_t plane_width = n / 5;
        const auto triple_ranks = CollectRanksByCount(cnt, 3);
        if (static_cast<int32_t>(triple_ranks.size()) == plane_width &&
            IsConsecutive(triple_ranks) && IsValidSequenceRanks(triple_ranks)) {
            int32_t pair_count = 0;
            bool valid_wings = true;
            std::set<int32_t> triple_rank_set(triple_ranks.begin(), triple_ranks.end());
            for (const auto& kv : cnt) {
                if (kv.second == 3) {
                    continue;
                }
                if (kv.second != 2 || triple_rank_set.find(kv.first) != triple_rank_set.end()) {
                    valid_wings = false;
                    break;
                }
                pair_count += 1;
            }
            if (valid_wings && pair_count == plane_width) {
                combo.type = CardComboType::PlanePair;
                combo.main_rank = MaxRank(triple_ranks);
                combo.valid = true;
                return combo;
            }
        }
    }

    if (n == 6) {
        int32_t four_rank = 0;
        int32_t single_cnt = 0;
        bool valid = true;
        for (const auto& kv : cnt) {
            if (kv.second == 4) {
                four_rank = kv.first;
            } else if (kv.second == 1) {
                single_cnt += 1;
            } else {
                valid = false;
                break;
            }
        }
        if (valid && four_rank > 0 && single_cnt == 2) {
            combo.type = CardComboType::FourTwoSingles;
            combo.main_rank = four_rank;
            combo.valid = true;
            return combo;
        }
    }

    if (n == 8) {
        int32_t four_rank = 0;
        int32_t pair_cnt = 0;
        bool valid = true;
        for (const auto& kv : cnt) {
            if (kv.second == 4) {
                four_rank = kv.first;
            } else if (kv.second == 2) {
                pair_cnt += 1;
            } else {
                valid = false;
                break;
            }
        }
        if (valid && four_rank > 0 && pair_cnt == 2) {
            combo.type = CardComboType::FourTwoPairs;
            combo.main_rank = four_rank;
            combo.valid = true;
            return combo;
        }
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
    if ((challenger.type == CardComboType::Straight ||
         challenger.type == CardComboType::PairStraight ||
         challenger.type == CardComboType::Plane ||
         challenger.type == CardComboType::PlaneSingle ||
         challenger.type == CardComboType::PlanePair) &&
        challenger.card_count != previous.card_count) {
        return false;
    }
    return challenger.main_rank > previous.main_rank;
}

}  // namespace ddz
