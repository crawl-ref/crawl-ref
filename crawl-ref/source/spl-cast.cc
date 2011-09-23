/**
 * @file
 * @brief Spell casting and miscast functions.
**/

#include "AppHdr.h"

#include <sstream>
#include <iomanip>

#include "spl-cast.h"

#include "externs.h"
#include "options.h"

#include "areas.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "describe.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "food.h"
#include "format.h"
#include "godabil.h"
#include "godconduct.h"
#include "goditem.h"
#include "hints.h"
#include "invent.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "map_knowledge.h"
#include "menu.h"
#include "misc.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-wpnench.h"
#include "spl-zap.h"
#include "state.h"
#include "stuff.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "view.h"

static bool _surge_identify_boosters(spell_type spell)
{
    const unsigned int typeflags = get_spell_disciplines(spell);
    if ((typeflags & SPTYP_FIRE) || (typeflags & SPTYP_ICE))
    {
        int num_unknown = 0;
        for (int i = EQ_LEFT_RING; i < NUM_EQUIP; ++i)
        {
            if (i == EQ_AMULET)
                continue;

            if (player_wearing_slot(i)
                && !item_type_known(you.inv[you.equip[i]]))
            {
                ++num_unknown;
            }
        }

        // We can also identify cases with two unknown rings, both
        // of fire (or both of ice)...let's skip it.
        if (num_unknown == 1)
        {
            for (int i = EQ_LEFT_RING; i < NUM_EQUIP; ++i)
            {
                if (i == EQ_AMULET)
                    continue;

                if (player_wearing_slot(i))
                {
                    item_def& ring = you.inv[you.equip[i]];
                    if (!item_ident(ring, ISFLAG_KNOW_PROPERTIES)
                        && (ring.sub_type == RING_FIRE
                            || ring.sub_type == RING_ICE))
                    {
                        set_ident_type(ring.base_type, ring.sub_type,
                                        ID_KNOWN_TYPE);
                        set_ident_flags(ring, ISFLAG_KNOW_PROPERTIES);
                        mprf("You are wearing: %s",
                             ring.name(DESC_INVENTORY_EQUIP).c_str());
                    }
                }
            }
            return (true);
        }
    }
    return (false);
}

static void _surge_power(spell_type spell)
{
    int enhanced = 0;

    _surge_identify_boosters(spell);
    enhanced += spell_enhancement(get_spell_disciplines(spell));

    if (enhanced)               // one way or the other {dlb}
    {
        mprf("You feel a%s %s",
             (enhanced < -2)  ? "n extraordinarily" :
             (enhanced == -2) ? "n extremely" :
             (enhanced == 2)  ? " strong" :
             (enhanced > 2)   ? " huge"
                              : "",
             (enhanced < 0) ? "numb sensation."
                            : "surge of power!");
    }
}

static std::string _spell_base_description(spell_type spell)
{
    std::ostringstream desc;

    int highlight =  spell_highlight_by_utility(spell, COL_UNKNOWN, true);

    desc << "<" << colour_to_str(highlight) << ">" << std::left;

    // spell name
    desc << chop_string(spell_title(spell), 30);

    // spell schools
    desc << spell_schools_string(spell);

    const int so_far = strwidth(desc.str()) - (strwidth(colour_to_str(highlight))+2);
    if (so_far < 60)
        desc << std::string(60 - so_far, ' ');

    // spell fail rate, level
    desc << chop_string(failure_rate_to_string(spell_fail(spell)), 12)
         << spell_difficulty(spell);
    desc << "</" << colour_to_str(highlight) <<">";

    return desc.str();
}

static std::string _spell_extra_description(spell_type spell)
{
    std::ostringstream desc;

    int highlight =  spell_highlight_by_utility(spell, COL_UNKNOWN, true);

    desc << "<" << colour_to_str(highlight) << ">" << std::left;

    // spell name
    desc << chop_string(spell_title(spell), 30);

    // spell power, spell range, hunger level, level
    const std::string rangestring = spell_range_string(spell);

    desc << chop_string(spell_power_string(spell), 14)
         << chop_string(rangestring, 16 + tagged_string_tag_length(rangestring))
         << chop_string(spell_hunger_string(spell), 12)
         << spell_difficulty(spell);

    desc << "</" << colour_to_str(highlight) <<">";

    return desc.str();
}

// selector is a boolean function that filters spells according
// to certain criteria. Currently used for Tiles to distinguish
// spells targeted on player vs. spells targeted on monsters.
int list_spells(bool toggle_with_I, bool viewing, bool allow_preselect,
                int minRange, spell_selector selector)
{
    if (toggle_with_I && get_spell_by_letter('I') != SPELL_NO_SPELL)
        toggle_with_I = false;

#ifdef USE_TILE_LOCAL
    const bool text_only = false;
#else
    const bool text_only = true;
#endif

    ToggleableMenu spell_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                              | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING,
                              text_only);
#ifdef USE_TILE_LOCAL
    {
        // [enne] - Hack.  Make title an item so that it's aligned.
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(
                " Your Spells                       Type          "
                "                Success   Level",
                " Your Spells                       Power         "
                "Range           Hunger    Level",
                MEL_ITEM);
        me->colour = BLUE;
        spell_menu.add_entry(me);
    }
#else
    spell_menu.set_title(
        new ToggleableMenuEntry(
            " Your Spells                       Type          "
            "                Success   Level",
            " Your Spells                       Power         "
            "Range           Hunger    Level",
            MEL_TITLE));
#endif
    spell_menu.set_highlighter(NULL);
    spell_menu.set_tag("spell");
    spell_menu.add_toggle_key('!');

    std::string more_str = "Press '!' ";
    if (toggle_with_I)
    {
        spell_menu.add_toggle_key('I');
        more_str += "or 'I' ";
    }
    if (!viewing)
        spell_menu.menu_action = Menu::ACT_EXECUTE;
    more_str += "to toggle spell view.";
    spell_menu.set_more(formatted_string(more_str));

    // If there's only a single spell in the offered spell list,
    // taking the selector function into account, preselect that one.
    bool preselect_first = false;
    if (allow_preselect)
    {
        int count = 0;
        if (you.spell_no == 1)
            count = 1;
        else if (selector)
        {
            for (int i = 0; i < 52; ++i)
            {
                const char letter = index_to_letter(i);
                const spell_type spell = get_spell_by_letter(letter);
                if (!is_valid_spell(spell) || !(*selector)(spell))
                    continue;

                // Break out early if we've got > 1 spells.
                if (++count > 1)
                    break;
            }
        }
        // Preselect the first spell if it's only spell applicable.
        preselect_first = (count == 1);
    }
    if (allow_preselect || preselect_first
                           && you.last_cast_spell != SPELL_NO_SPELL)
    {
        spell_menu.set_flags(spell_menu.get_flags() | MF_PRESELECTED);
    }

    for (int i = 0; i < 52; ++i)
    {
        const char letter = index_to_letter(i);
        const spell_type spell = get_spell_by_letter(letter);

        if (!is_valid_spell(spell))
            continue;

        if (selector && !(*selector)(spell))
            continue;

        bool preselect = (preselect_first
                          || allow_preselect && you.last_cast_spell == spell);

        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(_spell_base_description(spell),
                                    _spell_extra_description(spell),
                                    MEL_ITEM, 1, letter, preselect);

#ifdef USE_TILE_LOCAL
        me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif
        spell_menu.add_entry(me);
    }

    while (true)
    {
        std::vector<MenuEntry*> sel = spell_menu.show();
        if (!crawl_state.doing_prev_cmd_again)
            redraw_screen();
        if (sel.empty())
            return 0;

        ASSERT(sel.size() == 1);
        ASSERT(sel[0]->hotkeys.size() == 1);
        if (spell_menu.menu_action == Menu::ACT_EXAMINE)
        {
            describe_spell(get_spell_by_letter(sel[0]->hotkeys[0]));
            redraw_screen();
        }
        else
            return sel[0]->hotkeys[0];
    }
}

