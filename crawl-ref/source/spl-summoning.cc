/**
 * @file
 * @brief Summoning spells and other effects creating monsters.
**/

#include "AppHdr.h"

#include "spl-summoning.h"

#include <algorithm>
#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "corpse.h"
#include "database.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "fineff.h"
#include "fprop.h"
#include "ghost.h"
#include "god-conduct.h"
#include "god-item.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "item-use.h"
#include "items.h"
#include "libutil.h"
#include "los.h"
#include "losglobal.h"
#include "losparam.h"
#include "mapmark.h"
#include "melee-attack.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-aura.h"
#include "mon-behv.h"
#include "mon-book.h" // MON_SPELL_WIZARD
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-movetarget.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "mutation.h"
#include "place.h" // absdungeon_depth
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-book.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "tilepick.h"
#include "timed-effects.h"
#include "throw.h"
#include "unwind.h"
#include "viewchar.h"
#include "xom.h"

static void _monster_greeting(monster *mons, const string &key)
{
    string msg = getSpeakString(key);
    if (msg == "__NONE")
        msg.clear();
    mons_speaks_msg(mons, msg, MSGCH_TALK, silenced(mons->pos()));
}

// combine with MG_AUTOFOE
static int _auto_autofoe(const actor *caster)
{
    return (!caster || caster->is_player())
        ? int{MHITYOU}
        : caster->as_monster()->foe;
}

static mgen_data _summon_data(const actor &caster, monster_type mtyp,
                              int dur, spell_type spell,
                              bool abjurable = true)
{
    return mgen_data(mtyp, BEH_COPY, caster.pos(),
                     _auto_autofoe(&caster),
                     MG_AUTOFOE)
                     .set_summoned(&caster, spell, dur, abjurable);
}

static mgen_data _pal_data(monster_type pal, int dur, spell_type spell,
                           bool abjurable = true)
{
    return _summon_data(you, pal, dur, spell, abjurable);
}

spret cast_summon_small_mammal(int pow, bool fail)
{
    if (stop_summoning_prompt())
        return spret::abort;

    fail_check();

    monster_type mon = MONS_PROGRAM_BUG;

    if (x_chance_in_y(10, pow + 1))
        mon = random_choose(MONS_BAT, MONS_RAT);
    else
        mon = MONS_QUOKKA;

    if (!create_monster(_pal_data(mon, summ_dur(3), SPELL_SUMMON_SMALL_MAMMAL)))
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

// Mid of an active canine familiar. Should exist only so long as the player
// has a living one.
#define CANINE_FAMILIAR_MID "canine_familiar_mid"

monster *find_canine_familiar()
{
    if (!you.props.exists(CANINE_FAMILIAR_MID))
        return nullptr;
    const int mid = you.props[CANINE_FAMILIAR_MID].get_int();
    return monster_by_mid(mid);
}

bool canine_familiar_is_alive()
{
    return find_canine_familiar() != nullptr;
}

spret cast_call_canine_familiar(int pow, bool fail)
{
    // Many parts of this spell behave differently if our familiar has already
    // been summoned.
    monster *old_dog = find_canine_familiar();

    if (!old_dog && stop_summoning_prompt())
        return spret::abort;

    if (old_dog && !you.can_see(*old_dog))
    {
        mprf(MSGCH_PROMPT, "Your familiar is too far away to imbue with magic.");
        return spret::abort;
    }

    fail_check();

    // Summon our dog if one isn't already active
    if (!old_dog)
    {
        mgen_data mg = _pal_data(MONS_INUGAMI, summ_dur(5), SPELL_CALL_CANINE_FAMILIAR);

        monster* dog = create_monster(mg);
        if (!dog)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            return spret::success;
        }

        // Use a ghost_demon to handle the familiar's scaling damage and stats.
        // The ghost_demon is created, but not initialized, during the
        // create_monster call above.
        ASSERT(dog->ghost);
        dog->ghost->init_inugami_from_player(pow);
        dog->ghost_demon_init();

        mpr("You call for your canine familiar and it appears with a howl!");
        you.props[CANINE_FAMILIAR_MID].get_int() = dog->mid;
    }
    // If it's active, instead heal and boost its next attack.
    else
    {
        // Heal familiar and make its next attack (within the new few turns,
        // so that you don't just prebuff for this) an instant cleave.
        mpr("You imbue your familiar with magical energy and its fangs glint"
            " viciously.");

        old_dog->heal(random_range(5, 9) + div_rand_round(pow, 5));
        old_dog->lose_ench_levels(ENCH_POISON, 1);
        old_dog->add_ench(mon_enchant(ENCH_INSTANT_CLEAVE, 1, &you, 50));

        // Give our familiar a small amount of extra duration, if its duration
        // is currently low, to avoid imbuing it and then having it immediately
        // poof before it can even do anything with the buff.
        mon_enchant timer = old_dog->get_ench(ENCH_SUMMON_TIMER);
        if (timer.duration < 110)
            timer.duration += random_range(60, 90);
        old_dog->update_ench(timer);
    }

    return spret::success;
}

spret cast_summon_cactus(int pow, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    mgen_data mg = _pal_data(MONS_CACTUS_GIANT, summ_dur(3), SPELL_SUMMON_CACTUS);
    mg.hp = hit_points(pow + 27, 1);
    if (!create_monster(mg))
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_awaken_armour(int pow, bool fail)
{
    const item_def *armour = you.body_armour();
    if (armour == nullptr)
    {
        // I don't think we can ever reach this line, but let's be safe.
        mpr("You aren't wearing any armour!");
        return spret::abort;
    }

    int mitm_slot = get_mitm_slot(10);
    if (mitm_slot == NON_ITEM)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::abort;
    }

    fail_check();

    mgen_data mg = _pal_data(MONS_ARMOUR_ECHO, summ_dur(2),
                             SPELL_AWAKEN_ARMOUR, false);
    mg.hd = 15 + div_rand_round(pow, 10);
    mg.set_range(1, 2);
    monster* spirit = create_monster(mg);
    if (!spirit)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    mprf("You draw out an echo of %s.", armour->name(DESC_YOUR).c_str());

    item_def &fake_armour = env.item[mitm_slot];
    fake_armour.clear();
    fake_armour = *armour;
    fake_armour.flags |= ISFLAG_SUMMONED | ISFLAG_IDENTIFIED;

    spirit->pickup_item(fake_armour, false, true);

    return spret::success;
}



