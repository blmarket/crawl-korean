/**
 * @file
 * @brief Crawl Lua test cases
 *
 * ctest runs Lua tests found in the test directory. The intent here
 * is to test parts of Crawl that can be easily tested from within Crawl
 * itself (such as LOS). As a side-effect, writing Lua bindings to support
 * tests will expand the available Lua bindings. :-)
 *
 * Tests will run only with Crawl built in its source tree without
 * DATA_DIR_PATH set.
**/

#include "AppHdr.h"

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)

#include "ctest.h"

#include "clua.h"
#include "cluautil.h"
#include "dlua.h"
#include "errors.h"
#include "files.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "ng-init.h"
#include "state.h"
#include "stuff.h"
#include "zotdef.h"

#include <algorithm>
#include <vector>

static const string test_dir = "test";
static const string script_dir = "scripts";
static const string test_player_name = "Superbug99";
static const species_type test_player_species = SP_HUMAN;
static const job_type test_player_job = JOB_FIGHTER;
static const char *activity = "test";

static int ntests = 0;
static int nsuccess = 0;

typedef pair<string, string> file_error;
static vector<file_error> failures;

static void _reset_test_data()
{
    ntests = 0;
    nsuccess = 0;
    failures.clear();
    you.your_name = test_player_name;
    you.species = test_player_species;
    you.char_class = test_player_job;
}

static int crawl_begin_test(lua_State *ls)
{
    mprf(MSGCH_PROMPT, "Starting %s: %s",
         activity,
         luaL_checkstring(ls, 1));
    lua_pushnumber(ls, ++ntests);
    return 1;
}

static int crawl_test_success(lua_State *ls)
{
    if (!crawl_state.script)
        mprf(MSGCH_PROMPT, "Test success: %s", luaL_checkstring(ls, 1));
    lua_pushnumber(ls, ++nsuccess);
    return 1;
}

static int crawl_script_args(lua_State *ls)
{
    return clua_stringtable(ls, crawl_state.script_args);
}

static const struct luaL_reg crawl_test_lib[] =
{
    { "begin_test", crawl_begin_test },
    { "test_success", crawl_test_success },
    { "script_args", crawl_script_args },
    { NULL, NULL }
};

static void _init_test_bindings()
{
    lua_stack_cleaner clean(dlua);
    luaL_openlib(dlua, "crawl", crawl_test_lib, 0);
    dlua.execfile("dlua/test.lua", true, true);
    initialise_branch_depths();
    initialise_item_descriptions();
}

static bool _is_test_selected(const string &testname)
{
    if (crawl_state.tests_selected.empty() && !starts_with(testname, "big/"))
        return true;
    for (int i = 0, size = crawl_state.tests_selected.size();
         i < size; ++i)
    {
        const string &phrase(crawl_state.tests_selected[i]);
        if (testname.find(phrase) != string::npos)
            return true;
    }
    return false;
}

static void run_test(const string &file)
{
    if (!_is_test_selected(file))
        return;

    ++ntests;
    mprf(MSGCH_DIAGNOSTICS, "Running %s %d: %s",
         activity, ntests, file.c_str());
    flush_prev_message();

    const string path(catpath(crawl_state.script? script_dir : test_dir, file));
    dlua.execfile(path.c_str(), true, false);
    if (dlua.error.empty())
        ++nsuccess;
    else
        failures.push_back(file_error(file, dlua.error));
}

static bool _has_test(const string& test)
{
    if (crawl_state.script)
        return false;
    if (crawl_state.tests_selected.empty())
        return true;
    return crawl_state.tests_selected[0].find(test) != string::npos;
}

static void _run_test(const string &name, void (*func)(void))
{
    if (!_has_test(name))
        return;

    try
    {
        (*func)();
    }
    catch (const ext_fail_exception &E)
    {
        failures.push_back(file_error(name, E.msg));
    }
}

// Assumes curses has already been initialized.
bool run_tests(bool exit_on_complete)
{
    if (crawl_state.script)
        activity = "script";

    flush_prev_message();

    run_map_global_preludes();
    run_map_local_preludes();
    _reset_test_data();

    _init_test_bindings();

    _run_test("makeitem", makeitem_tests);
    _run_test("zotdef_wave", debug_waves);
    _run_test("mon-pick", debug_monpick);
    _run_test("mon-data", debug_mondata);

    // Get a list of Lua files in test. Order of execution of
    // tests should be irrelevant.
    const vector<string> tests(
        get_dir_files_recursive(crawl_state.script? script_dir : test_dir,
                          ".lua"));
    for_each(tests.begin(), tests.end(), run_test);

    if (failures.empty() && !ntests && crawl_state.script)
        failures.push_back(
            file_error(
                "Script setup",
                "No scripts found matching " +
                comma_separated_line(crawl_state.tests_selected.begin(),
                                     crawl_state.tests_selected.end(),
                                     ", ",
                                     ", ")));

    if (exit_on_complete)
    {
        cio_cleanup();
        for (int i = 0, size = failures.size(); i < size; ++i)
        {
            const file_error &fe(failures[i]);
            fprintf(stderr, "%s error: %s\n",
                    activity, fe.second.c_str());
        }
        const int code = failures.empty() ? 0 : 1;
        end(code, false, "%d %ss, %d succeeded, %d failed",
            ntests, activity, nsuccess, (int)failures.size());
    }
    return failures.empty();
}

#endif // DEBUG_DIAGNOSTICS