static int _apply_spellcasting_success_boosts(spell_type spell, int chance)
{
    int wizardry = player_mag_abil(false);
    int fail_reduce = 100;
    int wiz_factor = 87;

    if (you.religion == GOD_VEHUMET
        && !player_under_penance() && you.piety >= piety_breakpoint(1)
        && vehumet_supports_spell(spell))
    {
        // [dshaligram] Fail rate multiplier used to be .5, scaled
        // back to 67%.
        fail_reduce = fail_reduce * 67 / 100;
    }

    // [dshaligram] Apply wizardry factor here, rather than mixed into the
    // pre-scaling spell power.
    while (wizardry-- > 0)
    {
        fail_reduce  = fail_reduce * wiz_factor / 100;
        wiz_factor  += (100 - wiz_factor) / 3;
    }

    // Apply Brilliance factor here.
    if (you.duration[DUR_BRILLIANCE])
        fail_reduce = fail_reduce * 67 / 100;

    // Draconians get a boost to dragon-form.
    if (spell == SPELL_DRAGON_FORM && player_genus(GENPC_DRACONIAN))
        fail_reduce = fail_reduce * 70 / 100;

    // Hard cap on fail rate reduction.
    if (fail_reduce < 50)
        fail_reduce = 50;

    return (chance * fail_reduce / 100);
}

int spell_fail(spell_type spell)
{
    int chance = 60;
    int chance2 = 0;

    // Don't cap power for failure rate purposes.
    chance -= 6 * calc_spell_power(spell, false, true, false);
    chance -= (you.intel() * 2);

    const int armour_shield_penalty = player_armour_shield_spell_penalty();
    dprf("Armour+Shield spell failure penalty: %d", armour_shield_penalty);
    chance += armour_shield_penalty;

    switch (spell_difficulty(spell))
    {
    case  1: chance +=   3; break;
    case  2: chance +=  15; break;
    case  3: chance +=  35; break;
    case  4: chance +=  70; break;
    case  5: chance += 100; break;
    case  6: chance += 150; break;
    case  7: chance += 200; break;
    case  8: chance += 260; break;
    case  9: chance += 330; break;
    case 10: chance += 420; break;
    case 11: chance += 500; break;
    case 12: chance += 600; break;
    default: chance += 750; break;
    }

    chance2 = chance;

    const int chance_breaks[][2] = {
        {45, 45}, {42, 43}, {38, 41}, {35, 40}, {32, 38}, {28, 36},
        {22, 34}, {16, 32}, {10, 30}, {2, 28}, {-7, 26}, {-12, 24},
        {-18, 22}, {-24, 20}, {-30, 18}, {-38, 16}, {-46, 14},
        {-60, 12}, {-80, 10}, {-100, 8}, {-120, 6}, {-140, 4},
        {-160, 2}, {-180, 0}
    };

    for (unsigned int i = 0; i < ARRAYSZ(chance_breaks); ++i)
        if (chance < chance_breaks[i][0])
            chance2 = chance_breaks[i][1];

    if (you.duration[DUR_TRANSFORMATION] > 0)
    {
        switch (you.form)
        {
        case TRAN_BLADE_HANDS:
            chance2 += 20;
            break;

        case TRAN_SPIDER:
        case TRAN_BAT:
            chance2 += 10;
            break;

        default:
            break;
        }
    }

    // Apply the effects of Vehumet and items of wizardry.
    chance2 = _apply_spellcasting_success_boosts(spell, chance2);

    if (chance2 > 100)
        chance2 = 100;

    return (chance2);
}

int calc_spell_power(spell_type spell, bool apply_intel, bool fail_rate_check,
                     bool cap_power, bool rod)
{
    int power = 0;
    if (rod)
        power = 5 + you.skill(SK_EVOCATIONS, 3);
    else
    {
        int enhanced = 0;

        unsigned int disciplines = get_spell_disciplines(spell);

        //jmf: evil evil evil -- exclude HOLY bit
        disciplines &= (~SPTYP_HOLY);

        int skillcount = count_bits(disciplines);
        if (skillcount)
        {
            for (int ndx = 0; ndx <= SPTYP_LAST_EXPONENT; ndx++)
            {
                unsigned int bit = (1 << ndx);
                if (disciplines & bit)
                    power += you.skill(spell_type2skill(bit), 200);
            }
            power /= skillcount;
        }

        power += you.skill(SK_SPELLCASTING, 50);

        // Brilliance boosts spell power a bit (equivalent to three
        // spell school levels).
        if (!fail_rate_check && you.duration[DUR_BRILLIANCE])
            power += 600;

        if (apply_intel)
            power = (power * you.intel()) / 10;

        // [dshaligram] Enhancers don't affect fail rates any more, only spell
        // power. Note that this does not affect Vehumet's boost in castability.
        if (!fail_rate_check)
            enhanced = spell_enhancement(disciplines);

        if (enhanced > 0)
        {
            for (int i = 0; i < enhanced; i++)
            {
                power *= 15;
                power /= 10;
            }
        }
        else if (enhanced < 0)
        {
            for (int i = enhanced; i < 0; i++)
                power /= 2;
        }

        power = stepdown_value(power / 100, 50, 50, 150, 200);
    }

    const int cap = spell_power_cap(spell);
    if (cap > 0 && cap_power)
        power = std::min(power, cap);

    return (power);
}


int spell_enhancement(unsigned int typeflags)
{
    int enhanced = 0;

    if (typeflags & SPTYP_CONJURATION)
        enhanced += player_spec_conj();

    if (typeflags & (SPTYP_HEXES|SPTYP_CHARMS))
        enhanced += player_spec_ench();

    if (typeflags & SPTYP_SUMMONING)
        enhanced += player_spec_summ();

    if (typeflags & SPTYP_POISON)
        enhanced += player_spec_poison();

    if (typeflags & SPTYP_NECROMANCY)
        enhanced += player_spec_death() - player_spec_holy();

    if (typeflags & SPTYP_FIRE)
    {
        enhanced += player_spec_fire() - player_spec_cold();

        // if it's raining... {due}
        if (in_what_cloud(CLOUD_RAIN))
            enhanced--;
    }

    if (typeflags & SPTYP_ICE)
        enhanced += player_spec_cold() - player_spec_fire();

    if (typeflags & SPTYP_EARTH)
        enhanced += player_spec_earth() - player_spec_air();

    if (typeflags & SPTYP_AIR)
        enhanced += player_spec_air() - player_spec_earth();

    if (you.attribute[ATTR_SHADOWS])
        enhanced -= 2;

    if (player_equip_ego_type(EQ_BODY_ARMOUR, SPARM_ARCHMAGI))
        enhanced++;

    // These are used in an exponential way, so we'll limit them a bit. -- bwr
    if (enhanced > 3)
        enhanced = 3;
    else if (enhanced < -3)
        enhanced = -3;

    return (enhanced);
}

