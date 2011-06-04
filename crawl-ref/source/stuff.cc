/**
 * @file
 * @brief Misc stuff.
**/

#include "AppHdr.h"

#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <stack>

#ifdef UNIX
 #ifndef USE_TILE
 #include "libunix.h"
 #endif
#endif


#include "cio.h"
#include "colour.h"
#include "database.h"
#include "delay.h"
#include "dungeon.h"
#include "files.h"
#include "hints.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "view.h"
#include "viewgeom.h"


// Crude, but functional.
std::string make_time_string(time_t abs_time, bool terse)
{
    const int days  = abs_time / 86400;
    const int hours = (abs_time % 86400) / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    std::string buff;
    if (days > 0)
    {
        buff += make_stringf("%d%s ", days, terse ? ","
                             : days > 1 ? "days" : "day");
    }
    return buff + make_stringf("%02d:%02d:%02d", hours, mins, secs);
}

std::string make_file_time(time_t when)
{
    if (tm *loc = TIME_FN(&when))
    {
        return make_stringf("%04d%02d%02d-%02d%02d%02d",
                 loc->tm_year + 1900,
                 loc->tm_mon + 1,
                 loc->tm_mday,
                 loc->tm_hour,
                 loc->tm_min,
                 loc->tm_sec);
    }
    return ("");
}

void set_redraw_status(uint64_t flags)
{
    you.redraw_status_flags |= flags;
}

unsigned char get_ch()
{
    mouse_control mc(MOUSE_MODE_MORE);
    unsigned char gotched = getchm();

    if (gotched == 0)
        gotched = getchm();

    return gotched;
}

void cio_init()
{
    crawl_state.io_inited = true;

#if defined(UNIX) && !defined(USE_TILE)
    unixcurses_startup();
#endif

#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE)
    init_libw32c();
#endif

    set_cursor_enabled(false);

    crawl_view.init_geometry();

#ifdef USE_TILE
    tiles.resize();
#endif
}

void cio_cleanup()
{
    if (!crawl_state.io_inited)
        return;

#if defined(USE_TILE)
    tiles.shutdown();
#elif defined(UNIX)
    unixcurses_shutdown();
#endif

#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE)
    deinit_libw32c();
#endif

    crawl_state.io_inited = false;
}

// Clear some globally defined variables.
void clear_globals_on_exit()
{
    clear_rays_on_exit();
    clear_zap_info_on_exit();
    clear_colours_on_exit();
    dgn_clear_vault_placements(env.level_vaults);
}

// Used by do_crash_dump() to tell if the crash happened during exit() hooks.
// Not a part of crawl_state, since that's a global C++ instance which is
// free'd by exit() hooks when exit() is called, and we don't want to reference
// free'd memory.
bool CrawlIsExiting = false;
bool CrawlIsCrashing = false;

NORETURN void end(int exit_code, bool print_error, const char *format, ...)
{
    std::string error = print_error? strerror(errno) : "";
    if (format)
    {
        va_list arg;
        va_start(arg, format);
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, format, arg);
        va_end(arg);

        if (error.empty())
            error = std::string(buffer);
        else
            error = std::string(buffer) + ": " + error;

        if (!error.empty() && error[error.length() - 1] != '\n')
            error += "\n";
    }

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE)) \
     || defined(DGL_PAUSE_AFTER_ERROR)
    bool need_pause = true;
    if (exit_code && !error.empty())
    {
        if (print_error_screen("%s", error.c_str()))
            need_pause = false;
    }
#endif

    cio_cleanup();
    msg::deinitialise_mpr_streams();
    clear_globals_on_exit();
    databaseSystemShutdown();

    if (!error.empty())
    {
        fprintf(stderr, "%s", error.c_str());
        error.clear();
    }

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE)) \
     || defined(DGL_PAUSE_AFTER_ERROR)
    if (need_pause && exit_code && !crawl_state.game_is_arena()
        && !crawl_state.seen_hups && !crawl_state.test)
    {
        fprintf(stderr, "Hit Enter to continue...\n");
        getchar();
    }
#endif

    CrawlIsExiting = true;
    if (exit_code)
        CrawlIsCrashing = true;

#ifdef DEBUG_GLOBALS
    delete real_env;         real_env = 0;
    delete real_crawl_state; real_crawl_state = 0;
    delete real_dlua;        real_dlua = 0;
    delete real_clua;        real_clua = 0;
    delete real_you;         real_you = 0;
    delete real_Options;     real_Options = 0;
#endif

    exit(exit_code);
}

NORETURN void game_ended()
{
    if (!crawl_state.seen_hups)
        throw game_ended_condition();
    else
        end(0);
}

