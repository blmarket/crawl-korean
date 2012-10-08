/**
 * @file
 * @brief Functions for handling player mutations.
**/


#ifndef MUTATION_H
#define MUTATION_H

#include <string>

class formatted_string;

enum mutation_activity_type
{
    MUTACT_INACTIVE, // form-based mutations in most forms
    MUTACT_HUNGER,   // non-physical mutations on vampires
    MUTACT_PARTIAL,  // scales on statues
    MUTACT_FULL,     // other mutations
};

struct body_facet_def
{
    equipment_type eq;
    mutation_type mut;
    int level_lost;
};

struct facet_def
{
    int tier;
    mutation_type muts[3];
    int when[3];
};

struct demon_mutation_info
{
    mutation_type mut;
    int when;
    int facet;

    demon_mutation_info(mutation_type m, int w, int f)
        : mut(m), when(w), facet(f) { }
};

struct mutation_def
{
    mutation_type mutation;
    short       rarity;     // Rarity of the mutation.
    short       levels;     // The number of levels of the mutation.
    bool        bad;        // A mutation that's more bad than good. Xom uses
                            // this to decide which mutations to hand out as
                            // rewards.
    bool        physical;   // A mutation affecting a character's outward
                            // appearance; active for thirsty semi-undead.
    bool        form_based; // A mutation that is suppressed when shapechanged.
    const char* short_desc; // What appears on the '%' screen.
    const char* have[3];    // What appears on the 'A' screen.
    const char* gain[3];    // Message when you gain the mutation.
    const char* lose[3];    // Message when you lose the mutation.
    const char* wizname;    // For gaining it in wizmode.
};

void init_mut_index();

bool is_valid_mutation(mutation_type mut);
bool is_body_facet(mutation_type mut);
const mutation_def& get_mutation_def(mutation_type mut);

void fixup_mutations();

bool mutate(mutation_type which_mutation, const std::string &reason,
            bool failMsg = true,
            bool force_mutation = false, bool god_gift = false,
            bool stat_gain_potion = false, bool demonspawn = false,
            bool no_rot = false);

static inline bool give_bad_mutation(const std::string &reason,
                              bool failMsg = true,
                              bool force_mutation = false)
{
    return mutate(RANDOM_BAD_MUTATION, reason, failMsg, force_mutation,
                  false, false, false);
}

void display_mutations();
mutation_activity_type mutation_activity_level(mutation_type mut);
std::string describe_mutations(bool center_title);

bool delete_mutation(mutation_type which_mutation, const std::string &reason,
                     bool failMsg = true,
                     bool force_mutation = false, bool god_gift = false,
                     bool disallow_mismatch = false);

bool delete_all_mutations(const std::string &reason);

std::string mutation_name(mutation_type which_mutat, int level = -1,
                          bool colour = false);

void roll_demonspawn_mutations();

bool perma_mutate(mutation_type which_mut, int how_much, const std::string &reason);
int how_mutated(bool all = false, bool levels = false);

void check_demonic_guardian();
void check_antennae_detect();
int handle_pbd_corpses(bool do_rot);
equipment_type beastly_slot(int mut);
bool physiology_mutation_conflict(mutation_type mutat);
int augmentation_amount();

#endif