void inspect_spells()
{
    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        return;
    }

    // Maybe we should honour auto_list here, but if you want the
    // description, you probably want the listing, too.
    list_spells(true, true);
}

static bool _can_cast()
{
    if (player_in_bat_form() || you.form == TRAN_PIG)
    {
        canned_msg(MSG_PRESENT_FORM);
        return false;
    }

    if (you.stat_zero[STAT_INT])
    {
        mpr("You lack the mental capacity to cast spells.");
        return false;
    }

    // Randart weapons.
    if (scan_artefacts(ARTP_PREVENT_SPELLCASTING))
    {
        mpr("Something interferes with your magic!");
        return false;
    }

    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (silenced(you.pos()))
    {
        mpr("You cannot cast spells when silenced!");
        more();
        return false;
    }

    return true;
}

void do_cast_spell_cmd(bool force)
{
    if (!cast_a_spell(!force))
        flush_input_buffer(FLUSH_ON_FAILURE);
}

// Returns false if spell failed, and true otherwise.
bool cast_a_spell(bool check_range, spell_type spell)
{
    if (!_can_cast())
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    if (crawl_state.game_is_hints())
        Hints.hints_spell_counter++;

    const int minRange = get_dist_to_nearest_monster();

    if (spell == SPELL_NO_SPELL)
    {
        int keyin = 0;

        while (true)
        {
            if (keyin == 0)
            {
                if (you.spell_no == 1)
                {
                    // Set last_cast_spell to the current only spell.
                    for (int i = 0; i < 52; ++i)
                    {
                        const char letter = index_to_letter(i);
                        const spell_type spl = get_spell_by_letter(letter);

                        if (!is_valid_spell(spl))
                            continue;

                        you.last_cast_spell = spl;
                        break;
                    }
                }

                if (you.last_cast_spell == SPELL_NO_SPELL)
                    mpr("Cast which spell? (? or * to list) ", MSGCH_PROMPT);
                else
                {
                    mprf(MSGCH_PROMPT, "Casting: <w>%s</w>",
                         spell_title(you.last_cast_spell));
                    mpr("Confirm with . or Enter, or press ? or * to list all spells.", MSGCH_PROMPT);
                }

                keyin = get_ch();
            }

            if (keyin == '?' || keyin == '*')
            {
                keyin = list_spells(true, false, minRange);
                if (!keyin)
                    keyin = ESCAPE;

                if (!crawl_state.doing_prev_cmd_again)
                    redraw_screen();

                if (isaalpha(keyin) || key_is_escape(keyin))
                    break;
                else
                    mesclr();

                keyin = 0;
            }
            else
                break;
        }

        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return (false);
        }
        else if (keyin == '.' || keyin == CK_ENTER)
        {
            spell = you.last_cast_spell;
        }
        else if (!isaalpha(keyin))
        {
            mpr("You don't know that spell.");
            crawl_state.zero_turns_taken();
            return (false);
        }
        else
        {
            spell = get_spell_by_letter(keyin);
        }
    }

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    if (spell_mana(spell) > you.magic_points)
    {
        mpr("You don't have enough magic to cast that spell.");
        return (false);
    }

    if (check_range && spell_no_hostile_in_range(spell, minRange))
    {
        // Abort if there are no hostiles within range, but flash the range
        // markers for a short while.
        mpr("There are no visible monsters within range! (Use <w>Z</w> to "
            "cast anyway.)");

        if (Options.darken_beyond_range)
        {
            targetter_smite range(&you, calc_spell_range(spell), 0, 0, true);
            range_view_annotator show_range(&range);
            delay(50);
        }
        return (false);
    }

    if (!you.is_undead
        && (you.hunger_state == HS_STARVING
            || you.hunger <= spell_hunger(spell)))
    {
        canned_msg(MSG_NO_ENERGY);
        return (false);
    }

    if (god_hates_spell(spell, you.religion))
    {
        // None currently dock just piety, right?
        if (!yesno(god_loathes_spell(spell, you.religion) ?
            "<lightred>Casting this spell will cause instant excommunication!"
                "<lighred> Really cast?" :
            "Casting this spell will put you into penance. Really cast?",
            true, 'n'))
        {
            crawl_state.zero_turns_taken();
            return (false);
        }
    }

    const bool staff_energy = player_energy();
    if (you.confused())
        random_uselessness();
    else
    {
        you.last_cast_spell = spell;
        const spret_type cast_result = your_spells(spell, 0, true, check_range);
        if (cast_result == SPRET_ABORT)
        {
            crawl_state.zero_turns_taken();
            return (false);
        }

        if (cast_result == SPRET_SUCCESS)
        {
            practise(EX_DID_CAST, spell);
            did_god_conduct(DID_SPELL_CASTING, 1 + random2(5));
            if (you.spell_usage.find(spell) == you.spell_usage.end())
                you.spell_usage[spell].init(0);
            you.spell_usage[spell][you.experience_level - 1]++;
        }
        else
            practise(EX_DID_MISCAST, spell);
    }

    dec_mp(spell_mana(spell));

    if (!staff_energy && you.is_undead != US_UNDEAD)
    {
        const int spellh = calc_hunger(spell_hunger(spell));
        if (spellh > 0)
        {
            make_hungry(spellh, true);
            learned_something_new(HINT_SPELL_HUNGER);
        }
    }

    you.turn_is_over = true;
    alert_nearby_monsters();

    return (true);
}

static void _spellcasting_side_effects(spell_type spell, int pow, god_type god)
{
    // If you are casting while a god is acting, then don't do conducts.
    // (Presumably Xom is forcing you to cast a spell.)
    if (is_holy_spell(spell) && !crawl_state.is_god_acting())
        did_god_conduct(DID_HOLY, 10 + spell_difficulty(spell));

    if (is_unholy_spell(spell) && !crawl_state.is_god_acting())
        did_god_conduct(DID_UNHOLY, 10 + spell_difficulty(spell));

    if (is_unclean_spell(spell) && !crawl_state.is_god_acting())
        did_god_conduct(DID_UNCLEAN, 10 + spell_difficulty(spell));

    if (is_chaotic_spell(spell) && !crawl_state.is_god_acting())
        did_god_conduct(DID_CHAOS, 10 + spell_difficulty(spell));

    if (is_corpse_violating_spell(spell) && !crawl_state.is_god_acting())
        did_god_conduct(DID_CORPSE_VIOLATION, 10 + spell_difficulty(spell));

    // Linley says: Condensation Shield needs some disadvantages to keep
    // it from being a no-brainer... this isn't much, but its a start. - bwr
    if (spell_typematch(spell, SPTYP_FIRE))
        expose_player_to_element(BEAM_FIRE, pow * 3, false);

    if (spell_typematch(spell, SPTYP_NECROMANCY)
        && !crawl_state.is_god_acting())
    {
        did_god_conduct(DID_NECROMANCY, 10 + spell_difficulty(spell));

        if (spell == SPELL_NECROMUTATION && is_good_god(you.religion))
            excommunication();
    }
    if (spell == SPELL_STATUE_FORM && you.religion == GOD_YREDELEMNUL
        && !crawl_state.is_god_acting())
    {
        excommunication();
    }

    // Make some noise if it's actually the player casting.
    if (god == GOD_NO_GOD)
        noisy(spell_noise(spell), you.pos());

    alert_nearby_monsters();
}

