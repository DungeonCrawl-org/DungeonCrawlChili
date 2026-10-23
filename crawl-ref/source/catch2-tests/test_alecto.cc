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

struct AlectoFixture : MockPlayerYouTestsFixture
{
    monster* alecto;
    monster* foe;
    unwind_var<bool> testing;

    AlectoFixture() : testing(crawl_state.test, true)
    {
        init_zap_index();
        init_show_table();
        init_monsters();
        init_spell_descs();
        init_mons_spells();
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
        env.properties.clear();
        you.where_are_you = BRANCH_GEHENNA;
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

        alecto = spawn(MONS_ALECTO, coord_def(25, 20), BEH_FRIENDLY);
        foe = spawn(MONS_RAT, coord_def(28, 20), BEH_HOSTILE);
        foe->hit_points = foe->max_hit_points = 10000;
        alecto->foe = foe->mindex();
        alecto->target = foe->pos();
    }

    ~AlectoFixture()
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
};

TEST_CASE_METHOD(AlectoFixture, "Alecto has her Unique definition and immolation drop",
                 "[alecto]")
{
    CHECK(mons_is_unique(MONS_ALECTO));
    CHECK(alecto->airborne());
    CHECK(alecto->res_fire() == 3);
    CHECK(alecto->res_cold() == -1);
    CHECK(get_monster_data(MONS_ALECTO)->avg_hp_10x == 3000);
    CHECK(alecto->has_spell(SPELL_FUNERAL_TORCH));
    CHECK(alecto->has_spell(SPELL_INCITE_FURY));
    const mon_death_drop& drop = get_monster_data(MONS_ALECTO)->death_drop;
    CHECK(drop.chance == 50);
    REQUIRE(drop.option_count == 1);
    CHECK(drop.options[0].base_type == OBJ_SCROLLS);
    CHECK(drop.options[0].sub_type == SCR_IMMOLATION);
}

TEST_CASE_METHOD(AlectoFixture, "Alecto warns before throwing at the marked square",
                 "[alecto]")
{
    const int hp = foe->hit_points;
    REQUIRE(alecto_action(*alecto));
    CHECK(foe->hit_points == hp);
    CHECK(alecto->props[ALECTO_TARGET_KEY].get_coord() == coord_def(28, 20));
    CHECK(alecto->props[ALECTO_READY_KEY].get_bool());
    REQUIRE(foe->move_to(coord_def(28, 21), MV_INTERNAL));
    REQUIRE(alecto_action(*alecto));
    CHECK(foe->hit_points == hp);
    CHECK_FALSE(alecto->props[ALECTO_READY_KEY].get_bool());
    CHECK(alecto->props[ALECTO_ACTION_KEY].get_int() == ALECTO_CRY);
}

TEST_CASE_METHOD(AlectoFixture, "Alecto briefly enrages at most three existing allies",
                 "[alecto]")
{
    you.set_position(coord_def(20, 20));
    monster* allies[] = {
        spawn(MONS_ORC, coord_def(24, 21), BEH_FRIENDLY),
        spawn(MONS_ORC, coord_def(25, 22), BEH_FRIENDLY),
        spawn(MONS_ORC, coord_def(26, 22), BEH_FRIENDLY),
        spawn(MONS_ORC, coord_def(24, 22), BEH_FRIENDLY),
    };
    alecto->props[ALECTO_ACTION_KEY] = ALECTO_CRY;
    REQUIRE(alecto_action(*alecto));
    for (monster* ally : allies)
        CHECK_FALSE(ally->berserk());
    REQUIRE(alecto_action(*alecto));
    int enraged = 0;
    for (monster* ally : allies)
    {
        if (ally->berserk())
        {
            ++enraged;
            CHECK(ally->get_ench(ENCH_BERSERK).duration <= 70);
        }
    }
    CHECK(enraged == 3);
    CHECK_FALSE(alecto->berserk());
    CHECK_FALSE(foe->berserk());
}

TEST_CASE_METHOD(AlectoFixture, "Silence stops a prepared fury cry",
                 "[alecto]")
{
    you.set_position(coord_def(20, 20));
    monster* ally = spawn(MONS_ORC, coord_def(24, 21), BEH_FRIENDLY);
    alecto->props[ALECTO_ACTION_KEY] = ALECTO_CRY;
    REQUIRE(alecto_action(*alecto));
    alecto->add_ench(ENCH_SILENCE);
    invalidate_agrid();
    REQUIRE(alecto->is_silenced());
    REQUIRE(alecto_action(*alecto));
    CHECK_FALSE(ally->berserk());
}

#ifdef USE_TILE
TEST_CASE_METHOD(AlectoFixture, "Alecto uses a static tile in the monster atlas",
                 "[alecto][tiles]")
{
    const monster_info info(alecto);
    const tileidx_t tile = tileidx_monster(info).tile();
    CHECK(tile == TILEP_MONS_ALECTO);
    CHECK(tile < TILEP_MCACHE_START);
    CHECK(mcache.register_monster(info) == 0);
    CHECK(get_tile_texture(tile) == TEX_PLAYER);
}
#endif
