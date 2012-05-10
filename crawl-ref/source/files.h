/**
 * @file
 * @brief Functions used to save and load levels/games.
**/


#ifndef FILES_H
#define FILES_H

#include "externs.h"
#include "player.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <set>

enum load_mode_type
{
    LOAD_START_GAME,            // game has just begun
    LOAD_RESTART_GAME,          // loaded savefile
    LOAD_ENTER_LEVEL,           // entered a level normally
    LOAD_VISITOR,               // Visitor pattern to see all levels
};

bool file_exists(const std::string &name);
bool dir_exists(const std::string &dir);
bool is_absolute_path(const std::string &path);
void assert_read_safe_path(const std::string &path) throw (std::string);
unsigned long file_size(FILE *handle);

std::vector<std::string> get_dir_files(const std::string &dir);
std::vector<std::string> get_dir_files_ext(const std::string &dir,
                                           const std::string &ext);
std::vector<std::string> get_dir_files_recursive(
    const std::string &dirname,
    const std::string &ext = "",
    int recursion_depth = -1,
    bool include_directories = false);

std::string datafile_path(
    std::string basename,
    bool croak_on_fail = true,
    bool test_base_path = false,
    bool (*thing_exists)(const std::string&) = file_exists);

std::string get_parent_directory(const std::string &filename);
std::string get_base_filename(const std::string &filename);
std::string get_cache_name(const std::string &filename);
std::string get_path_relative_to(const std::string &referencefile,
                                 const std::string &relativepath);
std::string catpath(const std::string &first, const std::string &second);
std::string canonicalise_file_separator(const std::string &path);

bool check_mkdir(const std::string &what, std::string *dir,
                 bool silent = false);

// Find saved games for all game types.
std::vector<player_save_info> find_all_saved_characters();

std::string get_save_filename(const std::string &name);
std::string get_savedir_filename(const std::string &name);
std::string savedir_versioned_path(const std::string &subdirs = "");
std::string get_prefs_filename();
std::string change_file_extension(const std::string &file,
                                  const std::string &ext);

time_t file_modtime(const std::string &file);
time_t file_modtime(FILE *f);
std::vector<std::string> get_title_files();


class level_id;

bool load_level(dungeon_feature_type stair_taken, load_mode_type load_mode,
                const level_id& old_level);
void delete_level(const level_id &level);

void save_game(bool leave_game, const char *bye = NULL);

// Save game without exiting (used when changing levels).
void save_game_state();

bool get_save_version(reader &file, int &major, int &minor);

bool save_exists(const std::string& filename);
bool restore_game(const std::string& filename);

void sighup_save_and_exit();

bool is_existing_level(const level_id &level);

class level_excursion
{
protected:
    level_id original;
    bool ever_changed_levels;

public:
    level_excursion();
    ~level_excursion();

    void go_to(const level_id &level);
};

void save_ghost(bool force = false);
bool load_ghost(bool creating_level);

FILE *lk_open(const char *mode, const std::string &file);
void lk_close(FILE *handle, const char *mode, const std::string &file);

// file locking stuff
bool lock_file_handle(FILE *handle, bool write);
bool unlock_file_handle(FILE *handle);

class file_lock
{
public:
    file_lock(const std::string &filename, const char *mode,
              bool die_on_fail = true);
    ~file_lock();
private:
    FILE *handle;
    const char *mode;
    std::string filename;
};

FILE *fopen_replace(const char *name);
#endif
