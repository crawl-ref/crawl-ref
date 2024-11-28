/**
 * @file
 * @brief Crude macro-capability
**/

/*
 * The macro-implementation works like this:
 *   - getchm() works by reading characters from an internal
 *     buffer. If none are available, new characters are read into
 *     the buffer with _getch_mul().
 *   - _getch_mul() reads at least one character, but will read more
 *     if available (determined using kbhit(), which should be defined
 *     in the platform specific libraries).
 *   - Before adding the characters read into the buffer, any keybindings
 *     in the sequence are replaced (see macro_add_buf_long for the
 *     details).
 *
 * (When the above text mentions characters, it actually means int).
 */

#include "AppHdr.h"

#include "macro.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// TODO: these includes look not very general relative to how other files
// approach it...
#ifdef USE_TILE_LOCAL
#include <SDL.h>
#include <SDL_keycode.h>
#endif

#include "cio.h"
#include "command.h"
#include "files.h"
#include "initfile.h"
#include "libutil.h"
#include "menu.h"
#include "message.h"
#include "misc.h" // erase_val
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "state.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "unicode.h"
#include "version.h"

typedef deque<int> keybuf;
typedef map<keyseq,keyseq> macromap;

static macromap Keymaps[KMC_CONTEXT_COUNT];
static macromap Macros;

// does this need to have all maps for real?
static macromap *all_maps[] =
{
    &Keymaps[KMC_DEFAULT],
    &Keymaps[KMC_LEVELMAP],
    &Keymaps[KMC_TARGETING],
    &Keymaps[KMC_CONFIRM],

    &Macros,
};

static keybuf Buffer;
static keybuf SendKeysBuffer;

#define USERFUNCBASE -10000
static vector<string> userfunctions;

static vector<key_recorder*> recorders;

typedef map<string, int> name_to_cmd_map;
typedef map<int, string> cmd_to_name_map;

struct command_name
{
    command_type cmd;
    const char*  name;
};

static command_name _command_name_list[] =
{
#include "cmd-name.h"
};

static name_to_cmd_map _names_to_cmds;
static cmd_to_name_map _cmds_to_names;

struct default_binding
{
    int           key;
    command_type  cmd;
};

static default_binding _default_binding_list[] =
{
#include "cmd-keys.h"
};

typedef map<int, int> key_to_cmd_map;
typedef map<int, int> cmd_to_key_map;

static key_to_cmd_map _keys_to_cmds[KMC_CONTEXT_COUNT];
static cmd_to_key_map _cmds_to_keys[KMC_CONTEXT_COUNT];

static inline int userfunc_index(int key)
{
    int index = (key <= USERFUNCBASE ? USERFUNCBASE - key : -1);
    return index < 0 || index >= (int) userfunctions.size()? -1 : index;
}

static int userfunc_index(const keyseq &seq)
{
    if (seq.empty())
        return -1;

    return userfunc_index(seq.front());
}

bool is_userfunction(int key)
{
    return userfunc_index(key) != -1;
}

static bool is_userfunction(const keyseq &seq)
{
    return userfunc_index(seq) != -1;
}

string get_userfunction(int key)
{
    int index = userfunc_index(key);
    return index == -1 ? nullptr : userfunctions[index];
}

static string get_userfunction(const keyseq &seq)
{
    int index = userfunc_index(seq);
    return index == -1 ? nullptr : userfunctions[index];
}

static bool userfunc_referenced(int index, const macromap &mm)
{
    for (const auto &entry : mm)
        if (userfunc_index(entry.second) == index)
            return true;

    return false;
}

static bool userfunc_referenced(int index)
{
    for (auto m : all_maps)
        if (userfunc_referenced(index, *m))
            return true;

    return false;
}

// Expensive function to discard unused function names
static void userfunc_collectgarbage()
{
    for (int i = userfunctions.size() - 1; i >= 0; --i)
    {
        if (!userfunctions.empty() && !userfunc_referenced(i))
            userfunctions[i].clear();
    }
}

static int userfunc_getindex(const string &fname)
{
    if (fname.length() == 0)
        return -1;

    userfunc_collectgarbage();

    // Pass 1 to see if we have this string already
    for (int i = 0, count = userfunctions.size(); i < count; ++i)
    {
        if (userfunctions[i] == fname)
            return i;
    }

    // Pass 2 to hunt for gaps.
    for (int i = 0, count = userfunctions.size(); i < count; ++i)
    {
        if (userfunctions[i].empty())
        {
            userfunctions[i] = fname;
            return i;
        }
    }

    userfunctions.push_back(fname);
    return userfunctions.size() - 1;
}

// Returns the name of the file that contains macros.
static string get_macro_file()
{
    string dir = !Options.macro_dir.empty() ? Options.macro_dir :
                 !SysEnv.crawl_dir.empty()  ? SysEnv.crawl_dir : "";

    if (!dir.empty())
    {
#ifndef DGL_MACRO_ABSOLUTE_PATH
        if (dir[dir.length() - 1] != FILE_SEPARATOR)
            dir += FILE_SEPARATOR;
#endif
    }

#if defined(DGL_MACRO_ABSOLUTE_PATH)
    return dir.empty()? "macro.txt" : dir;
#endif

    check_mkdir("Macro directory", &dir, true);

#if defined(DGL_NAMED_MACRO_FILE)
    return dir + strip_filename_unsafe_chars(you.your_name) + "-macro.txt";
#else
    return dir + "macro.txt";
#endif
}

static void buf2keyseq(const char *buff, keyseq &k)
{
    // Sanity check
    if (!buff)
        return;

    // convert c_str to keyseq

    // Check for user functions
    if (*buff == '=' && buff[1] == '=' && buff[2] == '=' && buff[3])
    {
        int index = userfunc_getindex(buff + 3);
        if (index != -1)
            k.push_back(USERFUNCBASE - index);
    }
    else
    {
        const int len = strlen(buff);
        for (int i = 0; i < len; i++)
            k.push_back(buff[i]);
    }
}

static int _name_to_keycode(string s)
{
    // brute-force partial inversion of keycode_to_name. This could probably
    // be done much more elegantly via a map or similar..
    // Does not handle modifier keys besides ctrl (TODO)
    const string lower = lowercase(s);
    if (s[0] == '^' && s.size() == 2)
    {
        // ^A = 1, etc.
        return 1 + toupper_safe(s[1]) - 'A';
    }
    if (starts_with(lower, "np") && s.size() == 3)
    {
        switch (s[2])
        {
        case '0': return CK_NUMPAD_0;
        case '1': return CK_NUMPAD_1;
        case '2': return CK_NUMPAD_2;
        case '3': return CK_NUMPAD_3;
        case '4': return CK_NUMPAD_4;
        case '5': return CK_NUMPAD_5;
        case '6': return CK_NUMPAD_6;
        case '7': return CK_NUMPAD_7;
        case '8': return CK_NUMPAD_8;
        case '9': return CK_NUMPAD_9;
        case '*': return CK_NUMPAD_MULTIPLY;
        case '+': return CK_NUMPAD_ADD;
        case '-': return CK_NUMPAD_SUBTRACT;
        case '.': return CK_NUMPAD_DECIMAL;
        case '/': return CK_NUMPAD_DIVIDE;
        case '=': return CK_NUMPAD_EQUALS;
        default:
            return CK_NO_KEY;
        }
    }
    else if (lower == "npenter")
        return CK_NUMPAD_ENTER;
    else if (starts_with(lowercase(s), "f") && s.size() == 2)
    {
        switch (s[1])
        {
        case '0': return CK_F0;
        case '1': return CK_F1;
        case '2': return CK_F2;
        case '3': return CK_F3;
        case '4': return CK_F4;
        case '5': return CK_F5;
        case '6': return CK_F6;
        case '7': return CK_F7;
        case '8': return CK_F8;
        case '9': return CK_F9;
        default:
            return CK_NO_KEY;
        }
    }
    else if (starts_with(lower, "f1") && s.size() == 3)
    {
        switch (s[1])
        {
        case '0': return CK_F10;
        case '1': return CK_F11;
        case '2': return CK_F12;
        case '3': return CK_F13;
        case '4': return CK_F14;
        case '5': return CK_F15;
        case '6': return -280;
        case '7': return -281;
        case '8': return -282;
        case '9': return -283;
        default:
            return CK_NO_KEY;
        }
    }
    else if (lower == "backspace")
        return CK_BKSP;
    else if (lower == "tab")
        return CK_TAB;
    else if (lower == "esc")
        return CK_ESCAPE;
    else if (lower == "enter")
        return CK_ENTER;
    else if (lower == "del")
        return CK_DELETE;
    else if (lower == "up")
        return CK_UP;
    else if (lower == "down")
        return CK_DOWN;
    else if (lower == "left")
        return CK_LEFT;
    else if (lower == "right")
        return CK_RIGHT;
    else if (lower == "ins")
        return CK_INSERT;
    else if (lower == "home")
        return CK_HOME;
    else if (lower == "end")
        return CK_END;
    else if (lower == "clear")
        return CK_CLEAR;
    else if (lower == "pgup")
        return CK_PGUP;
    else if (lower == "pgdn")
        return CK_PGDN;

    return CK_NO_KEY;
}

