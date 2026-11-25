#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "act-iter.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "env.h"
#include "losglobal.h"
#include "mgen-data.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "spl-util.h"
#include "test_player_fixture.h"

#ifdef USE_TILE
#include "mon-info.h"
#include "rltiles/tiledef-player.h"
#include "tilemcache.h"
#include "tilepick.h"
#endif

struct AquiloFixture : MockPlayerYouTestsFixture
{
    monster* aquilo;
    monster* foe;

    AquiloFixture()
    {
        init_show_table();
        init_monsters();
        init_spell_descs();
        init_mons_spells();
        delete_all_clouds();
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
        env.properties.clear();
        you.where_are_you = BRANCH_COCYTUS;
        you.depth = 2;
        for (rectangle_iterator ri(0); ri; ++ri)
        {
            env.grid(*ri) = DNGN_ROCK_WALL;
            env.mgrid(*ri) = NON_MONSTER;
        }
        for (int x = 10; x <= 50; ++x)
            for (int y = 10; y <= 40; ++y)
                env.grid(coord_def(x, y)) = DNGN_FLOOR;
        invalidate_los();
        you.set_position(coord_def(15, 20));

        aquilo = mons_place(mgen_data(MONS_AQUILO, BEH_FRIENDLY,
                            coord_def(25, 20), MHITNOT, MG_FORCE_PLACE));
        foe = mons_place(mgen_data(MONS_RAT, BEH_HOSTILE,
                         coord_def(27, 20), MHITYOU, MG_FORCE_PLACE));
        REQUIRE(aquilo);
        REQUIRE(foe);
        foe->hit_points = foe->max_hit_points = 1000;
        aquilo->foe = foe->mindex();
        aquilo->target = foe->pos();
    }

    ~AquiloFixture()
    {
        delete_all_clouds();
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
    }

    int cold_clouds() const
    {
        int count = 0;
        for (rectangle_iterator ri(0); ri; ++ri)
            if (cloud_at(*ri) && cloud_at(*ri)->type == CLOUD_COLD)
                ++count;
        return count;
    }
};

TEST_CASE_METHOD(AquiloFixture, "Aquilo has a live, cold-immune Unique definition",
                 "[aquilo]")
{
    CHECK(mons_is_unique(MONS_AQUILO));
    CHECK(aquilo->airborne());
    CHECK(aquilo->res_cold() == 3);
    CHECK(aquilo->res_fire() == -1);
    CHECK(is_valid_mon_spell(SPELL_BOREAL_GALE));
    CHECK(aquilo->has_spell(SPELL_BOREAL_GALE));
    CHECK(max_mons_charge(MONS_AQUILO) == 1);
    CHECK(max_mons_charge(MONS_ORB_SPIDER) == 1);
    CHECK(spell_range(SPELL_BOREAL_GALE, aquilo) == 5);
}

TEST_CASE_METHOD(AquiloFixture, "Aquilo and Boreas have distinct definitions and spellbooks",
                 "[aquilo][boreas]")
{
    monster* boreas = mons_place(mgen_data(MONS_BOREAS, BEH_HOSTILE,
                                 coord_def(35, 20), MHITYOU, MG_FORCE_PLACE));
    REQUIRE(boreas);
    CHECK(aquilo->type != boreas->type);
    CHECK(mons_is_unique(boreas->type));
    CHECK(aquilo->name(DESC_PLAIN, true) == "Aquilo");
    CHECK(boreas->name(DESC_PLAIN, true) == "Boreas");
    CHECK(mons_genus(boreas->type) == MONS_CENTAUR);
    CHECK(get_monster_data(MONS_AQUILO)->avg_hp_10x == 3000);
    CHECK(get_monster_data(MONS_BOREAS)->avg_hp_10x == 1300);
    CHECK(aquilo->has_spell(SPELL_BOREAL_GALE));
    CHECK_FALSE(aquilo->has_spell(SPELL_STAMPEDE));
    CHECK(boreas->has_spell(SPELL_STAMPEDE));
    CHECK(boreas->has_spell(SPELL_SLEETSTRIKE));
    CHECK(boreas->has_spell(SPELL_DEFLECT_MISSILES));
    CHECK_FALSE(boreas->has_spell(SPELL_BOREAL_GALE));
#ifdef USE_TILE
    CHECK(get_mon_base_tile(MONS_AQUILO) != get_mon_base_tile(MONS_BOREAS));
    CHECK(get_mon_base_tile(MONS_BOREAS) < TILEP_MCACHE_START);
#endif
}

