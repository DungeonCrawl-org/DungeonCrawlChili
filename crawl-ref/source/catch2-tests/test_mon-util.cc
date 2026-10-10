#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "mon-enum.h"
#include "monster-type.h"
#include "mon-util.h"
#include "mon-death.h"
#include "monster.h"
#include "spell-type.h"
#include "env.h"
#include "items.h"
#include "item-prop.h"
#include "mon-gear.h"

TEST_CASE("mons_is_removed() returns correct values", "[single-file]")
{
    init_monsters();

    SECTION("mons_is_removed() returns true for removed monster")
    {
        const bool removed = mons_is_removed(MONS_BUMBLEBEE);

        REQUIRE(removed == true);
    }

    SECTION("mons_is_removed() returns false for current monster")
    {
        const bool removed = mons_is_removed(MONS_BUTTERFLY);

        REQUIRE(removed == false);
    }
}

TEST_CASE("can get names for removed monster types", "[single-file]")
{
    const auto name = mons_type_name(MONS_BUMBLEBEE, DESC_PLAIN);

    REQUIRE(name == "removed bumblebee");
}

TEST_CASE("Baba Yaga survives the destruction of her hut", "[baba-yaga]")
{
    init_monsters();
    monster witch;
    witch.type = MONS_BABA_YAGA;
    witch.set_position(coord_def(20, 20));
    define_monster(witch);
    witch.mid = 1234;
    witch.speed_increment = 42;
    witch.mname = "The bone-legged witch";
    witch.base_attitude = ATT_FRIENDLY;
    witch.god = GOD_ZIN;
    witch.damage_total = 37;
    witch.damage_friendly = 12;
    witch.props["test_identity"] = 17;
    witch.add_ench(mon_enchant(ENCH_SLOW, nullptr, 100));
    witch.add_ench(mon_enchant(ENCH_SUMMON_TIMER, nullptr, 200));
    const auto old_slow = witch.get_ench(ENCH_SLOW);
    const auto old_timer = witch.get_ench(ENCH_SUMMON_TIMER);

    REQUIRE(baba_yaga_leave_hut(witch, true));
    REQUIRE(witch.type == MONS_BABA_YAGA_EXPOSED);
    REQUIRE(witch.hit_points > 0);
    REQUIRE(witch.hit_points == witch.max_hit_points);
    REQUIRE(witch.mid == 1234);
    REQUIRE(witch.speed_increment == 42);
    REQUIRE(witch.mname == "The bone-legged witch");
    REQUIRE(witch.base_attitude == ATT_FRIENDLY);
    REQUIRE(witch.god == GOD_ZIN);
    REQUIRE(witch.damage_total == 37);
    REQUIRE(witch.damage_friendly == 12);
    REQUIRE(witch.props["test_identity"].get_int() == 17);
    REQUIRE(witch.get_ench(ENCH_SLOW).duration == old_slow.duration);
    REQUIRE(witch.get_ench(ENCH_SUMMON_TIMER).duration == old_timer.duration);
    for (const auto& spell : witch.spells)
        REQUIRE(spell.spell != SPELL_SPLINTERSPRAY);

    const int hp = witch.hit_points;
    REQUIRE_FALSE(baba_yaga_leave_hut(witch, true));
    REQUIRE(witch.hit_points == hp);
    init_mon_name_cache();
    REQUIRE(get_monster_by_name("Baba Yaga") == MONS_BABA_YAGA);
}

TEST_CASE("Lethal hut damage exposes Baba Yaga with deferred cleanup", "[baba-yaga]")
{
    init_monsters();
    monster witch;
    witch.type = MONS_BABA_YAGA;
    witch.set_position(coord_def(20, 20));
    define_monster(witch);
    witch.hit_points = 5;

    REQUIRE(witch.hurt(nullptr, 2, BEAM_MISSILE, KILLED_BY_MONSTER,
                       "", "", false) == 2);
    REQUIRE(witch.type == MONS_BABA_YAGA);
    REQUIRE(witch.hit_points == 3);
    REQUIRE(witch.hurt(nullptr, 1000, BEAM_MISSILE, KILLED_BY_MONSTER,
                       "", "", false) == 3);
    REQUIRE(witch.type == MONS_BABA_YAGA_EXPOSED);
    REQUIRE(witch.alive());

    witch.hurt(nullptr, 1000, BEAM_MISSILE, KILLED_BY_MONSTER,
               "", "", false);
    REQUIRE_FALSE(witch.alive());
}