spret cast_summon_ice_beast(int pow, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    mgen_data ice_beast = _pal_data(MONS_ICE_BEAST, summ_dur(3), SPELL_SUMMON_ICE_BEAST);
    ice_beast.hd = (3 + div_rand_round(pow, 13));

    if (create_monster(ice_beast))
        mpr("A chill wind blows around you.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_monstrous_menagerie(actor* caster, int pow, bool fail)
{
    if (caster->is_player() && stop_summoning_prompt())
        return spret::abort;

    fail_check();
    monster_type type = MONS_PROGRAM_BUG;

    if (random2(pow) > 60 && coinflip())
        type = MONS_GUARDIAN_SPHINX;
    else
        type = coinflip() ? MONS_MANTICORE : MONS_LINDWURM;

    mgen_data mdata = _summon_data(*caster, type, summ_dur(3), SPELL_MONSTROUS_MENAGERIE);
    if (caster->is_player())
        mdata.hd = get_monster_data(type)->HD + div_rand_round(pow - 50, 25);

    monster* beast = create_monster(mdata);
    if (!beast)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    if (you.can_see(*beast))
    {
        mprf("%s %s %s!", caster->name(DESC_THE).c_str(),
                          caster->conj_verb("summon").c_str(),
                          mons_type_name(type, DESC_A).c_str());
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_summon_hydra(actor *caster, int pow, bool fail)
{
    if (caster->is_player() && stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();
    // Power determines number of heads. Minimum 4 heads, maximum 12.
    // Rare to get more than 8.
    const int maxheads = one_chance_in(6) ? 12 : 8;
    const int heads = max(4, min(div_rand_round(random2(1 + pow), 6), maxheads));

    // Duration is always very short - just 1.
    mgen_data mg = _summon_data(*caster, MONS_HYDRA, summ_dur(1), SPELL_SUMMON_HYDRA);
    mg.props[MGEN_NUM_HEADS] = heads;
    if (monster *hydra = create_monster(mg))
    {
        if (you.see_cell(hydra->pos()))
            mprf("%s appears.", hydra->name(DESC_A).c_str());
    }
    else if (caster->is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static monster_type _choose_dragon_type(int pow, bool player)
{
    monster_type mon = MONS_PROGRAM_BUG;

    const int chance = random2(pow);

    if (chance >= 80 || one_chance_in(6))
        mon = random_choose(MONS_GOLDEN_DRAGON, MONS_QUICKSILVER_DRAGON);
    else if (chance >= 40 || one_chance_in(6))
        mon = random_choose(MONS_IRON_DRAGON, MONS_SHADOW_DRAGON, MONS_STORM_DRAGON);
    else
        mon = random_choose(MONS_FIRE_DRAGON, MONS_ICE_DRAGON);

    // For good gods, switch away from shadow dragons to storm/iron dragons.
    if (player && god_hates_monster(mon))
        mon = random_choose(MONS_STORM_DRAGON, MONS_IRON_DRAGON);

    return mon;
}

spret cast_dragon_call(int pow, bool fail)
{
    if (stop_summoning_prompt(MR_NO_FLAGS, M_FLIES, "call dragons"))
        return spret::abort;

    fail_check();

    mpr("You call out to the draconic realm, and the dragon horde roars back!");
    noisy(spell_effect_noise(SPELL_DRAGON_CALL), you.pos());

    you.duration[DUR_DRAGON_CALL] = (15 + div_rand_round(pow, 5) + random2(15))
                                    * BASELINE_DELAY;
    you.props[DRAGON_CALL_POWER_KEY].get_int() = pow;

    return spret::success;
}

static void _place_dragon()
{
    const int pow = you.props[DRAGON_CALL_POWER_KEY].get_int();
    monster_type mon = _choose_dragon_type(pow, true);
    int mp_cost = random_range(2, 3);

    vector<monster*> targets;

    // Pick a random hostile in sight
    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
        if (!mons_aligned(&you, *mi) && mons_is_threatening(**mi))
            targets.push_back(*mi);

    shuffle_array(targets);

    // Attempt to place adjacent to the first chosen hostile. If there is no
    // valid spot, move on to the next one.
    for (monster *target : targets)
    {
        // Chose a random valid spot adjacent to the selected target which is
        // also in the player's LoS.
        coord_def pos = find_newmons_square(MONS_FIRE_DRAGON, target->pos(),
                                            1, 1, -1, &you);

        // Try the next enemy, if we can't place one to fight this enemy.
        if (pos.origin())
            continue;

        // Abort if we lack sufficient MP, but the dragon call duration
        // remains, as the player might soon have enough again.
        if (!enough_mp(mp_cost, true))
        {
            mpr("A dragon tries to answer your call, but you don't have enough "
                "magical power!");
            return;
        }

        monster *dragon = create_monster(
            mgen_data(mon, BEH_COPY, pos, MHITYOU, MG_FORCE_PLACE | MG_AUTOFOE)
            .set_summoned(&you, SPELL_DRAGON_CALL, summ_dur(2)));
        if (!dragon)
            continue;

        pay_mp(mp_cost);
        if (you.see_cell(dragon->pos()))
            mpr("A dragon arrives to answer your call!");
        finalize_mp_cost();

        // The dragon is allowed to act immediately here
        dragon->flags &= ~MF_JUST_SUMMONED;
        return;
    }

    return;
}

void do_dragon_call(int time)
{
    noisy(spell_effect_noise(SPELL_DRAGON_CALL), you.pos());

    while (time > you.attribute[ATTR_NEXT_DRAGON_TIME]
           && you.duration[DUR_DRAGON_CALL])
    {
        time -= you.attribute[ATTR_NEXT_DRAGON_TIME];
        _place_dragon();
        you.attribute[ATTR_NEXT_DRAGON_TIME] = 3 + random2(5)
                                               + count_summons(&you, SPELL_DRAGON_CALL) * 5;
    }
    you.attribute[ATTR_NEXT_DRAGON_TIME] -= time;
}

/**
 * Handle the Doom Howl status effect, possibly summoning hostile nasties
 * around the player.
 *
 * @param time      The number of aut that the howling has been going on for
 *                  since the last doom_howl call.
 */
void doom_howl(int time)
{
    // TODO: pull hound-count generation into a helper function
    int howlcalled_count = 0;
    if (!you.props.exists(NEXT_DOOM_HOUND_KEY))
        you.props[NEXT_DOOM_HOUND_KEY] = random_range(20, 40);
    // 1 nasty beast every 2-4 turns
    while (time > 0)
    {
        const int time_to_call = you.props[NEXT_DOOM_HOUND_KEY].get_int();
        if (time_to_call <= time)
        {
            you.props[NEXT_DOOM_HOUND_KEY] = random_range(20, 40);
            ++howlcalled_count;
        }
        else
            you.props[NEXT_DOOM_HOUND_KEY].get_int() -= time;
        time -= time_to_call;
    }

    if (!howlcalled_count)
        return;

    const actor *target = &you;

    for (int i = 0; i < howlcalled_count; ++i)
    {
        const monster_type howlcalled = random_choose(
                MONS_BONE_DRAGON, MONS_REAPER, MONS_TORMENTOR, MONS_TZITZIMITL,
                MONS_PUTRID_MOUTH
        );

        monster *mons = create_monster(mgen_data(howlcalled, BEH_HOSTILE,
                                                 target->pos(), target->mindex(),
                                                 MG_FORCE_BEH).set_range(1));
        if (mons)
        {
            mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, target,
                                       INFINITE_DURATION));
            mons->behaviour = BEH_SEEK;
            mons_add_blame(mons, "called by a doom hound"); // assumption!
            check_place_cloud(CLOUD_BLACK_SMOKE, mons->pos(),
                              random_range(1,2), mons);
        }
    }
}

spret cast_summon_dragon(actor *caster, int pow, bool fail)
{
    // Dragons are always friendly. Dragon type depends on power and
    // random chance, with two low-tier dragons possible at high power.
    // Duration fixed at 6.

    fail_check();
    bool success = false;

    int how_many = 1;
    monster_type mon = _choose_dragon_type(pow, caster->is_player());

    if (pow >= 100 && (mon == MONS_FIRE_DRAGON || mon == MONS_ICE_DRAGON))
        how_many = 2;

    for (int i = 0; i < how_many; ++i)
    {
        if (monster *dragon = create_monster(
                _summon_data(*caster, mon, summ_dur(6), SPELL_SUMMON_DRAGON)))
        {
            if (you.see_cell(dragon->pos()))
                mpr("A dragon appears.");
            success = true;
        }
    }

    if (!success && caster->is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_summon_mana_viper(int pow, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    mgen_data viper = _pal_data(MONS_MANA_VIPER, summ_dur(2), SPELL_SUMMON_MANA_VIPER);
    viper.hd = (7 + div_rand_round(pow, 12));

    if (create_monster(viper))
        mpr("A mana viper appears with a sibilant hiss.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

// This assumes that the specified monster can go berserk.
static void _make_mons_berserk_summon(monster* mon)
{
    mon->go_berserk(false);
    mon_enchant berserk = mon->get_ench(ENCH_BERSERK);
    mon_enchant timer = mon->get_ench(ENCH_SUMMON_TIMER);

    // Let Trog's gifts berserk longer, and set the abjuration timeout
    // to the berserk timeout.
    berserk.duration = berserk.duration * 3 / 2;
    berserk.maxduration = berserk.duration;
    timer.duration = timer.maxduration = berserk.duration;
    mon->update_ench(berserk);
    mon->update_ench(timer);
}

// This is actually one of Trog's wrath effects.
bool summon_berserker(int pow, actor *caster, monster_type override_mons)
{
    monster_type mon = MONS_PROGRAM_BUG;

    const int dur = min(2 + (random2(pow) / 4), 6);

    if (override_mons != MONS_PROGRAM_BUG)
        mon = override_mons;
    else
    {
        if (pow <= 100)
        {
            // bears
            mon = random_choose(MONS_BLACK_BEAR, MONS_POLAR_BEAR);
        }
        else if (pow <= 140)
        {
            // ogres
            mon = random_choose_weighted(1, MONS_TWO_HEADED_OGRE, 2, MONS_OGRE);
        }
        else if (pow <= 180)
        {
            // trolls
            mon = random_choose_weighted(3, MONS_TROLL,
                                         3, MONS_DEEP_TROLL,
                                         2, MONS_IRON_TROLL);
        }
        else
        {
            // giants
            mon = random_choose(MONS_CYCLOPS, MONS_STONE_GIANT);
        }
    }

    mgen_data mg(mon, caster ? BEH_COPY : BEH_HOSTILE,
                 caster ? caster->pos() : you.pos(),
                 _auto_autofoe(caster),
                 MG_AUTOFOE, GOD_TROG);

    if (!caster)
    {
        mg.set_summoned(nullptr, MON_SUMM_WRATH);
        mg.non_actor_summoner = "the rage of " + god_name(GOD_TROG, false);
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
    }
    else
        mg.set_summoned(caster, MON_SUMM_AID, summ_dur(dur));

    monster *mons = create_monster(mg);

    if (!mons)
        return false;

    _make_mons_berserk_summon(mons);
    return true;
}

// Not a spell. Rather, this is TSO's doing.
bool summon_holy_warrior(int pow, bool punish)
{
    mgen_data mg(random_choose(MONS_ANGEL, MONS_DAEVA),
                 punish ? BEH_HOSTILE : BEH_FRIENDLY,
                 you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE, GOD_SHINING_ONE);
    mg.set_summoned(punish ? 0 : &you, SPELL_NO_SPELL,
                    summ_dur(punish ? 0 : min(2 + (random2(pow) / 4), 6)));

    if (punish)
    {
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
        mg.non_actor_summoner = god_name(GOD_SHINING_ONE, false);
    }

    monster *summon = create_monster(mg);

    if (!summon)
        return false;

    summon->flags |= MF_ATT_CHANGE_ATTEMPT;

    if (!punish)
        mpr("You are momentarily dazzled by a brilliant light.");

    return true;
}

/**
 * Essentially a macro to allow for a generic fail pattern to avoid leaking
 * information about invisible enemies. (Not implemented as a macro because I
 * find they create unreadable code.)
 *
 * @return spret::success
 **/
static bool _fail_tukimas()
{
    mpr("You can't see a target there!");
    return false; // Waste the turn - no anti-invis tech
}

/**
 * Checks if Tukima's Dance can actually affect the target (and anger them)
 *
 * @param target  The targeted monster (or player).
 * @return        Whether the target can be affected by Tukima's Dance.
 **/
bool tukima_affects(const actor &target)
{
    const item_def* wpn = target.weapon();
    return wpn
           && is_weapon(*wpn)
           && !target.is_player()
           && !is_special_unrandom_artefact(*wpn)
           && !mons_class_is_animated_weapon(target.type)
           && !mons_is_hepliaklqana_ancestor(target.type)
           && !(target.is_monster() && target.as_monster()->type == MONS_ORC_APOSTLE);
}

/**
 * Checks if Tukima's Dance is being cast on a valid target.
 *
 * @param target     The spell's target.
 * @return           Whether the target is valid.
 **/
static bool _check_tukima_validity(const actor *target)
{
    const item_def* wpn = target->weapon();
    bool can_see_target = target->visible_to(&you);

    // See if the wielded item is appropriate.
    if (!wpn)
    {
        if (!can_see_target)
            return _fail_tukimas();

        // FIXME: maybe move hands_act to class actor?
        bool plural = true;
        const string hand = target->hand_name(true, &plural);

        mprf("%s %s %s.",
             apostrophise(target->name(DESC_THE)).c_str(),
             hand.c_str(), conjugate_verb("twitch", plural).c_str());

        return false;
    }

    if (!tukima_affects(*target))
    {
        if (!can_see_target)
            return _fail_tukimas();

        if (mons_class_is_animated_weapon(target->type))
        {
            simple_monster_message(*(monster*)target,
                                   " is already dancing.");
        }
        else
        {
            mprf("%s vibrate%s crazily for a second.",
                 wpn->name(DESC_THE).c_str(),
                 wpn->quantity > 1 ? "" : "s");
        }
        return false;
    }

    return true;
}


/**
 * Actually animates the weapon of the target creature (no checks).
 *
 * @param pow               Spellpower.
 * @param target            The spell's target.
 **/
static void _animate_weapon(int pow, actor* target)
{
    item_def * const wpn = target->weapon();
    ASSERT(wpn);
    // If sac love, the weapon will go after you, not the target.
    const bool hostile = you.allies_forbidden();
    const int dur = min(2 + div_rand_round(random2(1 + pow), 5), 6);

    mgen_data mg(MONS_DANCING_WEAPON,
                 hostile ? BEH_HOSTILE : BEH_FRIENDLY,
                 target->pos(),
                 hostile ? MHITYOU : target->mindex(),
                 hostile ? MG_NONE : MG_FORCE_BEH);
    mg.set_summoned(&you, SPELL_TUKIMAS_DANCE, summ_dur(dur));
    mg.set_range(1, 2);
    mg.props[TUKIMA_WEAPON] = *wpn;
    mg.props[TUKIMA_POWER] = pow;

    monster * const mons = create_monster(mg);

    if (!mons)
    {
        mprf("%s twitches for a moment.", wpn->name(DESC_THE).c_str());
        return;
    }

    // Don't haunt yourself under sac love.
    if (!hostile)
    {
        mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, target,
                                   INFINITE_DURATION));
        mons->foe = target->mindex();
    }

    // We are successful. Unwield the weapon, removing any wield effects.
    mprf("%s dances into the air!", wpn->name(DESC_THE).c_str());

    monster * const montarget = target->as_monster();
    const int primary_weap = montarget->inv[MSLOT_WEAPON];
    const mon_inv_type wp_slot = (primary_weap != NON_ITEM
                                  && &env.item[primary_weap] == wpn) ?
                                     MSLOT_WEAPON : MSLOT_ALT_WEAPON;
    ASSERT(montarget->inv[wp_slot] != NON_ITEM);
    ASSERT(&env.item[montarget->inv[wp_slot]] == wpn);

    montarget->unequip(*(montarget->mslot_item(wp_slot)), false, true);
    montarget->inv[wp_slot] = NON_ITEM;

    // Find out what our god thinks before killing the item.
    conduct_type why = god_hates_item_handling(*wpn);

    wpn->clear();

    if (why)
    {
        simple_god_message(" booms: How dare you animate that foul thing!");
        did_god_conduct(why, 10, true, mons);
    }
}

/**
 * Casts Tukima's Dance, animating the weapon of the target creature (if valid)
 *
 * @param pow               Spellpower.
 * @param where             The target grid.
 **/
void cast_tukimas_dance(int pow, actor* target)
{
    ASSERT(target);

    if (!_check_tukima_validity(target))
        return;

    _animate_weapon(pow, target);
}

/// When the player conjures ball lightning with the given spellpower, what
/// HD will the resulting lightning have?
int ball_lightning_hd(int pow, bool random)
{
    if (random)
        return max(1, div_rand_round(pow, 6) - 6);
    return max(1, pow / 6 - 6);
}

int mons_ball_lightning_hd(int pow, bool random)
{
    // We love players, don't we? Let's be nice.
    return ball_lightning_hd(pow, random) / 2;
}

spret cast_conjure_ball_lightning(int pow, bool fail)
{
    fail_check();
    bool success = false;

    mgen_data cbl = _pal_data(MONS_BALL_LIGHTNING, 0,
                              SPELL_CONJURE_BALL_LIGHTNING);
    cbl.foe = MHITNOT;
    cbl.hd = ball_lightning_hd(pow);
    cbl.set_summoned(&you, SPELL_CONJURE_BALL_LIGHTNING, random_range(40, 70), false, false);

    for (int i = 0; i < 3; ++i)
    {
        if (monster *ball = create_monster(cbl))
        {
            success = true;

            // Avoid ball lightnings without targets always moving towards (0,0)
            if (!(ball->get_foe() && ball->get_foe()->is_monster()))
                set_random_target(ball);
        }
    }

    if (success)
        mpr("You create some ball lightning!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static int _lightning_spire_hd(int pow, bool random = true)
{
    if (random)
        return max(1, div_rand_round(pow, 10));
    else
        return max(1, pow / 10);
}

dice_def lightning_spire_damage(int pow)
{
    return zap_damage(ZAP_ELECTRICAL_BOLT, _lightning_spire_hd(pow, false) * 12, true, false);
}

spret cast_forge_lightning_spire(int pow, bool fail)
{
    fail_check();

    mgen_data spire = _pal_data(MONS_LIGHTNING_SPIRE, summ_dur(2),
                                SPELL_FORGE_LIGHTNING_SPIRE, false);
    spire.hd = _lightning_spire_hd(pow);

    monster* mons = create_monster(spire);

    if (mons && !silenced(mons->pos()))
        mpr("An electric hum fills the air.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_forge_blazeheart_golem(int pow, bool fail)
{
    fail_check();

    mgen_data golem = _pal_data(MONS_BLAZEHEART_GOLEM, summ_dur(3),
                                SPELL_FORGE_BLAZEHEART_GOLEM, false);
    golem.hd = 6 + div_rand_round(pow, 10);

    monster* mons = (create_monster(golem));

    if (mons)
    {
        mpr("You bind the heart of a blast furnace in slag iron.");

        // Give an extra turn of grace period on the turn it is summoned.
        mons->blazeheart_heat = 4;
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

/**
 * Cast the spell Call Imp, summoning a friendly imp nearby.
 *
 * @param pow   The spellpower at which the spell is being cast.
 * @param fail  Whether the caster (you) failed to cast the spell.
 * @return      spret::fail if fail is true; spret::success otherwise.
 */
spret cast_call_imp(int pow, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON, M_FLIES))
        return spret::abort;

    fail_check();

    mgen_data imp_data = _pal_data(MONS_CERULEAN_IMP, summ_dur(3), SPELL_CALL_IMP);
    if (monster *imp = create_monster(imp_data))
    {
        mpr("A tiny devil pulls itself out of the air.");
        imp->weapon()->plus = pow/10 - 4;
        _monster_greeting(imp, "_friendly_imp_greeting");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static bool _butterfly_knockback(coord_def p)
{
    monster* mon = monster_at(p);
    if (!mon)
        return false;

    const int dist = random_range(2, 4);
    if (!mon->knockback(you, dist, 0, "sudden gust"))
        return false;

    behaviour_event(mon, ME_ALERT, &you);
    return true;
}

spret summon_butterflies()
{
    // Just fizzle instead of creating hostile butterflies.
    if (you.allies_forbidden())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    if (stop_summoning_prompt(MR_NO_FLAGS, M_FLIES))
        return spret::abort;

    if (silenced(you.pos()))
        mpr("Somewhere, a butterfly flaps its wings.");
    else
        mpr("You hear the flapping of tiny wings.");

    bool success = false;
    // push away further-away things first, so middle ones don't get stuck
    // XXX: this is weird if a dispersal trap gets hit.
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
        if (grid_distance(*ri, you.pos()) == 2 && _butterfly_knockback(*ri))
            success = true;
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (_butterfly_knockback(*ai))
            success = true;

    // place some in a tight cluster, distance 2. Max 24 squares, so this is
    // always at least 2/3 density.
    const int how_many_inner = random_range(16, 22);
    for (int i = 0; i < how_many_inner; ++i)
    {
        mgen_data butterfly(MONS_BUTTERFLY, BEH_FRIENDLY, you.pos(), MHITYOU,
                            MG_AUTOFOE);
        butterfly.set_summoned(&you, MON_SUMM_BUTTERFLIES, summ_dur(3));

        if (create_monster(butterfly))
            success = true;
    }
    // place another set more sparsely. These will try to find a placement
    // within range 5 of the player. This can backfill the inner zone.
    const int how_many_outer = random_range(12, 28);
    for (int i = 0; i < how_many_outer; ++i)
    {
        mgen_data butterfly(MONS_BUTTERFLY, BEH_FRIENDLY, you.pos(), MHITYOU,
                            MG_AUTOFOE);
        butterfly.set_summoned(&you, MON_SUMM_BUTTERFLIES, summ_dur(3));
        butterfly.set_range(5);

        if (create_monster(butterfly))
            success = true;
    }


    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);
    return spret::success;
}

spret summon_shadow_creatures()
{
    // Hard to predict what resistances might come from this.
    if (stop_summoning_prompt())
        return spret::abort;

    mpr("Wisps of shadow whirl around you...");

    int num = roll_dice(2, 2);
    int num_created = 0;

    for (int i = 0; i < num; ++i)
    {
        if (monster *mons = create_monster(
            mgen_data(RANDOM_COMPATIBLE_MONSTER, BEH_FRIENDLY, you.pos(),
                      MHITYOU, MG_FORCE_BEH | MG_AUTOFOE | MG_NO_OOD)
                      // This duration is only used for band members.
                      .set_summoned(&you, MON_SUMM_SCROLL, summ_dur(2))
                      .set_place(level_id::current()),
            false))
        {
            // Choose a new duration based on HD.
            int x = max(mons->get_experience_level() - 3, 1);
            int d = min(4, 1 + div_rand_round(17, x));
            mon_enchant me = mon_enchant(ENCH_SUMMON_TIMER, d);
            me.set_duration(mons, &me);
            mons->update_ench(me);

            // Set summon ID, to share summon cap with its band members
            mons->props[SUMMON_ID_KEY].get_int() = mons->mid;

            // Remove any band members that would turn hostile, and link their
            // summon IDs
            for (monster_iterator mi; mi; ++mi)
            {
                if (mi->is_band_follower_of(*mons))
                {
                    if (god_hates_monster(**mi))
                        monster_die(**mi, KILL_RESET, NON_MONSTER);

                    mi->props[SUMMON_ID_KEY].get_int() = mons->mid;
                }
            }

            num_created++;
        }
    }

    if (!num_created)
        mpr("The shadows disperse without effect.");

    return spret::success;
}

bool can_cast_malign_gateway()
{
    timeout_malign_gateways(0);

    return count_malign_gateways() < 1;
}

static bool _is_malign_gateway_summoning_spot(const actor& caster,
    const coord_def location,
    bool targeting)
{
    if (!in_bounds(location)
        || !feat_is_malign_gateway_suitable(env.grid(location)))
    {
        return false;
    }

    const actor* const creature = actor_at(location);
    if (creature)
    {
        if (!targeting)
            return false;

        if (creature->visible_to(&caster))
            return false;
    }

    if (targeting)
    {
        for (adjacent_iterator ai(location); ai; ++ai)
        {
            const map_cell& map_info = env.map_knowledge(*ai);
            if (map_info.seen() && feat_is_solid(map_info.feat()))
                return false;
        }
    }
    else if (count_neighbours_with_func(location, &feat_is_solid) != 0)
        return false;

    if (!caster.see_cell_no_trans(location))
        return false;

    return true;
}

bool is_gateway_target(const actor& caster, coord_def location)
{
    const coord_def delta = location - caster.pos();

    // location is to close
    if (delta.rdist() < 2)
        return false;

    int abs_x = abs(delta.x);
    int abs_y = abs(delta.y);

    // location isn't on one of the 8 compass directions
    if (abs_x != 0 && abs_y != 0 && abs_x != abs_y)
        return false;

    // Monster range of vision is equal to player range of vision, so this
    // is accurate for mosters to.
    const int current_vision = you.current_vision;

    // location is to far
    if (abs_x > current_vision || abs_y > current_vision)
        return false;

    return _is_malign_gateway_summoning_spot(caster, location, true);
}

coord_def find_gateway_location(actor* caster)
{
    vector<coord_def> points;

    // Monster range of vision is equal to player range of vision, so this
    // is accurate for mosters to.
    const int current_vision = you.current_vision;

    for (int i = 0; i < 8; ++i)
    {
        const coord_def delta = Compass[i];

        for (int t = 2; t <= current_vision; ++t)
        {
            const coord_def test = caster->pos() + delta * t;

            if (_is_malign_gateway_summoning_spot(*caster, test, false))
                points.push_back(test);
        }
    }

    if (points.empty())
        return coord_def(0, 0);

    return points[random2(points.size())];
}

void create_malign_gateway(coord_def point, beh_type beh, string cause,
                           int pow, bool is_player)
{
    const int malign_gateway_duration = BASELINE_DELAY * (random2(2) + 1);
    env.markers.add(new map_malign_gateway_marker(point,
                            malign_gateway_duration,
                            is_player,
                            is_player ? "" : cause,
                            beh,
                            GOD_NO_GOD,
                            pow));
    env.markers.clear_need_activate();
    env.grid(point) = DNGN_MALIGN_GATEWAY;
    set_terrain_changed(point);

    noisy(spell_effect_noise(SPELL_MALIGN_GATEWAY), point);
    mprf(MSGCH_WARN, "The dungeon shakes, a horrible noise fills the air, "
                     "and a portal to some otherworldly place is opened!");
}

spret cast_malign_gateway(actor * caster, int pow, bool fail, bool test)
{
    if (!test && caster->is_player()
        && stop_summoning_prompt(MR_RES_POISON, M_FLIES))
    {
        return spret::abort;
    }

    coord_def point = find_gateway_location(caster);
    bool success = point != coord_def(0, 0);
    if (test)
        return success ? spret::success : spret::abort;

    bool is_player = caster->is_player();

    if (success)
    {
        fail_check();

        create_malign_gateway(
            point,
            is_player ? BEH_FRIENDLY
                      : attitude_creation_behavior(
                          caster->as_monster()->attitude),
            is_player ? ""
                      : caster->as_monster()->full_name(DESC_A),
            pow,
            is_player);

        return spret::success;
    }

    return spret::abort;
}

spret cast_summon_horrible_things(int pow, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();
    if (one_chance_in(4))
    {
        // if someone deletes the db, no message is ok
        mpr(getMiscString("SHT_int_loss"));

        // XXX: Temporary effect until something else is implemented.
        temp_mutate(MUT_WEAK_WILLED, "glimpsing the beyond");
    }

    int num_abominations = random_range(2, 4) + x_chance_in_y(pow, 200);
    int num_tmons = random2(pow) > 120 ? 2 : random2(pow) > 50 ? 1 : 0;

    if (num_tmons == 0)
        num_abominations++;

    int count = 0;

    while (num_abominations-- > 0)
    {
        const mgen_data abom = _pal_data(MONS_ABOMINATION_LARGE, summ_dur(3),
                                         SPELL_SUMMON_HORRIBLE_THINGS);
        if (create_monster(abom))
            ++count;
    }

    while (num_tmons-- > 0)
    {
        const mgen_data tmons = _pal_data(MONS_TENTACLED_MONSTROSITY, summ_dur(3),
                                          SPELL_SUMMON_HORRIBLE_THINGS);
        if (create_monster(tmons))
            ++count;
    }

    if (!count)
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static bool _water_adjacent(coord_def p)
{
    for (orth_adjacent_iterator ai(p); ai; ++ai)
    {
        if (feat_is_water(env.grid(*ai)))
            return true;
    }

    return false;
}


// Is this area open enough to summon a forest?
static bool _can_summon_forest(actor &caster)
{
    for (adjacent_iterator ai(caster.pos(), false); ai; ++ai)
        if (count_neighbours_with_func(*ai, &feat_is_solid) == 0)
            return true;
    return false;
}

/**
 * Cast summon forest.
 *
 * @param caster The caster.
 * @param pow    The spell power.
 * @param fail   Did this spell miscast? If true, abort the cast.
 * @return       spret::abort if a summoning area couldn't be found,
 *               spret::fail if one could be found but we miscast, and
 *               spret::success if the spell was successfully cast.
*/
spret cast_summon_forest(actor* caster, int pow, bool fail, bool test)
{
    if (!_can_summon_forest(*caster))
        return spret::abort;

    if (test)
        return spret::success;

    const int duration = random_range(120 + pow,
                                      200 + div_rand_round(pow * 3, 2));

    // Hm, should dryads have rPois?
    if (stop_summoning_prompt(MR_NO_FLAGS, M_NO_FLAGS, "summon a forest"))
        return spret::abort;

    fail_check();
    // Replace some rock walls with trees, then scatter a smaller number
    // of trees on unoccupied floor (such that they do not break connectivity)
    for (distance_iterator di(caster->pos(), false, true,
                              LOS_DEFAULT_RANGE); di; ++di)
    {
        if ((feat_is_wall(env.grid(*di)) && !feat_is_permarock(env.grid(*di))
             && x_chance_in_y(pow, 150))
            || (env.grid(*di) == DNGN_FLOOR && x_chance_in_y(pow, 1250)
                && !actor_at(*di) && !plant_forbidden_at(*di, true)))
        {
            temp_change_terrain(*di, DNGN_TREE, duration,
                    TERRAIN_CHANGE_FORESTED);
        }
    }

    // Maybe make a pond
    if (coinflip())
    {
        coord_def pond = find_gateway_location(caster);
        int num = random_range(10, 22);
        int deep = (!one_chance_in(3) ? div_rand_round(num, 3) : 0);

        for (distance_iterator di(pond, true, false, 4); di && num > 0; ++di)
        {
            if (env.grid(*di) == DNGN_FLOOR
                && (di.radius() == 0 || _water_adjacent(*di))
                && x_chance_in_y(4, di.radius() + 3))
            {
                num--;
                deep--;

                dungeon_feature_type feat = DNGN_SHALLOW_WATER;
                if (deep > 0 && *di != you.pos())
                {
                    monster* mon = monster_at(*di);
                    if (!mon || mon->is_habitable_feat(DNGN_DEEP_WATER))
                        feat = DNGN_DEEP_WATER;
                }

                temp_change_terrain(*di, feat, duration, TERRAIN_CHANGE_FORESTED);
            }
        }
    }

    mpr("A forested plane collides here with a resounding crunch!");
    noisy(spell_effect_noise(SPELL_SUMMON_FOREST), caster->pos());

    mgen_data dryad_data = _pal_data(MONS_DRYAD, 1, SPELL_SUMMON_FOREST);
    dryad_data.hd = 6 + div_rand_round(pow, 16);

    if (monster *dryad = create_monster(dryad_data))
    {
        mon_enchant timer = dryad->get_ench(ENCH_SUMMON_TIMER);
        timer.duration = duration - 10;
        dryad->update_ench(timer);

        // Pre-awaken the forest just summoned.
        bolt dummy;
        mons_cast(dryad, dummy, SPELL_AWAKEN_FOREST,
                  dryad->spell_slot_flags(SPELL_AWAKEN_FOREST));
    }

    you.duration[DUR_FORESTED] = duration;

    return spret::success;
}

monster_type pick_random_wraith()
{
    return random_choose_weighted(1, MONS_SHADOW_WRAITH,
                                  5, MONS_WRAITH,
                                  2, MONS_FREEZING_WRAITH,
                                  2, MONS_PHANTASMAL_WARRIOR);
}

spret cast_haunt(int pow, const coord_def& where, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON, M_FLIES, "haunt your foe"))
        return spret::abort;

    monster* m = monster_at(where);

    if (m == nullptr)
    {
        fail_check();
        mpr("An evil force gathers, but it quickly dissipates.");
        return spret::success; // still losing a turn
    }
    else if (m->wont_attack())
    {
        mpr("You cannot haunt those who bear you no hostility.");
        return spret::abort;
    }

    int mi = m->mindex();
    ASSERT(!invalid_monster_index(mi));

    if (stop_attack_prompt(m, false, you.pos()))
        return spret::abort;

    fail_check();

    bool friendly = true;
    int success = 0;
    int to_summon = min(6, 2 + random2avg(1 + div_rand_round(pow, 18), 2));

    while (to_summon--)
    {
        const monster_type mon = pick_random_wraith();

        if (monster *mons = create_monster(
                mgen_data(mon, BEH_FRIENDLY, where, mi, MG_FORCE_BEH | MG_SEE_SUMMONER)
                .set_summoned(&you, SPELL_HAUNT, summ_dur(3))))
        {
            success++;
            mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, m, INFINITE_DURATION));
            mons->foe = mi;
        }
    }

    if (success > 1)
    {
        mpr(friendly ? "Insubstantial figures form in the air."
                     : "You sense hostile presences.");
    }
    else if (success)
    {
        mpr(friendly ? "An insubstantial figure forms in the air."
                     : "You sense a hostile presence.");
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    return spret::success;
}

spret cast_martyrs_knell(const actor* caster, int pow, bool fail)
{
    if (caster->is_player() && stop_summoning_prompt(MR_RES_POISON, M_FLIES))
        return spret::abort;

    fail_check();

    mgen_data mg = _summon_data(*caster, MONS_MARTYRED_SHADE, summ_dur(2),
                                SPELL_MARTYRS_KNELL);
    mg.hd = (6 + div_rand_round(pow, 11));

    monster* shade = create_monster(mg);
    if (shade)
    {
        if (caster->is_player())
            mpr("You call forth a shade to shield your allies.");
        else if (you.can_see(*shade))
        {
            mprf("%s calls forth a shade to shield themselves.",
                 caster->name(DESC_THE).c_str());
        }

        mons_update_aura(*shade);
    }
    else if (caster->is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static spell_type servitor_spells[] =
{
    // primary spells
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_IOOD,
    SPELL_BOMBARD,
    SPELL_PLASMA_BEAM, // maybe should be higher?
    SPELL_PERMAFROST_ERUPTION,
    SPELL_FIREBALL,
    SPELL_ARCJOLT,
    SPELL_STONE_ARROW,
    SPELL_LRD,
    SPELL_AIRSTRIKE,
    // less desirable
    SPELL_IRRADIATE,
    SPELL_CONJURE_BALL_LIGHTNING, // but VERY funny
    SPELL_FREEZING_CLOUD,
    SPELL_MEPHITIC_CLOUD,
};

/**
 * Return the spell a player spellspark servitor would use, for the spell
 * description.
 *
 * @return spell_type  The spell a player servitor would use if cast now
 */
spell_type player_servitor_spell()
{
    if (you.props.exists(SERVITOR_SPELL_KEY))
    {
        spell_type spell = (spell_type)you.props[SERVITOR_SPELL_KEY].get_int();
        // Double-check that we know the spell and can cast it well enough, since
        // both things may have changed since it was saved.
        if (you.has_spell(spell) && failure_rate_to_int(raw_spell_fail(spell)) <= 20)
            return spell;
    }

    // Fallback using default list if none was specified.
    for (const spell_type spell : servitor_spells)
        if (you.has_spell(spell) && failure_rate_to_int(raw_spell_fail(spell)) <= 20)
            return spell;

    return SPELL_NO_SPELL;
}

bool spell_servitorable(spell_type to_serve)
{
    for (const spell_type spell : servitor_spells)
        if (spell == to_serve)
            return true;
    return false;
}

/**
 * Initialize the given spellspark servitor's HD and spellset, based on the
 * caster's spellpower and castable attack spells.
 *
 * @param mon       The spellspark servitor to be initialized.
 * @param caster    The entity summoning the servitor; may be the player.
 * @param pow       The caster's spellpower.
 */
static void _init_servitor_monster(monster &mon, const actor& caster, int pow)
{
    const monster* caster_mon = caster.as_monster();

    mon.set_hit_dice(7 + div_rand_round(pow, 14));
    mon.max_hit_points = mon.hit_points = 60 + roll_dice(7, 5); // 67-95
                                            // mhp doesn't vary with HD
    int spell_levels = 0;

    if (caster.is_player())
    {
        const spell_type spell = player_servitor_spell();
        mon.spells.emplace_back(spell, 0, MON_SPELL_WIZARD);
        spell_levels += spell_difficulty(spell);
    }
    else
    {
        for (const spell_type spell : servitor_spells)
        {
            if (caster.has_spell(spell))
            {
                mon.spells.emplace_back(spell, 0, MON_SPELL_WIZARD);
                spell_levels += spell_difficulty(spell);
            }
        }
    }

    // Fix up frequencies now that we know the total number of spell levels.
    const int base_freq = caster_mon ? 67 : 150;
    for (auto& slot : mon.spells)
    {
        slot.freq = max(1, div_rand_round(spell_difficulty(slot.spell)
                                          * base_freq,
                                          spell_levels));
    }
    mon.props[CUSTOM_SPELLS_KEY].get_bool() = true;
}

void init_servitor(monster* servitor, actor* caster, int pow)
{
    ASSERT(servitor); // XXX: change to monster &servitor
    ASSERT(caster); // XXX: change to actor &caster
    _init_servitor_monster(*servitor, *caster, pow);

    if (you.can_see(*caster))
    {
        mprf("%s %s a servant imbued with %s destructive magic!",
             caster->name(DESC_THE).c_str(),
             caster->conj_verb("forge").c_str(),
             caster->pronoun(PRONOUN_POSSESSIVE).c_str());
    }
    else
        simple_monster_message(*servitor, " appears!");

    int shortest_range = LOS_RADIUS + 1;
    for (const mon_spell_slot &slot : servitor->spells)
    {
        if (slot.spell == SPELL_NO_SPELL)
            continue;

        int range = spell_range(slot.spell, 100, false);
        if (range < shortest_range)
            shortest_range = range;
    }
    servitor->props[IDEAL_RANGE_KEY].get_int() = shortest_range;
}

spret cast_spellspark_servitor(int pow, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON, M_FLIES))
        return spret::abort;

    fail_check();

    mgen_data mdata = _pal_data(MONS_SPELLSPARK_SERVITOR, summ_dur(3),
                                SPELL_SPELLSPARK_SERVITOR, false);

    if (monster* mon = create_monster(mdata))
        init_servitor(mon, &you, pow);
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

void remove_player_servitor()
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_SPELLSPARK_SERVITOR && mi->summoner == MID_PLAYER)
        {
            monster_die(**mi, KILL_RESET, NON_MONSTER);
            return;
        }
    }
}

monster* find_battlesphere(const actor* agent)
{
    if (agent->props.exists(BATTLESPHERE_KEY))
        return monster_by_mid(agent->props[BATTLESPHERE_KEY].get_int());
    else
        return nullptr;
}

static int _battlesphere_hd(int pow, bool random = true)
{
    if (random)
        return 1 + div_rand_round(pow, 9);
    return 1 + pow / 9;
}

static dice_def _battlesphere_damage(int hd)
{
    return dice_def(2, 6 + hd);
}

dice_def battlesphere_damage(int pow)
{
    return _battlesphere_damage(_battlesphere_hd(pow, false));
}

spret cast_battlesphere(actor* agent, int pow, bool fail)
{
    fail_check();

    monster* battlesphere;
    if (agent->is_player() && (battlesphere = find_battlesphere(&you)))
    {
        bool recalled = false;
        if (!you.can_see(*battlesphere))
        {
            coord_def empty;
            if (find_habitable_spot_near(agent->pos(), MONS_BATTLESPHERE, 2, empty)
                && battlesphere->move_to_pos(empty))
            {
                recalled = true;
            }
        }

        if (recalled)
        {
            mpr("You recall your battlesphere and imbue it with additional"
                " charge.");
        }
        else
            mpr("You imbue your battlesphere with additional charge.");

        battlesphere->battlecharge = min(20, (int) battlesphere->battlecharge
                                              + random_range(6, 10));

        // Increase duration and update HD
        mon_enchant timer = battlesphere->get_ench(ENCH_SUMMON_TIMER);
        battlesphere->set_hit_dice(_battlesphere_hd(pow));
        timer.duration = min(timer.duration + random_range(300, 500), 500);
        battlesphere->update_ench(timer);
    }
    else
    {
        ASSERT(!find_battlesphere(agent));
        mgen_data mg (MONS_BATTLESPHERE,
                      agent->is_player() ? BEH_FRIENDLY
                                         : SAME_ATTITUDE(agent->as_monster()),
                      agent->pos(), agent->mindex());
        mg.set_summoned(agent, SPELL_BATTLESPHERE, random_range(300, 500), false);
        mg.hd = _battlesphere_hd(pow);
        battlesphere = create_monster(mg);

        if (battlesphere)
        {
            agent->props[BATTLESPHERE_KEY].get_int() = battlesphere->mid;

            if (agent->is_player())
                mpr("You conjure a globe of magical energy.");
            else
            {
                if (you.can_see(*agent) && you.can_see(*battlesphere))
                {
                    simple_monster_message(*agent->as_monster(),
                                           " conjures a globe of magical energy!");
                }
                else if (you.can_see(*battlesphere))
                    simple_monster_message(*battlesphere, " appears!");

                if (agent->is_monster())
                    battlesphere->set_band_leader(*(agent->as_monster()));
            }
            battlesphere->battlecharge = random_range(6, 10);
            battlesphere->foe = agent->mindex();
            battlesphere->target = agent->pos();
        }
        else if (agent->is_player() || you.can_see(*agent))
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return spret::success;
}

void end_battlesphere(monster* mons, bool killed)
{
    // Should only happen if you dismiss it in wizard mode, I think
    if (!mons)
        return;

    actor* agent = actor_by_mid(mons->summoner);
    if (agent)
        agent->props.erase(BATTLESPHERE_KEY);

    if (!killed)
    {
        if (agent && agent->is_player())
        {
            if (you.can_see(*mons))
            {
                if (mons->battlecharge == 0)
                {
                    mpr("Your battlesphere expends the last of its energy"
                        " and dissipates.");
                }
                else
                    mpr("Your battlesphere wavers and loses cohesion.");
            }
            else
                mpr("You feel your bond with your battlesphere wane.");
        }
        else if (you.can_see(*mons))
            simple_monster_message(*mons, " dissipates.");

        if (!cell_is_solid(mons->pos()))
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);

        monster_die(*mons, KILL_RESET, NON_MONSTER);
    }
}

bool battlesphere_can_mirror(spell_type spell)
{
    return (spell_typematch(spell, spschool::conjuration)
            || (get_spell_flags(spell) & spflag::destructive))
           && spell != SPELL_BATTLESPHERE
           && spell != SPELL_SPELLSPARK_SERVITOR;
}

vector<spell_type> player_battlesphere_spells()
{
    vector<spell_type> results;

    for (const spell_type spell : you.spells)
        if (battlesphere_can_mirror(spell))
            results.push_back(spell);

    return results;
}

static bool _is_valid_battlesphere_target(actor* caster, actor* targ)
{
    return targ && targ->alive() && !targ->is_firewood()
           && !mons_aligned(caster, targ)
           && !(caster->is_player() && targ->is_monster() && targ->as_monster()->pacified())
           && caster->can_see(*targ);
}

// Returns whether a battlesphere should fire from a given position if it wants
// to be able to hit a given target.
//
// fallback is set to our current firing position if this aim hits *some* actor
// (even if it was not our primary target) and fallback is currently (0, 0)
static bool _battlesphere_should_fire(actor* target,
                                      const coord_def& firing_pos,
                                      bolt& beam, coord_def& fallback)
{
    beam.source = firing_pos;
    beam.target = target->pos();
    beam.foe_info.reset();
    beam.friend_info.reset();

    // Fire tracer.
    beam.fire();

    // If we hit at least *something*, mark this as a possible fallback
    if (beam.friend_info.count == 0 && beam.foe_info.count > 0)
    {
        // And if we hit our primary target, actually fire
        if (beam.hit_count.count(target->mid))
            return true;
        else if (fallback.origin())
            fallback = firing_pos;
    }

    return false;
}

static void _fire_battlesphere(monster* battlesphere, bolt& beam)
{
    beam.thrower = battlesphere->summoner == MID_PLAYER ? KILL_YOU : KILL_MON;
    beam.is_tracer = false;

    battlesphere->foe = actor_at(beam.target)->mindex();
    battlesphere->target = beam.target;

    if (you.can_see(*battlesphere))
    {
        mprf("%s fires at %s!", battlesphere->name(DESC_THE).c_str(),
                                actor_at(beam.target)->name(DESC_THE).c_str());
    }
    beam.fire();

    // Decrement # of volleys left and possibly expire the battlesphere.
    if (--battlesphere->battlecharge == 0)
        end_battlesphere(battlesphere, false);
}

bool trigger_battlesphere(actor* agent)
{
    monster* battlesphere = find_battlesphere(agent);

    // If we've somehow gotten separated from the battlesphere (ie:
    // abyss level teleport), bail out and cancel the battlesphere bond
    if (!battlesphere)
    {
        agent->props.erase(BATTLESPHERE_KEY);
        return false;
    }

    // Scan visible enemies and pick the most injured one.
    // (Not the one with the lowest hp - the one with the most hp *missing*.)
    vector<actor*> targets;
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
        if (_is_valid_battlesphere_target(agent, *ai))
            targets.push_back(*ai);

    if (targets.empty())
        return false;

    shuffle_array(targets);
    sort(targets.begin( ), targets.end( ), [](actor* a, actor* b)
    {
        return a->stat_maxhp() - a->stat_hp()
               > b->stat_maxhp() - b->stat_hp();
    });

    actor* target = targets[0];

    dprf("Battlesphere targeting %s at (%d, %d).",
         target->name(DESC_THE).c_str(),
         target->pos().x,
         target->pos().y);

    // Set up the battlesphere beam
    bolt beam;
    beam.source_name = battlesphere->name(DESC_YOUR).c_str();
    beam.name        = "barrage of energy";
    beam.range       = LOS_RADIUS;
    beam.hit         = AUTOMATIC_HIT;
    beam.damage      = _battlesphere_damage(battlesphere->get_hit_dice());
    beam.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.colour      = MAGENTA;
    beam.flavour     = BEAM_MMISSILE;
    beam.pierce      = false;
    beam.target      = target->pos();
    beam.source_id   = battlesphere->mid;
    beam.attitude    = mons_attitude(*battlesphere);
    beam.is_tracer   = true;

    coord_def fallback_pos;
    // First, just try to fire from our present position
    if (_battlesphere_should_fire(target, battlesphere->pos(), beam, fallback_pos))
    {
        _fire_battlesphere(battlesphere, beam);
        return true;
    }

    // If that wasn't good enough, gather all the nearby spots that the battlesphere
    // could reach in one turn and see if any of *these* spots have a good firing
    // position. If so, move there and fire.

    // Claculate reachability by performing short-range pathfinding with monsters
    // considered opaque.
    monster_pathfind mp;
    mp.fill_traversability(battlesphere, 3, true);

    for (distance_iterator di(battlesphere->pos(), true, true, 3); di; ++di)
    {
        if (*di == target->pos() || actor_at(*di) || cell_is_solid(*di)
            || !agent->see_cell(*di) || !mp.is_reachable(*di))
        {
            continue;
        }

        if (_battlesphere_should_fire(target, *di, beam, fallback_pos))
        {
            const coord_def old_pos = battlesphere->pos();
            battlesphere->move_to_pos(*di, true, true);

            // Show battlesphere at its new location and attempt to prevent it
            // from moving further until after the next action.
            battlesphere->check_redraw(old_pos);
            battlesphere->speed_increment -= 30;

            _fire_battlesphere(battlesphere, beam);
            battlesphere->apply_location_effects(old_pos);
            return true;
        }
    }

    // We didn't find any reachable spot that fires on our primary target, but
    // we did find a spot that fires at *something* hostile, so use that.
    if (!fallback_pos.origin())
    {
        const coord_def old_pos = battlesphere->pos();
        battlesphere->move_to_pos(fallback_pos, true, true);
        battlesphere->check_redraw(old_pos);
        battlesphere->speed_increment -= 30;

        beam.source = fallback_pos;
        _fire_battlesphere(battlesphere, beam);
        battlesphere->apply_location_effects(old_pos);
        return true;
    }

    return true;
}

int prism_hd(int pow, bool random)
{
    if (random)
        return div_rand_round(pow, 10);
    return pow / 10;
}

spret cast_fulminating_prism(actor* caster, int pow, const coord_def& where,
                             bool fail, bool is_shadow)
{
    if (grid_distance(where, caster->pos())
        > spell_range(SPELL_FULMINANT_PRISM, pow))
    {
        if (caster->is_player())
            mpr("That's too far away.");
        return spret::abort;
    }

    if (cell_is_solid(where))
    {
        if (caster->is_player())
            mpr("You can't conjure that within a solid object!");
        return spret::abort;
    }

    actor* victim = monster_at(where);
    if (victim)
    {
        if (caster->can_see(*victim))
        {
            if (caster->is_player())
                mpr("You can't place the prism on a creature.");
            return spret::abort;
        }

        fail_check();

        // FIXME: maybe should do _paranoid_option_disable() here?
        if (caster->is_player()
            || (you.can_see(*caster) && you.see_cell(where)))
        {
            if (you.can_see(*victim))
            {
                mprf("%s %s.", victim->name(DESC_THE).c_str(),
                               victim->conj_verb("twitch").c_str());
            }
            else
                canned_msg(MSG_GHOSTLY_OUTLINE);
        }
        return spret::success;      // Don't give free detection!
    }

    fail_check();

    const int hd = prism_hd(pow);

    mgen_data prism_data = mgen_data(is_shadow
                                        ? MONS_SHADOW_PRISM
                                        : MONS_FULMINANT_PRISM,
                                     caster->is_player()
                                        ? BEH_FRIENDLY
                                        : SAME_ATTITUDE(caster->as_monster()),
                                     where, MHITNOT, MG_FORCE_PLACE);
    prism_data.set_summoned(caster, is_shadow ? SPELL_SHADOW_PRISM
                                              : SPELL_FULMINANT_PRISM, 0, false, false);
    prism_data.hd = hd;
    monster *prism = create_monster(prism_data);

    if (prism)
    {
        if (caster->observable())
        {
            mprf("%s %s a prism of %s energy!",
                 caster->name(DESC_THE).c_str(),
                 caster->conj_verb("conjure").c_str(),
                 is_shadow ? "shadowy" : "explosive");
        }
        else if (you.can_see(*prism))
        {
            mprf("A prism of %s energy appears from nowhere!",
                 is_shadow ? "shadowy" : "explosive");
        }

        // This looks silly, but prevents the even sillier-looking situation of
        // monster-cast prisms displaying as 'unaware of you'.
        if (caster->is_monster())
        {
            prism->foe = caster->as_monster()->foe;
            prism->behaviour = BEH_SEEK;
            prism->flags |= MF_WAS_IN_VIEW;
        }
    }
    else if (you.can_see(*caster))
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

monster* find_spectral_weapon(const actor* agent)
{
    if (agent->props.exists(SPECTRAL_WEAPON_KEY))
        return monster_by_mid(agent->props[SPECTRAL_WEAPON_KEY].get_int());
    else
        return nullptr;
}

void end_spectral_weapon(monster* mons, bool killed, bool quiet)
{
    // Should only happen if you dismiss it in wizard mode, I think
    if (!mons)
        return;

    actor *owner = actor_by_mid(mons->summoner);

    if (owner)
        owner->props.erase(SPECTRAL_WEAPON_KEY);

    if (!quiet && you.can_see(*mons))
        simple_monster_message(*mons, " disappears.");

    if (!killed)
        monster_die(*mons, KILL_RESET, NON_MONSTER);
}

void check_spectral_weapon(actor &agent)
{
    if (!agent.triggered_spectral)
        if (monster* sw = find_spectral_weapon(&agent))
            end_spectral_weapon(sw, false, false);
    agent.triggered_spectral = false;
}

static void _setup_infestation(bolt &beam, int pow)
{
    beam.name         = "infestation";
    beam.aux_source   = "infestation";
    beam.flavour      = BEAM_INFESTATION;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour       = GREEN;
    beam.source_id    = MID_PLAYER;
    beam.thrower      = KILL_YOU;
    beam.is_explosion = true;
    beam.ex_size      = 2;
    beam.ench_power   = pow;
    beam.origin_spell = SPELL_INFESTATION;
}

spret cast_infestation(int pow, bolt &beam, bool fail)
{
    if (cell_is_solid(beam.target))
    {
        canned_msg(MSG_SOMETHING_IN_WAY);
        return spret::abort;
    }

    fail_check();

    _setup_infestation(beam, pow);
    mpr("You call forth a plague of scarabs!");
    beam.explode();

    return spret::success;
}

struct summon_cap
{
    int player_cap;
    int monster_cap;
};

// spell type, player cap, monster cap
static const map<spell_type, summon_cap> summonsdata =
{
    // Player- and monster-castable spells
    { SPELL_SUMMON_SMALL_MAMMAL,      { 2, 4 } },
    { SPELL_CALL_CANINE_FAMILIAR,     { 1, 1 } },
    { SPELL_SUMMON_ICE_BEAST,         { 1, 3 } },
    { SPELL_SUMMON_HYDRA,             { 2, 3 } },
    { SPELL_SUMMON_MANA_VIPER,        { 1, 3 } },
    { SPELL_CALL_IMP,                 { 1, 3 } },
    { SPELL_MONSTROUS_MENAGERIE,      { 2, 3 } },
    { SPELL_SUMMON_HORRIBLE_THINGS,   { 8, 8 } },
    { SPELL_FORGE_LIGHTNING_SPIRE,    { 1, 1 } },
    { SPELL_FORGE_BLAZEHEART_GOLEM,   { 1, 1 } },
    { SPELL_SPELLSPARK_SERVITOR,      { 1, 1 } },
    { SPELL_AWAKEN_ARMOUR,            { 1, 1 } },
    { SPELL_MARTYRS_KNELL,            { 1, 1 } },
    { SPELL_HAUNT,                    { 8, 8 } },
    { SPELL_SUMMON_CACTUS,            { 1, 1 } },
    { SPELL_SOUL_SPLINTER,            { 1, 1 } },
    { SPELL_SIMULACRUM,               { 5, 5 } },
    { SPELL_HELLFIRE_MORTAR,          { 1, 1 } },
    { SPELL_SURPRISING_CROCODILE,     { 1, 1 } },
    { SPELL_PLATINUM_PARAGON,         { 1, 1 } },
    { SPELL_WALKING_ALEMBIC,          { 1, 1 } },
    { SPELL_SUMMON_SEISMOSAURUS_EGG,  { 1, 1 } },
    { SPELL_PHALANX_BEETLE,           { 1, 1 } },
    { SPELL_RENDING_BLADE,            { 1, 1 } },
    // Monster-only spells
    { SPELL_SHADOW_CREATURES,         { 0, 4 } },
    { SPELL_SUMMON_SPIDERS,           { 0, 5 } },
    { SPELL_SUMMON_UFETUBUS,          { 0, 8 } },
    { SPELL_SUMMON_SIN_BEAST,         { 0, 5 } },
    { SPELL_SUMMON_UNDEAD,            { 0, 8 } },
    { SPELL_SUMMON_DRAKES,            { 0, 4 } },
    { SPELL_SUMMON_MUSHROOMS,         { 0, 8 } },
    { SPELL_SUMMON_EYEBALLS,          { 0, 4 } },
    { SPELL_WATER_ELEMENTALS,         { 0, 3 } },
    { SPELL_FIRE_ELEMENTALS,          { 0, 3 } },
    { SPELL_EARTH_ELEMENTALS,         { 0, 3 } },
    { SPELL_AIR_ELEMENTALS,           { 0, 3 } },
    { SPELL_STICKS_TO_SNAKES,         { 0, 2 } },
    { SPELL_VANQUISHED_VANGUARD,      { 0, 3 } },
    { SPELL_SUMMON_MORTAL_CHAMPION,   { 0, 1 } },
    { SPELL_FIRE_SUMMON,              { 0, 4 } },
    { SPELL_SUMMON_MINOR_DEMON,       { 0, 3 } },
    { SPELL_CALL_LOST_SOULS,          { 0, 3 } },
    { SPELL_SUMMON_VERMIN,            { 0, 5 } },
    { SPELL_FORCEFUL_INVITATION,      { 0, 3 } },
    { SPELL_PLANEREND,                { 0, 6 } },
    { SPELL_SUMMON_DRAGON,            { 0, 4 } },
    { SPELL_PHANTOM_MIRROR,           { 0, 4 } },
    { SPELL_FAKE_MARA_SUMMON,         { 0, 2 } },
    { SPELL_SUMMON_EMPEROR_SCORPIONS, { 0, 6 } },
    { SPELL_SUMMON_SCARABS,           { 0, 8 } },
    { SPELL_SUMMON_HOLIES,            { 0, 4 } },
    { SPELL_SUMMON_EXECUTIONERS,      { 0, 3 } },
    { SPELL_GREATER_SERVANT_MAKHLEB,  { 0, 1 } },
    { SPELL_SUMMON_GREATER_DEMON,     { 0, 3 } },
    { SPELL_SUMMON_DEMON,             { 0, 3 } },
    { SPELL_SUMMON_TZITZIMITL,        { 0, 3 } },
    { SPELL_SUMMON_HELL_SENTINEL,     { 0, 3 } },
    { SPELL_CONJURE_LIVING_SPELLS,    { 0, 4 } },
    { SPELL_SHEZAS_DANCE,             { 0, 6 } },
    { SPELL_DIVINE_ARMAMENT,          { 0, 1 } },
    { SPELL_FLASHING_BALESTRA,        { 0, 2 } },
    { SPELL_PHANTOM_BLITZ,            { 0, 2 } },
    { SPELL_SHADOW_PUPPET,            { 3, 3 } },
    { SPELL_SHADOW_TURRET,            { 2, 2 } },
};

bool summons_are_capped(spell_type spell)
{
    ASSERT_RANGE(spell, 0, NUM_SPELLS);
    return summonsdata.count(spell);
}

int summons_limit(spell_type spell, bool player)
{
    const summon_cap *cap = map_find(summonsdata, spell);
    if (!cap)
        return 0;
    else
        return player ? cap->player_cap : cap->monster_cap;
}

static bool _spell_has_variable_cap(spell_type spell)
{
    return spell == SPELL_SHADOW_CREATURES;
}

static void _expire_capped_summon(monster* mon, bool recurse)
{
    // Timeout the summon
    monster_die(*mon, KILL_TIMEOUT, NON_MONSTER);

    if (recurse && mon->props.exists(SUMMON_ID_KEY))
    {
        const int summon_id = mon->props[SUMMON_ID_KEY].get_int();
        for (monster_iterator mi; mi; ++mi)
        {
            // Summoner check should be technically unnecessary, but saves
            // scanning props for all monsters on the level.
            if (mi->summoner == mon->summoner
                && mi->props.exists(SUMMON_ID_KEY)
                && mi->props[SUMMON_ID_KEY].get_int() == summon_id)
            {
                _expire_capped_summon(*mi, false);
            }
        }
    }
}

// Call when a monster has been summoned, to manage this summoner's caps.
void summoned_monster(const monster *mons, const actor *caster,
                      spell_type spell)
{
    int cap = summons_limit(spell, caster->is_player());
    if (!cap) // summons aren't capped
        return;

    // Cap large abominations and tentacled monstrosities separately
    if (spell == SPELL_SUMMON_HORRIBLE_THINGS)
    {
        cap = (mons->type == MONS_ABOMINATION_LARGE ? cap * 3 / 4
                                                    : cap * 1 / 4);
    }
    // Only the simulacra themselves are capped with this spell. Blocks of ice
    // are free.
    else if (spell == SPELL_SIMULACRUM && mons->type != MONS_SIMULACRUM)
        return;

    monster* oldest_summon = 0;
    int oldest_duration = 0;

    // Linked summons that have already been counted once
    set<int> seen_ids;

    int count = 1;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons == *mi)
            continue;

        // Ignore monsters that were not summoned by the caster.
        if (mi->summoner != caster->mid || !mi->is_summoned()
            || !mons_aligned(caster, *mi))
        {
            continue;
        }

        // Check that the monster was created by the spell in question.
        mon_enchant summ = mi->get_ench(ENCH_SUMMON);
        if (summ.degree != spell)
            continue;

        // Count large abominations and tentacled monstrosities separately.
        // (And don't count blocks of ice at all.)
        if ((spell == SPELL_SUMMON_HORRIBLE_THINGS || spell == SPELL_SIMULACRUM)
            && mi->type != mons->type)
        {
            continue;
        }

        if (_spell_has_variable_cap(spell) && mi->props.exists(SUMMON_ID_KEY))
        {
            const int id = mi->props[SUMMON_ID_KEY].get_int();

            // Skip any linked summon whose set we have seen already,
            // otherwise add it to the list of seen summon IDs
            if (seen_ids.find(id) == seen_ids.end())
                seen_ids.insert(id);
            else
                continue;
        }

        count++;

        // If this summon is the closest to expiry, remember it
        if (!oldest_summon || summ.duration < oldest_duration)
        {
            oldest_summon = *mi;
            oldest_duration = summ.duration;
        }
    }

    if (oldest_summon && count > cap)
        _expire_capped_summon(oldest_summon, true);
}

int count_summons(const actor *summoner, spell_type spell)
{
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (summoner == *mi)
            continue;

        if (mi->was_created_by(*summoner, spell) && mons_aligned(summoner, *mi))
            count++;
    }

    return count;
}

static bool _create_briar_patch(coord_def& target)
{
    mgen_data mgen = mgen_data(MONS_BRIAR_PATCH, BEH_FRIENDLY, target,
            MHITNOT, MG_FORCE_PLACE, GOD_FEDHAS);
    mgen.hd = mons_class_hit_dice(MONS_BRIAR_PATCH) +
        you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, SPELL_NO_SPELL,
                        summ_dur(min(2 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6)));

    if (create_monster(mgen))
    {
        mpr("A briar patch grows up from the ground.");
        return true;
    }

    return false;
}

vector<coord_def> find_briar_spaces(bool just_check)
{
    vector<coord_def> result;

    for (adjacent_iterator adj_it(you.pos()); adj_it; ++adj_it)
    {
        if (monster_habitable_grid(MONS_BRIAR_PATCH, *adj_it)
            && (!actor_at(*adj_it)
                || just_check && !you.can_see(*actor_at(*adj_it))))
        {
            result.push_back(*adj_it);
        }
    }

    return result;
}

void fedhas_wall_of_briars()
{
    // How many adjacent open spaces are there?
    vector<coord_def> adjacent = find_briar_spaces();

    if (adjacent.empty())
    {
        mpr("Something you can't see blocks your briars from growing!");
        return;
    }

    int created_count = 0;
    for (auto p : adjacent)
    {
        if (_create_briar_patch(p))
            created_count++;
    }

    if (!created_count)
        canned_msg(MSG_NOTHING_HAPPENS);

    return;
}

static void _overgrow_wall(const coord_def &pos)
{
    const dungeon_feature_type feat = env.grid(pos);
    const string what = feature_description(feat, NUM_TRAPS, "", DESC_THE);

    if (monster_at(pos))
    {
        mprf("Something unseen blocks growth in %s.", what.c_str());
        return;
    }

    destroy_wall(pos);

    const monster_type mon = random_choose_weighted(4, MONS_OKLOB_SAPLING,
                                                    4, MONS_SCRUB_NETTLE,
                                                    4, MONS_WANDERING_MUSHROOM,
                                                    1, MONS_BALLISTOMYCETE,
                                                    1, MONS_OKLOB_PLANT);
    mgen_data mgen(mon, BEH_FRIENDLY, pos, MHITYOU, MG_FORCE_PLACE, GOD_FEDHAS);
    mgen.hd = mons_class_hit_dice(mon) + you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, SPELL_NO_SPELL,
                        summ_dur(min(3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6)));
    if (const monster* const plant = create_monster(mgen))
    {
        mprf("%s is torn apart as %s grows in its place.", what.c_str(),
                plant->name(DESC_A).c_str());
    }
    // XXX: Maybe try to make this revert the terrain if a monster isn't placed.
    else
        mprf("%s falls apart, but nothing grows.", what.c_str());
}

spret fedhas_overgrow(bool fail)
{
    targeter_overgrow tgt;
    direction_chooser_args args;
    args.hitfunc = &tgt;
    args.restricts = DIR_TARGET;
    args.mode = TARG_ANY;
    args.range = LOS_RADIUS;
    args.just_looking = false;
    args.needs_path = false;
    args.top_prompt = "Aiming: <white>Overgrow</white>";
    dist sdirect;
    direction(sdirect, args);
    if (!sdirect.isValid)
        return spret::abort;

    fail_check();

    for (auto site : tgt.affected_positions)
        _overgrow_wall(site);

    return spret::success;
}

spret fedhas_grow_ballistomycete(const coord_def& target, bool fail)
{
    if (grid_distance(target, you.pos()) > 2 || !in_bounds(target))
    {
        mpr("That's too far away.");
        return spret::abort;
    }

    if (!monster_habitable_grid(MONS_BALLISTOMYCETE, target))
    {
        mpr("You can't grow a ballistomycete there.");
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (mons)
    {
        if (you.can_see(*mons))
        {
            mpr("That space is already occupied.");
            return spret::abort;
        }

        fail_check();

        // invisible monster
        mpr("Something you can't see occupies that space!");
        return spret::success;
    }

    fail_check();

    mgen_data mgen(MONS_BALLISTOMYCETE, BEH_FRIENDLY, target, MHITYOU,
            MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE, GOD_FEDHAS);
    mgen.hd = mons_class_hit_dice(MONS_BALLISTOMYCETE) +
        you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, SPELL_NO_SPELL,
                        summ_dur(min(3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6)));

    if (create_monster(mgen))
        mpr("A ballistomycete grows from the ground.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret fedhas_grow_oklob(const coord_def& target, bool fail)
{
    if (grid_distance(target, you.pos()) > 2 || !in_bounds(target))
    {
        mpr("That's too far away.");
        return spret::abort;
    }

    if (!monster_habitable_grid(MONS_OKLOB_PLANT, target))
    {
        mpr("You can't grow an oklob plant there.");
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (mons)
    {
        if (you.can_see(*mons))
        {
            mpr("That space is already occupied.");
            return spret::abort;
        }

        fail_check();

        // invisible monster
        mpr("Something you can't see is occupying that space!");
        return spret::success;
    }

    fail_check();

    mgen_data mgen(MONS_OKLOB_PLANT, BEH_FRIENDLY, target, MHITYOU,
            MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE, GOD_FEDHAS);
    mgen.hd = mons_class_hit_dice(MONS_OKLOB_PLANT) +
        you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, SPELL_NO_SPELL,
                        summ_dur(min(3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6)));

    if (create_monster(mgen))
        mpr("An oklob plant grows from the ground.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

void kiku_unearth_wretches()
{
    const int pow = you.skill(SK_NECROMANCY, 5);
    const int min_wretches = 1 + random2(2);
    const int max_wretches = min_wretches + div_rand_round(pow, 27); // 7 max
    const int wretches = random_range(min_wretches, max_wretches);
    bool created = false;
    for (int i = 0; i < wretches; ++i)
    {
        // choose a type
        const int typ_pow = you.skill(SK_NECROMANCY, 4);
        const int adjusted_power = min(typ_pow / 4, random2(random2(typ_pow)));
        const level_id lev(you.where_are_you, adjusted_power
                           - absdungeon_depth(you.where_are_you, 0));
        const monster_type mon_type = pick_local_corpsey_monster(lev);
        ASSERT(mons_class_can_be_zombified(mons_species(mon_type)));
        // place a monster
        mgen_data mg(mon_type,
                     BEH_HOSTILE, // so players don't have attack prompts
                     you.pos(),
                     MHITNOT);
        mg.extra_flags = MF_NO_REWARD // ?
                       | MF_NO_REGEN;
        mg.set_range(2, 4);
        mg.props[KIKU_WRETCH_KEY] = true;

        monster *mon = create_monster(mg);
        if (!mon)
            continue;

        created = true;
        mons_add_blame(mon, "unearthed by the player character");
        mon->hit_points = 1;
        mon->props[ALWAYS_CORPSE_KEY] = true;
        mon->props[KIKU_WRETCH_KEY] = true;

        // Die in 2-3 turns.
        mon->add_ench(mon_enchant(ENCH_SLOWLY_DYING, 1, nullptr,
                                   20 + random2(10)));
        mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0, &you, 9999));
    }
    if (!created)
        simple_god_message(" has no space to call forth the wretched!");
    else
        simple_god_message(" calls piteous wretches from the earth!");
}

static bool _create_foxfire(const actor &agent, coord_def pos, int pow,
                            bool marshlight = false)
{
    const auto att = agent.is_player() ? BEH_FRIENDLY
                                       : SAME_ATTITUDE(agent.as_monster());
    mgen_data fox(MONS_FOXFIRE, att,
                  pos, (att != BEH_FRIENDLY && agent.is_monster())
                            ? agent.as_monster()->foe : int{MHITNOT},
                  MG_FORCE_PLACE | MG_AUTOFOE);
    fox.set_summoned(&agent, SPELL_FOXFIRE, random_range(40, 70), false, false);
    fox.hd = pow;
    monster *foxfire;

    if (cell_is_solid(pos) || actor_at(pos))
        return false;

    foxfire = create_monster(fox);
    if (!foxfire)
        return false;

    foxfire->steps_remaining = you.current_vision + 2;
    if (marshlight)
        foxfire->props[MONSTER_TILE_KEY] = TILEP_MONS_MARSHLIGHT;

    // Avoid foxfire without targets always moving towards (0,0)
    if (!foxfire->get_foe()
        || !foxfire->get_foe()->is_monster() && !agent.is_monster())
    {
        set_random_target(foxfire);
    }
    return true;
}

spret cast_foxfire(actor &agent, int pow, bool fail, bool marshlight)
{
    bool see_space = false;
    for (adjacent_iterator ai(agent.pos()); ai; ++ai)
    {
        if (cell_is_solid(*ai))
            continue;
        if (actor_at(*ai) && agent.can_see(*actor_at(*ai)))
            continue;
        see_space = true;
        break;
    }

    if (agent.is_player() && !see_space)
    {
        mpr("There is not enough space to conjure foxfire!");
        return spret::abort;
    }

    fail_check();

    int created = 0;

    for (fair_adjacent_iterator ai(agent.pos()); ai; ++ai)
    {
        if (!_create_foxfire(agent, *ai, pow, marshlight))
            continue;
        ++created;
        if (created == 2)
            break;
    }

    if (created && you.see_cell(agent.pos()))
    {
        mprf("%s conjure%s some %s!",
             agent.name(DESC_THE).c_str(),
             agent.is_monster() ? "s" : "",
             marshlight ? "marshlight" : "foxfire");
    }
    else if (agent.is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret foxfire_swarm()
{
    bool created = false;
    bool unknown_unseen = false;
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (_create_foxfire(you, *ri, GOD_NO_GOD, 20))
        {
            created = true;
            continue;
        }
        const actor* agent = actor_at(*ri);
        if (agent && !you.can_see(*agent))
            unknown_unseen = true;
    }
    if (created)
    {
        mpr("Flames leap up around you!");
        return spret::success;
    }
    if (!unknown_unseen)
    {
        mpr("There's no space to create foxfire here.");
        return spret::abort;
    }
    canned_msg(MSG_NOTHING_HAPPENS);
    return spret::fail; // don't spend piety, do spend a turn
}

bool summon_hell_out_of_bat(const actor &agent, coord_def pos)
{
    // Since this isn't really used as for a spell: count how many creatures
    // this has already summoned, and abort at our max defined here (four).
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
       if (mi->summoner == agent.mid)
           ++count;

        if (count > 3)
           return false;
    }

    monster_type mon = coinflip() ? MONS_HELL_HOUND : MONS_HELL_RAT;

    monster *mons = create_monster(
            mgen_data(mon, BEH_COPY, pos, _auto_autofoe(&agent), MG_AUTOFOE)
                      .set_summoned(&agent, SPELL_NO_SPELL, summ_dur(1)));
    if (mons)
        return true;

    return false;
}

bool summon_swarm_clone(const monster& agent, coord_def target_pos)
{
    // Go up the summon chain to find the highest-level version of ourselves
    const monster* parent = &agent;
    while (parent->summoner && monster_by_mid(parent->summoner)
           && monster_by_mid(parent->summoner)->type == agent.type)
    {
        parent = monster_by_mid(parent->summoner);
    }

    // Apply pseudo-summon cap
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
       if (mi->summoner == parent->mid)
           ++count;

        if (count > 8)
           return false;
    }

    mgen_data mg(agent.type, BEH_COPY, target_pos, _auto_autofoe(parent), MG_AUTOFOE);
    mg.set_summoned(parent, SPELL_NO_SPELL, summ_dur(2));

    if (monster* spawn = create_monster(mg))
    {
        if (you.can_see(*spawn))
            mprf("Another %s is drawn to the feast!", spawn->name(DESC_PLAIN).c_str());
        return true;
    }

    return false;
}

bool summon_spider(const actor &agent, coord_def pos,
                        spell_type spell, int pow)
{
    monster_type mon = random_choose_weighted(100, MONS_REDBACK,
                                              100, MONS_JUMPING_SPIDER,
                                               75, MONS_TARANTELLA,
                                               50, MONS_CULICIVORA,
                                               50, MONS_ORB_SPIDER,
                                               pow / 2, MONS_WOLF_SPIDER);

    monster *mons = create_monster(
            mgen_data(mon, BEH_COPY, pos, _auto_autofoe(&agent), MG_AUTOFOE)
                      .set_summoned(&agent, spell, summ_dur(3)));
    if (mons)
        return true;

    return false;
}

spret summon_spiders(actor &agent, int pow, bool fail)
{
    // Can't happen at present, but why not check just to be sure.
    if (agent.is_player() && stop_summoning_prompt())
        return spret::abort;

    fail_check();

    int created = 0;

    for (int i = 0; i < 1 + div_rand_round(random2(pow), 80); i++)
    {
        if (summon_spider(agent, agent.pos(), SPELL_SUMMON_SPIDERS, pow))
            created++;
    }

    if (created && you.see_cell(agent.pos()))
    {
        mprf("%s %s %s!",
             agent.name(DESC_THE).c_str(),
             agent.conj_verb("summon").c_str(),
             created > 1 ? "spiders" : "a spider");
    }
    else if (agent.is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

int barrelling_boulder_hp(int pow)
{
    return 15 + pow / 3;
}

spret cast_broms_barrelling_boulder(actor& agent, coord_def targ, int pow, bool fail)
{
    fail_check();

    ray_def ray;
    if (!find_ray(agent.pos(), targ, ray, opc_solid) || !ray.advance())
    {
        mpr("There's something in the way.");
        return spret::abort;
    }
    const coord_def pos = ray.pos();

    // For unseen invisble enemies
    if (actor_at(pos))
    {
        mpr("Something unseen is already there!");
        return spret::success;
    }

    mgen_data mg = mgen_data(MONS_BOULDER,
                             agent.is_player()
                                ? BEH_FRIENDLY
                                : SAME_ATTITUDE(agent.as_monster()),
                             pos, MHITNOT, MG_FORCE_PLACE);
    mg.set_summoned(&agent, SPELL_BOULDER);
    mg.hp = barrelling_boulder_hp(pow);
    monster *boulder = create_monster(mg);

    // If some other reason prevents this from working (I'm not sure what?)
    if (!boulder)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    boulder->props[BOULDER_POWER_KEY] = pow;
    // XX: This will produce weird data if the caster isn't adjacent to the target spot.
    //     Currently that can't happen, but in future it might.
    boulder->props[BOULDER_DIRECTION_KEY] = pos - agent.pos();

    mpr("You send a boulder barrelling forward!");

    // Let the boulder roll one space immediately.
    boulder->speed_increment = 80;

    return spret::success;
}

string mons_simulacrum_immune_reason(const monster *mons)
{
    if (!mons || !you.can_see(*mons))
        return "You can't see anything there.";

    if (!mons_can_be_spectralised(*mons))
        return "You can't make a simulacrum of that!";

    if (mons->friendly() || mons->neutral())
        return "That would be terribly rude!";

    return "";
}

spret cast_simulacrum(coord_def target, int pow, bool fail)
{
    if (cell_is_solid(target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (!mons || !you.can_see(*mons))
    {
        fail_check();
        canned_msg(MSG_NOTHING_CLOSE_ENOUGH);
        // If there's no monster there, you still pay the costs in
        // order to prevent locating invisible monsters.
        return spret::success;
    }

    if (!mons_can_be_spectralised(*mons))
    {
        mpr("You can't make simulacra of that!");
        return spret::abort;
    }

    fail_check();

    mprf("You sublimate a sliver of %s essence and reconstitute it in ice.",
         apostrophise(mons->name(DESC_THE)).c_str());

    int num_simulacra = 2;
    if (x_chance_in_y(pow, 200))
        num_simulacra = 3;

    int delay = random_range(3, 5) * BASELINE_DELAY;
    for (int i = 0; i < num_simulacra; ++i)
    {
        // Note that this *not* marked as coming from SPELL_SIMULACRUM
        mgen_data mg = _pal_data(MONS_BLOCK_OF_ICE, 0, SPELL_NO_SPELL);
        mg.set_summoned(&you, SPELL_SIMULACRUM, delay, false, false);
        mg.base_type = mons->type;
        mg.hd = 8; // make them more durable
        monster *ice = create_monster(mg);

        if (ice)
        {
            ice->props[SIMULACRUM_TYPE_KEY] = mons->type;
            ice->add_ench(mon_enchant(ENCH_SIMULACRUM_SCULPTING, 0, &you, INFINITE_DURATION));
            ice->flags |= MF_WAS_IN_VIEW;

            // Make each one shift a little later than the last
            delay += random_range(1, 3) * BASELINE_DELAY;
        }
    }

    return spret::success;
}

static int _hoarfrost_cannon_hd(int pow, bool random = true)
{
    if (random)
        return 4 + div_rand_round(pow, 20);
    return 4 + pow / 20;
}

dice_def hoarfrost_cannonade_damage(int pow, bool finale)
{
    if (finale)
        return zap_damage(ZAP_HOARFROST_BULLET_FINALE, _hoarfrost_cannon_hd(pow, false) * 12, true, false);
    else
        return zap_damage(ZAP_HOARFROST_BULLET, _hoarfrost_cannon_hd(pow, false) * 12, true, false);
}

spret cast_hoarfrost_cannonade(const actor& agent, int pow, bool fail)
{
    fail_check();

    // Remove any existing cannons we may have first
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->was_created_by(agent, SPELL_HOARFROST_CANNONADE))
            monster_die(**mi, KILL_TIMEOUT, NON_MONSTER);
    }

    // Monsters summon cannons around their foe instead of themselves (both to
    // let them operate with other monsters in the way and because it's too easy
    // for the player to back out of range otherwise)
    mgen_data cannon = _summon_data(agent, MONS_HOARFROST_CANNON, 0,
                                    SPELL_HOARFROST_CANNONADE);
    cannon.pos = (agent.is_player() ? you.pos()
                                    : agent.as_monster()->get_foe()->pos());
    cannon.hd = _hoarfrost_cannon_hd(pow);
    cannon.set_range(3, 3, 1);

    // Make both cannons share the same duration
    cannon.set_summoned(&agent, SPELL_HOARFROST_CANNONADE,
                        random_range(16, 22) * BASELINE_DELAY, false);

    int num_seen = 0;


    for (int i = 0; i < 2; ++i)
    {
        monster* mons = create_monster(cannon);
        if (mons)
        {
            // Give a bit of instant energy so the slow cannons don't take
            // multiple turns to fire their first shot.
            mons->speed_increment = 70;

            if (you.can_see(*mons))
                ++num_seen;
        }
    }

    if (num_seen > 1)
    {
        mprf("%s sculpt%s a pair of cannons out of ice!",
             agent.name(DESC_THE).c_str(),
             agent.is_player() ? ""  : "s");
    }
    else if (num_seen == 1)
    {
        mprf("%s sculpt%s a cannon out of ice!",
             agent.name(DESC_THE).c_str(),
             agent.is_player() ? ""  : "s");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static int _hellfire_mortar_hd(int pow, bool random = true)
{
    if (random)
        return 1 + div_rand_round(pow, 10);
    return 1 + pow / 10;
}

dice_def hellfire_mortar_damage(int pow)
{
    return zap_damage(ZAP_MAGMA_BARRAGE, _hellfire_mortar_hd(pow, false) * 12, true, false);
}

static bool _hellfire_stops_here(bolt& beam, coord_def pos)
{
    return actor_at(pos) && !beam.ignores_monster(monster_at(pos))
           || cell_is_solid(pos) && !beam.can_affect_wall(pos);
}

spret cast_hellfire_mortar(const actor& agent, bolt& beam, int pow, bool fail)
{
    // Determine path by firing digging tracer
    zappy(ZAP_HELLFIRE_MORTAR_DIG, pow, agent.is_monster(), beam);
    beam.source = agent.pos();
    beam.source_id = agent.mid;
    beam.origin_spell = SPELL_HELLFIRE_MORTAR;
    beam.is_tracer = true;
    beam.fire();

    // Check whether the path ends because it reached max range, or because it
    // hit something invalid.
    const int len = _hellfire_stops_here(beam, beam.path_taken.back())
                    ? beam.path_taken.size() - 1
                    : beam.path_taken.size();

    // Abort if the player knows this path is blocked. (Invisible monsters at
    // close range need to be handled later, alas)
    if (agent.is_player())
    {
        monster* mon = monster_at(beam.path_taken[0]);
        if (mon && you.can_see(*mon))
        {
            mprf("%s is in the way!", mon->name(DESC_THE).c_str());
            return spret::abort;
        }
        else if (len == 0 && !mon)
        {
            mpr("There's no room!");
            return spret::abort;
        }
    }

    fail_check();

    // Likely because of an invisible monster standing in our first cannon spot
    if (actor_at(beam.path_taken[0]))
    {
        if (agent.is_player())
            mpr("Something prevents your mortar from forming!");
        return spret::success;
    }

    // Make the lava
    int dur = random_range(15, 19) * BASELINE_DELAY;
    for (unsigned int i = 0; i < beam.path_taken.size(); ++i)
    {
        const coord_def pos = beam.path_taken[i];

        if (!in_bounds(pos))
            break;

        // Don't make lava under things that can't survive there.
        if (monster_at(pos) && !beam.ignores_monster(monster_at(pos))
            || pos == you.pos() && !you.airborne())
        {
            break;
        }

        if (feat_is_solid(env.grid(pos)) && !feat_is_diggable(env.grid(pos))
            && !feat_is_tree(env.grid(pos)))
        {
            break;
        }

        temp_change_terrain(beam.path_taken[i], DNGN_LAVA,
                            dur - (i * BASELINE_DELAY),
                            TERRAIN_CHANGE_HELLFIRE_MORTAR);

        flash_tile(pos, RED, 5);
    }

    noisy(spell_effect_noise(SPELL_HELLFIRE_MORTAR), agent.pos(), agent.mid);

    mgen_data mg = _summon_data(agent, MONS_HELLFIRE_MORTAR, 0,
                                SPELL_HELLFIRE_MORTAR);
    mg.set_summoned(&agent, SPELL_HELLFIRE_MORTAR, dur, false, false);
    mg.flags |= MG_FORCE_PLACE;
    mg.pos = beam.path_taken[0];
    mg.hd = _hellfire_mortar_hd(pow);
    mg.hp = 100;
    monster* cannon = create_monster(mg);

    // Unclear why this could happen (we've already checked that the spot is
    // empty), but let's guard against it anyway.
    if (!cannon)
    {
        if (agent.is_player())
            mpr("Something prevents your mortar from forming!");
        return spret::success;
    }

    // Store the cannon's movement path
    CrawlVector& path = cannon->props[HELLFIRE_PATH_KEY].get_vector();
    for (unsigned int i = 0; i < beam.path_taken.size(); ++i)
    {
        const coord_def pos = beam.path_taken[i];
        path.push_back(pos);
    }

    mprf("With a deafening crack, the ground splits apart in the path of %s "
        "chthonic artillery!", agent.name(DESC_ITS).c_str());

    return spret::success;
}

bool make_soul_wisp(const actor& agent, actor& victim)
{
    if (victim.is_monster() && !mons_can_be_spectralised(*victim.as_monster(), true))
        return false;

    // Don't try to create a wisp from a monster who's already had one made from
    // them. (This causes weird messaging and removes the Weak effect).
    if (victim.is_monster() && victim.props.exists(SOUL_SPLINTERED_KEY))
    {
        if (agent.is_player() && victim.observable())
        {
            mprf("A fragment of %s is already outside their body!",
                    victim.name(DESC_THE).c_str());
        }
        return false;
    }

    mgen_data mg = _summon_data(agent, MONS_SOUL_WISP, summ_dur(2), SPELL_SOUL_SPLINTER);
    mg.pos = victim.pos();
    mg.set_range(1);
    mg.flags |= MG_SEE_SUMMONER;

    // Damage improves when extracted from stronger enemies, but they are always fragile.
    mg.hd = 2 + div_rand_round(victim.get_experience_level(), 2);
    mg.hp = random_range(5, 8);

    if (monster* wisp = create_monster(mg))
    {
        if (you.see_cell(victim.pos()))
        {
            mprf("A fragment of %s soul is dislodged from %s body.",
                    victim.name(DESC_ITS).c_str(),
                    victim.pronoun(PRONOUN_POSSESSIVE).c_str());

        }

        wisp->add_ench(mon_enchant(ENCH_HAUNTING, 1, &victim, INFINITE_DURATION));
        // Let wisp act immediately (so that if it appears behind the enemy, the
        // enemy won't simply move out of range first).
        wisp->speed_increment = 80;
        victim.weaken(&agent, wisp->get_ench(ENCH_SUMMON_TIMER).duration / 10);
        victim.props[SOUL_SPLINTERED_KEY]= true;

        return true;
    }
    else if (you.see_cell(victim.pos()))
    {
        if (agent.is_player()
            && !find_habitable_spot_near(victim.pos(), MONS_SOUL_WISP, 1, mg.pos, 0, &you))
        {
            mpr("There's no room for the soul wisp to form!");
        }
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return false;
}

static monster* _get_clockwork_bee_target(const monster* bee = nullptr)
{
    // Try to resume targeting the last thing we were targeting before going
    // dormant (if it's still around and valid).
    if (bee && bee->props.exists(CLOCKWORK_BEE_TARGET))
    {
        monster* targ = monster_by_mid(bee->props[CLOCKWORK_BEE_TARGET].get_int());
        if (targ && targ->alive() && !targ->wont_attack() && you.can_see(*targ))
            return targ;
    }

    monster* targ = nullptr;
    int num_seen = 0;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mi->wont_attack() && !mi->is_firewood() && you.can_see(**mi))
        {
            if (one_chance_in(++num_seen))
                targ = *mi;
        }
    }

    return targ;
}

void handle_clockwork_bee_spell(int turn)
{
    if (turn < 4)
    {
        mpr("You continue winding your clockwork bee...");
        return;
    }

    stop_channelling_spells(true);

    // Try to find what we targeted when we started casting.
    monster* targ = monster_by_mid(you.props[CLOCKWORK_BEE_TARGET].get_int());

    // If the original target is not available, pick another suitable one at random.
    if (!targ || !targ->alive() || !targ->visible_to(&you)
        || !you.see_cell(targ->pos()))
    {
        targ = _get_clockwork_bee_target();

        // Couldn't find anything, so just deposit an inert bee near us
        if (!targ)
        {
            mgen_data mg = _pal_data(MONS_CLOCKWORK_BEE_INACTIVE,
                                        random_range(80, 120),
                                     SPELL_CLOCKWORK_BEE, false).set_range(1, 2);
            mg.hd = 5 + div_rand_round(calc_spell_power(SPELL_CLOCKWORK_BEE), 20);
            if (monster* bee = create_monster(mg))
            {
                mprf("Without a target, your clockwork bee falls to the ground.");

                // Still remember our original target, just in case it merely
                // ducked out of sight for a moment and the player wants to
                // reactivate us.
                if (targ)
                    bee->props[CLOCKWORK_BEE_TARGET].get_int() = targ->mid;

                return;
            }
        }
    }

    mgen_data mg = _pal_data(MONS_CLOCKWORK_BEE, random_range(400, 500),
                             SPELL_CLOCKWORK_BEE, false).set_range(1, 2);
    mg.hd = 5 + div_rand_round(calc_spell_power(SPELL_CLOCKWORK_BEE), 20);

    monster* bee = create_monster(mg);
    if (bee)
    {
        bee->number = 3 + div_rand_round(calc_spell_power(SPELL_CLOCKWORK_BEE), 15);
        bee->add_ench(mon_enchant(ENCH_HAUNTING, 1, targ, INFINITE_DURATION));

        mprf("With a metallic buzz, your clockwork bee launches itself at %s.",
             targ->name(DESC_THE).c_str());

        bee->speed_increment = 80;
        bee->props[CLOCKWORK_BEE_TARGET].get_int() = targ->mid;
    }
    else
        mpr("Your bee fails to deploy!");
}

spret cast_clockwork_bee(coord_def target, bool fail)
{
    fail_check();

    monster* targ = monster_at(target);

    if (!targ || !you.can_see(*targ))
    {
        mpr("You see nothing there to target.");
        return spret::abort;
    }
    else if (targ->wont_attack())
    {
        mpr("Your bee can only target hostiles.");
        return spret::abort;
    }

    you.props[CLOCKWORK_BEE_TARGET].get_int() = targ->mid;

    mprf("You lock target on %s and prepare to deploy your bee.", targ->name(DESC_THE).c_str());
    start_channelling_spell(SPELL_CLOCKWORK_BEE, "continue winding your bee", false);

    // Remove any existing bee
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->was_created_by(you, SPELL_CLOCKWORK_BEE))
        {
            if (you.can_see(**mi))
                mprf("Your existing bee falls apart.");
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
        }
    }

    return spret::success;
}

void clockwork_bee_go_dormant(monster& bee)
{
    mpr("Your clockwork bee winds down and falls to the ground.");

    int old_hd = bee.get_experience_level();
    int old_max_hp = bee.max_hit_points;
    int old_hp = bee.hit_points;
    change_monster_type(&bee, MONS_CLOCKWORK_BEE_INACTIVE);
    bee.set_hit_dice(old_hd);
    bee.max_hit_points = old_max_hp;
    bee.hit_points = old_hp;

    // Set duration to no longer than a small-ish amount (since the active bee's
    // duration is deliberately high so that it doesn't expire while active in
    // normal circumstances)
    mon_enchant timer = bee.get_ench(ENCH_SUMMON_TIMER);
    timer.duration = min(timer.duration, random_range(80, 120));
    bee.update_ench(timer);

    // Might have falled into deep water or lava!
    mons_check_pool(&bee, bee.pos(), KILL_RESET);
}

// Attempts to repair and rewind a clockwork bee.
// Returns false if we lacked the MP to do so or there was no valid target for it.
bool clockwork_bee_recharge(monster& bee)
{
    if (you.berserk())
    {
        mpr("If you tried to rewind gears in your present state, you'd only break them.");
        return false;
    }

    monster* targ = _get_clockwork_bee_target(&bee);

    // Nothing around for it to attack.
    if (!targ)
    {
        mpr("You need a visible target to rewind your bee! "
            "(Use ctrl+direction or * direction to deconstruct it instead.)");
        return false;
    }

    if (!enough_mp(1, true))
    {
        mpr("You lack sufficient magical power to recharge your bee.");
        return false;
    }

    pay_mp(1);
    finalize_mp_cost();

    mprf("You wind your clockwork bee back up and it locks its sights upon %s!",
         targ->name(DESC_THE).c_str());
    int old_max_hp = bee.max_hit_points;
    int old_hp = bee.hit_points;
    int old_hd = bee.get_experience_level();
    change_monster_type(&bee, MONS_CLOCKWORK_BEE);
    bee.set_hit_dice(old_hd);
    bee.max_hit_points = old_max_hp;
    bee.hit_points = old_hp;
    bee.heal(roll_dice(3, 5));
    bee.add_ench(mon_enchant(ENCH_SUMMON_TIMER, 0, &you, random_range(400, 500)));
    bee.add_ench(mon_enchant(ENCH_HAUNTING, 0, targ, INFINITE_DURATION));
    bee.number = 3 + div_rand_round(calc_spell_power(SPELL_CLOCKWORK_BEE), 15);
    bee.speed_increment = 80;

    return true;
}

void clockwork_bee_pick_new_target(monster& bee)
{
    monster* targ = _get_clockwork_bee_target();

    // Nothing around for it to attack
    if (!targ)
       clockwork_bee_go_dormant(bee);
    else
    {
        mprf("Your clockwork bee locks its sights upon %s.", targ->name(DESC_THE).c_str());
        bee.add_ench(mon_enchant(ENCH_HAUNTING, 0, targ, INFINITE_DURATION));
        bee.props[CLOCKWORK_BEE_TARGET].get_int() = targ->mid;
    }
}

dice_def diamond_sawblade_damage(int power)
{
    return zap_damage(ZAP_SHRED, (1 + div_rand_round(power, 10)) * 12, true, false);
}

vector<coord_def> diamond_sawblade_spots(bool actual)
{
    const static coord_def offsets[4] =
    {
        coord_def(-2, -2),
        coord_def(2, -2),
        coord_def(2, 2),
        coord_def(-2, 2)
    };

    vector<coord_def> spots;

    for (int i = 0; i < 4; ++i)
    {
        coord_def spot = you.pos() + offsets[i];
        if (!in_bounds(spot) || !you.see_cell_no_trans(spot)
            || !feat_has_solid_floor(env.grid(spot)))
        {
            continue;
        }

        // Cannot place on top of another actor (but can always replace
        // existing sawblades made by this spell).
        if (const actor* act = actor_at(spot))
        {
            if (!(act->type == MONS_DIAMOND_SAWBLADE
                  && act->was_created_by(SPELL_DIAMOND_SAWBLADES))
                && (actual || you.can_see(*act)))
            {
                continue;
            }
        }

        spots.push_back(spot);
    }

    return spots;
}

spret cast_diamond_sawblades(int power, bool fail)
{
    fail_check();

    vector<coord_def> spots = diamond_sawblade_spots(true);

    // Implies there is an invisible monster occupying one of the spaces we were
    // going to make a sawblade.
    if (spots.empty())
    {
        mpr("You try to forge sawblades, but something it already there!");
        return spret::success;
    }

    // Remove all our old sawblades first.
    for (monster_iterator mi; mi; ++mi)
        if (mi->was_created_by(you, SPELL_DIAMOND_SAWBLADES))
            monster_die(**mi, KILL_TIMEOUT, NON_MONSTER);

    int dur = random_range(140, 220);
    mgen_data mg = _pal_data(MONS_DIAMOND_SAWBLADE, dur,
                             SPELL_DIAMOND_SAWBLADES, false).set_range(0);
    mg.hd = 1 + div_rand_round(power, 10);

    // Dmage should increase with power, but HP stay constant.
    mg.hp = 40 + random2avg(20, 2);

    for (size_t i = 0; i < spots.size(); ++i)
    {
        mg.pos = spots[i];
        create_monster(mg);
    }

    if (spots.size() == 1)
        mpr("You forge a whirling saw of razor-sharp crystal.");
    else
        mpr("You forge whirling saws of razor-sharp crystal.");

    return spret::success;
}

string surprising_crocodile_unusable_reason(const actor& agent, const coord_def& target,
                                            bool actual)
{
    if (!monster_habitable_grid(MONS_CROCODILE, agent.pos()))
        return "A crocodile could not survive beneath you.";

    actor* targ = actor_at(target);
    if (!targ || !agent.can_see(*targ) || mons_aligned(&agent, targ))
        return "You can't see a valid target there.";

    const coord_def drag_shift = -(target - agent.pos()).sgn();
    const coord_def move_pos = agent.pos() + drag_shift;
    if (cell_is_solid(move_pos)
        || actor_at(move_pos) && (actual || agent.can_see(*actor_at(move_pos))))
    {
        return "There's not enough room behind you.";
    }

    if (!agent.is_habitable(move_pos)
         || agent.is_player() && need_expiration_warning(move_pos))
    {
        return "It isn't safe to move backwards.";
    }

    // Nothing is preventing this spell from being cast (even if the crocodile
    // will not be able to drag the monster anywhere).
    return "";
}

// Assuming the spell is castable at all (see surprising_crocodile_unusable_reason()
// above), is there room and suitable terrain for the crocodile to drag the
// target back a tile at the same time?
bool surprising_crocodile_can_drag(const actor& agent, const coord_def& target,
                                   bool actual)
{
    // First, check if it is possible to pull the target at all.
    // (We specifically *don't* check constriction here, since this spell will
    // pull constricted monsters, unlike normal dragging.)
    actor* targ = actor_at(target);
    if (targ->is_stationary()
        || !targ->is_habitable(agent.pos())
        || targ->is_player() && need_expiration_warning(agent.pos())
        || targ->resists_dislodge(""))
    {
        return false;
    }

    // Then check if the summoned and crocodile can also back off a space
    const coord_def drag_shift = -(target - agent.pos()).sgn();
    coord_def croc_move_pos = agent.pos() + drag_shift;
    coord_def agent_move_pos = croc_move_pos + drag_shift;

    if (!agent.is_habitable(agent_move_pos)
        || agent.is_player() && need_expiration_warning(agent_move_pos)
        || !monster_habitable_grid(MONS_CROCODILE, croc_move_pos))
    {
        return false;
    }

    // Can't move the agent into an occupied space
    if (actor_at(agent_move_pos) && (actual || agent.can_see(*actor_at(agent_move_pos))))
        return false;

    return true;
}

spret cast_surprising_crocodile(actor& agent, const coord_def& targ, int pow, bool fail)
{
    fail_check();

    // The targeter will prevent casting this at times where the player *knows*
    // it won't work, but it's possible there's still an invisible creature in
    // the way that will cause this to fail.
    string msg = surprising_crocodile_unusable_reason(agent, targ, true);
    if (!msg.empty())
    {
        mpr("Something unseen prevents your spell from working!");
        return spret::success;
    }

    // At this point, we know we can place the crocodile and move the caster,
    // but we don't know where the caster needs to move. Check whether they will
    // move one tile back or two.
    bool can_drag = surprising_crocodile_can_drag(agent, targ, true);

    const coord_def start_pos = agent.pos();
    const coord_def drag_shift = -(targ - agent.pos()).sgn();
    coord_def agent_pos = agent.pos() + drag_shift;
    if (can_drag)
        agent_pos += drag_shift;

    // Move agent to theiir destination grid *first*, so there's room to move the
    // other things (but don't trigger location effects yet)
    agent.move_to_pos(agent_pos, true, true);

    actor* victim = actor_at(targ);

    mgen_data mg = _pal_data(MONS_CROCODILE, summ_dur(2), SPELL_SURPRISING_CROCODILE);
    mg.pos = start_pos;
    mg.set_range(0);
    mg.foe = victim->mindex();
    mg.hd = 4 + div_rand_round(pow, 15);
    monster* croc = create_monster(mg);

    // Probably only possible if the monster array is filled?
    if (!croc)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    if (you.can_see(agent))
    {
        mprf("A crocodile bursts forth beneath %s and grabs %s in its jaws!",
                agent.name(DESC_THE).c_str(),
                victim->name(DESC_THE).c_str());
    }

    // Perform the drag silently, before the actual attack (so that it will
    // always happen, regardless of whether we do 0 damage)
    melee_attack atk(croc, victim);
    atk.needs_message = false;
    atk.do_drag();

    // Then perform the actual attack, with bonus power.
    // (But check that we didn't pull them into a shaft first.)
    if (victim->alive())
    {
        atk.needs_message = true;
        atk.dmg_mult = 20 + pow;
        atk.to_hit = AUTOMATIC_HIT;
        atk.attack();
    }

    croc->flags & ~MF_JUST_SUMMONED;

    if (you.can_see(agent))
    {
        mprf("%s dismount%s %s crocodile.",
             agent.name(DESC_THE).c_str(),
             agent.is_player() ? "" : "s",
             agent.pronoun(PRONOUN_POSSESSIVE).c_str());
    }

    // Make the temporary water (after the movement, so we don't get slash
    // messages before the main part appears to happen).)
    for (int i = 0; i < 3; ++i)
    {
        coord_def spot = start_pos + (drag_shift * (i - 1));
        if (env.grid(spot) == DNGN_FLOOR || env.grid(spot) == DNGN_SHALLOW_WATER)
        {
            temp_change_terrain(spot, DNGN_SHALLOW_WATER, random_range(130, 190),
                                TERRAIN_CHANGE_FLOOD);
        }
    }

    agent.apply_location_effects(start_pos);

    return spret::success;
}

static void _paragon_tempest(const coord_def& target)
{
    monster* paragon = find_player_paragon();
    const int level = paragon_charge_level(*paragon);

    const int radius = level == 2 ? 3 : 2;
    const int mult = level == 2 ? 200 : 125;

    // Make the paragon leap to its new position if the player told it to move.
    if (target != paragon->pos())
    {
        const coord_def old_pos = paragon->pos();

        bolt visual;
        visual.flavour = BEAM_VISUAL;
        visual.colour = WHITE;
        visual.source = old_pos;
        visual.target = target;
        visual.range = LOS_RADIUS;
        visual.aimed_at_spot = true;
        visual.fire();

        paragon->move_to_pos(target, true, true);
        paragon->check_redraw(old_pos);
    }

    vector<monster*> targs;
    for (radius_iterator ri(paragon->pos(), radius, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
    {
        if (!you.see_cell_no_trans(*ri))
            continue;

        if (monster* mon = monster_at(*ri))
        {
            if (!mon->wont_attack())
                targs.push_back(mon);
        }
    }

    shuffle_array(targs);
    for (monster* mon : targs)
    {
        flash_tile(mon->pos(), WHITE, 20, TILE_BOLT_PARAGON_TEMPEST);
        view_clear_overlays();
    }

    mprf("Your paragon expends all of its energy in an overwhelming flurry of blows!");
    for (monster* mon : targs)
    {
        melee_attack atk(paragon, mon);
        atk.dmg_mult = mult;
        // The flavor is that the paragon is actually dashing around, but let's
        // not worry about the details.
        atk.is_projected = true;
        atk.attack();
    }

    monster_die(*paragon, KILL_NON_ACTOR, NON_MONSTER);
}


spret cast_platinum_paragon(const coord_def& target, int pow, bool fail)
{
    fail_check();

    monster* old_paragon = find_player_paragon();
    if (old_paragon)
    {
        _paragon_tempest(target);
        return spret::success;
    }

    if (!monster_habitable_grid(MONS_PLATINUM_PARAGON, target))
    {
        mpr("You cannot deploy your paragon there!");
        return spret::abort;
    }

    const int dur = summ_dur(4);
    mgen_data mg = _pal_data(MONS_PLATINUM_PARAGON, dur,
                             SPELL_PLATINUM_PARAGON, false);
    mg.set_range(0).pos = target;

    // If there's an invisible monster at the target, kindly toss it out of the
    // way. This is too high-level a spell to fail for such reasons.
    if (monster* blocker = monster_at(target))
    {
        coord_def spot;
        if (random_near_space(blocker, blocker->pos(), spot, true)
            && env.grid(spot) != DNGN_TRAP_DISPERSAL)
        {
            blocker->blink_to(spot, true);
        }
        else
            monster_teleport(blocker, true);

        // If we somehow cannot move them, just crush them. (This will probably
        // never happen, but it's better not to have even theoretical problems.)
        if (blocker->pos() == target)
            monster_die(*blocker, KILL_NON_ACTOR, NON_MONSTER);
    }

    monster* paragon = create_monster(mg);

    if (!paragon)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    mpr("You craft a gleaming metal champion and it leaps into the fray!");
    you.duration[DUR_PARAGON_ACTIVE] = dur;

    // Grab our imprinted weapon
    if (you.props.exists(PARAGON_WEAPON_KEY))
    {
        item_def wpn;
        wpn = you.props[PARAGON_WEAPON_KEY].get_item();
        wpn.flags |= (ISFLAG_SUMMONED | ISFLAG_REPLICA);
        give_specific_item(paragon, wpn);
    }

    paragon->ghost->init_platinum_paragon(pow);
    paragon->ghost_demon_init();

    // Do the landing shockwave.
    bolt shockwave;
    shockwave.source_id = paragon->mid;
    shockwave.source = target;
    shockwave.target = target;
    shockwave.is_explosion = true;
    shockwave.ex_size = 1;
    zappy(ZAP_PARAGON_IMPACT, pow, true, shockwave);
    shockwave.explode(true, true);

    return spret::success;
}

monster* find_player_paragon()
{
    if (!you.duration[DUR_PARAGON_ACTIVE])
        return nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_PLATINUM_PARAGON && mi->was_created_by(you, SPELL_PLATINUM_PARAGON))
            return *mi;
    }

    return nullptr;
}

void paragon_attack_trigger()
{
    monster* paragon = find_player_paragon();
    if (!paragon || !you.can_see(*paragon))
        return;

    int reach = paragon->reach_range();

    int num_found = 0;
    monster* targ = nullptr;
    for (radius_iterator ri(paragon->pos(), reach, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
    {
        monster* mon = monster_at(*ri);

        if (!mon || mon->wont_attack() || mon->is_firewood() || !you.see_cell_no_trans(*ri))
            continue;

        if (one_chance_in(++num_found))
            targ = mon;
    }

    if (!targ)
        return;

    mpr("Your paragon attacks with you!");
    fight_melee(paragon, targ);
    paragon->speed_increment += paragon->action_energy(EUT_ATTACK);
}

int paragon_charge_level(const monster& paragon)
{
    if (paragon.type != MONS_PLATINUM_PARAGON)
        return 0;

    if (paragon.number >= 13)
        return 2;
    else if (paragon.number >= 7)
        return 1;

    return 0;
}

void paragon_charge_up(monster& paragon)
{
    const int old_charge = paragon_charge_level(paragon);
    ++paragon.number;

    // A little fuzzing
    if (one_chance_in(5))
        ++paragon.number;

    const int new_charge = paragon_charge_level(paragon);

    if (new_charge > old_charge)
    {
        if (new_charge == 2)
            mprf(MSGCH_DURATION, "Your paragon has reached its maximum power!");
        else if (new_charge == 1)
            mprf(MSGCH_DURATION, "Your paragon is now ready to unleash a masterful attack.");
    }
}

bool paragon_defense_bonus_active()
{
    const monster* paragon = find_player_paragon();

    return paragon && grid_distance(you.pos(), paragon->pos()) <=2
           && you.can_see(*paragon);
}

spret cast_walking_alembic(const actor& agent, int pow, bool fail)
{
    fail_check();

    mgen_data mg = _summon_data(agent, MONS_WALKING_ALEMBIC, summ_dur(3),
                                SPELL_WALKING_ALEMBIC, false);
    mg.hd = (7 + div_rand_round(pow, 16));

    if (monster* mon = create_monster(mg))
    {
        if (you.can_see(*mon))
            mpr("A lumbering aparatus takes shape within a cloud of fumes.");

        mon->number = random_range(4, 6);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static void _do_player_potion()
{
    if (player_in_branch(BRANCH_COCYTUS))
    {
        mpr("The potion freezes solid before it can reach you!");
        return;
    }
    else if (!you.can_drink())
    {
        mpr("You sigh wistfully at the memory of the taste.");
        return;
    }

    vector<pair<potion_type, int>> weights;

    if (get_potion_effect(POT_HASTE)->can_quaff())
        weights.push_back({POT_HASTE, 50});
    // The double HP/MP check here is that so that oni won't be extra-likely to
    // get these potions over something more exciting, just because they could
    // technically cleave with it.
    if (you.magic_points < you.max_magic_points && get_potion_effect(POT_MAGIC)->can_quaff())
        weights.push_back({POT_MAGIC, 30});
    if (you.hp < you.hp_max && (get_potion_effect(POT_HEAL_WOUNDS)->can_quaff()))
        weights.push_back({POT_HEAL_WOUNDS, 50});
    if (get_potion_effect(POT_MIGHT)->can_quaff())
        weights.push_back({POT_MIGHT, 50});
    if (get_potion_effect(POT_INVISIBILITY)->can_quaff())
        weights.push_back({POT_INVISIBILITY, 30});

    if (weights.empty())
        return;

    flash_tile(you.pos(), random_choose(LIGHTBLUE, LIGHTGREEN, LIGHTMAGENTA),
               120, TILE_BOLT_ALEMBIC_POTION);

    potion_type potion = *random_choose_weighted(weights);

    mprf("Mmmm... tastes like %s.", potion_type_name(potion));

    if (you.has_mutation(MUT_DRUNKEN_BRAWLING) && oni_likes_potion(potion))
        oni_drunken_swing();

    // Mildly shorter duration than drinking the potion normally.
    get_potion_effect(potion)->effect(true, 15);
}

// Pick a potion effect that would *do* something to the monster
static bool _do_monster_potion(monster& mons, monster& alembic)
{
    vector<pair<potion_type, int>> weights;

    if (!mons.has_ench(ENCH_HASTE))
        weights.push_back({POT_HASTE, 50});
    if (!mons.has_ench(ENCH_MIGHT) && mons_has_attacks(mons))
        weights.push_back({POT_MIGHT, 75});
    if (!mons.has_ench(ENCH_EMPOWERED_SPELLS) && mons.antimagic_susceptible())
        weights.push_back({POT_BRILLIANCE, 75});
    if (mons.hit_points * 2 / 3 < mons.max_hit_points)
        weights.push_back({POT_HEAL_WOUNDS, 35});

    if (weights.empty())
        return false;

    potion_type potion = *random_choose_weighted(weights);

    flash_tile(mons.pos(), random_choose(LIGHTBLUE, LIGHTGREEN, LIGHTMAGENTA),
               60, TILE_BOLT_ALEMBIC_POTION);

    switch (potion)
    {
        case POT_HASTE:
            enchant_actor_with_flavour(&mons, &alembic, BEAM_HASTE);
            return true;

        case POT_MIGHT:
            enchant_actor_with_flavour(&mons, &alembic, BEAM_MIGHT);
            return true;

        case POT_BRILLIANCE:
            simple_monster_message(mons, " magic is enhanced!", true);
            mons.add_ench(mon_enchant(ENCH_EMPOWERED_SPELLS, 1, &alembic));
            return true;

        case POT_HEAL_WOUNDS:
            simple_monster_message(mons, " is healed!");
            mons.heal(random_range(30, 50));
            return true;

        default:
            break;
    }

    return false;
}

void alembic_brew_potion(monster& mons)
{
    simple_monster_message(mons, " finishes brewing potions and dispenses them!",
                            false, MSGCH_MONSTER_SPELL);

    if (mons.friendly() && you.see_cell_no_trans(mons.pos()))
    {
        if (grid_distance(you.pos(), mons.pos()) <= 3)
            _do_player_potion();
        else
            mpr("But you're too far away!");
    }

    int num_potions = 5;
    for (distance_iterator di(mons.pos(), true, true, 3); di && num_potions > 0; ++di)
    {
        if (monster* targ = monster_at(*di))
        {
            if (mons_aligned(targ, &mons) && !targ->is_peripheral()
                && mons.see_cell_no_trans(*di))
            {
                if (_do_monster_potion(*targ, mons))
                    --num_potions;
            }
        }
    }

    simple_monster_message(mons, " collapses with a clattering noise.", false,
                           MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    monster_die(mons, KILL_RESET, NON_MONSTER);
}

spret cast_monarch_bomb(const actor& agent, int pow, bool fail)
{
    fail_check();

    if (count_summons(&agent, SPELL_MONARCH_BOMB))
        return monarch_detonation(agent, pow);

    mgen_data mg = _summon_data(agent, MONS_MONARCH_BOMB, summ_dur(3),
                                SPELL_MONARCH_BOMB, false);
    mg.hd = (7 + div_rand_round(pow, 12));

    if (monster* mon = create_monster(mg))
    {
        if (you.can_see(*mon))
        {
            mprf("%s construct%s an explosive harbinger and set it loose.",
                 agent.name(DESC_THE).c_str(), agent.is_player() ? "" : "s");
        }

        mon->number = 5 + div_rand_round(pow, 50);
        //mon->number = random_range(1, 3);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

bool monarch_deploy_bomblet(monster& original, const coord_def& target,
                            bool quiet)
{
    mgen_data mg = mgen_data(MONS_BOMBLET, original.friendly()
                                                ? BEH_FRIENDLY
                                                : BEH_HOSTILE,
                             target, MG_AUTOFOE);
    mg.set_summoned(actor_by_mid(original.summoner),
                    SPELL_MONARCH_BOMB, random_range(90, 160), false);
    mg.set_range(0, 2);
    if (create_monster(mg))
    {
        if (!quiet && you.can_see(original))
            mprf("%s deploys a bomblet.", original.name(DESC_THE).c_str());

        return true;
    }

    return false;
}

vector<coord_def> get_monarch_detonation_spots(const actor& agent)
{
    vector<coord_def> spots;

    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        if (mi->was_created_by(you, SPELL_MONARCH_BOMB))
        {
            for (adjacent_iterator ai(mi->pos(), false); ai; ++ai)
            {
                if (agent.see_cell_no_trans(*ai) && !cell_is_solid(*ai)
                    && *ai != agent.pos())
                {
                    spots.push_back(*ai);
                }
            }
        }
    }

    return spots;
}

spret monarch_detonation(const actor& agent, int pow)
{
    vector<coord_def> spots = get_monarch_detonation_spots(agent);
    if (agent.is_player()
        && warn_about_bad_targets("Detonate Monarch Bomb", spots,
                                  [](const monster& m) { return m.type == MONS_MONARCH_BOMB
                                                                || m.type == MONS_BOMBLET; }))
    {
        return spret::abort;
    }

    if (you.can_see(agent))
    {
        mprf("%s command%s %s explosives to detonate!",
                agent.name(DESC_THE).c_str(),
                agent.is_player() ? "" : "s",
                agent.pronoun(PRONOUN_POSSESSIVE).c_str());
    }

    for (coord_def spot : spots)
        flash_tile(spot, RED, 0, TILE_BOLT_BOMBLET_BLAST);
    if (Options.use_animations & UA_BEAM)
        animation_delay(100, true);

    // Now remove all the bomblets, whether they blew up or not (ie: ones that
    // weren't in LoS of the caster at the time).
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->was_created_by(agent, SPELL_MONARCH_BOMB))
        {
            noisy(spell_effect_noise(SPELL_MONARCH_BOMB), mi->pos());
            monster_die(**mi, KILL_RESET, NON_MONSTER);
        }
    }

    bolt detonation;
    zappy(ZAP_MONARCH_DETONATION, pow, false, detonation);
    detonation.set_agent(&agent);
    detonation.ex_size       = 0;
    detonation.apply_beam_conducts();
    detonation.in_explosion_phase = true;

    for (coord_def spot : spots)
    {
        if (!actor_at(spot) || actor_at(spot) != &agent)
        {
            bolt det = detonation;
            det.explosion_affect_cell(spot);
        }
    }

    return spret::success;
}

static bool _push_line_back(const coord_def& center, const coord_def& dir)
{
    // We want to trace a line of all connected monsters in a row, in the
    // direction we're moving, and then push them away in reverse order.
    vector<actor*> push_targs;
    coord_def pos = center + dir;
    while (actor_at(pos) && !actor_at(pos)->is_stationary())
    {
        push_targs.push_back(actor_at(pos));
        pos += dir;
    }

    for (int i = push_targs.size() - 1; i >= 0; --i)
        if (push_targs[i]->alive()) // died from earlier knockback?
            push_targs[i]->stumble_away_from(center);

    return !actor_at(center + dir);
}


vector<coord_def> get_splinterfrost_block_spots(const actor& agent,
                                              const coord_def& aim, int num_walls)
{
    vector<coord_def> spots;

    // Convert aim to a compass direction
    coord_def delta = (aim - agent.pos()).sgn();
    int dir = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (Compass[i] == delta)
        {
            dir = i;
            break;
        }
    }

    // Now choose adjacent compass spots to test
    int start = dir - ((num_walls - 1) / 2);
    if (start < 0)
        start = start + 8;

    for (int i = start; i < start + num_walls; ++i)
    {
        const int index = i % 8;
        const coord_def spot = agent.pos() + Compass[index];
        if (in_bounds(spot) && !cell_is_solid(spot)
            && env.grid(spot) != DNGN_LAVA
            && !feat_is_trap(env.grid(spot)))
        {
            spots.push_back(spot);
        }
    }

    return spots;
}

spret cast_splinterfrost_shell(const actor& agent, const coord_def& aim,
                             int pow, bool fail)
{
    fail_check();

    for (monster_iterator mi; mi; ++mi)
        if (mi->was_created_by(agent, SPELL_SPLINTERFROST_SHELL))
            monster_die(**mi, KILL_RESET, NON_MONSTER);

    const int dur = random_range(110, 160);
    mgen_data mg = _summon_data(agent, MONS_SPLINTERFROST_BARRICADE, dur,
                                SPELL_SPLINTERFROST_SHELL, false);
    mg.hd = 10 + div_rand_round(pow, 20);
    mg.set_range(0);

    vector<coord_def> spots = get_splinterfrost_block_spots(agent, aim, 4);
    int num_created = 0;
    for (size_t i = 0; i < spots.size(); ++i)
    {
        if (actor_at(spots[i]))
        {
            if (!_push_line_back(agent.pos(), spots[i] - agent.pos()))
                continue;
        }

        mg.pos = spots[i];
        if (monster* block = create_monster(mg))
        {
            block->props[SPLINTERFROST_POWER_KEY] = pow;
            ++num_created;
        }
    }

    if (num_created > 0)
        mprf("You construct a shell of ice in front of yourself.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

bool splinterfrost_block_fragment(monster& block, const coord_def& aim)
{
    const int pow = block.props[SPLINTERFROST_POWER_KEY].get_int();
    actor* agent = actor_by_mid(block.summoner);

    ray_def ray;
    if (!find_ray(block.pos(), aim, ray, opc_solid_see))
        return false;

    // Examine spaces one at a time, stopping before we'd hit a friendly
    // non-firewood actor.
    // XXX: It feels wrong not using a beam tracer for this, but tracers will
    //      simply refuse to fire if an ally is anywhere in the path, rather
    //      than ending their shot a bit earlier. That is fine for normal
    //      monsters, but as a reactive player thing, it feels bad if the
    //      barricade doesn't detonate when it looks like it should.
    int steps_taken = 0;
    coord_def aim_spot;
    while (ray.advance() && steps_taken < LOS_RADIUS)
    {
        ++steps_taken;
        const coord_def p = ray.pos();

        if (!in_bounds(p) || cell_is_solid(p))
            break;
        else if (actor* targ = actor_at(p))
        {
            // Don't hurt allies.
            if (mons_aligned(&block, targ) && !targ->is_firewood())
                break;
        }

        aim_spot = p;
    }

    if (aim_spot.origin())
        return false;

    bolt beam;
    zappy(ZAP_SPLINTERFROST_FRAGMENT, pow, false, beam);
    beam.source = block.pos();
    beam.attitude = block.attitude;
    beam.set_agent(agent);
    beam.dont_stop_player = true;
    beam.target = aim_spot;
    beam.range = steps_taken;
    beam.aimed_at_spot = true;

    string msg;
    if (you.can_see(block))
    {
        msg = make_stringf("%s fragments into a salvo of icicles!",
                            block.name(DESC_THE).c_str());
    }
    splinterfrost_fragment_fineff::schedule(beam, msg);

    return true;
}

spret cast_summon_seismosaurus_egg(const actor& agent, int pow, bool fail)
{
    fail_check();

    mgen_data egg = _summon_data(agent, MONS_SEISMOSAURUS_EGG, summ_dur(3),
                                 SPELL_SUMMON_SEISMOSAURUS_EGG);
    egg.hd = (7 + div_rand_round(pow, 20));
    egg.set_range(3, 3, 1);

    if (monster* mons = create_monster(egg))
    {
        mpr("A rock-encrusted egg appears nearby and begins to stir.");
        mons->add_ench(mon_enchant(ENCH_HATCHING, 0, &agent, random_range(6, 9)));
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_phalanx_beetle(const actor& agent, int pow, bool fail)
{
    fail_check();

    mgen_data beetle = _summon_data(agent, MONS_PHALANX_BEETLE, summ_dur(4),
                                    SPELL_PHALANX_BEETLE, false);
    beetle.hd = 9 + div_rand_round(pow, 25);
    beetle.set_range(1, 2);

    if (monster* mons = create_monster(beetle))
    {
        mpr("You forge a skittering defender to stand by your side.");
        you.props[PHALANX_BARRIER_POWER_KEY] = 400 + pow * 3;
        mons_update_aura(*mons);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static int _rending_blade_power(int base_power)
{
    const int mp_spent = max(0, you.magic_points - spell_difficulty(SPELL_RENDING_BLADE));
    const int mp_bonus = stepdown(mp_spent, 10);
    return base_power * (100 + mp_bonus * 3) / 100;
}

dice_def rending_blade_damage(int power, bool include_mp)
{
    const int pow = include_mp ? _rending_blade_power(power) : power;
    return zap_damage(ZAP_RENDING_SLASH, pow, true, false);
}

spret cast_rending_blade(int pow, bool fail)
{
    fail_check();

    mgen_data blade = _pal_data(MONS_RENDING_BLADE, random_range(7, 10) * BASELINE_DELAY,
                                SPELL_RENDING_BLADE, false);

    if (monster* mon = create_monster(blade))
    {
        mpr("You condense your magic into a crackling blade!");
        you.props[RENDING_BLADE_MP_KEY].get_int() = you.magic_points;
        mon->props[RENDING_BLADE_POWER_KEY] = _rending_blade_power(pow);
        pay_mp(you.magic_points);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

// Find our rending blade and increase its stored trigger count by one, so that
// it will try to fire when its turn comes around.
void trigger_rending_blade()
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_RENDING_BLADE && mi->summoner == MID_PLAYER)
        {
            // Only save up to 3 charges at once, so that axes aren't overly
            // ridiclous and that you can't bank too many charges while the
            // blade isn't positioned to act.
            mi->number = min((unsigned int)3, mi->number + 1);
            mi->speed_increment = 100;
            return;
        }
    }
}
