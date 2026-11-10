#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "artefact.h"
#include "art-enum.h"
#include "items.h"
#include "item-name.h"
#include "item-use.h"
#include "item-prop.h"
#include "item-prop-enum.h"
#include "item-status-flag-type.h"
#include "invent.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion-type.h"
#include "random.h"
#include "species.h"
#include "tags.h"
#include "makeitem.h"
#include "env.h"
#include "spl-util.h"
#include "state.h"
#include "unwind.h"
#include "uncancellable-type.h"

#include "test_player_fixture.h"

TEST_CASE( "Create item def", "[single-file]" ) {
    item_def foo;
}

TEST_CASE( "Create a specific item def", "[single-file]" ) {
    item_def scroll_of_fear;

    get_item_by_exact_name(scroll_of_fear, "scroll of fear");

    REQUIRE(scroll_of_fear.base_type == OBJ_SCROLLS);
    REQUIRE(scroll_of_fear.sub_type == SCR_FEAR);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Check mock player has no items", "[single-file]" ) {
    for (int i=0; i < ENDOFPACK; i++)
        REQUIRE(you.inv[i].base_type == OBJ_UNASSIGNED);

    REQUIRE(!any_items_of_type(OSEL_ANY));

}

static item_def simple_create_item(object_class_type base_type,
                                   int sub_type, int plus = 0,
                                   special_armour_type ego = SPARM_NORMAL);

static item_def simple_create_item(object_class_type base_type,
                                   int sub_type, int plus,
                                   special_armour_type ego){
    item_def item;

    item.clear();

    item.base_type = base_type;
    item.sub_type = sub_type;
    item.quantity = 1;
    item.plus = plus;
    item.brand = ego;

    return item;
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Brand Weapon saves become Bless Item", "[single-file][scroll-compat]")
{
    const int subtype = GENERATE(SCR_BRAND_WEAPON, SCR_BLESS_ITEM);
    item_def original = simple_create_item(OBJ_SCROLLS, subtype);
    original.rnd = 1;
    original.quantity = 3;
    original.inscription = "saved scrolls";
    vector<unsigned char> bytes;
    writer output(&bytes);
    marshallItem(output, original);
    reader input(bytes, TAG_MINOR_VERSION);
    item_def restored;
    unmarshallItem(input, restored);
    REQUIRE(restored.is_type(OBJ_SCROLLS, SCR_BLESS_ITEM));
    REQUIRE(restored.quantity == 3);
    REQUIRE(restored.inscription == original.inscription);
    REQUIRE(item_type_removed(OBJ_SCROLLS, SCR_BRAND_WEAPON));
    REQUIRE_FALSE(item_type_removed(OBJ_SCROLLS, SCR_BLESS_ITEM));
    REQUIRE(consumable_rarity(OBJ_SCROLLS, SCR_BRAND_WEAPON) == RARITY_NONE);
    REQUIRE(consumable_rarity(OBJ_SCROLLS, SCR_BLESS_ITEM) != RARITY_NONE);
}

TEST_CASE("Saved scroll action IDs remain stable", "[single-file][scroll-compat]")
{
    REQUIRE(UNC_BRAND_WEAPON == 6);
    REQUIRE(UNC_BLESS_ITEM == 7);
    REQUIRE(UNC_AMNESIA == 8);
    REQUIRE(UNC_BLINKING == 9);
    REQUIRE(UNC_IDENTIFY == 10);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Legacy Brand Weapon requests create Bless Item", "[single-file][scroll-compat]")
{
    const int index = items(false, OBJ_SCROLLS, SCR_BRAND_WEAPON, 1);
    REQUIRE(index != NON_ITEM);
    CHECK(env.item[index].is_type(OBJ_SCROLLS, SCR_BLESS_ITEM));
    destroy_item(index);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Felids can enchant boots with known scrolls", "[single-file]")
{
    you.species = SP_FELID;
    give_basic_mutations(SP_FELID);
    REQUIRE(you.has_mutation(MUT_NO_GRASPING));

    item_def scroll = simple_create_item(OBJ_SCROLLS, SCR_ENCHANT_ARMOUR);
    you.inv[0] = simple_create_item(OBJ_ARMOUR, ARM_BOOTS);
    item_def &boots = you.inv[0];
    boots.flags |= ISFLAG_IDENTIFIED;
    REQUIRE(can_equip_item(boots));

    SECTION("Unknown and known scrolls both allow enchanting")
    {
        const bool known = GENERATE(false, true);
        you.type_ids[OBJ_SCROLLS][SCR_ENCHANT_ARMOUR] = known;
        REQUIRE(cannot_read_item_reason(&scroll).empty());
        REQUIRE(any_items_of_type(OSEL_ENCHANTABLE_ARMOUR));
        REQUIRE(enchant_armour(boots, true));
        REQUIRE(boots.plus == 1);
    }

    SECTION("Known scrolls still require an enchantable target")
    {
        you.type_ids[OBJ_SCROLLS][SCR_ENCHANT_ARMOUR] = true;
        boots.plus = armour_max_enchant(boots);
        REQUIRE_FALSE(any_items_of_type(OSEL_ENCHANTABLE_ARMOUR));
        REQUIRE_FALSE(cannot_read_item_reason(&scroll).empty());
        REQUIRE_FALSE(enchant_armour(boots, true));
    }

    SECTION("Weapon enhancement remains blocked")
    {
        scroll.sub_type = SCR_ENCHANT_WEAPON;
        REQUIRE(cannot_read_item_reason(&scroll, false, true)
                == "There's no point in enhancing weapons you can't use!");
    }
}

static int find_inv_index_with_exact_item(object_class_type base_type, int sub_type){

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == base_type
               && item.sub_type == sub_type)
        {
            return i;
        }
    }

    return -1;
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock player item", "[single-file]" ) {
    item_def scroll_of_fear;

    get_item_by_exact_name(scroll_of_fear, "scroll of fear");

    move_item_to_inv(scroll_of_fear);

    REQUIRE(any_items_of_type(OSEL_ANY));

    int num_fear_stacks_in_invent = 0;

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == OBJ_SCROLLS
               && item.sub_type == SCR_FEAR)
        {
            num_fear_stacks_in_invent++;
        }
    }

    REQUIRE(num_fear_stacks_in_invent == 1);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Mock player armour skill", "[single-file]" ) {
    REQUIRE(you.skill(SK_ARMOUR,20) == 0);
    REQUIRE(you.strength() == 11);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock armour", "[single-file]" ) {

    item_def armour = simple_create_item(OBJ_ARMOUR, ARM_SCALE_MAIL);

    move_item_to_inv(armour);

    REQUIRE(you.base_ac_from(armour, 100) == 600);

    REQUIRE(any_items_of_type(OSEL_ANY));

    int num_armours = 0;

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == OBJ_ARMOUR
               && item.sub_type == ARM_SCALE_MAIL)
        {
            num_armours++;
        }
    }

    REQUIRE(num_armours == 1);

    REQUIRE(you.base_ac(100) == 0);
}