TEST_CASE("Baba Yaga keeps her enchanted pestle when the hut breaks", "[baba-yaga]")
{
    init_monsters();
    init_properties();
    const int pestle = make_mons_weapon(MONS_BABA_YAGA, 20);
    REQUIRE(pestle != NON_ITEM);
    REQUIRE(env.item[pestle].is_type(OBJ_WEAPONS, WPN_GREAT_MACE));
    REQUIRE(env.item[pestle].plus >= 3);
    REQUIRE(env.item[pestle].plus <= 5);
    REQUIRE(item_base_name(env.item[pestle]) == "pestle");

    monster witch;
    witch.type = MONS_BABA_YAGA;
    define_monster(witch);
    witch.inv[MSLOT_WEAPON] = pestle;
    REQUIRE(baba_yaga_leave_hut(witch, true));
    REQUIRE(witch.inv[MSLOT_WEAPON] == pestle);
    REQUIRE(witch.weapon() == &env.item[pestle]);
    destroy_item(pestle);
}

TEST_CASE("mons_habitat_type returns correct habitats", "[single-file]")
{
    SECTION("Land monsters should be HT_LAND")
    {
        const auto habitat = mons_habitat_type(MONS_KOBOLD, MONS_KOBOLD);
        REQUIRE(habitat == HT_LAND);
    }

    SECTION("Lava monsters should be HT_LAVA")
    {
        const auto habitat = mons_habitat_type(MONS_LAVA_SNAKE, MONS_LAVA_SNAKE);

        REQUIRE(habitat == HT_LAVA);
    }

    SECTION("Water monsters should be HT_WATER")
    {
        const auto habitat = mons_habitat_type(MONS_ELECTRIC_EEL, MONS_ELECTRIC_EEL);

        REQUIRE(habitat == HT_WATER);
    }

    SECTION("Deep water monsters should be HT_DEEP_WATER")
    {
        const auto habitat = mons_habitat_type(MONS_KRAKEN, MONS_KRAKEN);

        REQUIRE(habitat == HT_DEEP_WATER);
    }

    SECTION("Amphibious monsters should be HT_AMPHIBIOUS")
    {
        const auto habitat = mons_habitat_type(MONS_FRILLED_LIZARD, MONS_FRILLED_LIZARD);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Amphibious (lava) monsters should be HT_AMPHIBIOUS_LAVA")
    {
        const auto habitat = mons_habitat_type(MONS_SALAMANDER, MONS_SALAMANDER);

        REQUIRE(habitat == HT_AMPHIBIOUS_LAVA);
    }

    SECTION("Zombies of amphibious monsters should still be HT_AMPHIBIOUS")
    {
        const auto habitat = mons_habitat_type(MONS_ZOMBIE, MONS_FRILLED_LIZARD);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Giants should show up as HT_AMPHIBIOUS unless explicitly told not to")
    {
        const auto habitat = mons_habitat_type(MONS_CYCLOPS, MONS_CYCLOPS);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Giants should not show up as HT_AMPHIBIOUS normally")
    {
        const auto habitat = mons_habitat_type(MONS_CYCLOPS, MONS_CYCLOPS, true);

        REQUIRE(habitat == HT_LAND);
    }

    SECTION("Amphibious giants should always show up as HT_AMPHIBIOUS")
    {
        const auto habitat = mons_habitat_type(MONS_TENTACLED_MONSTROSITY, MONS_TENTACLED_MONSTROSITY, true);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Simulacrum Salamanders should show HT_AMPHIBIOUS")
    {
        const auto habitat = mons_habitat_type(MONS_SIMULACRUM, MONS_SALAMANDER);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }
}
