#pragma once

#include <string>

void show_coaching_help();
void show_postmortem_help(const std::string &dump);
#ifdef USE_TILE_WEB
std::string postmortem_prompt(const std::string &dump, bool include_recording = false,
                             bool detailed = false);
#endif
