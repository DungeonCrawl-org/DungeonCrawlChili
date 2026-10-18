#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "ability.h"
#include "delay.h"
#include "env.h"
#include "feature.h"
#include "terrain.h"
#include "item-prop.h"
#include "lignifite.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "player-equip.h"
#include "species.h"
#include "status.h"
#include "store.h"
#include "tags.h"
#include "tag-version.h"
#include "test_player_fixture.h"
#include "transform.h"

struct LignifiteFixture : MockPlayerYouTestsFixture
{
    LignifiteFixture()
    {
        init_show_table();
        you.species = SP_LIGNIFITE;
        give_basic_mutations(SP_LIGNIFITE);
        env.grid(you.pos()) = DNGN_FLOOR;
        you.props.erase(EMERGENCY_FLIGHT_KEY);
    }
};

TEST_CASE_METHOD(LignifiteFixture, "Lignifites grow through six stages with valid sizes", "[lignifite]")
{
    const int speed[] = {6, 7, 8, 10, 12, 14};
    const size_type sizes[] = {
        SIZE_LITTLE, SIZE_SMALL, SIZE_MEDIUM, SIZE_LARGE, SIZE_LARGE, SIZE_GIANT
    };
    for (int xl = 1; xl <= 27; ++xl)
    {
        CAPTURE(xl);
        you.experience_level = xl;
        const int stage = min(5, xl / 5);
        CHECK(you.body_size(PSIZE_BODY, true) == sizes[stage]);
        CHECK(you.body_size(PSIZE_BODY, true) < NUM_SIZE_LEVELS);
        CHECK(species::get_hp_modifier(SP_LIGNIFITE) == -1 + stage);
        CHECK(player_movement_speed(false, false, 100) == speed[stage] * 100);
        CHECK(player_displayed_shield_class() == 1 + xl / 3);
        CHECK(you.racial_ac(false) == max(0, xl - 12) * 100);
    }
    // Non-player species queries must not inherit the current character's XL.
    you.species = SP_HUMAN;
    CHECK(species::size(SP_LIGNIFITE) == SIZE_LITTLE);
    CHECK(species::get_hp_modifier(SP_LIGNIFITE) == -1);
    CHECK(player_movement_speed(false, false, 100) == 1000);
}

TEST_CASE_METHOD(LignifiteFixture, "Lignifite anatomy is suppressed by major transformations", "[lignifite]")
{
    you.experience_level = 27;
    REQUIRE(player_displayed_shield_class() == 10);
    you.form = transformation::dragon;
    CHECK_FALSE(lignifite_natural_body());
    CHECK(you.racial_ac(true) == 0);
    CHECK(player_displayed_shield_class() == 0);
    CHECK(player_movement_speed(false, false, 100) == get_form()->base_move_speed * 100);
    CHECK_FALSE(player_has_ability(ABIL_PLANT_ROOTS));
}

TEST_CASE_METHOD(LignifiteFixture, "Lignifites retain living plant strengths and weaknesses", "[lignifite]")
{
    CHECK(you.holiness() == MH_PLANT);
    CHECK(you.is_unbreathing());
    CHECK_FALSE(you.has_blood());
    CHECK(you.res_torment());
    CHECK(player_res_fire() == -1);
    CHECK(player_res_poison() == 1);
    CHECK_FALSE(drain_player(100, false, false, true));
    CHECK(you.hp_max_adj_temp == 0);
}