NORETURN void game_ended_with_error(const std::string &message)
{
    if (crawl_state.seen_hups)
        end(1);

    if (Options.restart_after_game)
    {
        if (crawl_state.io_inited)
        {
            mprf(MSGCH_ERROR, "%s", message.c_str());
            more();
        }
        else
        {
            fprintf(stderr, "%s\nHit Enter to continue...\n", message.c_str());
            getchar();
        }
        game_ended();
    }
    else
    {
        end(1, false, "%s", message.c_str());
    }
}

// Print error message on the screen.
// Ugly, but better than not showing anything at all. (jpeg)
bool print_error_screen(const char *message, ...)
{
    if (!crawl_state.io_inited || crawl_state.seen_hups)
        return false;

    // Get complete error message.
    std::string error_msg;
    {
        va_list arg;
        va_start(arg, message);
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, message, arg);
        va_end(arg);

        error_msg = std::string(buffer);
    }
    if (error_msg.empty())
        return false;

    // Escape '<'.
    // NOTE: This assumes that the error message doesn't contain
    //       any formatting!
    error_msg = replace_all(error_msg, "<", "<<");

    error_msg += "\n\n\nHit any key to exit...\n";

    // Break message into correctly sized lines.
    int width = 80;
#ifdef USE_TILE
    width = crawl_view.msgsz.x;
#else
    width = std::min(80, get_number_of_cols());
#endif
    linebreak_string(error_msg, width);

    // And finally output the message.
    clrscr();
    formatted_string::parse_string(error_msg, false).display();
    getchm();
    return true;
}

void redraw_screen(void)
{
    if (!crawl_state.need_save)
    {
        // If the game hasn't started, don't do much.
        clrscr();
        return;
    }

    draw_border();

    you.redraw_hit_points   = true;
    you.redraw_magic_points = true;
    you.redraw_stats.init(true);
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;
    you.wield_change        = true;
    you.redraw_quiver       = true;

    set_redraw_status(
        REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK);

    print_stats();

    bool note_status = notes_are_active();
    activate_notes(false);
    print_stats_level();
#ifdef DGL_SIMPLE_MESSAGING
    update_message_status();
#endif
    update_turn_count();
    activate_notes(note_status);

    viewwindow();

    // Display the message window at the end because it places
    // the cursor behind possible prompts.
    display_message_window();
}

// STEPDOWN FUNCTION to replace conditional chains in spells2.cc 12jan2000 {dlb}
// it is a bit more extensible and optimises the logical structure, as well
// usage: cast_summon_swarm() cast_haunt() cast_summon_scorpions()
//        cast_summon_horrible_things()
// ex(1): stepdown_value (foo, 2, 2, 6, 8) replaces the following block:
//

/*
   if (foo > 2)
     foo = (foo - 2) / 2 + 2;
   if (foo > 4)
     foo = (foo - 4) / 2 + 4;
   if (foo > 6)
     foo = (foo - 6) / 2 + 6;
   if (foo > 8)
     foo = 8;
 */

//
// ex(2): bar = stepdown_value(bar, 2, 2, 6, -1) replaces the following block:
//

/*
   if (bar > 2)
     bar = (bar - 2) / 2 + 2;
   if (bar > 4)
     bar = (bar - 4) / 2 + 4;
   if (bar > 6)
     bar = (bar - 6) / 2 + 6;
 */

// I hope this permits easier/more experimentation with value stepdowns
// in the code.  It really needs to be rewritten to accept arbitrary
// (unevenly spaced) steppings.
int stepdown_value(int base_value, int stepping, int first_step,
                   int last_step, int ceiling_value)
{
    int return_value = base_value;

    // values up to the first "step" returned unchanged:
    if (return_value <= first_step)
        return return_value;

    for (int this_step = first_step; this_step <= last_step;
         this_step += stepping)
    {
        if (return_value > this_step)
            return_value = ((return_value - this_step) / 2) + this_step;
        else
            break;              // exit loop iff value fully "stepped down"
    }

    // "no final ceiling" == -1
    if (ceiling_value != -1 && return_value > ceiling_value)
        return ceiling_value;   // highest value to return is "ceiling"
    else
        return return_value;    // otherwise, value returned "as is"

}

int div_round_up(int num, int den)
{
    return (num / den + (num % den != 0));
}