int read_key_code(string s)
{
    if (s.empty())
        return 0;

    // try human-readable variants
    int key = _name_to_keycode(s);
    if (key != CK_NO_KEY)
        return key;

    int base = 10;
    if (s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }

    char *tail;
    return strtol(s.c_str(), &tail, base);
}

/*
 * Takes as argument a string, and returns a sequence of keys described
 * by the string. Most characters produce their own ASCII code. These
 * are the cases:
 *   \\ produces the ASCII code of a single \
 *   \{123} produces 123 (decimal)
 *   \{^A}  produces 1 (Ctrl-A)
 *   \{x40} produces 64 (hexadecimal code)
 *   \{!more} or \{!m} disables -more- prompt until the end of the macro.
 */
keyseq parse_keyseq(string s)
{
    // TODO parse readable descriptions of special keys, e.g. \{F1} or something
    int state = 0;
    keyseq v;

    if (starts_with(s, "==="))
    {
        buf2keyseq(s.c_str(), v);
        return v;
    }

    bool more_reset = false;
    for (int i = 0, size = s.length(); i < size; ++i)
    {
        char c = s[i];

        switch (state)
        {
        case 0: // Normal state
            if (c == '\\')
                state = 1;
            else
                v.push_back(c);
            break;

        case 1: // Last char is a '\'
            if (c == '\\')
            {
                state = 0;
                v.push_back(c);
            }
            else if (c == '{')
                state = 2;
            // XXX Error handling
            break;

        case 2: // Inside \{}
        {
            const string::size_type clb = s.find('}', i);
            if (clb == string::npos)
                break;

            const string arg = s.substr(i, clb - i);
            if (!more_reset && (arg == "!more" || arg == "!m"))
            {
                more_reset = true;
                v.push_back(KEY_MACRO_MORE_PROTECT);
            }
            else
            {
                const int key = read_key_code(arg);
                v.push_back(key);
            }

            state = 0;
            i = clb;
            break;
        }
        }
    }

    return v;
}

/*
 * Serialises a key sequence into a string of the format described
 * above.
 */
static string vtostr(const keyseq &seq)
{
    ostringstream s;

    const keyseq *v = &seq;
    keyseq dummy;
    if (is_userfunction(seq))
    {
        // Laboriously reconstruct the original user input
        string newseq = string("==") + get_userfunction(seq);
        buf2keyseq(newseq.c_str(), dummy);
        dummy.push_front('=');

        v = &dummy;
    }

    for (auto key : *v)
    {
        if (key <= 32 || key > 127)
        {
            if (key == KEY_MACRO_MORE_PROTECT)
                s << "\\{!more}";
            else
            {
                char buff[20];
                // TODO: get rid of negative values in user-facing uses of
                // keycodes
                snprintf(buff, sizeof(buff), "\\{%d}", key);
                s << buff;
            }
        }
        else if (key == '\\')
            s << "\\\\";
        else
            s << static_cast<char>(key);
    }

    return s.str();
}

/*
 * Add a macro (surprise, surprise).
 */
static void macro_add(macromap &mapref, keyseq key, keyseq action)
{
    mapref[key] = action;
}

/*
 * Remove a macro.
 */
static bool macro_del(macromap &mapref, keyseq key)
{
    return mapref.erase(key) != 0;
}

static void _register_expanded_keys(int num, bool reverse);

// Safely add macro-expanded keystrokes to the end of Crawl's keyboard
// buffer. Macro-expanded keystrokes will be held in a separate
// keybuffer until the primary keybuffer is empty, at which point
// they'll be added to the primary keybuffer.
void macro_sendkeys_end_add_expanded(int key)
{
    SendKeysBuffer.push_back(key);
}

static void _macro_inject_sent_keys()
{
    if (Buffer.empty() && !SendKeysBuffer.empty())
    {
        // Transfer keys from SendKeysBuffer to the main Buffer as
        // expanded keystrokes and empty SendKeysBuffer.
        macro_buf_add(SendKeysBuffer, false, true);
        SendKeysBuffer.clear();
    }
}

/*
 * Adds keypresses from a sequence into the internal keybuffer. Ignores
 * macros.
 */
void macro_buf_add(const keyseq &actions, bool reverse, bool expanded)
{
    keyseq act;
    bool need_more_reset = false;
    for (auto key : actions)
    {
        if (key == KEY_MACRO_MORE_PROTECT)
        {
            key = KEY_MACRO_DISABLE_MORE;
            need_more_reset = true;
        }
        act.push_back(key);
    }
    if (need_more_reset)
        act.push_back(KEY_MACRO_ENABLE_MORE);

    Buffer.insert(reverse? Buffer.begin() : Buffer.end(),
                   act.begin(), act.end());

    if (expanded)
        _register_expanded_keys(act.size(), reverse);
}

/*
 * Adds a single keypress into the internal keybuffer.
 */
void macro_buf_add(int key, bool reverse, bool expanded)
{
    if (reverse)
        Buffer.push_front(key);
    else
        Buffer.push_back(key);
    if (expanded)
        _register_expanded_keys(1, reverse);
}

/*
 * Add a command into the internal keybuffer.
 */
void macro_buf_add_cmd(command_type cmd, bool reverse)
{
    ASSERT_RANGE(cmd, CMD_NO_CMD + 1, CMD_MIN_SYNTHETIC);

    // There should be plenty of room between the synthetic keys
    // (KEY_MACRO_MORE_PROTECT == -10) and USERFUNCBASE (-10000) for
    // command_type to fit (currently 1000 through 2069).
    macro_buf_add(-((int) cmd), reverse, true);
}

/*
 * Adds keypresses from a sequence into the internal keybuffer. Does some
 * O(N^2) analysis to the sequence to apply keymaps.
 */
static void macro_buf_add_long(keyseq actions,
                               macromap &keymap = Keymaps[KMC_DEFAULT])
{
    keyseq tmp;

    // debug << "Adding: " << vtostr(actions) << endl;
    // debug.flush();

    // Check whether any subsequences of the sequence are macros.
    // The matching starts from as early as possible, and is
    // as long as possible given the first constraint. I.e from
    // the sequence "abcdef" and macros "ab", "bcde" and "de"
    // "ab" and "de" are recognised as macros.

    while (!actions.empty())
    {
        tmp = actions;

        while (!tmp.empty())
        {
            auto subst = keymap.find(tmp);
            // Found a macro. Add the expansion (action) of the
            // macro into the buffer.
            if (subst != keymap.end() && !subst->second.empty())
            {
                macro_buf_add(subst->second, false, false);
                break;
            }

            // Didn't find a macro. Remove a key from the end
            // of the sequence, and try again.
            tmp.pop_back();
        }

        if (tmp.empty())
        {
            // Didn't find a macro. Add the first keypress of the sequence
            // into the buffer, remove it from the sequence, and try again.
            macro_buf_add(actions.front(), false, false);
            actions.pop_front();
        }
        else
        {
            // Found a macro, which has already been added above. Now just
            // remove the macroed keys from the sequence.
            for (unsigned int i = 0; i < tmp.size(); i++)
                actions.pop_front();
        }
    }
}

// Number of keys from start of buffer that have already gone through
// macro expansion.
static int expanded_keys_left = 0;
static void _register_expanded_keys(int num, bool reverse)
{
    expanded_keys_left += num;
    if (!reverse)
    {
        // We added at the end of the buffer, so the whole buffer had
        // better be full of expanded keys.
        ASSERT((int)Buffer.size() == expanded_keys_left);
        expanded_keys_left = Buffer.size();
    }
}

static int macro_keys_left = -1;

void macro_clear_buffers()
{
    crawl_state.cancel_cmd_repeat();
    crawl_state.cancel_cmd_again();

    Buffer.clear();
    SendKeysBuffer.clear();
    expanded_keys_left = 0;
    macro_keys_left = -1;
}

