/**
 * @file
 * @brief Monster abilities.
**/

#pragma once

#include "killer-type.h"
#include "mon-poly.h"

#define BORIS_ORB_KEY "boris orb key"

#define NOBODY_MEMORIES_KEY "nobody_memories"
#define NOBODY_MAX_MEMORIES 3
#define NOBODY_RECOVERY_KEY "nobody_recovery"

#define OCTAVIA_WRATH_KEY "octavia_wrath"

#define TESSERACT_SPAWN_COUNTER_KEY "tesseract_spawn_count"
#define TESSERACT_SPAWN_TIMER_KEY "tesseract_spawn_timer"

#define SLYMDRA_FAKE_HEADS_KEY "slymdra_fake_heads"
#define SLYMDRA_SLIMES_EATEN_KEY "slymdra_slimes_eaten"

#define CERBERUS_HEAD_KEY "cerberus_head"
#define CERBERUS_READY_KEY "cerberus_ready"

#define MINOS_VERDICT_KEY "minos_verdict"
#define MINOS_READY_KEY "minos_ready"
#define MINOS_TARGET_KEY "minos_target"

#define ALECTO_ACTION_KEY "alecto_action"
#define ALECTO_READY_KEY "alecto_ready"
#define ALECTO_TARGET_KEY "alecto_target"

enum alecto_action_type
{
    ALECTO_TORCH,
    ALECTO_CRY,
    NUM_ALECTO_ACTIONS
};

enum minos_verdict_type
{
    MINOS_IRON_SENTENCE,
    MINOS_BIND_THE_GUILTY,
    MINOS_CALL_BAILIFFS,
    NUM_MINOS_VERDICTS
};

enum cerberus_head_type
{
    CERBERUS_BITE,
    CERBERUS_BREATH,
    CERBERUS_HOWL,
    NUM_CERBERUS_HEADS
};

class actor;
class monster;
struct bolt;

bool mon_special_ability(monster* mons);
bool cerberus_action(monster& mon);
void cerberus_grave_howl(monster& mon);
bool minos_action(monster& mon);
void minos_call_bailiffs(monster& mon);
bool alecto_action(monster& mon);
void alecto_incite_fury(monster& mon);

void draconian_change_colour(monster* drac);

void octavia_suffers_wrath(monster* octavia);
void boris_covet_orb(monster* boris);

bool ugly_thing_mutate(monster& ugly, bool force = true);
bool slime_creature_polymorph(monster& slime, poly_power_type power = PPT_SAME);
void merge_ench_durations(monster& initial, monster& merge_to, bool usehd = false);
bool slymdra_polymorph(monster& slimedra, poly_power_type power = PPT_SAME);
void slymdra_scale_hp(monster& slymdra);

bool lost_soul_revive(monster& mons, killer_type killer);

void treant_release_fauna(monster& mons);
void check_grasping_roots(actor& act, bool quiet = false);

void seismosaurus_egg_hatch(monster* mons);
bool egg_is_incubating(const monster& egg);

void initialize_nobody_memories(monster& nobody);
bool pyrrhic_recollection(monster& nobody);

void solar_ember_blast();

void activate_tesseracts();
void tesseract_action(monster& mon);

int slymdra_split(monster& slymdra, int count = -1, bool quiet = false);

bool thorn_hunter_range_check(monster& mon);
void thorn_hunter_raise_barrier(monster& mon, bool skip_proximity_check = false);
