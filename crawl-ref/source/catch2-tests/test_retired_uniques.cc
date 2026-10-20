#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include <set>

#include "act-iter.h"
#include "branch.h"
#include "coordit.h"
#include "env.h"
#include "feature.h"
#include "item-prop.h"
#include "items.h"
#include "makeitem.h"
#include "mgen-data.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "test_player_fixture.h"

static const vector<monster_type> retired_uniques = {
    MONS_MICHAEL, MONS_DUANE, MONS_FRANCIS, MONS_NORBERT, MONS_TERPSICHORE,
    MONS_JOZEF, MONS_PAN, MONS_LAMIA, MONS_MAUD
};

struct RetiredUniqueFixture : MockPlayerYouTestsFixture
{
    RetiredUniqueFixture()
    {
        init_show_table();
        init_monsters();
        init_spell_descs();
        init_mons_spells();
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
        for (int i = 0; i < MAX_ITEMS; ++i)
            env.item[i].clear();
        env.properties.clear();
        you.where_are_you = BRANCH_NECROPOLIS;
        you.depth = 1;
        for (rectangle_iterator ri(0); ri; ++ri)
        {
            env.grid(*ri) = DNGN_ROCK_WALL;
            env.igrid(*ri) = NON_ITEM;
            env.mgrid(*ri) = NON_MONSTER;
        }
        for (int x = 10; x <= 50; ++x)
            for (int y = 10; y <= 40; ++y)
                env.grid(coord_def(x, y)) = DNGN_FLOOR;
        for (int y = 10; y <= 40; ++y)
            env.grid(coord_def(44, y)) = DNGN_ROCK_WALL;
        env.grid(coord_def(44, 20)) = DNGN_RUNED_DOOR;
        env.grid(coord_def(10, 20)) = DNGN_EXIT_NECROPOLIS;
        you.set_position(coord_def(11, 20));
    }

    ~RetiredUniqueFixture()
    {
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
    }
};

TEST_CASE_METHOD(RetiredUniqueFixture, "All nine retired uniques have live combat definitions", "[retired-uniques]")
{
    for (monster_type type : retired_uniques)
    {
        CAPTURE(type);
        REQUIRE_FALSE(mons_is_removed(type));
        CHECK(mons_is_unique(type));
        const monsterentry* data = get_monster_data(type);
        REQUIRE(data);
        CHECK(data->HD > 0);
        monster mon;
        mon.type = type;
        mon.set_position(coord_def(25, 20));
        define_monster(mon);
        CHECK(mon.max_hit_points > 0);
        for (const mon_spell_slot& slot : mon.spells)
        {
            CAPTURE(slot.spell);
            CHECK(is_valid_mon_spell(slot.spell));
        }
    }
    CHECK(MONS_MICHAEL == MONS_POOKA + 1);
    CHECK(MONS_MAUD < MONS_POOKA);
    CHECK(MONS_LAMIA < MONS_POOKA);
    CHECK(MONS_PAN < MONS_POOKA);
}

TEST_CASE_METHOD(RetiredUniqueFixture, "Necropolis pools cover all retired uniques at appropriate tiers", "[retired-uniques]")
{
    std::set<monster_type> all;
    for (int tier = 0; tier <= 4; ++tier)
    {
        auto pool = necropolis_retired_unique_pool(tier);
        REQUIRE_FALSE(pool.empty());
        for (monster_type type : pool)
        {
            all.insert(type);
            if (type == MONS_PAN || type == MONS_LAMIA)
                CHECK(tier == 4);
            if (type == MONS_TERPSICHORE || type == MONS_DUANE)
                CHECK(tier >= 3);
        }
    }
    CHECK(all == std::set<monster_type>(retired_uniques.begin(), retired_uniques.end()));
    CHECK(necropolis_retired_unique_pool(0) == vector<monster_type>{MONS_MICHAEL});
}

TEST_CASE_METHOD(RetiredUniqueFixture, "One awake unique spawns outside sealed Necropolis vaults", "[retired-uniques]")
{
    monster* mon = place_necropolis_retired_unique(0);
    REQUIRE(mon);
    CHECK(mon->type == MONS_MICHAEL);
    CHECK(mon->behaviour == BEH_WANDER);
    CHECK(mon->foe == MHITNOT);
    CHECK(mon->hostile());
    CHECK(mon->pos().x < 44);
    CHECK(grid_distance(mon->pos(), coord_def(10, 20)) > LOS_RADIUS);
    CHECK(you.unique_creatures[MONS_MICHAEL]);
    CHECK_FALSE(place_necropolis_retired_unique(4));
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
        if (mons_is_unique(mi->type))
            ++count;
    CHECK(count == 1);
}

