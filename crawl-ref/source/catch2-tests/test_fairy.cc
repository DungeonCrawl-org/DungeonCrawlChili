#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "env.h"
#include "feature.h"
#include "player.h"
#include "player-equip.h"
#include "species.h"
#include "spl-util.h"
#include "terrain.h"
#include "test_player_fixture.h"
#include "transform.h"

struct FairyFixture : MockPlayerYouTestsFixture
{
    FairyFixture()
    {
        init_show_table();
        init_spell_descs();
        you.species = SP_FAIRY;
        give_basic_mutations(SP_FAIRY);
        env.grid(you.pos()) = DNGN_FLOOR;
        you.props.erase(EMERGENCY_FLIGHT_KEY);
        you.equipment.update();
    }
};

TEST_CASE_METHOD(FairyFixture, "Fairies retain BCrawl's identity and equipment restrictions", "[fairy]")
{
    CHECK(species::is_starting_species(SP_FAIRY));
    CHECK(species::name(SP_FAIRY) == "Fairy");
    CHECK(string(species::get_abbrev(SP_FAIRY)) == "Fr");
    CHECK(species::get_hp_modifier(SP_FAIRY) == -1);
    CHECK(you.body_size(PSIZE_BODY, true) == SIZE_SMALL);
    CHECK(get_player_equip_slot_count(SLOT_BODY_ARMOUR) == 0);
    for (equipment_slot slot : {SLOT_WEAPON, SLOT_OFFHAND, SLOT_HELMET,
                               SLOT_GLOVES, SLOT_BOOTS, SLOT_CLOAK, SLOT_AMULET})
    {
        CHECK(get_player_equip_slot_count(slot) == 1);
    }
    CHECK(get_player_equip_slot_count(SLOT_RING) == 2);
    CHECK(player_movement_speed(false, false, 100) == 1000);
}

TEST_CASE_METHOD(FairyFixture, "Fairy scales and flight work at every experience level", "[fairy]")
{
    for (int xl = 1; xl <= 27; ++xl)
    {
        CAPTURE(xl);
        you.experience_level = xl;
        CHECK(you.racial_ac(false) == 300 + 100 * xl / 3);
        CHECK(player_displayed_shield_class() == 3 + xl / 3);
        CHECK(you.racial_permanent_flight());
        CHECK(you.permanent_flight(false));
        CHECK(you.airborne());
        CHECK_FALSE(is_feat_dangerous(DNGN_DEEP_WATER, true));
        CHECK_FALSE(is_feat_dangerous(DNGN_LAVA, true));
    }
    you.form = transformation::pig;
    CHECK(you.racial_ac(true) == 0);
    CHECK(player_displayed_shield_class() == 0);
    CHECK_FALSE(you.racial_permanent_flight());
    CHECK_FALSE(you.airborne());
    CHECK(is_feat_dangerous(DNGN_DEEP_WATER, true));
    you.form = transformation::dragon;
    CHECK(you.airborne());
    CHECK(you.racial_ac(true) == 0);
    CHECK(player_displayed_shield_class() == 0);
}

TEST_CASE_METHOD(FairyFixture, "Fairy wings trade stealth for evasion", "[fairy]")
{
    const int fairy_ev = you.evasion_scaled(100);
    const int fairy_stealth = player_stealth();
    // Keep equipment, attributes and skills identical to another small species.
    you.species = SP_KOBOLD;
    CHECK(you.body_size(PSIZE_BODY, true) == SIZE_SMALL);
    CHECK(fairy_ev > you.evasion_scaled(100));
    CHECK(fairy_stealth < player_stealth());
}

TEST_CASE_METHOD(FairyFixture, "Fairy magic discounts all spell levels without negative MP costs", "[fairy]")
{
    for (int s = SPELL_FIRST_SPELL; s < NUM_SPELLS; ++s)
    {
        const spell_type spell = static_cast<spell_type>(s);
        if (spell == SPELL_NO_SPELL || !is_valid_spell(spell))
            continue;
        CAPTURE(s);
        const int level = spell_mana(spell, false);
        CHECK(spell_mana(spell) == max(0, level - 1));
    }
    CHECK(spell_mana(SPELL_MAGIC_DART) == 0);
    CHECK(spell_mana(SPELL_BLINK) == 1);
    you.duration[DUR_BRILLIANCE] = 10;
    CHECK(spell_mana(SPELL_MAGIC_DART) == 0);
    CHECK(spell_mana(SPELL_BLINK) == 1);
    you.mutation[MUT_EFFICIENT_MAGIC] = 1;
    CHECK(spell_mana(SPELL_BLINK) == 0);
    you.species = SP_HUMAN;
    CHECK(spell_mana(SPELL_MAGIC_DART) == 1);
}
