#include "AppHdr.h"

#include "lignifite.h"

#include "env.h"
#include "item-name.h"
#include "mpr.h"
#include "player.h"
#include "player-equip.h"
#include "terrain.h"
#include "transform.h"

// Adapted from BcadrenCrawl's Lignifite, originally designed by Zeldest.
int lignifite_growth_stage(int xl)
{
    return min(5, max(0, xl / 5));
}

size_type lignifite_size(int xl)
{
    // Chili has no separate "big" size between large and giant. Both adult
    // stages remain large; the final growth at XL 25 reaches giant size.
    const size_type sizes[] = {
        SIZE_LITTLE, SIZE_SMALL, SIZE_MEDIUM, SIZE_LARGE, SIZE_LARGE, SIZE_GIANT
    };
    return sizes[lignifite_growth_stage(xl)];
}

int lignifite_hp_modifier(int xl)
{
    return -1 + lignifite_growth_stage(xl);
}

int lignifite_bark_ac(int xl)
{
    return max(0, xl - 12);
}

int lignifite_branch_sh(int xl)
{
    return 1 + xl / 3;
}

bool lignifite_rooted()
{
    return you.species == SP_LIGNIFITE
           && you.props.exists(LIGNIFITE_ROOTED_KEY)
           && you.props[LIGNIFITE_ROOTED_KEY].get_bool();
}

bool lignifite_natural_body()
{
    return you.species == SP_LIGNIFITE
           && !form_changes_anatomy() && !form_changes_substance();
}

string lignifite_rooting_blocker()
{
    if (you.species != SP_LIGNIFITE || you.experience_level < LIGNIFITE_ROOTS_XL)
        return "Your roots are not yet developed enough.";
    if (lignifite_rooted())
        return "You are already rooted.";
    if (!lignifite_natural_body())
        return "You must return to your natural shape to plant your roots.";
    if (you.airborne() || !feat_has_solid_floor(env.grid(you.pos()))
        || feat_is_water(env.grid(you.pos())))
    {
        return "You need solid ground beneath you to plant your roots.";
    }
    return "";
}

void lignifite_plant_roots()
{
    ASSERT(lignifite_rooting_blocker().empty());
    you.props[LIGNIFITE_ROOTED_KEY] = true;
    // Rooted stasis prevents an already pending teleport as well as new ones.
    you.duration[DUR_TELEPORT] = 0;
    you.duration[DUR_PETRIFYING] = 0;
    you.duration[DUR_PETRIFIED] = 0;
    you.redraw_armour_class = true;
    you.redraw_status_lights = true;
    mpr("Your roots penetrate the ground. You feel firmly anchored.");
    mpr("You must spend eight turns uprooting before you can move again.");
}

void lignifite_uproot()
{
    you.props.erase(LIGNIFITE_ROOTED_KEY);
    you.redraw_armour_class = true;
    you.redraw_status_lights = true;
    mpr("You finish extracting your roots and can move again.");
}

void lignifite_level_up(int xl)
{
    if (xl == LIGNIFITE_ROOTS_XL)
        mprf(MSGCH_INTRINSIC_GAIN, "Your roots are now strong enough to plant firmly in the ground.");

    if (xl % 5 == 0)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "You grow larger and sturdier, but your steps become slower.");
        // Growth can make armour or weapons unsuitable; preserve them in the
        // inventory rather than leave impossible equipment in active slots.
        for (item_def* item : you.equipment.get_forced_removal_list())
        {
            mprf("%s no longer fits your growing body.", item->name(DESC_YOUR).c_str());
            unequip_item(*item);
        }
        you.equipment.update();
        you.redraw_evasion = true;
        you.wield_change = true;
    }
    if (xl > 12)
        you.redraw_armour_class = true;
    if (xl % 3 == 0)
        you.redraw_armour_class = true;
}
