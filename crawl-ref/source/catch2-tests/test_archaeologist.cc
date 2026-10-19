#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include "evoke.h"
#include "item-name.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "jobs.h"
#include "ng-restr.h"
#include "player.h"
#include "tags.h"
#include "test_player_fixture.h"

struct ArchaeologistFixture : MockPlayerYouTestsFixture
{
    ArchaeologistFixture()
    {
        you.char_class = JOB_ARCHAEOLOGIST;
        init_item_name_cache();
    }

    item_def tome()
    {
        item_def item;
        item.base_type = OBJ_MISCELLANY;
        item.sub_type = MISC_DUSTY_TOME;
        item.quantity = 1;
        item.inscription = "keep this";
        return item;
    }

    item_def* find_item(object_class_type base, int sub)
    {
        for (item_def& item : you.inv)
            if (item.is_type(base, sub))
                return &item;
        return nullptr;
    }
};

TEST_CASE_METHOD(ArchaeologistFixture, "Archaeologists are selectable with BCrawl's abbreviation", "[archaeologist]")
{
    CHECK(is_starting_job(JOB_ARCHAEOLOGIST));
    CHECK(string(get_job_name(JOB_ARCHAEOLOGIST)) == "Archaeologist");
    CHECK(string(get_job_abbrev(JOB_ARCHAEOLOGIST)) == "Ac");
    CHECK(get_job_by_abbrev("Ac") == JOB_ARCHAEOLOGIST);
    CHECK(JOB_ARCHAEOLOGIST == JOB_MYSTIC + 1);
    CHECK_FALSE(job_has_weapon_choice(JOB_ARCHAEOLOGIST));
    for (species_type species : job_recommended_species(JOB_ARCHAEOLOGIST))
    {
        CAPTURE(species);
        CHECK(species_allowed(JOB_ARCHAEOLOGIST, species) == CC_UNRESTRICTED);
        CHECK(job_allowed(species, JOB_ARCHAEOLOGIST) == CC_UNRESTRICTED);
    }
}

TEST_CASE_METHOD(ArchaeologistFixture, "Archaeologists receive their adapted BCrawl starting kit", "[archaeologist]")
{
    const auto stats = you.base_stats;
    job_stat_init(JOB_ARCHAEOLOGIST);
    CHECK(you.base_stats[STAT_STR] == stats[STAT_STR] + 3);
    CHECK(you.base_stats[STAT_INT] == stats[STAT_INT] + 6);
    CHECK(you.base_stats[STAT_DEX] == stats[STAT_DEX] + 3);

    give_job_equipment(JOB_ARCHAEOLOGIST);
    REQUIRE(find_item(OBJ_WEAPONS, WPN_WHIP));
    for (armour_type type : {ARM_ROBE, ARM_HAT, ARM_BOOTS})
    {
        CAPTURE(type);
        item_def* armour = find_item(OBJ_ARMOUR, type);
        REQUIRE(armour);
        CHECK(armour->plus == 1);
    }
    item_def* darts = find_item(OBJ_MISSILES, MI_DART);
    REQUIRE(darts);
    CHECK(darts->quantity == 6);
    CHECK(darts->brand == SPMSL_CURARE);
    REQUIRE(find_item(OBJ_SCROLLS, SCR_REVELATION));
    REQUIRE(find_item(OBJ_MISCELLANY, MISC_DUSTY_TOME));
    CHECK_FALSE(find_item(OBJ_SCROLLS, SCR_ACQUIREMENT));
}

TEST_CASE_METHOD(ArchaeologistFixture, "The dusty tome unlocks once at XL 5 and stays identified", "[archaeologist]")
{
    item_def item = tome();
    REQUIRE(move_item_to_inv(item, true));
    for (int xl = 1; xl <= 4; ++xl)
    {
        you.experience_level = you.max_level = xl;
        archaeologist_level_change();
        REQUIRE(find_item(OBJ_MISCELLANY, MISC_DUSTY_TOME));
        CHECK_FALSE(find_item(OBJ_SCROLLS, SCR_ACQUIREMENT));
    }

    you.experience_level = you.max_level = 5;
    archaeologist_level_change();
    CHECK_FALSE(find_item(OBJ_MISCELLANY, MISC_DUSTY_TOME));
    item_def* scroll = find_item(OBJ_SCROLLS, SCR_ACQUIREMENT);
    REQUIRE(scroll);
    CHECK(scroll->is_identified());
    CHECK(item_type_known(*scroll));
    CHECK(scroll->inscription == "keep this");
    CHECK(scroll->quantity == 1);
    CHECK(scroll->slot != 0);

    for (int xl = 5; xl <= 27; ++xl)
    {
        you.experience_level = you.max_level = xl;
        archaeologist_level_change();
        CHECK(scroll->quantity == 1);
    }
    CHECK_FALSE(archaeologist_decode_tome(*scroll, true));
}

