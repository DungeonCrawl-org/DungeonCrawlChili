#pragma once

#include <string>

#include "size-type.h"

// Save these in player properties so existing saves need no new fixed fields.
#define LIGNIFITE_ROOTED_KEY "lignifite_rooted"
constexpr int LIGNIFITE_ROOTS_XL = 12;
constexpr int LIGNIFITE_UPROOT_TURNS = 8;

int lignifite_growth_stage(int xl);
size_type lignifite_size(int xl);
int lignifite_hp_modifier(int xl);
int lignifite_bark_ac(int xl);
int lignifite_branch_sh(int xl);
bool lignifite_rooted();
bool lignifite_natural_body();
std::string lignifite_rooting_blocker();
void lignifite_plant_roots();
void lignifite_uproot();
void lignifite_level_up(int xl);