bool is_processing_macro()
{
    return macro_keys_left >= 0;
}

bool has_pending_input()
{
    return !Buffer.empty() || !SendKeysBuffer.empty();
}

/*
 * Command macros are only applied from the immediate front of the
 * buffer, and only when the game is expecting a command.
 */
static void macro_buf_apply_command_macro()
{
    if (macro_keys_left > 0 || expanded_keys_left > 0)
        return;

    keyseq tmp = Buffer;

    // find the longest match from the start of the buffer and replace it
    while (!tmp.empty())
    {
        auto expansion = Macros.find(tmp);

        if (expansion != Macros.end() && !expansion->second.empty())
        {
            const keyseq &result = expansion->second;

            // Found macro, remove match from front:
            for (unsigned int i = 0; i < tmp.size(); i++)
                Buffer.pop_front();

            macro_keys_left = result.size();

            macro_buf_add(result, true, true);

            break;
        }

        tmp.pop_back();
    }
}

/*
 * Removes the earliest keypress from the keybuffer, and returns its
 * value. If buffer was empty, returns -1;
 */
int macro_buf_get()
{
    ASSERT(macro_keys_left <= (int)Buffer.size()
           && expanded_keys_left <= (int)Buffer.size());

    _macro_inject_sent_keys();

    if (Buffer.empty())
    {
        // If we're trying to fetch a new keystroke, then the processing
        // of the previous keystroke is complete.
        if (macro_keys_left == 0)
            macro_keys_left = -1;

        return -1;
    }

    int key = Buffer.front();
    Buffer.pop_front();

    if (macro_keys_left >= 0)
        macro_keys_left--;
    if (expanded_keys_left > 0)
        expanded_keys_left--;

    for (key_recorder *recorder : recorders)
        recorder->add_key(key);

    return key;
}

void process_command_on_record(command_type cmd)
{
    const int key = command_to_key(cmd);
    if (key != '\0')
        for (key_recorder *recorder : recorders)
            recorder->add_key(key);
    process_command(cmd);
}

static void write_map(FILE *f, const macromap &mp, const char *key)
{
    for (const auto &entry : mp)
    {
        // Need this check, since empty values are added into the
        // macro struct for all used keyboard commands.
        if (entry.second.size())
        {
            fprintf(f, "%s%s\nA:%s\n\n", OUTS(key),
                OUTS(vtostr(entry.first)), OUTS(vtostr(entry.second)));
        }
    }
}

/*
 * Saves macros into the macrofile, overwriting the old one.
 */
void macro_save()
{
    FILE *f;
    const string macrofile = get_macro_file();
    f = fopen_u(macrofile.c_str(), "w");
    if (!f)
    {
        mprf(MSGCH_ERROR, "Couldn't open %s for writing!", macrofile.c_str());
        return;
    }

    fprintf(f, "# %s %s macro file\n"
               "# WARNING: This file is entirely auto-generated.\n"
               "\n"
               "# Key Mappings:\n",
            OUTS(CRAWL), // ok, localizing the game name is not likely
            OUTS(Version::Long)); // nor the version string
    for (int mc = KMC_DEFAULT; mc < KMC_CONTEXT_COUNT; ++mc)
    {
        char buf[30] = "K:";
        if (mc)
            snprintf(buf, sizeof buf, "K%d:", mc);
        write_map(f, Keymaps[mc], buf);
    }

    fprintf(f, "# Command Macros:\n");
    write_map(f, Macros, "M:");

    crawl_state.unsaved_macros = false;
    fclose(f);
}

/*
 * Reads as many keypresses as are available (waiting for at least one),
 * and returns them as a single keyseq.
 */
static keyseq _getch_mul()
{
    keyseq keys;
    int    a;

    // Something's gone wrong with replaying keys if crawl needs to
    // get new keys from the user.
    if (crawl_state.is_replaying_keys())
    {
        mprf(MSGCH_ERROR, "(Key replay ran out of keys)");
        crawl_state.cancel_cmd_repeat();
        crawl_state.cancel_cmd_again();
    }

    // The a == 0 test is legacy code that I don't dare to remove. I
    // have a vague recollection of it being a kludge for conio support.
    do
    {
        a = getch_ck();
        if (a != CK_NO_KEY)
            keys.push_back(a);
    }
    while (keys.size() == 0 || ((kbhit() || a == 0) && a != CK_REDRAW));

    return keys;
}

/*
 * Replacement for getch(). Returns keys from the key buffer if available.
 * If not, adds some content to the buffer, and returns some of it.
 */
int getchm(KeymapContext mc)
{
    int a;

    // Got data from buffer.
    if ((a = macro_buf_get()) != -1)
        return a;

    // Read some keys...
    keyseq keys = _getch_mul();
    macro_buf_add_with_keymap(keys, mc);
    return macro_buf_get();
}

void macro_buf_add_with_keymap(keyseq keys, KeymapContext mc)
{
    if (mc == KMC_NONE)
        macro_buf_add(keys, false, false);
    else
        macro_buf_add_long(keys, Keymaps[mc]);
}

/**
 * Get a character?
 */
int get_ch()
{
    mouse_control mc(MOUSE_MODE_PROMPT);
    int gotched = getchm();

    if (gotched == 0)
        gotched = getchm();

    return gotched;
}

/*
 * Replacement for getch(). Returns keys from the key buffer if available.
 * If not, adds some content to the buffer, and returns some of it.
 */
int getch_with_command_macros()
{
    _macro_inject_sent_keys();

    if (Buffer.empty())
    {
        keyseq keys = _getch_mul();
        macro_buf_add_with_keymap(keys, KMC_DEFAULT);
    }

    // Apply longest matching macro at front of buffer:
    macro_buf_apply_command_macro();

    return macro_buf_get();
}

static string _buffer_to_string()
{
    string s;
    for (const int k : Buffer)
    {
        if (k > 0 && k <= numeric_limits<unsigned char>::max())
        {
            char c = static_cast<unsigned char>(k);
            if (c == '[' || c == ']')
                s += "\\";
            s += c;
        }
        else
            s += make_stringf("[%d]", k);
    }
    return s;
}

/*
 * Flush the buffer. Later we'll probably want to give the player options
 * as to when this happens (ex. always before command input, casting failed).
 */
void flush_input_buffer(int reason)
{
    ASSERT(reason != FLUSH_KEY_REPLAY_CANCEL
           || crawl_state.is_replaying_keys() || crawl_state.cmd_repeat_start);

    ASSERT(reason != FLUSH_ABORT_MACRO || is_processing_macro());

    // Any attempt to flush means that the processing of the previously
    // fetched keystroke is complete.
    if (macro_keys_left == 0)
        macro_keys_left = -1;

    if (crawl_state.is_replaying_keys() && reason != FLUSH_ABORT_MACRO
        && reason != FLUSH_KEY_REPLAY_CANCEL
        && reason != FLUSH_REPLAY_SETUP_FAILURE
        && reason != FLUSH_ON_FAILURE)
    {
        return;
    }

    if (Options.flush_input[ reason ] || reason == FLUSH_ABORT_MACRO
        || reason == FLUSH_KEY_REPLAY_CANCEL
        || reason == FLUSH_REPLAY_SETUP_FAILURE
        || reason == FLUSH_REPEAT_SETUP_DONE)
    {
        if (crawl_state.nonempty_buffer_flush_errors && !Buffer.empty())
        {
            if (you.wizard) // crash -- intended for tests
            {
                mprf(MSGCH_ERROR,
                    "Flushing non-empty key buffer (Buffer is '%s')",
                    _buffer_to_string().c_str());
                ASSERT(Buffer.empty());
            }
            else
                mprf(MSGCH_ERROR, "Flushing non-empty key buffer");
        }
        while (!Buffer.empty())
        {
            const int key = Buffer.front();
            Buffer.pop_front();
            if (key == KEY_MACRO_ENABLE_MORE)
                crawl_state.show_more_prompt = true;
        }
        macro_keys_left = -1;
        expanded_keys_left = 0;
    }
}

static string _keyseq_desc(const keyseq &key)
{
    string r = keycode_to_name(key[0], false);
    if (!keycode_is_printable(key[0]))
    {
        if (r.empty())
            r = vtostr(key);
        else
            r = make_stringf("%s (%s)", vtostr(key).c_str(), r.c_str());
    }
    return replace_all(r, "<", "<<");
}

