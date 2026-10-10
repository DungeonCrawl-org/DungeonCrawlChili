#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "death-recap.h"
#include "player.h"
#include "potion-type.h"
#include "state.h"
#include "store.h"
#include "tags.h"
#include "tag-version.h"
#include "test_player_fixture.h"
#include "unwind.h"

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Death recap retains three turns and survives saving",
                 "[death-recap]")
{
    unwind_var<bool> started(crawl_state.game_started, true);
    unwind_var<bool> generating(crawl_state.generating_level, false);
    you.props.erase("death_recap_turns");
    you.props.erase("death_recap_final");
    you.num_turns = 10;
    death_recap_begin_turn();
    death_recap_hp_change("Old hit", 50, 40, 10);
    you.num_turns = 11;
    death_recap_begin_turn();
    death_recap_hp_change("Potion of curing", 38, 46);
    you.num_turns = 12;
    death_recap_begin_turn();
    death_recap_hp_change("Orc warrior", 46, 29, 17);
    death_recap_hp_change("Orc warrior", 29, 21, 8);
    you.num_turns = 13;
    death_recap_begin_turn();
    death_recap_hp_change("Bolt of fire", 21, -5, 26);
    you.duration[DUR_SLOW] = 20;

    const string recap = death_recap_text();
    CHECK(recap.find("Old hit") == string::npos);
    CHECK(recap.find("HP by turn: 38 -> 46 -> 21 -> 0") != string::npos);
    CHECK(recap.find("Damage taken: 51; HP restored: 8") != string::npos);
    CHECK(recap.find("Orc warrior: 25 (49%), 2 hits") != string::npos);
    CHECK(recap.find("Bolt of fire: 26 (50%), 1 hit") != string::npos);
    CHECK(recap.find("Fatal hit: Bolt of fire dealt 26 damage at 21 HP (5 overkill)") != string::npos);
    CHECK(recap.find("Attempted to") == string::npos);
    CHECK(recap.find("Potion of curing (HP") == string::npos);
    CHECK(recap.find("slow") != string::npos);

    // Use the same property serialization as a saved player.
    vector<unsigned char> bytes;
    writer out(&bytes);
    you.props.write(out);
    you.props.clear();
    reader in(bytes);
    in.setMinorVersion(TAG_MINOR_VERSION);
    you.props.read(in);
    CHECK(death_recap_text() == recap);

    death_recap_finish();
    you.duration[DUR_SLOW] = 0;
    CHECK(final_death_recap() == recap);
    ++you.num_turns;
    death_recap_begin_turn();
    CHECK(final_death_recap().empty());
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Death recap bounds busy turns and ignores setup",
                 "[death-recap]")
{
    unwind_var<bool> started(crawl_state.game_started, false);
    unwind_var<bool> generating(crawl_state.generating_level, false);
    you.props.erase("death_recap_turns");
    death_recap_begin_turn();
    death_recap_hp_change("Setup heal", 1, 10);
    CHECK_FALSE(you.props.exists("death_recap_turns"));

    crawl_state.game_started = true;
    you.num_turns = 1;
    death_recap_begin_turn();
    for (int i = 0; i < 20; ++i)
        death_recap_hp_change("Hit", 30 - i, 29 - i, 1);
    death_recap_hp_change("Fatal hit", 10, -90, 100);
    const string recap = death_recap_text();
    CHECK(recap.find("Damage taken: 120; HP restored: 0") != string::npos);
    CHECK(recap.find("Hit: 20 (16%), 20 hits") != string::npos);
    CHECK(recap.find("Fatal hit dealt 100 damage at 10 HP (90 overkill)") != string::npos);
    CHECK(you.props["death_recap_turns"].get_vector()[0]
          .get_table()["sources"].get_table().size() == 2);
    for (int i = 0; i < 20; ++i)
        death_recap_hp_change("Source " + std::to_string(i), 10, 9, 1);
    CHECK(you.props["death_recap_turns"].get_vector()[0]
          .get_table()["sources"].get_table().size() == 13);
    CHECK(death_recap_text().find("Damage taken: 140") != string::npos);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Death recap freezes known supplies before death identification",
                 "[death-recap]")
{
    you.props.erase("death_recap_turns");
    you.inv[0].base_type = OBJ_POTIONS;
    you.inv[0].sub_type = POT_CURING;
    you.inv[0].quantity = 4;
    you.inv[1].base_type = OBJ_POTIONS;
    you.inv[1].sub_type = POT_HEAL_WOUNDS;
    you.inv[1].quantity = 3;
    you.type_ids[OBJ_POTIONS][POT_CURING] = false;
    you.type_ids[OBJ_POTIONS][POT_HEAL_WOUNDS] = true;
    death_recap_finish();
    const string frozen = final_death_recap();
    CHECK(frozen.find("curing 0, heal wounds 3") != string::npos);
    you.type_ids[OBJ_POTIONS][POT_CURING] = true;
    CHECK(final_death_recap() == frozen);
    CHECK(death_recap_text().find("curing 4, heal wounds 3") != string::npos);
}