TEST_CASE_METHOD(RetiredUniqueFixture, "Restored uniques stay out of normal branches and never repeat", "[retired-uniques]")
{
    SECTION("An already generated unique is excluded")
    {
        you.unique_creatures.set(MONS_MICHAEL);
        CHECK_FALSE(place_necropolis_retired_unique(0));
    }
    SECTION("The placement hook is Necropolis-only")
    {
        you.where_are_you = BRANCH_DUNGEON;
        CHECK_FALSE(place_necropolis_retired_unique(0));
        CHECK_FALSE(you.unique_creatures[MONS_MICHAEL]);
    }
}

TEST_CASE_METHOD(RetiredUniqueFixture, "Every restored unique can be created with its equipment", "[retired-uniques]")
{
    for (monster_type type : retired_uniques)
    {
        CAPTURE(type);
        monster* mon = mons_place(mgen_data(type, BEH_WANDER,
                                 coord_def(25, 20), MHITNOT, MG_FORCE_PLACE));
        REQUIRE(mon);
        if (type != MONS_TERPSICHORE)
            CHECK(mon->weapon());
        if (type == MONS_PAN)
            CHECK(is_range_weapon(*mon->weapon()));
        monster_die(*mon, KILL_RESET, NON_MONSTER, true);
    }
}

TEST_CASE_METHOD(RetiredUniqueFixture, "Pan's archers stay outside sealed vaults", "[retired-uniques]")
{
    for (monster_type type : retired_uniques)
        if (type != MONS_PAN)
            you.unique_creatures.set(type);
    monster* pan = place_necropolis_retired_unique(4);
    REQUIRE(pan);
    REQUIRE(pan->type == MONS_PAN);
    int archers = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type != MONS_FAUN)
            continue;
        ++archers;
        CHECK(mi->pos().x < 44);
        CHECK(grid_distance(mi->pos(), pan->pos()) <= 3);
        CHECK(mi->behaviour == BEH_WANDER);
    }
    CHECK(archers == 2);
}

TEST_CASE_METHOD(RetiredUniqueFixture, "Terpsichore animates floor weapons without duplicating or destroying them", "[retired-uniques]")
{
    monster* caster = mons_place(mgen_data(MONS_TERPSICHORE, BEH_WANDER,
                               coord_def(25, 20), MHITNOT, MG_FORCE_PLACE));
    REQUIRE(caster);
    for (int i = 0; i < 3; ++i)
    {
        int item = items(false, OBJ_WEAPONS, WPN_LONG_SWORD, 1);
        REQUIRE(item != NON_ITEM);
        move_item_to_grid(&item, coord_def(25 + i, 21));
    }
    REQUIRE(terpsichore_animate_weapons(*caster) == 2);
    CHECK(count_summons(caster, SPELL_SHEZAS_DANCE) == 2);
    CHECK(terpsichore_animate_weapons(*caster) == 0);

    vector<monster*> dancers;
    for (monster_iterator mi; mi; ++mi)
        if (mi->type == MONS_DANCING_WEAPON)
            dancers.push_back(*mi);
    REQUIRE(dancers.size() == 2);
    SECTION("Destroyed weapons return to the floor")
    {
        for (monster* dancer : dancers)
            monster_die(*dancer, KILL_MON, NON_MONSTER, true);
    }
    SECTION("Expired weapons return to the floor")
    {
        for (monster* dancer : dancers)
            monster_die(*dancer, KILL_TIMEOUT, NON_MONSTER, true);
    }
    int weapons = 0;
    for (int i = 0; i < MAX_ITEMS; ++i)
        if (env.item[i].is_type(OBJ_WEAPONS, WPN_LONG_SWORD))
            ++weapons;
    CHECK(weapons == 3);
}

TEST_CASE_METHOD(RetiredUniqueFixture, "Terpsichore can defend herself in an empty room", "[retired-uniques]")
{
    monster* caster = mons_place(mgen_data(MONS_TERPSICHORE, BEH_WANDER,
                               coord_def(25, 20), MHITNOT, MG_FORCE_PLACE));
    REQUIRE(caster);
    CHECK(terpsichore_animate_weapons(*caster) == 1);
    CHECK(count_summons(caster, SPELL_SHEZAS_DANCE) == 1);
}