static bool _vampire_cannot_cast(spell_type spell)
{
    if (you.species != SP_VAMPIRE)
        return (false);

    if (you.hunger_state > HS_SATIATED)
        return (false);

    // Satiated or less
    switch (spell)
    {
    case SPELL_BLADE_HANDS:
    case SPELL_CURE_POISON:
    case SPELL_DRAGON_FORM:
    case SPELL_ICE_FORM:
    case SPELL_SPIDER_FORM:
    case SPELL_STATUE_FORM:
    case SPELL_STONESKIN:
        return (true);
    default:
        return (false);
    }
}

bool is_prevented_teleport(spell_type spell)
{
    return ((spell == SPELL_BLINK
           || spell == SPELL_CONTROLLED_BLINK
           || spell == SPELL_TELEPORT_SELF)
           && item_blocks_teleport(false, false));
}

bool spell_is_uncastable(spell_type spell, std::string &msg)
{
    if (you.undead_or_demonic() && is_holy_spell(spell))
    {
        msg = "You can't use this type of magic!";
        return (true);
    }

    // Normally undead can't memorise these spells, so this check is
    // to catch those in Lich form.  As such, we allow the Lich form
    // to be extended here. - bwr
    if (spell != SPELL_NECROMUTATION && you_cannot_memorise(spell))
    {
        msg = "You cannot cast that spell in your current form!";
        return (true);
    }

    if (_vampire_cannot_cast(spell))
    {
        msg = "Your current blood level is not sufficient to cast that spell.";
        return (true);
    }

    return (false);
}

#ifdef WIZARD
static void _try_monster_cast(spell_type spell, int powc,
                              dist &spd, bolt &beam)
{
    if (monster_at(you.pos()))
    {
        mpr("Couldn't try casting monster spell because you're "
            "on top of a monster.");
        return;
    }

    monster* mon = get_free_monster();
    if (!mon)
    {
        mpr("Couldn't try casting monster spell because there is "
            "no empty monster slot.");
        return;
    }

    mpr("Invalid player spell, attempting to cast it as monster spell.");

    mon->mname      = "Dummy Monster";
    mon->type       = MONS_HUMAN;
    mon->behaviour  = BEH_SEEK;
    mon->attitude   = ATT_FRIENDLY;
    mon->flags      = (MF_NO_REWARD | MF_JUST_SUMMONED | MF_SEEN
                       | MF_WAS_IN_VIEW | MF_HARD_RESET);
    mon->hit_points = you.hp;
    mon->hit_dice   = you.experience_level;
    mon->set_position(you.pos());
    mon->target     = spd.target;
    mon->mid        = MID_PLAYER;

    if (!spd.isTarget)
        mon->foe = MHITNOT;
    else if (!monster_at(spd.target))
    {
        if (spd.isMe())
            mon->foe = MHITYOU;
        else
            mon->foe = MHITNOT;
    }
    else
        mon->foe = mgrd(spd.target);

    mgrd(you.pos()) = mon->mindex();

    mons_cast(mon, beam, spell);

    mon->reset();
}
#endif // WIZARD

static int _setup_evaporate_cast()
{
    int rc = prompt_invent_item("Throw which potion?", MT_INVLIST, OBJ_POTIONS);

    if (prompt_failed(rc))
    {
        rc = -1;
    }
    else if (you.inv[rc].base_type != OBJ_POTIONS)
    {
        mpr("This spell works only on potions!");
        rc = -1;
    }
    else
    {
        mprf(MSGCH_PROMPT, "Where do you want to aim %s?",
             you.inv[rc].name(DESC_NOCAP_YOUR).c_str());
    }
    return rc;
}

static bool _can_cast_detect()
{
    if (player_in_mappable_area())
        return (true);

    canned_msg(MSG_DISORIENTED);
    return (false);
}

static void _maybe_cancel_repeat(spell_type spell)
{
    switch (spell)
    {
    case SPELL_DELAYED_FIREBALL:
    case SPELL_TUKIMAS_DANCE:
        crawl_state.cant_cmd_repeat(make_stringf("You can't repeat %s.",
                                                 spell_title(spell)));
        break;

    default:
        break;
    }
}

static spret_type _do_cast(spell_type spell, int powc,
                           const dist& spd, bolt& beam,
                           god_type god, int potion,
                           bool check_range, bool fail);

static bool _spellcasting_aborted(spell_type spell,
                                  bool check_range_usability,
                                  bool wiz_cast)
{
    std::string msg;
    if (!wiz_cast && spell_is_uncastable(spell, msg))
    {
        mpr(msg);
        return (true);
    }

    if (is_prevented_teleport(spell))
    {
        mpr("You cannot teleport right now.");
        return (true);
    }

    if (check_range_usability
        && spell == SPELL_FULSOME_DISTILLATION
        && !corpse_at(you.pos()))
    {
        mpr("There aren't any corpses here.");
        return (true);
    }

    if (spell == SPELL_GOLUBRIAS_PASSAGE && !can_cast_golubrias_passage())
    {
        mpr("Only one passage may be opened at a time.");
        return (true);
    }

    if (spell == SPELL_MALIGN_GATEWAY && !can_cast_malign_gateway())
    {
        mpr("The dungeon can only cope with one malign gateway at a time!");
        return (true);
    }

    if (spell == SPELL_TORNADO
        && (you.duration[DUR_TORNADO] || you.duration[DUR_TORNADO_COOLDOWN]))
    {
        mpr("You need to wait for the winds to calm down.");
        return (true);
    }

    return (false);
}

static targetter* _spell_targetter(spell_type spell, int pow, int range)
{
    switch(spell)
    {
    case SPELL_FIRE_STORM:
        return new targetter_smite(&you, range, 2, pow > 76 ? 3 : 2);
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_HOLY_BREATH:
        return new targetter_cloud(&you, range);
    default:
        return 0;
    }
}

