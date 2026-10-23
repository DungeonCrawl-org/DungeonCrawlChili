#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "act-iter.h"
#include "areas.h"
#include "coordit.h"
#include "env.h"
#include "losglobal.h"
#include "mgen-data.h"
#include "mon-abil.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-info.h"
#include "mon-place.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "spl-util.h"
#include "state.h"
#include "test_player_fixture.h"
#include "unwind.h"

#ifdef USE_TILE
#include "rltiles/tiledef-player.h"
#include "tilemcache.h"
#include "tilepick.h"
#endif

struct MinosFixture : MockPlayerYouTestsFixture
{
    monster* minos;
    monster* foe;
    unwind_var<bool> testing;

    MinosFixture() : testing(crawl_state.test, true)
    {
        init_zap_index();
        init_show_table();
        init_monsters();
        init_spell_descs();
        init_mons_spells();
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
        env.properties.clear();
        you.where_are_you = BRANCH_DIS;
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
        invalidate_agrid();
        you.set_position(coord_def(15, 20));
        you.duration[DUR_TIME_STEP] = 1;

        minos = spawn(MONS_MINOS, coord_def(25, 20), BEH_FRIENDLY);
        foe = spawn(MONS_RAT, coord_def(27, 20), BEH_HOSTILE);
        foe->hit_points = foe->max_hit_points = 10000;
        minos->foe = foe->mindex();
        minos->target = foe->pos();
    }

    ~MinosFixture()
    {
        you.set_position(coord_def(1, 1));
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
        invalidate_agrid();
    }

    monster* spawn(monster_type type, coord_def pos, beh_type behaviour)
    {
        monster* mon = mons_place(mgen_data(type, behaviour, pos,
                                           MHITNOT, MG_FORCE_PLACE));
        REQUIRE(mon);
        return mon;
    }

    int bailiff_count() const
    {
        int count = 0;
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_WAR_GARGOYLE
                && mi->was_created_by(*minos, SPELL_CALL_BAILIFFS))
            {
                ++count;
            }
        }
        return count;
    }
};

TEST_CASE_METHOD(MinosFixture, "Minos has his Unique definition and ring drop",
                 "[minos]")
{
    CHECK(mons_is_unique(MONS_MINOS));
    CHECK(get_monster_data(MONS_MINOS)->avg_hp_10x == 3200);
    CHECK(minos->has_spell(SPELL_CALL_BAILIFFS));
    const mon_death_drop& drop = get_monster_data(MONS_MINOS)->death_drop;
    CHECK(drop.chance == 50);
    REQUIRE(drop.option_count == 1);
    CHECK(drop.options[0].base_type == OBJ_JEWELLERY);
    CHECK(drop.options[0].sub_type == RING_WILLPOWER);
}

TEST_CASE_METHOD(MinosFixture, "Minos warns before aiming at a fixed square",
                 "[minos]")
{
    const int hp = foe->hit_points;
    REQUIRE(minos_action(*minos));
    CHECK(foe->hit_points == hp);
    CHECK(minos->props[MINOS_READY_KEY].get_bool());
    CHECK(minos->props[MINOS_TARGET_KEY].get_coord() == coord_def(27, 20));
    REQUIRE(foe->move_to(coord_def(27, 21), MV_INTERNAL));
    REQUIRE(minos_action(*minos));
    CHECK(foe->hit_points == hp);
    CHECK_FALSE(minos->props[MINOS_READY_KEY].get_bool());
    CHECK(minos->props[MINOS_VERDICT_KEY].get_int() == MINOS_BIND_THE_GUILTY);
}

TEST_CASE_METHOD(MinosFixture, "Minos has at most two temporary bailiffs",
                 "[minos]")
{
    minos->props[MINOS_VERDICT_KEY] = MINOS_CALL_BAILIFFS;
    REQUIRE(minos_action(*minos));
    CHECK(bailiff_count() == 0);
    REQUIRE(minos_action(*minos));
    CHECK(bailiff_count() == 2);
    minos_call_bailiffs(*minos);
    CHECK(bailiff_count() == 2);
}

#ifdef USE_TILE
TEST_CASE_METHOD(MinosFixture, "Minos uses a static tile in the monster atlas",
                 "[minos][tiles]")
{
    const monster_info info(minos);
    const tileidx_t tile = tileidx_monster(info).tile();
    CHECK(tile == TILEP_MONS_MINOS);
    CHECK(tile < TILEP_MCACHE_START);
    CHECK(mcache.register_monster(info) == 0);
    CHECK(get_tile_texture(tile) == TEX_PLAYER);
}
#endif
