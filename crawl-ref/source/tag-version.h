#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
#define TAG_CHR_FORMAT 0

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_VERSION 34

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_INVALID         = -1,
    TAG_MINOR_RESET           = 0, // Minor tags were reset
    TAG_MINOR_BRANCHES_LEFT,       // Note the first time branches are left
    TAG_MINOR_VAULT_LIST,          // Don't try to store you.vault_list as prop
    TAG_MINOR_TRAPS_DETERM,        // Searching for traps is deterministic.
    TAG_MINOR_ACTION_THROW,        // Store base type of throw objects.
    TAG_MINOR_TEMP_MUTATIONS,      // Enable transient mutations
    TAG_MINOR_AUTOINSCRIPTIONS,    // Artefact inscriptions are added on the fly
    TAG_MINOR_UNCANCELLABLES,      // Restart uncancellable questions upon save load
    TAG_MINOR_DEEP_ABYSS,          // Multi-level abyss
    TAG_MINOR_COORD_SERIALIZER,    // Serialize coord_def as int
    TAG_MINOR_REMOVE_ABYSS_SEED,   // Remove the abyss seed.
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