static void make_and_equip_item(object_class_type base_type, int sub_type,
                                int plus = 0,
                                special_armour_type ego = SPARM_NORMAL);

static void find_and_equip_exact_item(item_def item){
    move_item_to_inv(item);

    int index =
        find_inv_index_with_exact_item(item.base_type, item.sub_type);

    REQUIRE(index != -1);

    equip_item(get_all_item_slots(item)[0], index);
}

static void make_and_equip_item(object_class_type base_type, int sub_type,
                                int plus, special_armour_type ego){

    item_def item = simple_create_item(base_type, sub_type, plus,
                                       ego);
    find_and_equip_exact_item(item);

}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock armour and check after after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_SCALE_MAIL);

    REQUIRE(you.base_ac(100) == 600);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock robe and check after after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_ROBE);

    REQUIRE(you.base_ac(100) == 200);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give multiple mock items and check after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_FIRE_DRAGON_ARMOUR);
    make_and_equip_item(OBJ_ARMOUR, ARM_HELMET);
    make_and_equip_item(OBJ_ARMOUR, ARM_BOOTS);
    make_and_equip_item(OBJ_ARMOUR, ARM_GLOVES);
    make_and_equip_item(OBJ_ARMOUR, ARM_CLOAK);

    REQUIRE(you.base_ac(100) == 1200);
}

TEST_CASE("armour_prop_test", "[single-file]"){
    REQUIRE(armour_prop(ARM_SCALE_MAIL, PARM_AC) == 6);
}

struct BlessItemFixture : MockPlayerYouTestsFixture
{
    rng::subgenerator random_scope{1234};
    unwind_var<int> time_step{you.duration[DUR_TIME_STEP], 1};

    BlessItemFixture()
    {
        init_show_table();
        init_spell_descs();
        you.set_position(coord_def(10, 10));
        env.grid(you.pos()) = DNGN_FLOOR;
        update_vision_range();
    }

    item_def& armour(armour_type type, special_armour_type ego, bool worn = true)
    {
        item_def created = simple_create_item(OBJ_ARMOUR, type, 0, ego);
        move_item_to_inv(created);
        item_def& item = you.inv[find_inv_index_with_exact_item(OBJ_ARMOUR, type)];
        if (worn)
            equip_item(get_armour_slot(item), item.link, false, true);
        return item;
    }
};

