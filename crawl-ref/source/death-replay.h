#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "coord-def.h"
#include "level-id.h"

struct newgame_def;
class scorefile_entry;

struct death_replay_data
{
    std::string name;
    uint64_t seed = 0;
    level_id level;
    coord_def position;
    int turns = -1;
};

struct replay_checkpoint
{
    std::string path, name;
    uint64_t seed = 0;
    int64_t birth = 0;
    int turn = -1;
};

bool parse_death_replay(const std::string &text, death_replay_data &data, std::string &error);
bool read_replay_checkpoint(const std::string &path, replay_checkpoint &checkpoint);
int select_death_replay_checkpoint(const std::vector<replay_checkpoint> &checkpoints,
                                   int death_turn);
bool death_replay_checkpoint_due(int turn, int last);
void checkpoint_death_replay();
void archive_death_replay(const scorefile_entry &score);
bool prepare_death_replay(const std::string &morgue_path);
void queue_death_replay(const std::string &morgue_file);
bool start_queued_death_replay(const std::string &player_name);
bool death_replay_pending();
void configure_death_replay(newgame_def &game);
bool restore_death_replay(const newgame_def &game);
void start_death_replay();
bool is_death_replay();
void set_death_replay_recommendation(bool recommend);
int death_replay_recommendation_vote();
bool death_replay_marker_position(coord_def &position);