TEST_CASE_METHOD(LignifiteFixture, "Rooting costs MP, grants stasis, and persists in saves", "[lignifite]")
{
    you.experience_level = 11;
    CHECK_FALSE(player_has_ability(ABIL_PLANT_ROOTS));
    you.experience_level = 12;
    CHECK(player_has_ability(ABIL_PLANT_ROOTS));
    you.experience_level = 27; // At this XL the ability cannot fail.
    you.magic_points = you.max_magic_points = 10;
    INFO(lignifite_rooting_blocker());
    CAPTURE(you.airborne(), env.grid(you.pos()), feat_has_solid_floor(env.grid(you.pos())));
    REQUIRE(lignifite_rooting_blocker().empty());
    const int ac = you.armour_class_scaled(100);
    const int regen = player_regen();
    you.duration[DUR_TELEPORT] = 20;
    const int root_hotkey = get_talent(ABIL_PLANT_ROOTS).hotkey;
    REQUIRE(activate_talent(get_talent(ABIL_PLANT_ROOTS)));
    CHECK(you.magic_points == 7);
    CHECK(lignifite_rooted());
    CHECK(you.cannot_move());
    CHECK(you.stasis());
    CHECK(you.res_petrify());
    CHECK_FALSE(you.res_petrify(false));
    CHECK(you.duration[DUR_TELEPORT] == 0);
    CHECK_FALSE(you.no_tele_reason(false, true).empty());
    CHECK(you.armour_class_scaled(100) == ac + 800);
    CHECK(player_regen() == regen + 100);
    CHECK_FALSE(player_has_ability(ABIL_PLANT_ROOTS));
    CHECK(player_has_ability(ABIL_UPROOT));
    CHECK(get_talent(ABIL_UPROOT).hotkey == root_hotkey);
    CHECK_FALSE(check_transform_into(transformation::dragon, false));
    status_info status;
    CHECK(fill_status_info(STATUS_ROOTED, status));
    CHECK(status.light_text == "Rooted");
    vector<unsigned char> bytes;
    writer out(&bytes);
    you.props.write(out);
    you.props.clear();
    reader in(bytes);
    in.setMinorVersion(TAG_MINOR_VERSION);
    you.props.read(in);
    CHECK(lignifite_rooted());
    lignifite_uproot();
    CHECK(get_talent(ABIL_PLANT_ROOTS).hotkey == root_hotkey);
    CHECK_FALSE(you.stasis());
    CHECK_FALSE(you.cannot_move());
    CHECK(you.armour_class_scaled(100) == ac);
    CHECK(player_regen() == regen);
}

TEST_CASE_METHOD(LignifiteFixture, "Rooting requires solid ground and natural anatomy", "[lignifite]")
{
    you.experience_level = 12;
    INFO(lignifite_rooting_blocker());
    CAPTURE(you.airborne(), env.grid(you.pos()), feat_has_solid_floor(env.grid(you.pos())));
    REQUIRE(lignifite_rooting_blocker().empty());
    for (const auto feat : {DNGN_SHALLOW_WATER, DNGN_DEEP_WATER, DNGN_LAVA})
    {
        env.grid(you.pos()) = feat;
        CHECK_FALSE(lignifite_rooting_blocker().empty());
    }
    env.grid(you.pos()) = DNGN_FLOOR;
    you.duration[DUR_FLIGHT] = 50;
    CHECK_FALSE(lignifite_rooting_blocker().empty());
    you.duration[DUR_FLIGHT] = 0;
    you.form = transformation::dragon;
    CHECK_FALSE(lignifite_rooting_blocker().empty());
}

TEST_CASE_METHOD(LignifiteFixture, "Uprooting can be interrupted without losing roots", "[lignifite]")
{
    you.experience_level = 12;
    INFO(lignifite_rooting_blocker());
    CAPTURE(you.airborne(), env.grid(you.pos()), feat_has_solid_floor(env.grid(you.pos())));
    REQUIRE(lignifite_rooting_blocker().empty());
    lignifite_plant_roots();
    auto delay = start_delay<UprootDelay>(LIGNIFITE_UPROOT_TURNS - 1);
    CHECK(delay->duration == 7); // Beginning the ability costs the eighth turn.
    CHECK(delay->try_interrupt(true));
    CHECK(lignifite_rooted());
    you.delay_queue.clear();
    lignifite_uproot();
    CHECK_FALSE(player_has_ability(ABIL_UPROOT));
}

TEST_CASE_METHOD(LignifiteFixture, "Growth removes outgrown equipment without destroying it", "[lignifite]")
{
    you.experience_level = 10;
    you.inv[0].base_type = OBJ_ARMOUR;
    you.inv[0].sub_type = ARM_PLATE_ARMOUR;
    you.inv[0].quantity = 1;
    you.inv[0].link = 0;
    you.inv[0].pos = ITEM_IN_INVENTORY;
    item_def& armour = you.inv[0];
    REQUIRE(can_equip_item(armour));
    equip_item(SLOT_BODY_ARMOUR, 0);
    REQUIRE(you.body_armour() == &armour);
    you.experience_level = 15;
    lignifite_level_up(15);
    CHECK(you.body_armour() == nullptr);
    CHECK(armour.defined());
    CHECK_FALSE(can_equip_item(armour));
}