static string _keyseq_action_desc(keyseq &action)
{
    if (action.empty())
        return "<red>[none]</red>";

    string action_str = vtostr(action);
    action_str = replace_all(action_str, "<", "<<");
    return action_str;
}

static string _keyseq_action_desc(const keyseq &key, macromap &mapref)
{
    if (!mapref.count(key))
    {
        keyseq empty;
        return _keyseq_action_desc(empty);
    }
    else
        return _keyseq_action_desc(mapref[key]);
}

class MacroEditMenu : public Menu
{
private:
    const vector<pair<KeymapContext,string>> modes =
        { { KMC_NONE, "macros" }, // not actually what KMC_NONE means
          { KMC_DEFAULT, "default" },
          { KMC_MENU, "menu" },
          { KMC_LEVELMAP, "level" },
          { KMC_TARGETING, "targeting" },
          { KMC_CONFIRM, "confirmation" }
        };

public:
    MacroEditMenu()
        : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING
                | MF_ARROWS_SELECT | MF_WRAP | MF_SPECIAL_MINUS),
          selected_new_key(false), keymc(KMC_NONE), edited_keymaps(false)
    {
        set_tag("macros");
        remap_numpad = false;
#ifdef USE_TILE_LOCAL
        set_min_col_width(MIN_COLS);
#endif
        action_cycle = Menu::CYCLE_NONE;
        menu_action  = Menu::ACT_EXECUTE;
        set_title(new MenuEntry("", MEL_TITLE));
    }

    void fill_entries(int set_hover_keycode=0)
    {
        // TODO: this seems like somehow it should involve ui::Switcher, but I
        // have no idea how to use that class with a Menu
        clear();
        add_entry(new MenuEntry("Create/edit " + mode_name() + " from key", '~',
            [this](const MenuEntry &)
                {
                    edit_mapping(keyseq());
                    return true;
                }));
        if (get_map().size())
        {
            MenuEntry *clear_entry = new MenuEntry("Clear all " + mode_name() + "s", '-',
                [this](const MenuEntry &)
                    {
                        status_msg = "";
                        update_macro_more();
                        if (item_count() > 0)
                            clear_all();
                        return true;
                    });
            // manual numpad handling for this class
            clear_entry->add_hotkey(CK_NUMPAD_SUBTRACT);
            clear_entry->add_hotkey(CK_NUMPAD_SUBTRACT2);

            add_entry(clear_entry);
            add_entry(new MenuEntry("Current " + mode_name() + "s", MEL_SUBTITLE));
            for (auto &mapping : get_map())
            {
                // TODO: indicate if macro is from rc file somehow?
                string action_str = vtostr(mapping.second);
                action_str = replace_all(action_str, "<", "<<");
                MenuEntry *me = new MenuEntry(action_str, (int) mapping.first[0],
                    [this](const MenuEntry &item)
                    {
                        if (item.data)
                            edit_mapping(*static_cast<keyseq *>(item.data));
                        return true;
                    });
                me->data = (void *) &mapping.first;
                add_entry(me);
                if (set_hover_keycode == mapping.first[0])
                    last_hovered = item_count() - 1;
            }
        }
        // update more in case menu changes between empty and non-empty
        update_macro_more();
        update_menu(true); // necessary to update webtiles menu
        if (last_hovered == -1)
            cycle_hover();
    }

    bool cycle_mode(bool forward) override
    {
        if (!forward) // XX implement reverse
            return false;
        auto it = modes.begin();
        for (; it != modes.end(); it++)
            if (it->first == keymc)
                break;
        if (it == modes.end())
            keymc = KMC_NONE;
        else
        {
            it++;
            if (it == modes.end())
                keymc = KMC_NONE;
            else
                keymc = it->first;
        }
        status_msg = "";
        update_title();
        fill_entries();
        return true;
    }

    KeymapContext get_mode() const
    {
        return keymc;
    }

    void update_macro_more()
    {
        vector<string> result;
        for (auto m : modes)
            if (keymc == m.first)
                result.push_back("<w>" + m.second + "</w>");
            else
                result.push_back(m.second);

        string hint;
        if (keymc != KMC_NONE)
            edited_keymaps = true;

        // there's much less use-case for editing keymaps in-game, so hide the
        // details by default
        string mode_hint = edited_keymaps
            ? " cycle mode: " + join_strings(result.begin(), result.end(), "|")
            : " edit keymaps";

        set_more(formatted_string::parse_string(
            status_msg + "\n"
            "Arrows/[<w>enter</w>] to select, [<w>bksp</w>] to clear selected, [<w>?</w>] for help\n"
            + menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE)
            + mode_hint));

    }

    string mode_name()
    {
        // XX less stupid implementation
        switch (keymc)
        {
        case KMC_NONE: return "macro";
        case KMC_DEFAULT: return "regular keymap";
        case KMC_MENU: return "menu keymap";
        case KMC_TARGETING: return "targeting keymap";
        case KMC_LEVELMAP: return "level map keymap";
        case KMC_CONFIRM: return "confirmation keymap";
        default: return "buggy keymap";
        }
    }

    virtual formatted_string calc_title() override
    {
        return formatted_string::parse_string(
            "Editing <w>" + mode_name() + "s</w>.");
    }

    void clear_all()
    {
        const string clear_prompt = make_stringf("Really clear all %ss?",
                mode_name().c_str());
        if (yesno(clear_prompt.c_str(), true, 'N'))
        {
            get_map() = macromap();
            status_msg = "All " + mode_name() + "s cleared!";
            crawl_state.unsaved_macros = true;
            fill_entries();
        }
    }

    macromap &get_map()
    {
        return keymc != KMC_NONE ? Keymaps[keymc] : Macros;
    }

    void clear_mapping(keyseq key)
    {
        macromap &mapref = get_map();
        if (!mapref.count(key))
            return;

        const int keycode = key[0];
        string key_str = keycode_is_printable(keycode)
            ? keycode_to_name(keycode, false).c_str()
            : make_stringf("%s (%s)",
                    vtostr(key).c_str(), keycode_to_name(keycode, false).c_str());
        string action_str = vtostr(mapref[key]);

        action_str = replace_all(action_str, "<", "<<");
        key_str = replace_all(key_str, "<", "<<");

        status_msg = make_stringf("Cleared %s '%s' => '%s'.",
                    mode_name().c_str(),
                    key_str.c_str(),
                 action_str.c_str());

        macro_del(mapref, key);
        crawl_state.unsaved_macros = true;
        fill_entries();
    }

    void clear_hovered()
    {
        if (last_hovered < 0)
            return;
        keyseq *_key_chosen = static_cast<keyseq *>(items[last_hovered]->data);
        if (!_key_chosen)
            return;

        // TODO: add a quick undo key?
        clear_mapping(*_key_chosen);
    }

    class MappingEditMenu : public Menu
    {
    public:
        MappingEditMenu(keyseq _key, keyseq _action, MacroEditMenu &_parent)
            : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING | MF_ARROWS_SELECT
                | MF_SHOW_EMPTY | MF_SPECIAL_MINUS, ""),
              key(_key), action(_action), abort(false),
              parent(_parent),
              doing_key_input(false), doing_raw_action_input(false)
        {
            set_tag("macro_mapping");
            remap_numpad = false;
#ifdef USE_TILE_LOCAL
            set_min_col_width(62); // based on `r` more width
#endif
            set_more(string(""));
            if (key.size() == 0)
                initialize_needs_key();
            else if (action.size() == 0)
            {
                reset_key_prompt();
                on_show = [this]()
                {
                    if (edit_action())
                        return false;
                    initialize_with_key();
                    update_menu(true);
                    return true;
                };
            }
            else
                initialize_with_key();
        }

        /// show the menu and edit a mapping
        /// @return whether a mapping was set
        bool input_mapping()
        {
            show();
            return !abort;
        }

        /// Initialize the menu to accept key input immediately on show
        void initialize_needs_key()
        {
            clear();
            key.clear();
            set_more(string(""));
            prompt = make_stringf(
                "Input trigger key to edit or create a %s:",
                parent.mode_name().c_str());
            set_title(new MenuEntry(prompt, MEL_TITLE));
            set_more("<lightgray>([<w>~</w>] to enter by keycode)</lightgray>");
            doing_key_input = true;
        }

        void reset_key_prompt()
        {
            prompt = make_stringf("Current %s for %s: %s",
                        parent.mode_name().c_str(),
                        _keyseq_desc(key).c_str(),
                        _keyseq_action_desc(action).c_str());

            set_title(new MenuEntry(prompt + "\n", MEL_TITLE));
        }

        /// Initialize the menu for key editing, given some key to edit
        void initialize_with_key()
        {
            ASSERT(key.size());
            clear();
            reset_key_prompt();
            set_more(string(""));

            add_entry(new MenuEntry("redefine", 'r',
                [this](const MenuEntry &)
                {
                    set_more("");
                    return !edit_action();
                }));

            add_entry(new MenuEntry("redefine with raw key entry", 'R',
                [this](const MenuEntry &)
                {
                    set_more("");
                    edit_action_raw();
                    return true;
                }));

            if (!action.empty())
            {
                add_entry(new MenuEntry("clear", 'c',
                    [this](const MenuEntry &)
                    {
                        this->action.clear();
                        return false;
                    }));
            }

            add_entry(new MenuEntry("abort", 'a',
                [this](const MenuEntry &)
                {
                    abort = true;
                    return false;
                }));

            if (last_hovered == -1)
                cycle_hover();
        }

        /// Enter raw input mode for keymaps -- allows mapping to any key
        /// except enter and esc
        void edit_action_raw()
        {
            prompt = make_stringf(
                "<w>%s</w>\nInput (raw) new %s for %s: ",
                        prompt.c_str(),
                        parent.mode_name().c_str(),
                        _keyseq_desc(key).c_str());
            set_title(new MenuEntry(prompt, MEL_TITLE));
            set_more("Raw input: [<w>esc</w>] to abort, [<w>enter</w>] to accept.");
            update_menu(true);
#ifdef USE_TILE_WEB
            // put the javascript menu mode in raw input mode
            tiles.json_open_object();
            tiles.json_write_string("msg", "title_prompt");
            tiles.json_write_bool("raw", true);
            tiles.json_close_object();
            tiles.finish_message();
#endif
            doing_raw_action_input = true;
        }

        /// edit an action, using title_prompt for text entry
        /// @return true if an action was fully set
        bool edit_action()
        {
            char buff[1024];
            const string edit_prompt = make_stringf("<w>%s</w>\nInput new %s for %s:",
                        prompt.c_str(),
                        parent.mode_name().c_str(),
                        _keyseq_desc(key).c_str());

            int old_last_hovered = last_hovered;
            set_hovered(-1);
            set_more("Input a key sequence. Use <w>\\{n}</w> to enter keycode <w>n</w>. [<w>esc</w>] for menu");
            if (!title_prompt(buff, sizeof(buff), edit_prompt.c_str()))
            {
                set_hovered(old_last_hovered);
                set_more("");
                // line reader success code is 0
                return lastch == 0;
            }

            keyseq new_action = parse_keyseq(buff);
            // it's still possible to get a blank keyseq by having parsing
            // issues with backslashes, for example
            if (!new_action.size())
            {
                set_more(make_stringf("Parsing error in key sequence '%s'", buff));
                return true;
            }
            set_hovered(old_last_hovered);
            action = new_action;
            reset_key_prompt();
            update_menu(true);
            return true;
        }

        bool process_key(int keyin)
        {
            // rebinding mouse click generally won't work
            // TODO: CK_MOUSE_Bn keys don't generally seem implemented
            if (is_synthetic_key(keyin)
                || keyin == CK_MOUSE_B2
                || keyin == 0 || keyin == CK_NO_KEY)
            {
                return true;
            }
            // stateful key processing:
            // * in raw action input mode, fill keys into raw_tmp
            // * in key input mode, fill exactly one key into `key`, either
            //   by key entry or keycode
            // * otherwise, use normal menu key handling
            //
            // regular action input as well as keycode entry use title_prompt,
            // and so are handled in the superclass
            if (doing_raw_action_input)
            {
                if (keyin == CK_MOUSE_CLICK || keyin == CK_MOUSE_B1 || keyin == CK_MOUSE_B2)
                    return true;
                if (keyin == ESCAPE || keyin == CONTROL('G'))
                {
                    doing_raw_action_input = false;
                    raw_tmp.clear();
                    set_more("");
                    reset_key_prompt();
#ifdef USE_TILE_WEB
                    tiles.json_open_object();
                    tiles.json_write_string("msg", "title_prompt");
                    tiles.json_write_bool("close", true);
                    tiles.json_close_object();
                    tiles.finish_message();
#endif
                    return true;
                }
                else if (keyin == '\r' || keyin == '\n')
                {
                    doing_raw_action_input = false;
                    if (raw_tmp.size() && raw_tmp[0] != 0)
                        action = raw_tmp;
                    set_more("");
                    reset_key_prompt();
                    return true;
                }
                raw_tmp.push_back(keyin);
                set_title(new MenuEntry(prompt + vtostr(raw_tmp)));
                return true;
            }
            else if (doing_key_input)
            {
                if (keyin == CK_MOUSE_CLICK || keyin == CK_MOUSE_B1 || keyin == CK_MOUSE_B2)
                    return true;
                doing_key_input = false;
                if (keyin == ESCAPE || keyin == CONTROL('G'))
                {
                    abort = true;
                    return false;
                }
                else if (keyin == '~')
                {
                    char buff[10];
                    set_more("[<w>?</w>] Keycode help. "
                        "Quick reference: 8: [<w>bksp</w>], "
                        "9: [<w>tab</w>], 13: [<w>enter</w>], 27: [<w>esc</w>]"
                        );
                    if (!title_prompt(buff, sizeof(buff),
                        "Enter keycode by number:"
#ifndef USE_TILE_LOCAL
                        , "console-keycodes"
#endif
                        ))
                    {
                        abort = true;
                        return false;
                    }
                    keyin = read_key_code(string(buff));
                    if (keyin == 0)
                    {
                        abort = true;
                        return false;
                    }
                }

                // intercept one key, and store it in `key`
                key.push_back(keyin); // TODO: vs _getch_mul?
                // switch to editing state and reinit the menu
                macromap &mapref = parent.get_map();
                if (!mapref.count(key))
                    action.clear();
                else
                    action = mapref[key];
                // if the mapping is new, edit immediately
                if (action.empty())
                {
                    prompt = make_stringf("%s %s", prompt.c_str(),
                                                    _keyseq_desc(key).c_str());
                    if (edit_action())
                        return false;
                }
                // TODO: this drops to the mapping edit menu at this point. It
                // would be faster to go back to the main macro menu, but this
                // allows the player to cancel. Which is better?
                initialize_with_key();
                update_menu(true);
                return true;
            }

            if (keyin == '?')
            {
                show_specific_helps({ "macro-menu"
#ifndef USE_TILE_LOCAL
                    , "console-keycodes"
#endif
                    });
            }
            else if (keyin == 'a')
                return false; // legacy key
            return Menu::process_key(keyin);
        }

        keyseq key;
        keyseq action;
        bool abort;
    protected:
        MacroEditMenu &parent;
        keyseq raw_tmp;
        bool doing_key_input;
        bool doing_raw_action_input;
        string prompt;
    };

    bool edit_mapping(keyseq key)
    {
        status_msg = "";
        update_macro_more();

        const bool existed = get_map().count(key);

        MappingEditMenu pop = MappingEditMenu(key,
            existed ? get_map()[key] : keyseq(), *this);
        if (pop.input_mapping())
        {
            if (pop.action.size()
                && (!get_map().count(pop.key) || get_map()[pop.key] != pop.action))
            {
                macro_add(get_map(), pop.key, pop.action);
                status_msg = make_stringf("%s %s '%s' => '%s'.",
                    existed ? "Redefined" : "Created",
                    mode_name().c_str(),
                    _keyseq_desc(pop.key).c_str(),
                    _keyseq_action_desc(pop.key, get_map()).c_str());
                crawl_state.unsaved_macros = true;
            }
            else if (!pop.action.size())
                clear_mapping(pop.key);
            if (pop.key.size())
                fill_entries(pop.key[0]);
            // else, we aborted
        }

        return false;
    }

    void add_mapping_from_last()
    {
        keyseq key;
        key.push_back(lastch);
        // XX could this jump right into editing?
        edit_mapping(key);
    }

    bool process_command(command_type cmd) override
    {
        switch (cmd)
        {
        case CMD_MENU_HELP:
            show_specific_helps({ "macro-menu"
#ifndef USE_TILE_LOCAL
                    , "console-keycodes"
#endif
                    });
            return true;
        default:
            return Menu::process_command(cmd);
        }
    }

    command_type get_command(int keyin) override
    {
        if (keyin == '?')
            return CMD_MENU_HELP;
        return Menu::get_command(keyin);
    }

    bool process_key(int keyin) override
    {
        // this menu overrides most superclass keyhandling. First pass some
        // special cases to the superclass unconditionally:
        switch (keyin)
        {
        case 0:
        case CK_REDRAW:
        case CK_MOUSE_CLICK:
        case CK_MOUSE_B1:
        case CK_ENTER:
        CASE_ESCAPE
        case '-': // menu item, always present
        // not generally remapping numpad, so have to manually handle these
        case CK_NUMPAD_SUBTRACT:
        case CK_NUMPAD_SUBTRACT2:
        case '~': // menu item, always present
            // then check for a few other special cases the superclass needs
            // to handle
            return Menu::process_key(keyin);
        case CK_NUMPAD_ENTER:
            // need to handle manually for this menu
            return Menu::process_key(CK_ENTER);
        case CK_DELETE:
        case CK_BKSP:
            // non-menu-item hotkey specific to this menu
            clear_hovered();
            return true;
        default:
            break;
        }

        // then check if the key would do an important menu control command,
        // pass if necessary
        command_type cmd = get_command(keyin);
        switch (cmd)
        {
            case CMD_MENU_PAGE_UP:
            case CMD_MENU_PAGE_DOWN:
            case CMD_MENU_UP:
            case CMD_MENU_DOWN:
            case CMD_MENU_SCROLL_TO_TOP:
            case CMD_MENU_SCROLL_TO_END:
            case CMD_MENU_EXIT:
            case CMD_MENU_HELP:
            case CMD_MENU_CYCLE_MODE: // somewhat weird defaults in this context?
            case CMD_MENU_CYCLE_MODE_REVERSE:
                return process_command(cmd);
            default:
                break;
        }

        // finally, handle any other key. Handles opening a submenu both if
        // a mapping exists, and if it doesn't.
        lastch = keyin;
        add_mapping_from_last();
        return true;
    }

    bool selected_new_key;
    string status_msg;