TEST_CASE_METHOD(ArchaeologistFixture, "A tome left behind can be decoded on pickup after XL 5", "[archaeologist]")
{
    you.experience_level = 4;
    you.max_level = 5;
    archaeologist_level_change();
    item_def item = tome();
    REQUIRE(move_item_to_inv(item, true));
    CHECK_FALSE(find_item(OBJ_MISCELLANY, MISC_DUSTY_TOME));
    REQUIRE(find_item(OBJ_SCROLLS, SCR_ACQUIREMENT));
}

TEST_CASE_METHOD(ArchaeologistFixture, "Other backgrounds cannot decode an Archaeologist's tome", "[archaeologist]")
{
    you.char_class = JOB_WANDERER;
    you.experience_level = you.max_level = 27;
    item_def item = tome();
    CHECK_FALSE(archaeologist_decode_tome(item, true));
    REQUIRE(move_item_to_inv(item, true));
    archaeologist_level_change();
    REQUIRE(find_item(OBJ_MISCELLANY, MISC_DUSTY_TOME));
    CHECK_FALSE(find_item(OBJ_SCROLLS, SCR_ACQUIREMENT));
}

TEST_CASE_METHOD(ArchaeologistFixture, "Decoding a tome does not duplicate scroll hotkeys or acquirement stacks", "[archaeologist]")
{
    item_def item = tome();
    REQUIRE(move_item_to_inv(item, true));
    const int old_letter = find_item(OBJ_MISCELLANY, MISC_DUSTY_TOME)->slot;
    item.base_type = OBJ_SCROLLS;
    item.flags |= ISFLAG_IDENTIFIED;
    item.inscription.clear();
    item.sub_type = SCR_REVELATION;
    REQUIRE(move_item_to_inv(item, true));
    item_def* revelation = find_item(OBJ_SCROLLS, SCR_REVELATION);
    REQUIRE(revelation);
    revelation->slot = old_letter;

    SECTION("A read hotkey already belongs to another scroll")
    {
        you.experience_level = you.max_level = 5;
        archaeologist_level_change();
        item_def* reward = find_item(OBJ_SCROLLS, SCR_ACQUIREMENT);
        REQUIRE(reward);
        CHECK(reward->slot != revelation->slot);
    }

    SECTION("An acquirement scroll is already in inventory")
    {
        item.sub_type = SCR_ACQUIREMENT;
        REQUIRE(move_item_to_inv(item, true));
        you.experience_level = you.max_level = 5;
        archaeologist_level_change();
        item_def* reward = find_item(OBJ_SCROLLS, SCR_ACQUIREMENT);
        REQUIRE(reward);
        CHECK(reward->quantity == 2);
        CHECK(reward->slot != revelation->slot);
        int stacks = 0;
        for (const item_def& scroll : you.inv)
            if (scroll.is_type(OBJ_SCROLLS, SCR_ACQUIREMENT))
                ++stacks;
        CHECK(stacks == 1);
    }
}

TEST_CASE_METHOD(ArchaeologistFixture, "The dusty tome survives a save round trip", "[archaeologist]")
{
    item_def original = tome();
    original.rnd = 1;
    vector<unsigned char> bytes;
    writer output(&bytes);
    marshallItem(output, original);
    reader input(bytes, TAG_MINOR_VERSION);
    item_def restored;
    unmarshallItem(input, restored);
    REQUIRE(restored.is_type(OBJ_MISCELLANY, MISC_DUSTY_TOME));
    CHECK(restored.inscription == original.inscription);
    CHECK_FALSE(item_ever_evokable(restored));
    CHECK_FALSE(is_useless_item(restored));
    you.experience_level = you.max_level = 5;
    REQUIRE(archaeologist_decode_tome(restored, true));
    CHECK(restored.is_type(OBJ_SCROLLS, SCR_ACQUIREMENT));
}
