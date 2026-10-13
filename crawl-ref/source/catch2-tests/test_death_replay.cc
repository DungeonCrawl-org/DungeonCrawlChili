#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "death-replay.h"
#include "player.h"
#include "store.h"
#include "test_player_fixture.h"

static string replay_morgue()
{
    return "Dungeon Crawl Chili (tiles) character file.\n\n"
        "Game seed: 18446744073709551615\n\n"
        "399 KoSt the Ducker (level 9, -5/50 HPs)\n"
        "Began as a Human Fighter on Oct 12, 2026.\n"
        "Slain by an ogre (26 damage)\n"
        "KoSt the Ducker (HuFi) Turns: 4193, Time: 00:15:28\n"
        "Health: -5/50 AC: 6 Str: 16 XL: 9\n"
        "Magic: 6/6 Int: 12 God: Okawaru [***...]\n"
        "Gold: 241 Dex: 18\n\n"
        "Inventory:\n\nHand Weapons\n a - a +3 dagger of venom (weapon)\n"
        "Potions\n b - 2 potions of heal wounds\n\n"
        "   Skills:\n + Level 7.5 Fighting\n + Level 4.2 Dodging\n\n"
        " Your Spells              Type           Power      Damage    Failure   Level\n"
        "a - Blink                 Tloc           6%         N/A       45%         2\n\n"
        "Your spell library contained the following spells:\n\n"
        " Spells                   Type           Power      Damage    Failure   Level\n"
        " Slow                     Hex            48%        N/A       13%         1\n\n"
        "Death recap (last three turns)\n"
        "  Final location: D:8, x=23, y=17 (0-based map coordinates), turn 4193\n"
        "  HP by turn: 38 -> 46 -> 21 -> 0\n\n"
        "Message History\n\nThe ogre hits you!\n\n"
        "#####\n#.@O#\n#####\n\nYou could see an ogre.\n\n"
        "Notes\nTurn | Place | Note\n4193 | D:8 | Slain by an ogre\n";
}

TEST_CASE("Death Replay parses seed, character and final position", "[death-replay]")
{
    death_replay_data data;
    string error;
    REQUIRE(parse_death_replay(replay_morgue(), data, error));
    CHECK(data.seed == UINT64_MAX);
    CHECK(data.name == "KoSt");
    CHECK(data.level == level_id(BRANCH_DUNGEON, 8));
    CHECK(data.position == coord_def(23, 17));
    CHECK(data.turns == 4193);
}

TEST_CASE("Death Replay rejects malformed replay metadata", "[death-replay]")
{
    death_replay_data data;
    string error;
    CHECK_FALSE(parse_death_replay("not a morgue", data, error));
    CHECK_FALSE(error.empty());
    const string original = replay_morgue();
    for (const auto &replacement : vector<pair<string, string>>{
        {"18446744073709551615", "18446744073709551616"},
        {"x=23", "x=99999"}, {"D:8,", "Unknown:8,"}})
    {
        string text = original;
        text.replace(text.find(replacement.first), replacement.first.size(), replacement.second);
        CHECK_FALSE(parse_death_replay(text, data, error));
    }
    CHECK_FALSE(parse_death_replay(string(2 * 1024 * 1024 + 1, 'x'), data, error));
}

TEST_CASE("Death Replay checkpoints at 50-turn boundaries without duplicate writes", "[death-replay]")
{
    CHECK(death_replay_checkpoint_due(0, -1));
    CHECK_FALSE(death_replay_checkpoint_due(0, 0));
    // Resuming an older game establishes a checkpoint immediately.
    CHECK(death_replay_checkpoint_due(17, -1));
    CHECK(death_replay_checkpoint_due(50, 0));
    CHECK_FALSE(death_replay_checkpoint_due(50, 50));
    CHECK_FALSE(death_replay_checkpoint_due(99, 50));
    CHECK(death_replay_checkpoint_due(100, 50));
    CHECK_FALSE(death_replay_checkpoint_due(100, 100));
    CHECK_FALSE(death_replay_checkpoint_due(125, 100));
    CHECK(death_replay_checkpoint_due(150, 100));
    CHECK(death_replay_checkpoint_due(200, 150));
    CHECK_FALSE(death_replay_checkpoint_due(200, 200));
    CHECK_FALSE(death_replay_checkpoint_due(-1, -1));
}

TEST_CASE("Death Replay requires coordinates inside the Death Recap section", "[death-replay]")
{
    string text = replay_morgue();
    const size_t start = text.find("  Final location:");
    text.erase(start, text.find('\n', start) + 1 - start);
    death_replay_data data;
    string error;
    CHECK_FALSE(parse_death_replay(text, data, error));
    CHECK(error.find("Final location entry") != string::npos);
    text += "Final location: D:8, x=23, y=17\n";
    CHECK_FALSE(parse_death_replay(text, data, error));
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture, "Death Replay marker survives saved-player properties", "[death-replay]")
{
    you.props.erase("death_replay_source");
    CHECK_FALSE(is_death_replay());
    you.props["death_replay_source"] = "Original character";
    CHECK(is_death_replay());
    CHECK(you.props["death_replay_source"].get_string() == "Original character");
}
