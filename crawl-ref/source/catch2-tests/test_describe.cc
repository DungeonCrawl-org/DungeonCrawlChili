#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "describe.h"
#include "mon-info.h"
#include "monster-type.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "player.h"
#include "player-equip.h"
#include "state.h"
#include "unwind.h"
#include "test_player_fixture.h"

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Armour comparison previews replacement without changing equipment",
                 "[armour-comparison]")
{
    unwind_var<bool> save_state(crawl_state.need_save, true);
    item_def &worn = you.inv[0];
    worn.base_type = OBJ_ARMOUR;
    worn.sub_type = ARM_FIRE_DRAGON_ARMOUR;
    worn.quantity = 1;
    worn.flags = ISFLAG_IDENTIFIED;
    worn.pos = ITEM_IN_INVENTORY;
    worn.link = 0;
    you.equipment.add(worn, SLOT_BODY_ARMOUR);
    you.equipment.update();

    item_def robe;
    robe.base_type = OBJ_ARMOUR;
    robe.sub_type = ARM_ROBE;
    robe.quantity = 1;
    robe.flags = ISFLAG_IDENTIFIED;

    const player_stats before = you.calc_stats(100);
    const player_stats preview = you.preview_stats_with_specific_item(100, robe);
    CHECK(preview.ac < before.ac);
    CHECK(preview.ev > before.ev);
    CHECK(before.res_fire == 2);
    CHECK(before.res_cold == -1);
    CHECK(preview.res_fire == 0);
    CHECK(preview.res_cold == 0);

    const string description = get_item_description(robe);
    CHECK(description.find("Armour comparison") != string::npos);
    CHECK(description.find("Compared with " + worn.name(DESC_A) + ":") != string::npos);
    CHECK(description.find("Armour class (AC): <lightred>") != string::npos);
    CHECK(description.find("Evasion (EV): <lightgreen>") != string::npos);
    CHECK(description.find("++ -> none (loss)") != string::npos);
    CHECK(description.find("vulnerable -> none (gain)") != string::npos);
    CHECK(you.body_armour() == &worn);
    CHECK(you.calc_stats(100).ac == before.ac);
    CHECK(you.calc_stats(100).res_fire == before.res_fire);
    CHECK_FALSE(you.inv[ENDOFPACK].defined());

    CHECK(get_item_description(worn).find("If you remove this armour:") != string::npos);
    CHECK(get_item_description(robe, IDM_MONSTER).find("Armour comparison") == string::npos);
    robe.flags = 0;
    CHECK(get_item_description(robe).find("Armour comparison") == string::npos);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Armour comparison supports empty slots and shields",
                 "[armour-comparison]")
{
    unwind_var<bool> save_state(crawl_state.need_save, true);
    item_def shield;
    shield.base_type = OBJ_ARMOUR;
    shield.sub_type = ARM_BUCKLER;
    shield.quantity = 1;
    shield.flags = ISFLAG_IDENTIFIED;
    const string description = get_item_description(shield);
    CHECK(description.find("Compared with an empty equipment slot:") != string::npos);
    CHECK(description.find("Shielding (SH): <lightgreen>") != string::npos);
    CHECK(description.find("Resistances and willpower: unchanged.") != string::npos);
    CHECK(you.shield() == nullptr);

    item_def cloak;
    cloak.base_type = OBJ_ARMOUR;
    cloak.sub_type = ARM_CLOAK;
    cloak.quantity = 1;
    cloak.flags = ISFLAG_IDENTIFIED;
    cloak.brand = SPARM_WILLPOWER;
    const player_stats before = you.calc_stats(100);
    const player_stats preview = you.preview_stats_with_specific_item(100, cloak);
    CHECK(preview.willpower > before.willpower);
    CHECK(get_item_description(cloak).find("Willpower: <lightgreen>") != string::npos);
    CHECK(you.calc_stats(100).willpower == before.willpower);

    you.duration[DUR_RESISTANCE] = 10;
    CHECK(you.preview_stats_with_specific_item(100, shield).res_fire == 0);
    CHECK(you.duration[DUR_RESISTANCE] == 10);
}

TEST_CASE("_monster_habitat_description outputs correct descriptions", "[single-file]")
{
    init_monsters();

    SECTION("Amphibious monsters append correct string")
    {
        const auto info = monster_info(MONS_FRILLED_LIZARD, MONS_FRILLED_LIZARD);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "It can travel through water.\n");
    }

    SECTION("Lava monsters output correct string")
    {
        const auto info = monster_info(MONS_SALAMANDER, MONS_SALAMANDER);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "It can travel through lava.\n");
    }

    SECTION("Monsters with other pronouns output correct string")
    {
        const auto info = monster_info(MONS_CEREBOV, MONS_CEREBOV);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "They can travel through water.\n");
    }

    SECTION("Other monsters output nothing")
    {
        const auto info = monster_info(MONS_KOBOLD, MONS_KOBOLD);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "");
    }
}
