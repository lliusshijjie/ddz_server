#include <cassert>
#include <iostream>
#include <vector>

#include "service/rule/ddz_rule_engine.h"

namespace {

void TestAnalyze() {
    using ddz::CardComboType;
    using ddz::DdzRuleEngine;

    auto c1 = DdzRuleEngine::Analyze({0});
    assert(c1.valid && c1.type == CardComboType::Single);

    auto c2 = DdzRuleEngine::Analyze({0, 13});
    assert(c2.valid && c2.type == CardComboType::Pair);

    auto c3 = DdzRuleEngine::Analyze({0, 13, 26});
    assert(c3.valid && c3.type == CardComboType::Triple);

    auto c4 = DdzRuleEngine::Analyze({0, 13, 26, 39});
    assert(c4.valid && c4.type == CardComboType::Bomb);

    auto c5 = DdzRuleEngine::Analyze({0, 13, 26, 1});
    assert(c5.valid && c5.type == CardComboType::TripleOne);

    auto c6 = DdzRuleEngine::Analyze({0, 13, 26, 1, 14});
    assert(c6.valid && c6.type == CardComboType::TriplePair);

    auto c7 = DdzRuleEngine::Analyze({0, 1, 2, 3, 4});
    assert(c7.valid && c7.type == CardComboType::Straight);

    auto c8 = DdzRuleEngine::Analyze({52, 53});
    assert(c8.valid && c8.type == CardComboType::Rocket);

    auto c9 = DdzRuleEngine::Analyze({0, 13, 1, 14, 2, 15});
    assert(c9.valid && c9.type == CardComboType::PairStraight);

    auto c10 = DdzRuleEngine::Analyze({0, 13, 26, 1, 14, 27});
    assert(c10.valid && c10.type == CardComboType::Plane);

    auto c11 = DdzRuleEngine::Analyze({0, 13, 26, 1, 14, 27, 6, 20});
    assert(c11.valid && c11.type == CardComboType::PlaneSingle);

    auto c12 = DdzRuleEngine::Analyze({0, 13, 26, 1, 14, 27, 6, 19, 7, 20});
    assert(c12.valid && c12.type == CardComboType::PlanePair);

    auto c13 = DdzRuleEngine::Analyze({0, 13, 26, 39, 8, 9});
    assert(c13.valid && c13.type == CardComboType::FourTwoSingles);

    auto c14 = DdzRuleEngine::Analyze({0, 13, 26, 39, 8, 21, 9, 22});
    assert(c14.valid && c14.type == CardComboType::FourTwoPairs);

    auto bad = DdzRuleEngine::Analyze({0, 1});
    assert(!bad.valid);

    auto bad_pair_straight = DdzRuleEngine::Analyze({0, 13, 1, 14, 2, 16});
    assert(!bad_pair_straight.valid);
}

void TestCompare() {
    using ddz::DdzRuleEngine;

    auto pair_low = DdzRuleEngine::Analyze({0, 13});
    auto pair_high = DdzRuleEngine::Analyze({1, 14});
    assert(DdzRuleEngine::CanBeat(pair_high, pair_low));
    assert(!DdzRuleEngine::CanBeat(pair_low, pair_high));

    auto straight5_low = DdzRuleEngine::Analyze({0, 1, 2, 3, 4});
    auto straight5_high = DdzRuleEngine::Analyze({1, 2, 3, 4, 5});
    assert(DdzRuleEngine::CanBeat(straight5_high, straight5_low));

    auto straight6 = DdzRuleEngine::Analyze({0, 1, 2, 3, 4, 5});
    assert(!DdzRuleEngine::CanBeat(straight6, straight5_low));

    auto bomb = DdzRuleEngine::Analyze({0, 13, 26, 39});
    assert(DdzRuleEngine::CanBeat(bomb, pair_high));

    auto rocket = DdzRuleEngine::Analyze({52, 53});
    assert(DdzRuleEngine::CanBeat(rocket, bomb));

    auto pair_straight_low = DdzRuleEngine::Analyze({0, 13, 1, 14, 2, 15});
    auto pair_straight_high = DdzRuleEngine::Analyze({1, 14, 2, 15, 3, 16});
    assert(DdzRuleEngine::CanBeat(pair_straight_high, pair_straight_low));

    auto plane_low = DdzRuleEngine::Analyze({0, 13, 26, 1, 14, 27});
    auto plane_high = DdzRuleEngine::Analyze({1, 14, 27, 2, 15, 28});
    assert(DdzRuleEngine::CanBeat(plane_high, plane_low));

    auto plane_single_low = DdzRuleEngine::Analyze({0, 13, 26, 1, 14, 27, 6, 20});
    auto plane_single_high = DdzRuleEngine::Analyze({1, 14, 27, 2, 15, 28, 6, 20});
    assert(DdzRuleEngine::CanBeat(plane_single_high, plane_single_low));

    auto plane_pair_low = DdzRuleEngine::Analyze({0, 13, 26, 1, 14, 27, 6, 19, 7, 20});
    auto plane_pair_high = DdzRuleEngine::Analyze({1, 14, 27, 2, 15, 28, 6, 19, 7, 20});
    assert(DdzRuleEngine::CanBeat(plane_pair_high, plane_pair_low));

    auto four_two_singles_low = DdzRuleEngine::Analyze({0, 13, 26, 39, 8, 9});
    auto four_two_singles_high = DdzRuleEngine::Analyze({1, 14, 27, 40, 8, 9});
    assert(DdzRuleEngine::CanBeat(four_two_singles_high, four_two_singles_low));

    auto four_two_pairs_low = DdzRuleEngine::Analyze({0, 13, 26, 39, 8, 21, 9, 22});
    auto four_two_pairs_high = DdzRuleEngine::Analyze({1, 14, 27, 40, 8, 21, 9, 22});
    assert(DdzRuleEngine::CanBeat(four_two_pairs_high, four_two_pairs_low));

    assert(!DdzRuleEngine::CanBeat(plane_low, plane_single_low));
}

}  // namespace

int main() {
    TestAnalyze();
    TestCompare();
    std::cout << "test_p4_ddz_rule_engine passed" << std::endl;
    return 0;
}