#ifdef USE_TILE
TEST_CASE_METHOD(AquiloFixture, "Aquilo renders his sprite without a monster cache entry",
                 "[aquilo][tiles]")
{
    const monster_info info(aquilo);
    const tileidx_t tile = tileidx_monster(info).tile();
    CHECK(tile == TILEP_MONS_AQUILO);
    CHECK(get_mon_base_tile(MONS_AQUILO) == tile);
    // Both the description and dungeon renderers treat indices at or above
    // MCACHE_START as dynamic cache entries, falling back to a silhouette.
    CHECK(tile < TILEP_MCACHE_START);
    CHECK(mcache.register_monster(info) == 0);
    CHECK(get_tile_texture(tile) == TEX_PLAYER);
}
#endif

TEST_CASE_METHOD(AquiloFixture, "Boreal Gale warns before pushing foes and freezing clouds",
                 "[aquilo]")
{
    const coord_def start = foe->pos();
    REQUIRE(try_mons_cast(*aquilo, SPELL_BOREAL_GALE));
    CHECK(aquilo->has_ench(ENCH_SPELL_CHARGED));
    CHECK(foe->pos() == start);
    CHECK(cold_clouds() == 0);

    REQUIRE(try_mons_cast(*aquilo, SPELL_BOREAL_GALE));
    CHECK_FALSE(aquilo->has_ench(ENCH_SPELL_CHARGED));
    CHECK(grid_distance(aquilo->pos(), foe->pos())
          > grid_distance(aquilo->pos(), start));
    CHECK(cold_clouds() > 0);
    bool carried_outwards = false;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const cloud_struct* cloud = cloud_at(*ri);
        if (!cloud || cloud->type != CLOUD_COLD)
            continue;
        CHECK(cloud->source == aquilo->mid);
        CHECK(cell_see_cell(aquilo->pos(), *ri, LOS_SOLID));
        carried_outwards |= grid_distance(aquilo->pos(), *ri) > 3;
    }
    CHECK(carried_outwards);
}

TEST_CASE_METHOD(AquiloFixture, "Aquilo does not shorten orb spiders' charge cycle",
                 "[aquilo]")
{
    monster* spider = mons_place(mgen_data(MONS_ORB_SPIDER, BEH_HOSTILE,
                                 coord_def(30, 25), MHITYOU, MG_FORCE_PLACE));
    REQUIRE(spider);
    mons_cast(spider, bolt(), SPELL_IOOD, MON_SPELL_MAGICAL, false);
    REQUIRE(spider->has_ench(ENCH_SPELL_CHARGED));
    CHECK(spider->get_ench(ENCH_SPELL_CHARGED).degree == 0);
    mons_cast(spider, bolt(), SPELL_IOOD, MON_SPELL_MAGICAL, false);
    REQUIRE(spider->has_ench(ENCH_SPELL_CHARGED));
    CHECK(spider->get_ench(ENCH_SPELL_CHARGED).degree == 1);
}

TEST_CASE_METHOD(AquiloFixture, "Solid cover stops a charged Boreal Gale",
                 "[aquilo]")
{
    REQUIRE(try_mons_cast(*aquilo, SPELL_BOREAL_GALE));
    const coord_def start = foe->pos();
    for (int y = 10; y <= 40; ++y)
        env.grid(coord_def(26, y)) = DNGN_ROCK_WALL;
    invalidate_los();

    CHECK_FALSE(is_mons_cast_possible(*aquilo, SPELL_BOREAL_GALE));
    CHECK_FALSE(try_mons_cast(*aquilo, SPELL_BOREAL_GALE));
    // Even a forced cast must not create clouds or push through a wall.
    mons_cast(aquilo, bolt(), SPELL_BOREAL_GALE, MON_SPELL_MAGICAL, false);
    CHECK(foe->pos() == start);
    CHECK(cold_clouds() == 0);
}

TEST_CASE_METHOD(AquiloFixture, "Aquilo avoids allies in his gale and respects its range",
                 "[aquilo]")
{
    SECTION("An ally in the cone prevents casting")
    {
        monster* ally = mons_place(mgen_data(MONS_RAT, BEH_FRIENDLY,
                                   coord_def(26, 20), MHITNOT, MG_FORCE_PLACE));
        REQUIRE(ally);
        CHECK_FALSE(is_mons_cast_possible(*aquilo, SPELL_BOREAL_GALE));
        CHECK_FALSE(aquilo->has_ench(ENCH_SPELL_CHARGED));
        monster_die(*ally, KILL_RESET, NON_MONSTER, true);
        CHECK(is_mons_cast_possible(*aquilo, SPELL_BOREAL_GALE));
    }
    SECTION("A foe beyond five paces cannot be targeted")
    {
        foe->move_to(coord_def(31, 20));
        REQUIRE(foe->pos() == coord_def(31, 20));
        aquilo->target = foe->pos();
        CHECK_FALSE(is_mons_cast_possible(*aquilo, SPELL_BOREAL_GALE));
    }
}