// Returns SPRET_SUCCESS if spell is successfully cast for purposes of
// exercising, SPRET_FAIL otherwise, or SPRET_ABORT if the player canceled
// the casting.
// Not all of these are actually real spells; invocations, decks, rods or misc.
// effects might also land us here.
// Others are currently unused or unimplemented.
spret_type your_spells(spell_type spell, int powc,
                       bool allow_fail, bool check_range)
{
    ASSERT(!crawl_state.game_is_arena());

    const bool wiz_cast = (crawl_state.prev_cmd == CMD_WIZARD && !allow_fail);

    dist spd;
    bolt beam;
    beam.origin_spell = spell;

    // [dshaligram] Any action that depends on the spellcasting attempt to have
    // succeeded must be performed after the switch().
    if (_spellcasting_aborted(spell, check_range, wiz_cast))
        return (SPRET_ABORT);

    const unsigned int flags = get_spell_flags(spell);

    ASSERT(wiz_cast || !(flags & SPFLAG_TESTING));

    int potion = -1;

    if (!powc)
        powc = calc_spell_power(spell, true);

    // XXX: This handles only some of the cases where spells need
    // targeting.  There are others that do their own that will be
    // missed by this (and thus will not properly ESC without cost
    // because of it).  Hopefully, those will eventually be fixed. - bwr
    if ((flags & SPFLAG_TARGETING_MASK) && spell != SPELL_PORTAL_PROJECTILE)
    {
        targ_mode_type targ =
              (testbits(flags, SPFLAG_HELPFUL) ? TARG_FRIEND : TARG_HOSTILE);

        if (testbits(flags, SPFLAG_NEUTRAL))
            targ = TARG_ANY;

        if (spell == SPELL_DISPEL_UNDEAD)
            targ = TARG_HOSTILE_UNDEAD;

        targeting_type dir  =
            (testbits(flags, SPFLAG_TARG_OBJ) ? DIR_TARGET_OBJECT :
             testbits(flags, SPFLAG_TARGET)   ? DIR_TARGET        :
             testbits(flags, SPFLAG_GRID)     ? DIR_TARGET        :
             testbits(flags, SPFLAG_DIR)      ? DIR_DIR           :
                                                DIR_NONE);

        const char *prompt = get_spell_target_prompt(spell);
        if (spell == SPELL_EVAPORATE)
        {
            potion = _setup_evaporate_cast();
            if (potion == -1)
                return (SPRET_ABORT);
        }
        else if (dir == DIR_DIR)
            mpr(prompt ? prompt : "Which direction?", MSGCH_PROMPT);

        const bool needs_path = (!testbits(flags, SPFLAG_GRID)
                                 && !testbits(flags, SPFLAG_TARGET));

        const bool dont_cancel_me = (testbits(flags, SPFLAG_HELPFUL)
                                     || testbits(flags, SPFLAG_ALLOW_SELF));

        const int range = calc_spell_range(spell, powc, false);

        targetter *hitfunc = _spell_targetter(spell, powc, range);

        std::string title = "Aiming: <white>";
        title += spell_title(spell);
        title += "</white>";

        if (!spell_direction(spd, beam, dir, targ, range,
                             needs_path, true, dont_cancel_me, prompt,
                             title.c_str(),
                             testbits(flags, SPFLAG_NOT_SELF),
                             hitfunc))
        {
            if (hitfunc)
                delete hitfunc;
            return (SPRET_ABORT);
        }

        if (hitfunc)
            delete hitfunc;
        beam.range = calc_spell_range(spell, powc, true);

        if (testbits(flags, SPFLAG_NOT_SELF) && spd.isMe())
        {
            if (spell == SPELL_TELEPORT_OTHER || spell == SPELL_POLYMORPH_OTHER)
                mpr("Sorry, this spell works on others only.");
            else
                canned_msg(MSG_UNTHINKING_ACT);

            return (SPRET_ABORT);
        }
    }

    // Enhancers only matter for calc_spell_power() and spell_fail().
    // Not sure about this: is it flavour or misleading? (jpeg)
    if (allow_fail)
        _surge_power(spell);

    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    int fail = 0;

    if (allow_fail)
    {
        int spfl = random2avg(100, 3);

        if (you.religion != GOD_SIF_MUNA
            && you.penance[GOD_SIF_MUNA] && one_chance_in(20))
        {
            god_speaks(GOD_SIF_MUNA, "You feel a surge of divine spite.");

            // This will cause failure and increase the miscast effect.
            spfl = -you.penance[GOD_SIF_MUNA];
        }
        else if (spell_typematch(spell, SPTYP_NECROMANCY)
                 && you.religion != GOD_KIKUBAAQUDGHA
                 && you.penance[GOD_KIKUBAAQUDGHA]
                 && one_chance_in(20))
        {
            // And you thought you'd Necromutate your way out of penance...
            simple_god_message(" does not allow the disloyal to dabble in "
                               "death!", GOD_KIKUBAAQUDGHA);

            // The spell still goes through, but you get a miscast anyway.
            MiscastEffect(&you, -god, SPTYP_NECROMANCY,
                          (you.experience_level / 2) + (spell_mana(spell) * 2),
                          random2avg(88, 3), "the malice of Kikubaaqudgha");
        }

        const int spfail_chance = spell_fail(spell);
        // Divination mappings backfire in Labyrinths and the Abyss.
        if (testbits(env.level_flags, LFLAG_NO_MAGIC_MAP)
            && testbits(flags, SPFLAG_MAPPING))
        {
            mpr("The warped magic of this place twists your spell in on "
                "itself!", MSGCH_WARN);
            spfl = spfail_chance / 2 - 1;
        }

        if (spfl < spfail_chance)
            fail = spfail_chance - spfl;
    }

    dprf("Spell #%d, power=%d", spell, powc);

    if (crawl_state.prev_cmd == CMD_CAST_SPELL && god == GOD_NO_GOD)
        _maybe_cancel_repeat(spell);

    switch (_do_cast(spell, powc, spd, beam, god, potion, check_range, fail))
    {
    case SPRET_SUCCESS:
        _spellcasting_side_effects(spell, powc, god);
        return (SPRET_SUCCESS);

    case SPRET_FAIL:
    {
        mprf("You miscast %s.", spell_title(spell));
        flush_input_buffer(FLUSH_ON_FAILURE);
        learned_something_new(HINT_SPELL_MISCAST);

        if (you.religion == GOD_SIF_MUNA
            && !player_under_penance()
            && you.piety >= 100 && x_chance_in_y(you.piety + 1, 150))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            return (SPRET_FAIL);
        }

        // All spell failures give a bit of magical radiation.
        // Failure is a function of power squared multiplied by how
        // badly you missed the spell.  High power spells can be
        // quite nasty: 9 * 9 * 90 / 500 = 15 points of
        // contamination!
        int nastiness = spell_mana(spell) * spell_mana(spell) * fail + 250;

        const int cont_points = div_rand_round(nastiness, 500);

        // miscasts are uncontrolled
        contaminate_player(cont_points, true);

        MiscastEffect(&you, NON_MONSTER, spell, spell_mana(spell), fail);

        return (SPRET_FAIL);
    }

    case SPRET_ABORT:
        return (SPRET_ABORT);

    case SPRET_NONE:
#ifdef WIZARD
        if (you.wizard && !allow_fail && is_valid_spell(spell)
            && (flags & SPFLAG_MONSTER))
        {
            _try_monster_cast(spell, powc, spd, beam);
            return (SPRET_SUCCESS);
        }
#endif

        if (is_valid_spell(spell))
        {
            mprf(MSGCH_ERROR, "Spell '%s' is not a player castable spell.",
                 spell_title(spell));
        }
        else
            mpr("Invalid spell!", MSGCH_ERROR);

        return (SPRET_ABORT);
    }

    return (SPRET_SUCCESS);
}

