#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "act-iter.h"
#include "areas.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "env.h"
#include "losglobal.h"
#include "mgen-data.h"
#include "mon-abil.h"
#include "mon-act.h"
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

struct CerberusFixture : MockPlayerYouTestsFixture
{
    monster* cerberus;
    monster* foe;
    unwind_var<bool> testing;

    CerberusFixture() : testing(crawl_state.test, true)
    {
        init_zap_index();
        init_show_table();
        init_monsters();
        init_spell_descs();
        init_mons_spells();
        delete_all_clouds();
        for (monster_iterator mi; mi; ++mi)
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
        env.properties.clear();
        you.where_are_you = BRANCH_TARTARUS;
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
        // These combat tests have no Local Tiles window. Keep viewwindow()
        // from drawing; all attack targets in this fixture are monsters.
        you.duration[DUR_TIME_STEP] = 1;

        cerberus = spawn(MONS_CERBERUS, coord_def(25, 20), BEH_FRIENDLY);
        foe = spawn(MONS_RAT, coord_def(26, 20), BEH_HOSTILE);
        foe->hit_points = foe->max_hit_points = 10000;
        cerberus->foe = foe->mindex();
        cerberus->target = foe->pos();
    }

    ~CerberusFixture()
    {
        // The mock level has no vault metadata for visible death updates.
        you.set_position(coord_def(1, 1));
        delete_all_clouds();
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

    void select_head(int head, bool ready = false)
    {
        cerberus->props[CERBERUS_HEAD_KEY] = head;
        cerberus->props[CERBERUS_READY_KEY] = ready;
    }

    bool has_attribute(const char* text) const
    {
        const auto attributes = monster_info(cerberus).attributes();
        return find(attributes.begin(), attributes.end(), text) != attributes.end();
    }
};

TEST_CASE_METHOD(CerberusFixture, "Cerberus is a demonic Unique with a summoning drop",
                 "[cerberus]")
{
    CHECK(mons_is_unique(MONS_CERBERUS));
    CHECK(mons_genus(MONS_CERBERUS) == MONS_HOUND);
    CHECK(!!(cerberus->holiness() & MH_DEMONIC));
    CHECK(cerberus->res_negative_energy() == 3);
    CHECK(get_monster_data(MONS_CERBERUS)->avg_hp_10x == 3600);
    CHECK(mons_base_speed(*cerberus) == 10);
    CHECK(is_valid_mon_spell(SPELL_GRAVE_HOWL));
    CHECK(is_valid_mon_spell(SPELL_STYGIAN_BREATH));
    CHECK(spell_range(SPELL_STYGIAN_BREATH, cerberus) == 5);
    const mon_death_drop& drop = get_monster_data(MONS_CERBERUS)->death_drop;
    CHECK(drop.chance == 50);
    REQUIRE(drop.option_count == 1);
    CHECK(drop.options[0].base_type == OBJ_SCROLLS);
    CHECK(drop.options[0].sub_type == SCR_SUMMONING);
}

TEST_CASE_METHOD(CerberusFixture, "Each head warns for an action before its turn",
                 "[cerberus]")
{
    CHECK(has_attribute("next head: crushing bite"));
    for (int head = CERBERUS_BITE; head < NUM_CERBERUS_HEADS; ++head)
    {
        const int hp = foe->hit_points;
        const int energy = cerberus->speed_increment;
        REQUIRE(cerberus_action(*cerberus));
        CHECK(cerberus->speed_increment < energy);
        CHECK(foe->hit_points == hp);
        CHECK(cerberus->props[CERBERUS_READY_KEY].get_bool());
        CHECK(cerberus->props[CERBERUS_HEAD_KEY].get_int() == head);
        REQUIRE(cerberus_action(*cerberus));
        CHECK_FALSE(cerberus->props[CERBERUS_READY_KEY].get_bool());
        CHECK(cerberus->props[CERBERUS_HEAD_KEY].get_int()
              == (head + 1) % NUM_CERBERUS_HEADS);
    }
    CHECK(has_attribute("next head: crushing bite"));
}

TEST_CASE_METHOD(CerberusFixture, "Normal monster turns respect Cerberus's preparation action",
                 "[cerberus]")
{
    // Friendly AI only engages foes that the player can see.
    you.set_position(coord_def(20, 20));
    select_head(CERBERUS_BREATH);
    const int hp = foe->hit_points;
    cerberus->speed_increment = 100;
    handle_monster_move(cerberus);
    CHECK(foe->hit_points == hp);
    CHECK(has_attribute("preparing: draining breath"));
    CHECK(cerberus->props[CERBERUS_HEAD_KEY].get_int() == CERBERUS_BREATH);
}

TEST_CASE_METHOD(CerberusFixture, "Stygian Breath damages a foe after its warning",
                 "[cerberus]")
{
    foe->add_ench(ENCH_PARALYSIS);
    const int hp = foe->hit_points;
    select_head(CERBERUS_BREATH);
    REQUIRE(cerberus_action(*cerberus));
    CHECK(foe->hit_points == hp);
    REQUIRE(cerberus_action(*cerberus));
    CHECK(foe->hit_points < hp);
    CHECK(cerberus->props[CERBERUS_HEAD_KEY].get_int() == CERBERUS_HOWL);
}

TEST_CASE_METHOD(CerberusFixture, "Cerberus can use his breathing head against a distant foe",
                 "[cerberus]")
{
    REQUIRE(foe->move_to(coord_def(28, 20)));
    CHECK_FALSE(cerberus_action(*cerberus));
    CHECK(has_attribute("next head: draining breath"));
    REQUIRE(cerberus_action(*cerberus));
    CHECK(has_attribute("preparing: draining breath"));
}

TEST_CASE_METHOD(CerberusFixture, "Retreat and walls prevent Cerberus's prepared attacks",
                 "[cerberus]")
{
    bool keeps_warning = true;
    SECTION("Bites need an adjacent foe")
    {
        REQUIRE(cerberus_action(*cerberus));
        REQUIRE(foe->move_to(coord_def(27, 20)));
        keeps_warning = false;
    }
    SECTION("Breath cannot reach beyond five paces")
    {
        select_head(CERBERUS_BREATH);
        REQUIRE(cerberus_action(*cerberus));
        REQUIRE(has_attribute("preparing: draining breath"));
        REQUIRE(foe->move_to(coord_def(31, 20)));
    }
    SECTION("A wall blocks the prepared breath")
    {
        select_head(CERBERUS_BREATH);
        REQUIRE(cerberus_action(*cerberus));
        REQUIRE(foe->move_to(coord_def(27, 20)));
        for (int y = 10; y <= 40; ++y)
            env.grid(coord_def(26, y)) = DNGN_ROCK_WALL;
        invalidate_los();
    }
    const int hp = foe->hit_points;
    const int energy = cerberus->speed_increment;
    CHECK_FALSE(cerberus_action(*cerberus));
    CHECK(cerberus->props[CERBERUS_READY_KEY].get_bool() == keeps_warning);
    CHECK(foe->hit_points == hp);
    CHECK(cerberus->speed_increment == energy);
}

TEST_CASE_METHOD(CerberusFixture, "Cerberus will not breathe through an ally",
                 "[cerberus]")
{
    REQUIRE(foe->move_to(coord_def(28, 20)));
    monster* ally = spawn(MONS_RAT, coord_def(26, 20), BEH_FRIENDLY);
    select_head(CERBERUS_BREATH);
    REQUIRE(cerberus_action(*cerberus));
    const int ally_hp = ally->hit_points;
    const int foe_hp = foe->hit_points;
    REQUIRE(cerberus_action(*cerberus));
    CHECK(ally->hit_points == ally_hp);
    CHECK(foe->hit_points == foe_hp);
    CHECK(cerberus->props[CERBERUS_HEAD_KEY].get_int() == CERBERUS_HOWL);
}

TEST_CASE_METHOD(CerberusFixture, "Grave Howl rallies existing allied undead only",
                 "[cerberus]")
{
    you.set_position(coord_def(20, 20));
    monster* ally = spawn(MONS_WIGHT, coord_def(25, 23), BEH_FRIENDLY);
    ally->foe = MHITNOT;
    ally->behaviour = BEH_SLEEP;
    monster* living = spawn(MONS_HOUND, coord_def(23, 23), BEH_FRIENDLY);
    living->foe = MHITNOT;
    living->behaviour = BEH_SLEEP;
    monster* enemy = spawn(MONS_WIGHT, coord_def(27, 23), BEH_HOSTILE);
    enemy->foe = MHITNOT;
    enemy->behaviour = BEH_SLEEP;
    monster* distant = spawn(MONS_WIGHT, coord_def(40, 23), BEH_FRIENDLY);
    distant->foe = MHITNOT;
    distant->behaviour = BEH_SLEEP;

    const int before = count_if(env.mons.begin(), env.mons.end(),
                               [](const monster& mon) { return mon.alive(); });
    select_head(CERBERUS_HOWL);
    REQUIRE(cerberus_action(*cerberus));
    CHECK(ally->asleep());
    REQUIRE(cerberus_action(*cerberus));
    CHECK_FALSE(ally->asleep());
    CHECK(ally->foe == foe->mindex());
    CHECK(ally->target == foe->pos());
    CHECK(living->foe == MHITNOT);
    CHECK(enemy->foe == MHITNOT);
    CHECK(enemy->attitude() == ATT_HOSTILE);
    CHECK(distant->asleep());
    CHECK(count_if(env.mons.begin(), env.mons.end(),
                   [](const monster& mon) { return mon.alive(); }) == before);
}

TEST_CASE_METHOD(CerberusFixture, "Silencing Cerberus stops the howl without stalling his cycle",
                 "[cerberus]")
{
    monster* ally = spawn(MONS_WIGHT, coord_def(25, 23), BEH_FRIENDLY);
    ally->foe = MHITNOT;
    ally->behaviour = BEH_SLEEP;
    SECTION("Mute")
    {
        cerberus->add_ench(ENCH_MUTE);
    }
    SECTION("Silence aura")
    {
        cerberus->add_ench(ENCH_SILENCE);
        invalidate_agrid();
    }
    REQUIRE(cerberus->is_silenced());
    select_head(CERBERUS_HOWL);
    REQUIRE(cerberus_action(*cerberus));
    REQUIRE(cerberus_action(*cerberus));
    CHECK(ally->asleep());
    CHECK(cerberus->props[CERBERUS_HEAD_KEY].get_int() == CERBERUS_BITE);
}

TEST_CASE_METHOD(CerberusFixture, "An incapacitated or friendly Cerberus cannot attack",
                 "[cerberus]")
{
    SECTION("Paralysis")
    {
        cerberus->add_ench(ENCH_PARALYSIS);
    }
    SECTION("Confusion")
    {
        cerberus->add_ench(ENCH_CONFUSION);
    }
    SECTION("An allied foe")
    {
        foe->base_attitude = ATT_FRIENDLY;
    }
    const int energy = cerberus->speed_increment;
    CHECK_FALSE(cerberus_action(*cerberus));
    CHECK(cerberus->speed_increment == energy);
}

#ifdef USE_TILE
TEST_CASE_METHOD(CerberusFixture, "Cerberus uses his original sprite below the cache boundary",
                 "[cerberus][tiles]")
{
    const monster_info info(cerberus);
    const tileidx_t tile = tileidx_monster(info).tile();
    CHECK(tile == TILEP_MONS_CERBERUS);
    CHECK(get_mon_base_tile(MONS_CERBERUS) == tile);
    CHECK(tile < TILEP_MCACHE_START);
    CHECK(mcache.register_monster(info) == 0);
    CHECK(get_tile_texture(tile) == TEX_PLAYER);
}
#endif