protected:
    KeymapContext keymc;
    bool edited_keymaps;
};

void macro_quick_add()
{
    MacroEditMenu menu;
    keyseq empty;
    menu.edit_mapping(empty);
    if (menu.status_msg.size())
        mpr(menu.status_msg);
    else
        canned_msg(MSG_OK);
}

void macro_menu()
{
    MacroEditMenu menu;
    menu.fill_entries();

    menu.show();

    redraw_screen();
    update_screen();
}

/*
 * Initialises the macros.
 */
static void _read_macros_from(const char* filename)
{
    if (!file_exists(filename))
        return;

    string s;
    FileLineInput f(filename);
    keyseq key, action;
    bool keymap = false;
    KeymapContext keymc = KMC_DEFAULT;

    while (!f.eof())
    {
        s = f.get_line();
        trim_string(s);  // remove white space from ends

        if (s.empty() || s[0] == '#')
            continue;    // skip comments
        else if (s.substr(0, 2) == "K:")
        {
            key = parse_keyseq(s.substr(2));
            keymap = true;
            keymc  = KMC_DEFAULT;
        }
        else if (s.length() >= 3 && s[0] == 'K' && s[2] == ':')
        {
            const KeymapContext ctx = KeymapContext(KMC_DEFAULT + s[1] - '0');
            if (ctx >= KMC_DEFAULT && ctx < KMC_CONTEXT_COUNT)
            {
                key    = parse_keyseq(s.substr(3));
                keymap = true;
                keymc  = ctx;
            }
        }
        else if (s.substr(0, 2) == "M:")
        {
            key = parse_keyseq(s.substr(2));
            keymap = false;
        }
        else if (s.substr(0, 2) == "A:")
        {
            action = parse_keyseq(s.substr(2));
            macro_add((keymap ? Keymaps[keymc] : Macros), key, action);
        }
    }
}