// Special-cased after-effects.
static void _spell_zap_effect(spell_type spell)
{
    // Casting pain costs 1 hp.
    // Deep Dwarves' damage reduction always blocks at least 1 hp.
    if (spell == SPELL_PAIN
        && (you.species != SP_DEEP_DWARF && !player_res_torment()))
    {
        dec_hp(1, false);
    }
}

// Returns SPRET_SUCCESS, SPRET_ABORT, SPRET_FAIL
// or SPRET_NONE (not a player spell).
static spret_type _do_cast(spell_type spell, int powc,
                           const dist& spd, bolt& beam,
                           god_type god, int potion,
                           bool check_range, bool fail)
{
    // First handle the zaps.
    zap_type zap = spell_to_zap(spell);
    if (zap != NUM_ZAPS)
    {
        spret_type ret = zapping(zap, spell_zap_power(spell, powc), beam, true,
                                 NULL, fail);

        if (ret == SPRET_SUCCESS)
            _spell_zap_effect(spell);

        return ret;
    }

    const coord_def target = spd.isTarget ? beam.target : you.pos() + spd.delta;

    switch (spell)
    {
    case SPELL_FREEZE:
        return cast_freeze(powc, monster_at(target), fail);

    case SPELL_SANDBLAST:
        return cast_sandblast(powc, beam, fail);

    case SPELL_VAMPIRIC_DRAINING:
        return vampiric_drain(powc, monster_at(target), fail);

    case SPELL_IOOD:
        return cast_iood(&you, powc, &beam, 0, 0, MHITNOT, fail);

    // Clouds and explosions.
    case SPELL_MEPHITIC_CLOUD:
        return stinking_cloud(powc, beam, fail);

    case SPELL_EVAPORATE:
        return cast_evaporate(powc, beam, potion, fail);

    case SPELL_POISONOUS_CLOUD:
        return cast_big_c(powc, CLOUD_POISON, &you, beam, fail);

    case SPELL_HOLY_BREATH:
        return cast_big_c(powc, CLOUD_HOLY_FLAMES, &you, beam, fail);

    case SPELL_FREEZING_CLOUD:
        return cast_big_c(powc, CLOUD_COLD, &you, beam, fail);

    case SPELL_FIRE_STORM:
        return cast_fire_storm(powc, beam, fail);

    // Demonspawn ability, no failure.
    case SPELL_HELLFIRE_BURST:
        return cast_hellfire_burst(powc, beam) ? SPRET_SUCCESS : SPRET_ABORT;

    case SPELL_FIREBALL:
        return fireball(powc, beam, fail);

    case SPELL_DELAYED_FIREBALL:
        return cast_delayed_fireball(fail);

    // LOS spells

    // Beogh ability and rod of smiting, no failure.
    case SPELL_SMITING:
        return cast_smiting(powc, monster_at(target)) ? SPRET_SUCCESS
                                                      : SPRET_ABORT;

    case SPELL_AIRSTRIKE:
        return cast_airstrike(powc, spd, fail);

    case SPELL_FRAGMENTATION:
        return cast_fragmentation(powc, spd, fail);

    case SPELL_PORTAL_PROJECTILE:
        return cast_portal_projectile(powc, fail);

    // other effects
    case SPELL_DISCHARGE:
        return cast_discharge(powc, fail);

    case SPELL_CHAIN_LIGHTNING:
        return cast_chain_lightning(powc, &you, fail);

    case SPELL_DISPERSAL:
        return cast_dispersal(powc, fail);

    case SPELL_SHATTER:
        return cast_shatter(powc, fail);

    case SPELL_LEDAS_LIQUEFACTION:
        return cast_liquefaction(powc, fail);

#if TAG_MAJOR_VERSION == 32
    case SPELL_SYMBOL_OF_TORMENT:
        mpr("Sorry, this spell is gone!");
        return SPRET_ABORT;
#endif

    case SPELL_OZOCUBUS_REFRIGERATION:
        return cast_refrigeration(powc, false, true, fail);

    case SPELL_IGNITE_POISON:
        return cast_ignite_poison(powc, fail);

    case SPELL_TORNADO:
        return cast_tornado(powc, fail);

    // Summoning spells, and other spells that create new monsters.
    // If a god is making you cast one of these spells, any monsters
    // produced will count as god gifts.
    case SPELL_SUMMON_BUTTERFLIES:
        return cast_summon_butterflies(powc, god, fail);

    case SPELL_SUMMON_SMALL_MAMMALS:
        return cast_summon_small_mammals(powc, god, fail);

    case SPELL_STICKS_TO_SNAKES:
        return cast_sticks_to_snakes(powc, god, fail);

    case SPELL_SUMMON_SCORPIONS:
        return cast_summon_scorpions(powc, god, fail);

    case SPELL_SUMMON_SWARM:
        return cast_summon_swarm(powc, god, fail);

    case SPELL_CALL_CANINE_FAMILIAR:
        return cast_call_canine_familiar(powc, god, fail);

    case SPELL_SUMMON_ELEMENTAL:
        return cast_summon_elemental(powc, god, MONS_NO_MONSTER, 2, 0, fail);

    case SPELL_SUMMON_ICE_BEAST:
        return cast_summon_ice_beast(powc, god, fail);

    case SPELL_SUMMON_UGLY_THING:
        return cast_summon_ugly_thing(powc, god, fail);

    case SPELL_SUMMON_DRAGON:
        return cast_summon_dragon(&you, powc, god, fail);

    case SPELL_SUMMON_HYDRA:
        return cast_summon_hydra(&you, powc, god, fail);

    case SPELL_TUKIMAS_DANCE:
        // Temporarily turns a wielded weapon into a dancing weapon.
        return cast_tukimas_dance(powc, god, false, fail);

    case SPELL_TUKIMAS_BALL:
        return cast_tukimas_ball(&you, powc, god, fail);

    case SPELL_CONJURE_BALL_LIGHTNING:
        return cast_conjure_ball_lightning(powc, god, fail);

    case SPELL_CALL_IMP:
        return cast_call_imp(powc, god, fail);

    case SPELL_SUMMON_DEMON:
        return cast_summon_demon(powc, god, fail);

    case SPELL_DEMONIC_HORDE:
        return cast_demonic_horde(powc, god, fail);

    case SPELL_SUMMON_GREATER_DEMON:
        return cast_summon_greater_demon(powc, god, fail);

    case SPELL_SHADOW_CREATURES:
        return cast_shadow_creatures(god, fail);

    case SPELL_SUMMON_HORRIBLE_THINGS:
        return cast_summon_horrible_things(powc, god, fail);

    case SPELL_MALIGN_GATEWAY:
        return cast_malign_gateway(&you, powc, god, fail);

    case SPELL_ANIMATE_SKELETON:
        return cast_animate_skeleton(god, fail);

    case SPELL_ANIMATE_DEAD:
        return cast_animate_dead(powc, god, fail);

    case SPELL_SIMULACRUM:
        return cast_simulacrum(powc, god, fail);

    case SPELL_TWISTED_RESURRECTION:
        return cast_twisted_resurrection(powc, god, fail);

    case SPELL_HAUNT:
        return cast_haunt(powc, beam.target, god, fail);

    case SPELL_DEATH_CHANNEL:
        return cast_death_channel(powc, god, fail);

    // Enchantments.
    case SPELL_CONFUSING_TOUCH:
        return cast_confusing_touch(powc, fail);

    case SPELL_CAUSE_FEAR:
        return mass_enchantment(ENCH_FEAR, powc, NULL, NULL, fail);

    case SPELL_INTOXICATE:
        return cast_intoxicate(powc, fail);

    case SPELL_MASS_CONFUSION:
        return mass_enchantment(ENCH_CONFUSION, powc, NULL, NULL, fail);

    case SPELL_ENGLACIATION:
        return cast_mass_sleep(powc, fail);

    case SPELL_CONTROL_UNDEAD:
        return mass_enchantment(ENCH_CHARM, powc, NULL, NULL, fail);

    case SPELL_ABJURATION:
        return cast_abjuration(powc, monster_at(target), fail);

    case SPELL_MASS_ABJURATION:
        return cast_mass_abjuration(powc, fail);

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        return cast_toxic_radiance(false, fail);

    // XXX: I don't think any call to healing goes through here. --rla
    case SPELL_MINOR_HEALING:
        if (cast_healing(5) < 0)
            return (SPRET_ABORT);
        break;

    case SPELL_MAJOR_HEALING:
        if (cast_healing(25) < 0)
            return (SPRET_ABORT);
        break;

    // Self-enchantments. (Spells that can only affect the player.)
    // Resistances.
    case SPELL_INSULATION:
        return cast_insulation(powc, fail);

#if TAG_MAJOR_VERSION == 32
    case SPELL_RESIST_POISON:
        mpr("Sorry, this spell is gone!");
        return SPRET_ABORT;
#endif

    case SPELL_SEE_INVISIBLE:
        return cast_see_invisible(powc, fail);

    case SPELL_CONTROL_TELEPORT:
        return cast_teleport_control(powc, fail);

    // Healing.
    case SPELL_CURE_POISON:
        return cast_cure_poison(powc, fail);

    // Weapon brands.
    case SPELL_SURE_BLADE:
        return cast_sure_blade(powc, fail);

    case SPELL_FIRE_BRAND:
        return brand_weapon(SPWPN_FLAMING, powc, fail);

    case SPELL_FREEZING_AURA:
        return brand_weapon(SPWPN_FREEZING, powc, fail);

#if TAG_MAJOR_VERSION == 32
    case SPELL_MAXWELLS_SILVER_HAMMER:
        mpr("Sorry, this spell is gone!");
        return SPRET_ABORT;
#endif

    case SPELL_POISON_WEAPON:
        return brand_weapon(SPWPN_VENOM, powc, fail);

    case SPELL_EXCRUCIATING_WOUNDS:
        return brand_weapon(SPWPN_PAIN, powc, fail);

    case SPELL_LETHAL_INFUSION:
        return brand_weapon(SPWPN_DRAINING, powc, fail);

    case SPELL_WARP_BRAND:
        return brand_weapon(SPWPN_DISTORTION, powc, fail);

    // Transformations.
    case SPELL_BLADE_HANDS:
        return cast_transform(powc, TRAN_BLADE_HANDS, fail);

    case SPELL_SPIDER_FORM:
        return cast_transform(powc, TRAN_SPIDER, fail);

    case SPELL_STATUE_FORM:
        return cast_transform(powc, TRAN_STATUE, fail);

    case SPELL_ICE_FORM:
        return cast_transform(powc, TRAN_ICE_BEAST, fail);

    case SPELL_DRAGON_FORM:
        return cast_transform(powc, TRAN_DRAGON, fail);

    case SPELL_NECROMUTATION:
        return cast_transform(powc, TRAN_LICH, fail);

#if TAG_MAJOR_VERSION == 32
    case SPELL_ALTER_SELF:
        mpr("You feel quite happy just as you are, actually.");
        return SPRET_ABORT;
#endif

    // General enhancement.
    case SPELL_REGENERATION:
        return cast_regen(powc, false, fail);

    case SPELL_REPEL_MISSILES:
        return missile_prot(powc, fail);

    case SPELL_DEFLECT_MISSILES:
        return deflection(powc, fail);

    case SPELL_SWIFTNESS:
        return cast_swiftness(powc, fail);

#if TAG_MAJOR_VERSION == 32
    case SPELL_LEVITATION:
#endif
    case SPELL_FLY:
        return cast_fly(powc, fail);

    case SPELL_STONESKIN:
        return cast_stoneskin(powc, fail);

#if TAG_MAJOR_VERSION == 32
    case SPELL_STONEMAIL:
        mpr("Sorry, this spell is gone!");
        return SPRET_ABORT;
#endif

    case SPELL_CONDENSATION_SHIELD:
        return cast_condensation_shield(powc, fail);

    case SPELL_OZOCUBUS_ARMOUR:
        return ice_armour(powc, fail);

    case SPELL_PHASE_SHIFT:
        return cast_phase_shift(powc, fail);

    case SPELL_SILENCE:
        return cast_silence(powc, fail);

    // other
#if TAG_MAJOR_VERSION == 32
    case SPELL_EXTENSION:
        mpr("Sorry, this spell is gone!");
        return SPRET_ABORT;
#endif

    case SPELL_BORGNJORS_REVIVIFICATION:
        return cast_revivification(powc, fail);

    case SPELL_SUBLIMATION_OF_BLOOD:
        return cast_sublimation_of_blood(powc, fail);

    case SPELL_DEATHS_DOOR:
        return cast_deaths_door(powc, fail);

    case SPELL_RING_OF_FLAMES:
        return cast_ring_of_flames(powc, fail);

    // Escape spells.
    case SPELL_BLINK:
        return cast_blink(god != GOD_XOM, fail);

    case SPELL_TELEPORT_SELF:
        return cast_teleport_self(fail);

    case SPELL_CONTROLLED_BLINK:
        return cast_controlled_blink(powc, fail);

    // Utility spells.
    // XXX: This should probably be removed.
    case SPELL_DETECT_SECRET_DOORS:
        if (_can_cast_detect())
            cast_detect_secret_doors(powc);
        break;

    // XXX: This one too.
    case SPELL_DETECT_TRAPS:
        if (_can_cast_detect())
            mprf("You detect %s", (detect_traps(powc) > 0) ? "traps!"
                                                           : "nothing.");
        break;

    // Only a Xom spell, no failure.
    case SPELL_DETECT_ITEMS:
        if (_can_cast_detect())
            mprf("You detect %s", (detect_items(powc) > 0) ? "items!"
                                                           : "nothing.");
        break;

    // Only a Xom spell, no failure.
    case SPELL_DETECT_CREATURES:
    {
        if (!_can_cast_detect())
            break;

        const int prev_detected = count_detected_mons();
        const int num_creatures = detect_creatures(powc);

        if (!num_creatures)
            mpr("You detect nothing.");
        else if (num_creatures == prev_detected)
        {
            // This is not strictly true. You could have cast
            // Detect Creatures with a big enough fuzz that the detected
            // glyph is still on the map when the original one has been
            // killed. Then another one is spawned, so the number is
            // the same as before. There's no way we can check this however.
            mpr("You detect no further creatures.");
        }
        else
            mpr("You detect creatures!");
        break;
    }

    case SPELL_PROJECTED_NOISE:
        return project_noise(fail);

    case SPELL_CONJURE_FLAME:
        return conjure_flame(powc, beam.target, fail);

    case SPELL_PASSWALL:
        return cast_passwall(spd.delta, powc, fail);

    case SPELL_APPORTATION:
        return cast_apportation(powc, beam, fail);

    case SPELL_RECALL:
        return cast_recall(fail);

    case SPELL_CORPSE_ROT:
        return cast_corpse_rot(fail);

    case SPELL_FULSOME_DISTILLATION:
        return cast_fulsome_distillation(powc, check_range, fail);

    case SPELL_GOLUBRIAS_PASSAGE:
        return cast_golubrias_passage(beam.target, fail);

    case SPELL_DARKNESS:
        return cast_darkness(powc, fail);

    case SPELL_SHROUD_OF_GOLUBRIA:
        return cast_shroud_of_golubria(powc, fail);

    default:
        return (SPRET_NONE);
    }

    return (SPRET_SUCCESS);
}