void canned_msg(canned_message_type which_message)
{
    switch (which_message)
    {
    case MSG_SOMETHING_APPEARS:
        mprf("Something appears %s!",
             (you.species == SP_NAGA || player_mutation_level(MUT_HOOVES))
                 ? "before you" : "at your feet");
        break;
    case MSG_NOTHING_HAPPENS:
        mpr("Nothing appears to happen.");
        break;
    case MSG_YOU_UNAFFECTED:
        mpr("You are unaffected.");
        break;
    case MSG_YOU_RESIST:
        mpr("You resist.");
        learned_something_new(HINT_YOU_RESIST);
        break;
    case MSG_YOU_PARTIALLY_RESIST:
        mpr("You partially resist.");
        break;
    case MSG_TOO_BERSERK:
        mpr("You are too berserk!");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_PRESENT_FORM:
        mpr("You can't do that in your present form.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_CARRIED:
        mpr("You aren't carrying anything.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_CANNOT_DO_YET:
        mpr("You can't do that yet.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_OK:
        mpr("Okay, then.", MSGCH_PROMPT);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_UNTHINKING_ACT:
        mpr("Why would you want to do that?");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_THERE:
        mpr("There's nothing there!");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_CLOSE_ENOUGH:
        mpr("There's nothing close enough!");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NO_ENERGY:
        mpr("You don't have the energy to cast that spell.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_SPELL_FIZZLES:
        mpr("The spell fizzles.");
        break;
    case MSG_HUH:
        mpr("Huh?", MSGCH_EXAMINE_FILTER);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_EMPTY_HANDED:
        if (you.species == SP_CAT)
            mpr("Your mouth is now empty.");
        else
            mpr("You are now empty-handed.");
        break;
    case MSG_YOU_BLINK:
        mpr("You blink.");
        break;
    case MSG_STRANGE_STASIS:
        mpr("You feel a strange sense of stasis.");
        break;
    case MSG_NO_SPELLS:
        mpr("You don't know any spells.");
        break;
    case MSG_MANA_INCREASE:
        mpr("You feel your mana capacity increase.");
        break;
    case MSG_MANA_DECREASE:
        mpr("You feel your mana capacity decrease.");
        break;
    case MSG_DISORIENTED:
        mpr("You feel momentarily disoriented.");
        break;
    case MSG_TOO_HUNGRY:
        mpr("You're too hungry.");
        break;
    }
}

// Like yesno, but requires a full typed answer.
// Unlike yesno, prompt should have no trailing space.
// Returns true if the user typed "yes", false if something else or cancel.
bool yes_or_no(const char* fmt, ...)
{
    char buf[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    buf[sizeof(buf)-1] = 0;

    mprf(MSGCH_PROMPT, "%s? (Confirm with \"yes\".) ", buf);

    if (cancelable_get_line(buf, sizeof buf))
        return (false);
    if (strcasecmp(buf, "yes") != 0)
        return (false);

    return (true);
}

// jmf: general helper (should be used all over in code)
//      -- idea borrowed from Nethack
bool yesno(const char *str, bool safe, int safeanswer, bool clear_after,
           bool interrupt_delays, bool noprompt,
           const explicit_keymap *map, GotoRegion region)
{
    bool message = (region == GOTO_MSG);
    if (interrupt_delays && !crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    std::string prompt = make_stringf("%s ", str ? str : "Buggy prompt?");

    mouse_control mc(MOUSE_MODE_MORE);
    while (true)
    {
        if (!noprompt)
        {
            if (message)
                mpr(prompt.c_str(), MSGCH_PROMPT);
            else
                cprintf("%s", prompt.c_str());
        }

        int tmp = getchm(KMC_CONFIRM);

        // Prevent infinite loop if Curses HUP signal handling happens;
        // if there is no safe answer, then just save-and-exit immediately,
        // since there's no way to know if it would be better to return
        // true or false.
        if (crawl_state.seen_hups && !safeanswer)
            sighup_save_and_exit();

        if (map && map->find(tmp) != map->end())
            tmp = map->find(tmp)->second;

        if (safeanswer
            && (tmp == ' ' || key_is_escape(tmp)
                || tmp == '\r' || tmp == '\n' || crawl_state.seen_hups))
        {
            tmp = safeanswer;
        }

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || Options.easy_confirm == CONFIRM_SAFE_EASY && safe)
        {
            tmp = toupper(tmp);
        }

        if (clear_after && message)
            mesclr();

        if (tmp == 'N')
            return (false);
        else if (tmp == 'Y')
            return (true);
        else if (!noprompt)
        {
            bool upper = (!safe && crawl_state.game_is_hints_tutorial());
            const std::string pr
                = make_stringf("%s[Y]es or [N]o only, please.",
                               upper ? "Uppercase " : "");
            if (message)
                mpr(pr);
            else
                cprintf(("\n" + pr + "\n").c_str());
        }
    }
}

static std::string _list_alternative_yes(char yes1, char yes2,
                                         bool lowered = false,
                                         bool brackets = false)
{
    std::string help = "";
    bool print_yes = false;
    if (yes1 != 'Y')
    {
        if (lowered)
            help += tolower(yes1);
        else
            help += yes1;
        print_yes = true;
    }

    if (yes2 != 'Y' && yes2 != yes1)
    {
        if (print_yes)
            help += "/";

        if (lowered)
            help += tolower(yes2);
        else
            help += yes2;
        print_yes = true;
    }

    if (print_yes)
    {
        if (brackets)
            help = " (" + help + ")";
        else
            help = "/" + help;
    }

    return help;
}

static std::string _list_allowed_keys(char yes1, char yes2,
                                      bool lowered = false,
                                      bool allow_all = false)
{
    std::string result = " [";
                result += (lowered ? "(y)es" : "(Y)es");
                result += _list_alternative_yes(yes1, yes2, lowered);
                if (allow_all)
                    result += (lowered? "/(a)ll" : "/(A)ll");
                result += (lowered ? "/(n)o/(q)uit" : "/(N)o/(Q)uit");
                result += "]";

    return (result);
}

// Like yesno(), but returns 0 for no, 1 for yes, and -1 for quit.
// alt_yes and alt_yes2 allow up to two synonyms for 'Y'.
// FIXME: This function is shaping up to be a monster. Help!
int yesnoquit(const char* str, bool safe, int safeanswer, bool allow_all,
               bool clear_after, char alt_yes, char alt_yes2)
{
    if (!crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    mouse_control mc(MOUSE_MODE_MORE);

    std::string prompt =
        make_stringf("%s%s ", str ? str : "Buggy prompt?",
                     _list_allowed_keys(alt_yes, alt_yes2,
                                        safe, allow_all).c_str());
    while (true)
    {
        mpr(prompt.c_str(), MSGCH_PROMPT);

        int tmp = getchm(KMC_CONFIRM);

        if (key_is_escape(tmp) || tmp == 'q' || tmp == 'Q'
            || crawl_state.seen_hups)
        {
            return -1;
        }

        if ((tmp == ' ' || tmp == '\r' || tmp == '\n') && safeanswer)
            tmp = safeanswer;

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || safe && Options.easy_confirm == CONFIRM_SAFE_EASY)
        {
            tmp = toupper(tmp);
        }

        if (clear_after)
            mesclr();

        if (tmp == 'N')
            return 0;
        else if (tmp == 'Y' || tmp == alt_yes || tmp == alt_yes2)
            return 1;
        else if (allow_all)
        {
            if (tmp == 'A')
                return 2;
            else
            {
                bool upper = (!safe && crawl_state.game_is_hints_tutorial());
                mprf("Choose %s[Y]es%s, [N]o, [Q]uit, or [A]ll!",
                     upper ? "uppercase " : "",
                     _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
            }
        }
        else
        {
            bool upper = (!safe && crawl_state.game_is_hints_tutorial());
            mprf("%s[Y]es%s, [N]o or [Q]uit only, please.",
                 upper ? "Uppercase " : "",
                 _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
        }
    }
}

char index_to_letter(int the_index)
{
    return (the_index + ((the_index < 26) ? 'a' : ('A' - 26)));
}

int letter_to_index(int the_letter)
{
    if (the_letter >= 'a' && the_letter <= 'z')
        // returns range [0-25] {dlb}
        the_letter -= 'a';
    else if (the_letter >= 'A' && the_letter <= 'Z')
        // returns range [26-51] {dlb}
        the_letter -= ('A' - 26);

    return (the_letter);
}

maybe_bool frombool(bool b)
{
    return (b ? B_TRUE : B_FALSE);
}

bool tobool(maybe_bool mb)
{
    ASSERT (mb != B_MAYBE);
    return (mb == B_TRUE);
}

bool tobool(maybe_bool mb, bool def)
{
    switch (mb)
    {
    case B_TRUE:
        return (true);
    case B_FALSE:
        return (false);
    case B_MAYBE:
    default:
        return (def);
    }
}

//---------------------------------------------------------------
//
// prompt_for_quantity
//
// Returns -1 if ; or enter is pressed (pickup all).
// Else, returns quantity.
//---------------------------------------------------------------
int prompt_for_quantity(const char *prompt)
{
    msgwin_prompt(prompt);

    int ch = getch_ck();
    if (ch == CK_ENTER || ch == ';')
        return -1;
    else if (ch == CK_ESCAPE)
        return 0;

    macro_buf_add(ch);
    return prompt_for_int("", false);
}

//---------------------------------------------------------------
//
// prompt_for_int
//
// If nonneg, then it returns a non-negative number or -1 on fail
// If !nonneg, then it returns an integer, and 0 on fail
//
//---------------------------------------------------------------
int prompt_for_int(const char *prompt, bool nonneg)
{
    char specs[80];

    msgwin_get_line(prompt, specs, sizeof(specs));

    if (specs[0] == '\0')
        return (nonneg ? -1 : 0);

    char *end;
    int   ret = strtol(specs, &end, 10);

    if (ret < 0 && nonneg || ret == 0 && end == specs)
        ret = (nonneg ? -1 : 0);

    return (ret);
}
