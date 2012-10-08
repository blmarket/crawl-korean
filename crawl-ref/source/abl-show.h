/**
 * @file
 * @brief Functions related to special abilities.
**/


#ifndef ABLSHOW_H
#define ABLSHOW_H

#include "enum.h"

#include <string>
#include <vector>

struct talent
{
    ability_type which;
    int hotkey;
    int fail;
    bool is_invocation;
    bool is_zotdef;
};

const std::string make_cost_description(ability_type ability);
talent get_talent(ability_type ability, bool check_confused);
const char* ability_name(ability_type ability);
std::vector<const char*> get_ability_names();
std::string get_ability_desc(const ability_type ability);
int choose_ability_menu(const std::vector<talent>& talents);
std::string describe_talent(const talent& tal);

void no_ability_msg();
bool activate_ability();
bool check_ability_possible(const ability_type ability, bool hungerCheck = true,
                            bool quiet = false);
bool activate_talent(const talent& tal);
std::vector<talent> your_talents(bool check_confused);
bool string_matches_ability_name(const std::string& key);
std::string print_abilities(void);

void set_god_ability_slots(void);
std::vector<ability_type> get_god_abilities(bool include_unusable = false);
void gain_god_ability(int i);
void lose_god_ability(int i);

#endif