const char* failure_rate_to_string(int fail)
{
    return (fail == 100) ? "Useless"   : // 0% success chance
           (fail > 77)   ? "Terrible"  : // 0-5%
           (fail > 59)   ? "Very Poor" : // 5-30%
           (fail > 50)   ? "Poor"      : // 30-50%
           (fail > 40)   ? "Fair"      : // 50-70%
           (fail > 35)   ? "Good"      : // 70-80%
           (fail > 28)   ? "Very Good" : // 80-90%
           (fail > 22)   ? "Great"     : // 90-95%
           (fail >  0)   ? "Excellent"   // 95-100%
                         : "Perfect";    // 100%
}

const char* spell_hunger_string(spell_type spell, bool rod)
{
    return hunger_cost_string(spell_hunger(spell, rod));
}

std::string spell_noise_string(spell_type spell)
{
    const int casting_noise = spell_noise(spell);
    int effect_noise;

    switch (spell)
    //When the noise can vary, we're most interested in the worst case.
    {
    // Clouds
    case SPELL_POISONOUS_CLOUD:
    case SPELL_FREEZING_CLOUD:
    case SPELL_CONJURE_FLAME:
        effect_noise = 2;
        break;

    case SPELL_AIRSTRIKE:
        effect_noise = 4;
        break;

    case SPELL_IOOD:
        effect_noise = 7;
        break;

    // Small explosions.
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_EVAPORATE:
    case SPELL_FIREBALL:
    case SPELL_DELAYED_FIREBALL:
    case SPELL_HELLFIRE_BURST:
    case SPELL_TORNADO:
        effect_noise = 15;
        break;

    // Medium explosions
    case SPELL_FRAGMENTATION:   //LRD most often do small and medium explosions
        effect_noise = 20;      //and sometimes big ones with green crystal
        break;

    // Big explosions
    case SPELL_FIRE_STORM:  //The storms make medium or big explosions
    case SPELL_ICE_STORM:
    case SPELL_LIGHTNING_BOLT:
    case SPELL_CHAIN_LIGHTNING:
    case SPELL_CONJURE_BALL_LIGHTNING:
        effect_noise = 25;
        break;

    case SPELL_SHATTER:
    case SPELL_PROJECTED_NOISE:
        effect_noise = 30;
        break;

    default:
        effect_noise = 0;
    }

    const int noise = std::max(casting_noise, effect_noise);

    const char* noise_descriptions[] = {
        "Silent", "Almost silent", "Quiet", "A bit loud", "Loud", "Very loud",
        "Extremely loud", "Deafening"
    };

    const int breakpoints[] = { 1, 2, 4, 8, 15, 20, 30 };

    const char* desc = noise_descriptions[breakpoint_rank(noise, breakpoints,
                                                ARRAYSZ(breakpoints))];

#ifdef WIZARD
    if (you.wizard)
        return make_stringf("%s (%d)", desc, noise);
    else
#endif
        return desc;
}