/*
 * Based on _read_macros_from(), reads macros or keymaps from rc files.
 *
 * @param field The orig_field (not lowercased) of the macro option (the
 * bit after "macros +=")
 *
 * @return The string of any errors which occurred, or "" if no error.
 * %s is the field argument.
 */

string read_rc_file_macro(const string& field)
{
    const int first_space = field.find(' ');

    if (first_space < 0)
        return "Cannot parse marcos += %s , there is only one argument";

    // Start by deciding what context the macro/keymap is in
    const string context = field.substr(0, first_space);

    bool keymap = false;
    KeymapContext keymc = KMC_DEFAULT;

    if (context == "K")
    {
        keymap = true;
        keymc  = KMC_DEFAULT;
    }
    else if (context.length() >= 2 && context[0] == 'K')
    {
        const KeymapContext ctx =
              KeymapContext(KMC_DEFAULT + context[1] - '0');

        if (ctx >= KMC_DEFAULT && ctx < KMC_CONTEXT_COUNT)
        {
            keymap = true;
            keymc  = ctx;
        }
    }
    else if (context == "M")
        keymap = false;
    else
        return "'" + context
                   + "' is not a valid macro or keymap context (macros += %s)";

    // Now grab the key and action to be performed
    const string key_and_action = field.substr((first_space + 1));

    const int second_space = key_and_action.find(' ');

    if (second_space < 0)
        return "Cannot parse macros += %s , there are only two arguments";

    const string macro_key_string = key_and_action.substr(0, second_space);
    const string action_string = key_and_action.substr((second_space + 1));


    keyseq key = parse_keyseq(macro_key_string);

    keyseq action = parse_keyseq(action_string);

    macro_add((keymap ? Keymaps[keymc] : Macros), key, action);

    // If we didn't save here, macros in rc files would be saved iff you also
    // changed another macro with cntrl-D and saved at the exit prompt.
    // Consistent behavior works better.
    macro_save();

    return "";
}

#ifdef DEBUG
// useful for debugging
string keyseq_to_str(const keyseq &seq)
{
    string s = "";
    for (auto k : seq)
    {
        if (k == '\n' || k == '\r')
            s += "newline";
        else if (k == '\t')
            s += "tab";
        else
            s += (char) k;
        s += ", ";
    }
    return s.size() == 0 ? s : s.substr(0, s.size() - 2);

}
#endif

bool keycode_is_printable(int keycode)
{
    switch (keycode)
    {
    case  0:
    case  8:
    case  9:
    case 27:
    case '\n':
    case '\r':
        return false;
    default:
#ifdef USE_TILE_LOCAL
        // the upper bound here is based on a comment in
        // windowmanager-sdl.cc:_translate_keysym. It could potentially be
        // applied more generally but I'm concerned about non-US keyboard
        // layouts etc. I'm also not sure how accurate it is for sdl...
        return keycode >= 32 && keycode < 256;
#elif defined(USE_TILE_WEB)
        return keycode >= 32 && keycode < 128;
#else
        return keycode >= 32;
#endif
    }
}

