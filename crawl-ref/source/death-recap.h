#pragma once

#include <string>

// Recorded facts only. The rolling history is stored in player properties.
void death_recap_begin_turn();
void death_recap_hp_change(const std::string &source, int before, int after,
                          int damage = 0);
std::string death_recap_text();
void death_recap_finish();
std::string final_death_recap();