int spell_power_colour(spell_type spell)
{
    const int powercap = spell_power_cap(spell);
    if (powercap == 0)
        return DARKGREY;
    const int power = calc_spell_power(spell, true);
    if (power >= powercap)
        return WHITE;
    if (power * 3 < powercap)
        return RED;
    if (power * 3 < powercap * 2)
        return YELLOW;
    return GREEN;
}

static int _power_to_barcount(int power)
{
    if (power == -1)
        return -1;

    const int breakpoints[] = { 5, 10, 15, 25, 35, 50, 75, 100, 150 };
    return (breakpoint_rank(power, breakpoints, ARRAYSZ(breakpoints)) + 1);
}

int spell_power_bars(spell_type spell, bool rod)
{
    const int cap = spell_power_cap(spell);
    if (cap == 0)
        return -1;
    const int power = std::min(calc_spell_power(spell, true, false, false, rod), cap);
    return _power_to_barcount(power);
}

#ifdef WIZARD
static std::string _wizard_spell_power_numeric_string(spell_type spell, bool rod)
{
    const int cap = spell_power_cap(spell);
    if (cap == 0)
        return "N/A";
    const int power = std::min(calc_spell_power(spell, true, false, false, rod), cap);
    return make_stringf("%d (%d)", power, cap);
}
#endif

std::string spell_power_string(spell_type spell, bool rod)
{
#ifdef WIZARD
    if (you.wizard)
        return _wizard_spell_power_numeric_string(spell, rod);
#endif

    const int numbars = spell_power_bars(spell, rod);
    const int capbars = _power_to_barcount(spell_power_cap(spell));
    ASSERT(numbars <= capbars);
    if (numbars < 0)
        return "N/A";
    else
        return std::string(numbars, '#') + std::string(capbars - numbars, '.');
}

int calc_spell_range(spell_type spell, int power, bool real_cast, bool rod)
{
    if (power == 0)
        power = calc_spell_power(spell, true, false, false, rod);
    const int range = spell_range(spell, power, real_cast);

    return (range);
}

std::string spell_range_string(spell_type spell, bool rod)
{
    const int cap      = spell_power_cap(spell);
    const int range    = calc_spell_range(spell, 0, false, rod);
    const int maxrange = spell_range(spell, cap, false);

    if (range < 0)
        return "N/A";
    else
    {
        return std::string("@") + std::string(range - 1, '-')
               + std::string(">") + std::string(maxrange - range, '.');
    }
}

std::string spell_schools_string(spell_type spell)
{
    std::string desc;

    bool already = false;
    for (int i = 0; i <= SPTYP_LAST_EXPONENT; ++i)
    {
        if (spell_typematch(spell, (1<<i)))
        {
            if (already)
                desc += "/";
            desc += spelltype_long_name(1 << i);
            already = true;
        }
    }

    return (desc);
}
