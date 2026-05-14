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

    auto bad = DdzRuleEngine::Analyze({0, 1});
    assert(!bad.valid);
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
}

}  // namespace

int main() {
    TestAnalyze();
    TestCompare();
    std::cout << "test_p4_ddz_rule_engine passed" << std::endl;
    return 0;
}