TEST_CASE_METHOD(BlessItemFixture, "Bless Item immediately updates worn stat bonuses",
                 "[bless-item]")
{
    const bool cursed = GENERATE(false, true);
    int base[NUM_STATS];
    for (int stat = 0; stat < NUM_STATS; ++stat)
        base[stat] = you.stat(static_cast<stat_type>(stat));
    item_def& gloves = armour(ARM_GLOVES, SPARM_STRENGTH);
    if (cursed)
        gloves.flags |= ISFLAG_CURSED;
    REQUIRE(you.strength() == base[STAT_STR] + 3);
    you.equipment.get_entry_for(gloves).attuned = true;
    const auto entry = you.equipment.get_entry_for(gloves);
    const int turn = you.num_turns;
    you.redraw_stats.init(false);
    you.redraw_armour_class = you.redraw_evasion = you.gear_change = false;

    bless_item(gloves);

    const auto ego = get_armour_ego_type(gloves);
    REQUIRE(ego != SPARM_STRENGTH);
    CHECK(you.wearing_ego(OBJ_ARMOUR, SPARM_STRENGTH) == 0);
    CHECK(you.wearing_ego(OBJ_ARMOUR, ego) == 1);
    CHECK(you.strength() == base[STAT_STR]);
    CHECK(you.dex() == base[STAT_DEX] + (ego == SPARM_DEXTERITY ? 3 : 0));
    CHECK(you.intel() == base[STAT_INT]);
    for (int stat = 0; stat < NUM_STATS; ++stat)
        CHECK(you.redraw_stats[stat]);
    CHECK(you.redraw_armour_class);
    CHECK(you.redraw_evasion);
    CHECK(you.gear_change);
    CHECK(gloves.cursed() == cursed);
    CHECK(you.equipment.get_entry_for(gloves).item == entry.item);
    CHECK(you.equipment.get_entry_for(gloves).slot == entry.slot);
    CHECK(you.equipment.get_entry_for(gloves).attuned == entry.attuned);
    CHECK(you.num_turns == turn);
}

TEST_CASE_METHOD(BlessItemFixture, "Bless Item immediately removes the old resistance",
                 "[bless-item]")
{
    const int fire = player_res_fire();
    const int cold = player_res_cold();
    item_def& scarf = armour(ARM_SCARF, SPARM_RESISTANCE);
    REQUIRE(player_res_fire() == fire + 1);
    REQUIRE(player_res_cold() == cold + 1);

    bless_item(scarf);

    REQUIRE(get_armour_ego_type(scarf) != SPARM_RESISTANCE);
    CHECK(player_res_fire() == fire);
    CHECK(player_res_cold() == cold);
    CHECK(item_is_equipped(scarf));
}

TEST_CASE_METHOD(BlessItemFixture, "Bless Item does not activate carried or melded gear",
                 "[bless-item]")
{
    const bool melded = GENERATE(false, true);
    const int strength = you.strength();
    const int dexterity = you.dex();
    item_def& gloves = armour(ARM_GLOVES, SPARM_STRENGTH, melded);
    if (melded)
    {
        you.equipment.get_entry_for(gloves).melded = true;
        you.equipment.update();
    }

    bless_item(gloves);

    CHECK(you.strength() == strength);
    CHECK(you.dex() == dexterity);
    CHECK(you.wearing_ego(OBJ_ARMOUR, get_armour_ego_type(gloves)) == 0);
    CHECK(item_is_equipped(gloves) == melded);
    if (melded)
        CHECK(item_is_melded(gloves));
}

TEST_CASE_METHOD(BlessItemFixture, "Bless Item immediately refreshes flight",
                 "[bless-item]")
{
    item_def& boots = armour(ARM_BOOTS, SPARM_FLYING);
    you.attribute[ATTR_PERM_FLIGHT] = 1;
    REQUIRE(you.airborne());

    bless_item(boots);

    REQUIRE(get_armour_ego_type(boots) != SPARM_FLYING);
    CHECK_FALSE(you.airborne());
    CHECK(you.attribute[ATTR_PERM_FLIGHT] == 0);
    // Exercise gaining flight too, using a private, deterministic RNG stream.
    for (int roll = 0; roll < 32 && !you.equip_flight(); ++roll)
        bless_item(boots);
    REQUIRE(get_armour_ego_type(boots) == SPARM_FLYING);
    CHECK(you.airborne());
    CHECK(you.attribute[ATTR_PERM_FLIGHT] == 1);
    CHECK(item_is_equipped(boots));
}

TEST_CASE_METHOD(BlessItemFixture, "Bless Item immediately refreshes shadow scarf vision",
                 "[bless-item]")
{
    const int vision = you.current_vision;
    item_def& scarf = armour(ARM_SCARF, SPARM_SHADOWS);
    update_vision_range();
    REQUIRE(you.current_vision == vision - 1);

    bless_item(scarf);

    REQUIRE(get_armour_ego_type(scarf) != SPARM_SHADOWS);
    CHECK(you.current_vision == vision);
    CHECK(item_is_equipped(scarf));
}

TEST_CASE("Test all_item_subtypes() does not include removed items",
          "[single-file]") {
    const auto items = all_item_subtypes(OBJ_POTIONS);

    const auto has_removed_item = find(items.begin(), items.end(), POT_POISON) != items.end();

    REQUIRE(has_removed_item == false);
}

TEST_CASE("Test all_item_subtypes() does include items for each category",
          "[single-file]") {
    // Note: CORPSES and RODS do not have any sub-items, so are excluded.
    REQUIRE(all_item_subtypes(OBJ_WEAPONS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_MISSILES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_ARMOUR).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_WANDS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_SCROLLS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_JEWELLERY).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_POTIONS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_BOOKS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_STAVES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_ORBS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_MISCELLANY).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_GOLD).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_RUNES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_TALISMANS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_GEMS).size() > 0);
}