string keycode_to_name(int keycode, bool shorten)
{
    // nb both of these use string literal concatenation
    #define CTRL_DESC(x) (shorten ? ("^" x) : ("Ctrl-" x))
    #define NP_DESC(x) (shorten ? ("NP" x) : ("Numpad " x))

    string prefix = "";

    // ugh
    if (keycode - CK_CMD_BASE < 256)
    {
        // could still also have alt on top of this
        prefix = (shorten ? "Cmd-" : "Command-");
        keycode -= CK_CMD_BASE;
    }

    if (keycode - CK_ALT_BASE >= CK_MIN_INTERNAL && keycode - CK_ALT_BASE <= 256)
    {
        prefix += "Alt-";
        keycode -= CK_ALT_BASE;
    }

    // this is printable, but it's very confusing to try to use ' ' to print it
    // in circumstances where a name is called for
    if (keycode == ' ')
        return prefix + "Space";

    if (keycode_is_printable(keycode))
        return prefix + string(1, keycode);

    // shift/ctrl-modified keys aside from shift-tab don't seem to work on mac
    // console, and are somewhat spotty on webtiles.
    const bool shift = (keycode >= CK_SHIFT_UP && keycode <= CK_SHIFT_PGDN);
    const bool ctrl  = (keycode >= CK_CTRL_UP && keycode <= CK_CTRL_PGDN);
    const bool ctrlshift = (keycode >= CK_CTRL_SHIFT_UP && keycode <= CK_CTRL_SHIFT_PGDN);

    if (shift)
    {
        keycode -= (CK_SHIFT_UP - CK_UP);
        prefix += "Shift-";
    }
    else if (ctrl)
    {
        keycode -= (CK_CTRL_UP - CK_UP);
        prefix += CTRL_DESC("");
    }
    else if (ctrlshift)
    {
        keycode -= (CK_CTRL_SHIFT_UP - CK_UP);
        prefix += CTRL_DESC("Shift-");
    }

    // placeholder
    switch (keycode)
    {
    case  0: return "NULL";
    case  8: return prefix + "Backspace"; // CK_BKSP
    case  9: return prefix + "Tab";
    CASE_ESCAPE return prefix + "Esc";
    case '\n':
    case '\r': // CK_ENTER
        return prefix + "Enter";
    case CK_DELETE: return prefix + "Del";
    case CK_UP:     return prefix+"Up";
    case CK_DOWN:   return prefix+"Down";
    case CK_LEFT:   return prefix+"Left";
    case CK_RIGHT:  return prefix+"Right";
    case CK_INSERT: return prefix+"Ins";
    case CK_HOME:   return prefix+"Home";
    case CK_END:    return prefix+"End";
    case CK_CLEAR:  return prefix+"Clear";
    case CK_PGUP:   return prefix+"PgUp";
    case CK_PGDN:   return prefix+"PgDn";
    // ugly manual handling for these
    case CK_SHIFT_TAB:         return prefix + "Shift-Tab";
    case CK_CTRL_TAB:          return prefix + CTRL_DESC("Tab");
    case CK_CTRL_SHIFT_TAB:    return prefix + CTRL_DESC("Shift-Tab");
    case CK_SHIFT_ENTER:       return prefix + "Shift-Enter";
    case CK_CTRL_ENTER:        return prefix + CTRL_DESC("Enter");
    case CK_CTRL_SHIFT_ENTER:  return prefix + CTRL_DESC("Shift-Enter");
    case CK_SHIFT_BKSP:        return prefix + "Shift-Backspace";
    case CK_CTRL_BKSP:         return prefix + CTRL_DESC("Backspace");
    case CK_CTRL_SHIFT_BKSP:   return prefix + CTRL_DESC("Shift-Backspace");
    case CK_SHIFT_ESCAPE:      return prefix + "Shift-Esc";
    case CK_CTRL_ESCAPE:       return prefix + CTRL_DESC("Esc");
    case CK_CTRL_SHIFT_ESCAPE: return prefix + CTRL_DESC("Shift-Esc");
    case CK_SHIFT_DELETE:      return prefix + "Shift-Del";
    case CK_CTRL_DELETE:       return prefix + CTRL_DESC("Del");
    case CK_CTRL_SHIFT_DELETE: return prefix + CTRL_DESC("Shift-Del");
    case CK_SHIFT_SPACE:       return prefix + "Shift-Space";
    case CK_CTRL_SPACE:        return prefix + CTRL_DESC("Space");
    case CK_CTRL_SHIFT_SPACE:  return prefix + CTRL_DESC("Shift-Space");
    case CK_F0:     return prefix + "F0";
    case CK_F1:     return prefix + "F1";
    case CK_F2:     return prefix + "F2";
    case CK_F3:     return prefix + "F3";
    case CK_F4:     return prefix + "F4";
    case CK_F5:     return prefix + "F5";
    case CK_F6:     return prefix + "F6";
    case CK_F7:     return prefix + "F7";
    case CK_F8:     return prefix + "F8";
    case CK_F9:     return prefix + "F9";
    case CK_F10:    return prefix + "F10";
    case CK_F11:    return prefix + "F11";
    case CK_F12:    return prefix + "F12";
    case CK_F13:    return prefix + "F13";
    case CK_F14:    return prefix + "F14";
    case CK_F15:    return prefix + "F15";
    // no keycode defined, but these are available on some terminals via
    // shifted fn keys, e.g. on xterm and the like, shift-F1 is F13 etc.
    // this won't necessarily match the actual key label on most keyboards/
    // terms, but it will give some moderately useful and human readable label.
    // TODO: define keycodes? see comments in cio.h.
    case -280:      return prefix + "F16";
    case -281:      return prefix + "F17";
    case -282:      return prefix + "F18";
    case -283:      return prefix + "F19";
    case -284:      return prefix + "F20";
    case -285:      return prefix + "F21";
    case -286:      return prefix + "F22";
    case -287:      return prefix + "F23";
    case -288:      return prefix + "F24";
    case CK_NUMPAD_0: return prefix + NP_DESC("0");
    case CK_NUMPAD_1: return prefix + NP_DESC("1");
    case CK_NUMPAD_2: return prefix + NP_DESC("2");
    case CK_NUMPAD_3: return prefix + NP_DESC("3");
    case CK_NUMPAD_4: return prefix + NP_DESC("4");
    case CK_NUMPAD_5: return prefix + NP_DESC("5");
    case CK_NUMPAD_6: return prefix + NP_DESC("6");
    case CK_NUMPAD_7: return prefix + NP_DESC("7");
    case CK_NUMPAD_8: return prefix + NP_DESC("8");
    case CK_NUMPAD_9: return prefix + NP_DESC("9");
    // many of these may not actually work on any given local console:
    // TODO: confirm the names. Some stuff in libunix.cc appears to have
    // incorrect comments.
    case CK_NUMPAD_MULTIPLY: return prefix + NP_DESC("*");
    case CK_NUMPAD_ADD:      return prefix + NP_DESC("+");
    case CK_NUMPAD_ADD2:     return prefix + NP_DESC("+"); // are there keyboards with both??
    case CK_NUMPAD_SUBTRACT: return prefix + NP_DESC("-");
    case CK_NUMPAD_SUBTRACT2: return prefix + NP_DESC("-");
    case CK_NUMPAD_DECIMAL:  return prefix + NP_DESC(".");
    case CK_NUMPAD_DIVIDE:   return prefix + NP_DESC("/");
    case CK_NUMPAD_ENTER:    return prefix + NP_DESC("enter");
    case CK_NUMPAD_EQUALS:   return prefix + NP_DESC("=");
    default:
        if (keycode >= CONTROL('A') && keycode <= CONTROL('Z'))
            return make_stringf("%s%s%c", prefix.c_str(), CTRL_DESC(""), UNCONTROL(keycode));
#ifdef USE_TILE_LOCAL
        // SDL allows control modifiers for some extra punctuation
        else if (keycode < 0 && keycode > LC_CONTROL(SDLK_EXCLAIM))
            return make_stringf("%s%s%c", prefix.c_str(), CTRL_DESC(""), LC_UNCONTROL(keycode));

        // SDL uses 1 << 30 to indicate non-printable keys, crawl uses negative
        // numbers; convert back to plain SDL form
        if (keycode < 0 && ((-keycode) & 1<<30))
            keycode = -keycode;
        if (keycode < 0)
            return prefix.empty() ? "" : prefix + "\uFFFD"; // �
        // SDL_GetKeyName strips capitalization, so we don't want to use it for
        // printable keys.
        return prefix + string(SDL_GetKeyName(keycode));
#else
    {
        keyseq v;
        v.push_back(keycode);
        return vtostr(v);
    }
#endif

#undef CTRL_DESC
#undef NP_DESC
    }
}

void macro_init()
{
    for (const auto &fn : Options.additional_macro_files)
        _read_macros_from(fn.c_str());

    _read_macros_from(get_macro_file().c_str());
}

void macro_userfn(const char *keys, const char *regname)
{
    UNUSED(keys, regname);
    // TODO: Implement.
    // Converting 'keys' to a key sequence is the difficulty. Doing it portably
    // requires a mapping of key names to whatever getch() spits back, unlikely
    // to happen in a hurry.
}

bool is_synthetic_key(int key)
{
    switch (key)
    {
    case KEY_MACRO_ENABLE_MORE:
    case KEY_MACRO_DISABLE_MORE:
    case KEY_MACRO_MORE_PROTECT:
    case CK_MOUSE_CMD:
    case CK_MOUSE_MOVE:
    case CK_REDRAW:
    case CK_RESIZE:
        return true;
    default:
        return false;
    }
}

key_recorder::key_recorder()
    : paused(false)
{
    keys.clear();
}

void key_recorder::add_key(int key, bool reverse)
{
    if (paused)
        return;

    if (reverse)
        keys.push_front(key);
    else
        keys.push_back(key);
}

void key_recorder::clear()
{
    keys.clear();
}

pause_all_key_recorders::pause_all_key_recorders()
{
    for (key_recorder *rec : recorders)
    {
        prev_pause_status.push_back(rec->paused);
        rec->paused = true;
    }
}

pause_all_key_recorders::~pause_all_key_recorders()
{
    for (unsigned int i = 0; i < recorders.size(); i++)
        recorders[i]->paused = prev_pause_status[i];
}

