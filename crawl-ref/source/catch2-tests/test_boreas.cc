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

struct BoreasFixture : MockPlayerYouTestsFixture
{
    monster* boreas;
    monster* foe;

    BoreasFixture()
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

        boreas = mons_place(mgen_data(MONS_BOREAS, BEH_FRIENDLY,
                            coord_def(25, 20), MHITNOT, MG_FORCE_PLACE));
        foe = mons_place(mgen_data(MONS_RAT, BEH_HOSTILE,
                         coord_def(27, 20), MHITYOU, MG_FORCE_PLACE));
        REQUIRE(boreas);
        REQUIRE(foe);
        foe->hit_points = foe->max_hit_points = 1000;
        boreas->foe = foe->mindex();
        boreas->target = foe->pos();
    }

    ~BoreasFixture()
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

TEST_CASE_METHOD(BoreasFixture, "Boreas has a live, cold-immune Unique definition",
                 "[boreas]")
{
    CHECK(mons_is_unique(MONS_BOREAS));
    CHECK(boreas->airborne());
    CHECK(boreas->res_cold() == 3);
    CHECK(boreas->res_fire() == -1);
    CHECK(is_valid_mon_spell(SPELL_BOREAL_GALE));
    CHECK(boreas->has_spell(SPELL_BOREAL_GALE));
    CHECK(max_mons_charge(MONS_BOREAS) == 1);
    CHECK(max_mons_charge(MONS_ORB_SPIDER) == 1);
    CHECK(spell_range(SPELL_BOREAL_GALE, boreas) == 5);
}

#ifdef USE_TILE
TEST_CASE_METHOD(BoreasFixture, "Boreas renders his sprite without a monster cache entry",
                 "[boreas][tiles]")
{
    const monster_info info(boreas);
    const tileidx_t tile = tileidx_monster(info).tile();
    CHECK(tile == TILEP_MONS_BOREAS);
    CHECK(get_mon_base_tile(MONS_BOREAS) == tile);
    // Both the description and dungeon renderers treat indices at or above
    // MCACHE_START as dynamic cache entries, falling back to a silhouette.
    CHECK(tile < TILEP_MCACHE_START);
    CHECK(mcache.register_monster(info) == 0);
    CHECK(get_tile_texture(tile) == TEX_PLAYER);
}
#endif

TEST_CASE_METHOD(BoreasFixture, "Boreal Gale warns before pushing foes and freezing clouds",
                 "[boreas]")
{
    const coord_def start = foe->pos();
    REQUIRE(try_mons_cast(*boreas, SPELL_BOREAL_GALE));
    CHECK(boreas->has_ench(ENCH_SPELL_CHARGED));
    CHECK(foe->pos() == start);
    CHECK(cold_clouds() == 0);

    REQUIRE(try_mons_cast(*boreas, SPELL_BOREAL_GALE));
    CHECK_FALSE(boreas->has_ench(ENCH_SPELL_CHARGED));
    CHECK(grid_distance(boreas->pos(), foe->pos())
          > grid_distance(boreas->pos(), start));
    CHECK(cold_clouds() > 0);
    bool carried_outwards = false;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const cloud_struct* cloud = cloud_at(*ri);
        if (!cloud || cloud->type != CLOUD_COLD)
            continue;
        CHECK(cloud->source == boreas->mid);
        CHECK(cell_see_cell(boreas->pos(), *ri, LOS_SOLID));
        carried_outwards |= grid_distance(boreas->pos(), *ri) > 3;
    }
    CHECK(carried_outwards);
}

TEST_CASE_METHOD(BoreasFixture, "Boreas does not shorten orb spiders' charge cycle",
                 "[boreas]")
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

TEST_CASE_METHOD(BoreasFixture, "Solid cover stops a charged Boreal Gale",
                 "[boreas]")
{
    REQUIRE(try_mons_cast(*boreas, SPELL_BOREAL_GALE));
    const coord_def start = foe->pos();
    for (int y = 10; y <= 40; ++y)
        env.grid(coord_def(26, y)) = DNGN_ROCK_WALL;
    invalidate_los();

    CHECK_FALSE(is_mons_cast_possible(*boreas, SPELL_BOREAL_GALE));
    CHECK_FALSE(try_mons_cast(*boreas, SPELL_BOREAL_GALE));
    // Even a forced cast must not create clouds or push through a wall.
    mons_cast(boreas, bolt(), SPELL_BOREAL_GALE, MON_SPELL_MAGICAL, false);
    CHECK(foe->pos() == start);
    CHECK(cold_clouds() == 0);
}

TEST_CASE_METHOD(BoreasFixture, "Boreas avoids allies in his gale and respects its range",
                 "[boreas]")
{
    SECTION("An ally in the cone prevents casting")
    {
        monster* ally = mons_place(mgen_data(MONS_RAT, BEH_FRIENDLY,
                                   coord_def(26, 20), MHITNOT, MG_FORCE_PLACE));
        REQUIRE(ally);
        CHECK_FALSE(is_mons_cast_possible(*boreas, SPELL_BOREAL_GALE));
        CHECK_FALSE(boreas->has_ench(ENCH_SPELL_CHARGED));
        monster_die(*ally, KILL_RESET, NON_MONSTER, true);
        CHECK(is_mons_cast_possible(*boreas, SPELL_BOREAL_GALE));
    }
    SECTION("A foe beyond five paces cannot be targeted")
    {
        foe->move_to(coord_def(31, 20));
        REQUIRE(foe->pos() == coord_def(31, 20));
        boreas->target = foe->pos();
        CHECK_FALSE(is_mons_cast_possible(*boreas, SPELL_BOREAL_GALE));
    }
}