void add_key_recorder(key_recorder* recorder)
{
    ASSERT(find(begin(recorders), end(recorders), recorder) == end(recorders));
    recorders.push_back(recorder);
}

void remove_key_recorder(key_recorder* recorder)
{
    erase_val(recorders, recorder);
}

int get_macro_buf_size()
{
    return Buffer.size();
}

///////////////////////////////////////////////////////////////
// Keybinding stuff

#define VALID_BIND_COMMAND(cmd) (cmd > CMD_NO_CMD && cmd < CMD_MIN_SYNTHETIC)

void init_keybindings()
{
    int i;

    for (i = 0; _command_name_list[i].cmd != CMD_NO_CMD
                && _command_name_list[i].name != nullptr; i++)
    {
        command_name &data = _command_name_list[i];

        ASSERT(VALID_BIND_COMMAND(data.cmd));
        ASSERT(!_names_to_cmds.count(data.name));
        ASSERT(_cmds_to_names.find(data.cmd)  == _cmds_to_names.end());

        _names_to_cmds[data.name] = data.cmd;
        _cmds_to_names[data.cmd]  = data.name;
    }

    ASSERT(i >= 130);

    for (i = 0; _default_binding_list[i].cmd != CMD_NO_CMD
                && _default_binding_list[i].key != '\0'; i++)
    {
        default_binding &data = _default_binding_list[i];
        ASSERT(VALID_BIND_COMMAND(data.cmd));

        KeymapContext context = context_for_command(data.cmd);

        ASSERT(context < KMC_CONTEXT_COUNT);

        key_to_cmd_map &key_map = _keys_to_cmds[context];
        cmd_to_key_map &cmd_map = _cmds_to_keys[context];

        // Only one command per key, but it's okay to have several
        // keys map to the same command.
        ASSERTM(!key_map.count(data.key), "bad mapping for key '%c'", data.key);

        key_map[data.key] = data.cmd;
        cmd_map[data.cmd] = data.key;
    }

    ASSERT(i >= 130);
}

command_type name_to_command(string name)
{
    return static_cast<command_type>(lookup(_names_to_cmds, name, CMD_NO_CMD));
}

string command_to_name(command_type cmd)
{
    return lookup(_cmds_to_names, cmd, "CMD_NO_CMD");
}

command_type key_to_command(int key, KeymapContext context)
{
    if (CMD_NO_CMD < -key && -key < CMD_MIN_SYNTHETIC)
    {
        const auto cmd = static_cast<command_type>(-key);
        const auto cmd_context = context_for_command(cmd);

        if (cmd == CMD_NO_CMD)
            return CMD_NO_CMD;

        if (cmd_context != context)
        {
            mprf(MSGCH_ERROR,
                 "key_to_command(): command '%s' (%d:%d) wrong for desired "
                 "context %d",
                 command_to_name(cmd).c_str(), -key - CMD_NO_CMD,
                 CMD_MAX_CMD + key, (int) context);
            if (is_processing_macro())
                flush_input_buffer(FLUSH_ABORT_MACRO);
            if (crawl_state.is_replaying_keys()
                || crawl_state.cmd_repeat_start)
            {
                flush_input_buffer(FLUSH_KEY_REPLAY_CANCEL);
            }
            flush_input_buffer(FLUSH_BEFORE_COMMAND);
            return CMD_NO_CMD;
        }
        return cmd;
    }

    const auto cmd = static_cast<command_type>(lookup(_keys_to_cmds[context],
                                                      key, CMD_NO_CMD));

    ASSERT(cmd == CMD_NO_CMD || context_for_command(cmd) == context);

    return cmd;
}

int command_to_key(command_type cmd)
{
    KeymapContext context = context_for_command(cmd);

    if (context == KMC_NONE)
        return '\0';

    return lookup(_cmds_to_keys[context], cmd, '\0');
}

vector<int> command_to_keys(command_type cmd)
{
    KeymapContext context = context_for_command(cmd);
    vector<int> result;

    if (context == KMC_NONE)
        return result;

    for (auto pair : _cmds_to_keys[context])
        if (pair.first == cmd)
            result.push_back(pair.second);
    return result;
}

KeymapContext context_for_command(command_type cmd)
{
    if (cmd > CMD_NO_CMD && cmd <= CMD_MAX_NORMAL)
        return KMC_DEFAULT;

    if (cmd >= CMD_MIN_OVERMAP && cmd <= CMD_MAX_OVERMAP)
        return KMC_LEVELMAP;

    if (cmd >= CMD_MIN_TARGET && cmd <= CMD_MAX_TARGET)
        return KMC_TARGETING;

    if (cmd >= CMD_MIN_MENU && cmd <= CMD_MAX_MENU)
        return KMC_MENU;

    if (cmd >= CMD_MIN_MENU_MS && cmd <= CMD_MAX_MENU_MS)
        return KMC_MENU_MULTISELECT;

#ifdef USE_TILE
    if (cmd >= CMD_MIN_DOLL && cmd <= CMD_MAX_DOLL)
        return KMC_DOLL;
#endif

    return KMC_NONE;
}

static bool _allow_rebinding(int key, KeymapContext context)
{
    // CK_REDRAW, CK_MOUSE_CMD, CK_MOUSE_MOVE are covered by `is_synthetic_key`
    if (context == KMC_MENU || context == KMC_MENU_MULTISELECT
        || context == KMC_TARGETING || context == KMC_LEVELMAP)
    {
        // prevent rebinding certain keys in popups/menus that would break
        // the UI too much. (These can still be modified with a keymap if you
        // really do want to break things.)
        switch (key)
        {
        CASE_ESCAPE
        case CK_MOUSE_CLICK:
        case CK_MOUSE_B2:
            return false;
        default:
            break;
        }
        // XX possibly need to prevent removing most default KMC_MENU bindings
        // so as to not break webtiles?
    }
    return true;
}

void bind_command_to_key(command_type cmd, int key)
{
    KeymapContext context = context_for_command(cmd);
    string   command_name = command_to_name(cmd);

    if (context == KMC_NONE || command_name == "CMD_NO_CMD"
        || !VALID_BIND_COMMAND(cmd))
    {
        if (command_name == "CMD_NO_CMD")
        {
            mprf(MSGCH_ERROR, "Cannot bind command #%d to a key.",
                 (int) cmd);
            return;
        }

        mprf(MSGCH_ERROR, "Cannot bind command '%s' to a key.",
             command_name.c_str());
        return;
    }
#ifdef USE_TILE_WEB
    else if ((context == KMC_MENU || context == KMC_MENU_MULTISELECT)
        && tiles.is_controlled_from_web())
    {
        // disable because they interact badly with webtiles client code.
        // TODO: is there any way to get these to work on webtiles?
        mprf(MSGCH_ERROR, "Ignoring menu bindkey of %s for webtiles client",
            command_name.c_str());
        return;
    }
#endif

    if (is_userfunction(key))
    {
        mprf(MSGCH_ERROR, "Cannot bind user function keys to a command.");
        return;
    }

    if (is_synthetic_key(key))
    {
        mprf(MSGCH_ERROR, "Cannot bind synthetic keys to a command.");
        return;
    }

    if (!_allow_rebinding(key, context))
    {
        mprf(MSGCH_ERROR, "Key %d in context %d cannot be rebound.",
            key, (int) context);
        return;
    }

    // We're good.
    key_to_cmd_map &key_map = _keys_to_cmds[context];
    cmd_to_key_map &cmd_map = _cmds_to_keys[context];

    key_map[key] = cmd;
    cmd_map[cmd] = key;
}

string command_to_string(command_type cmd, bool tutorial)
{
    const int key = command_to_key(cmd);

    string result;
    if (key >= 32 && key < 256)
    {
        if (tutorial && key >= 'A' && key <= 'Z')
            result = make_stringf("uppercase %c", (char) key);
        else
            result = string(1, (char) key);
    }
    else
        result = keycode_to_name(key, false);

    return result;
}

void insert_commands(string &desc, const vector<command_type> &cmds, bool formatted)
{
    desc = untag_tiles_console(desc);
    for (command_type cmd : cmds)
    {
        const string::size_type found = desc.find("%");
        if (found == string::npos)
            break;

        string command_name = command_to_string(cmd);
        if (formatted && command_name == "<")
            command_name += "<";
        else if (command_name == "%")
            command_name = "percent";

        desc.replace(found, 1, command_name);
    }
    desc = replace_all(desc, "percent", "%");
}
