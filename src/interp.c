/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik Strfeldt, Tom Madsen, and Katja Nyboe.    *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  ROM 2.4 improvements copyright (C) 1993-1998 Russ Taylor, Gabrielle    *
 *  Taylor and Brian Moore                                                 *
 *                                                                         *
 *  Crimson Skies (CS-Mud) copyright (C) 1998-2015 by Blake Pell (Rhien)   *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt' as well as the ROM license.  In particular,   *
 *  you may not remove these copyright notices.                            *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 **************************************************************************/

// System Specific Includes
#if defined(__APPLE__)
    #include <types.h>
    #include <time.h>
#elif defined(_WIN32)
    #include <sys/types.h>
    #include <time.h>
#else
    #include <sys/types.h>
    #include <sys/time.h>
    #include <time.h>
#endif

// General Includes
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "merc.h"
#include "interp.h"

bool check_social args ((CHAR_DATA * ch, char *command, char *argument));

/*
 * Command logging types.
 */
#define LOG_NORMAL  0
#define LOG_ALWAYS  1
#define LOG_NEVER   2

/*
 * Log-all switch.
 */
bool fLogAll = FALSE;

/*
 * Command table.
 */
const struct cmd_type cmd_table[] = {
    /*
     * Common movement commands.
     */
    { "north",     do_north,     POS_STANDING, 0, LOG_NEVER, 0},
    { "east",      do_east,      POS_STANDING, 0, LOG_NEVER, 0},
    { "south",     do_south,     POS_STANDING, 0, LOG_NEVER, 0},
    { "west",      do_west,      POS_STANDING, 0, LOG_NEVER, 0},
    { "up",        do_up,        POS_STANDING, 0, LOG_NEVER, 0},
    { "down",      do_down,      POS_STANDING, 0, LOG_NEVER, 0},
    { "northeast", do_northeast, POS_STANDING, 0, LOG_NEVER, 0},
    { "ne",        do_northeast, POS_STANDING, 0, LOG_NEVER, 0},
    { "northwest", do_northwest, POS_STANDING, 0, LOG_NEVER, 0},
    { "nw",        do_northwest, POS_STANDING, 0, LOG_NEVER, 0},
    { "southeast", do_southeast, POS_STANDING, 0, LOG_NEVER, 0},
    { "se",        do_southeast, POS_STANDING, 0, LOG_NEVER, 0},
    { "southwest", do_southwest, POS_STANDING, 0, LOG_NEVER, 0},
    { "sw",        do_southwest, POS_STANDING, 0, LOG_NEVER, 0},

    /*
     * Common other commands.
     * Placed here so one and two letter abbreviations work.
     */
    {"at",        do_at,        POS_DEAD,     L6, LOG_NORMAL, 1},
    {"cast",      do_cast,      POS_FIGHTING, 0,  LOG_NORMAL, 1},
    {"auction",   do_auction,   POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"buy",       do_buy,       POS_RESTING,  0,  LOG_NORMAL, 1},
    {"channels",  do_channels,  POS_DEAD,     0,  LOG_NORMAL, 1},
    {"exits",     do_exits,     POS_RESTING,  0,  LOG_NORMAL, 1},
    {"get",       do_get,       POS_RESTING,  0,  LOG_NORMAL, 1},
    {"goto",      do_goto,      POS_DEAD,     L8, LOG_NORMAL, 1},
    {"group",     do_group,     POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"guild",     do_guild,     POS_DEAD,     L4, LOG_ALWAYS, 1},
    {"hit",       do_kill,      POS_FIGHTING, 0,  LOG_NORMAL, 0},
    {"inventory", do_inventory, POS_DEAD,     0,  LOG_NORMAL, 1},
    {"kill",      do_kill,      POS_FIGHTING, 0,  LOG_NORMAL, 1},
    {"look",      do_look,      POS_RESTING,  0,  LOG_NORMAL, 1},
    {"clan",      do_clantalk,  POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"order",     do_order,     POS_RESTING,  0,  LOG_NORMAL, 1},
    {"practice",  do_practice,  POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"rest",      do_rest,      POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"scan",      do_scan,      POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"sit",       do_sit,       POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"sockets",   do_sockets,   POS_DEAD,     L4, LOG_NORMAL, 1},
    {"stand",     do_stand,     POS_SLEEPING, 0,  LOG_NORMAL, 1},
    {"tell",      do_tell,      POS_RESTING,  0,  LOG_NORMAL, 1},
    {"unlock",    do_unlock,    POS_RESTING,  0,  LOG_NORMAL, 1},
    {"wield",     do_wear,      POS_RESTING,  0,  LOG_NORMAL, 1},
    {"wizhelp",   do_wizhelp,   POS_DEAD,     IM, LOG_NORMAL, 1},
    {"clear",     do_clear,     POS_DEAD,     0,  LOG_NORMAL, 1},
    {"second",    do_second,    POS_RESTING,  0,  LOG_NORMAL, 1},
    {"dual",      do_second,    POS_RESTING,  0,  LOG_NORMAL, 1},

    /*
     * Informational commands.
     */
    {"affects",   do_affects,   POS_DEAD,     0, LOG_NORMAL, 1},
    {"areas",     do_areas,     POS_DEAD,     0, LOG_NORMAL, 1},
/*  {"bug",       do_bug,       POS_DEAD,     0, LOG_NORMAL, 1}, */
    {"commands",  do_commands,  POS_DEAD,     0, LOG_NORMAL, 1},
    {"compare",   do_compare,   POS_RESTING,  0, LOG_NORMAL, 1},
    {"consider",  do_consider,  POS_RESTING,  0, LOG_NORMAL, 1},
    {"count",     do_count,     POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"credits",   do_credits,   POS_DEAD,     0, LOG_NORMAL, 1},
    {"equipment", do_equipment, POS_DEAD,     0, LOG_NORMAL, 1},
    {"examine",   do_examine,   POS_RESTING,  0, LOG_NORMAL, 1},
    {"help",      do_help,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"info",      do_groups,    POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"motd",      do_motd,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"read",      do_read,      POS_RESTING,  0, LOG_NORMAL, 1},
    {"report",    do_report,    POS_RESTING,  0, LOG_NORMAL, 1},
    {"rules",     do_rules,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"score",     do_score,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"oldscore",  do_oldscore,  POS_DEAD,     0, LOG_NORMAL, 1},
    {"skills",    do_skills,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"socials",   do_socials,   POS_DEAD,     0, LOG_NORMAL, 1},
    {"show",      do_show,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"spells",    do_spells,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"time",      do_time,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"typo",      do_typo,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"weather",   do_weather,   POS_RESTING,  0, LOG_NORMAL, 1},
    {"who",       do_who,       POS_DEAD,     0, LOG_NORMAL, 1},
    {"whois",     do_whois,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"whoami",    do_whoami,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"wizlist",   do_wizlist,   POS_DEAD,     0, LOG_NORMAL, 1},
    {"worth",     do_worth,     POS_SLEEPING, 0, LOG_NORMAL, 1},

    /*
     * Configuration commands.
     */
    {"alia",      do_alia,        POS_DEAD,     0, LOG_NORMAL, 0},
    {"alias",     do_alias,       POS_DEAD,     0, LOG_NORMAL, 1},
    {"autolist",  do_autolist,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"autoall",   do_autoall,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"autoassist",do_autoassist,  POS_DEAD,     0, LOG_NORMAL, 1},
    {"autoexit",  do_autoexit,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"autogold",  do_autogold,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"autoloot",  do_autoloot,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"autosac",   do_autosac,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"autosplit", do_autosplit,   POS_DEAD,     0, LOG_NORMAL, 1},
    {"brief",     do_brief,       POS_DEAD,     0, LOG_NORMAL, 1},
    {"color",     do_color,       POS_DEAD,     0, LOG_NORMAL, 1},
    {"combine",   do_combine,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"compact",   do_compact,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"description", do_description,POS_DEAD,     0, LOG_NORMAL, 1},
    {"delet",     do_delet,       POS_DEAD,     0, LOG_ALWAYS, 0},
    {"delete",    do_delete,      POS_STANDING, 0, LOG_ALWAYS, 1},
    {"nofollow",  do_nofollow,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"noloot",    do_noloot,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"nosummon",  do_nosummon,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"outfit",    do_outfit,      POS_RESTING,  0, LOG_NORMAL, 1},
    {"password",  do_password,    POS_DEAD,     0, LOG_NEVER,  1},
    {"prompt",    do_prompt,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"scroll",    do_scroll,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"telnetga",  do_telnetga,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"title",     do_title,       POS_DEAD,     0, LOG_NORMAL, 1},
    {"unalias",   do_unalias,     POS_DEAD,     0, LOG_NORMAL, 1},
    {"wimpy",     do_wimpy,       POS_DEAD,     0, LOG_NORMAL, 1},

    /*
     * Communication commands.
     */
    {"afk",             do_afk,         POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"answer",          do_answer,      POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"deaf",            do_deaf,        POS_DEAD,     0, LOG_NORMAL, 1},
    {"emote",           do_emote,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"pmote",           do_pmote,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"gossip",          do_gossip,      POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"grats",           do_grats,       POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"gtell",           do_gtell,       POS_DEAD,     0, LOG_NORMAL, 1},
    {"question",        do_question,    POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"quiet",           do_quiet,       POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"reply",           do_reply,       POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"replay",          do_replay,      POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"say",             do_say,         POS_RESTING,  0, LOG_NORMAL, 1},
    {"shout",           do_shout,       POS_RESTING,  3, LOG_NORMAL, 1},
    {"yell",            do_yell,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"pray",            do_pray,        POS_DEAD,     0, LOG_ALWAYS, 1},
    {"immtalk",         do_immtalk,     POS_DEAD,    IM, LOG_NORMAL, 1},

    /*
     * Note commands.
     */
    {"oocn",            do_ooc_spool,    POS_DEAD,     0, LOG_NORMAL, 1},
    {"note",            do_note,         POS_DEAD,     0, LOG_NORMAL, 1},
    {"news",            do_news,         POS_DEAD,     0, LOG_NORMAL, 1},
    {"changes",         do_changes,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"storynote",       do_story_spool,  POS_DEAD,     0, LOG_NORMAL, 1},
    {"history",         do_history_spool,POS_DEAD,     0, LOG_NORMAL, 1},
    {"penalty",         do_penalty,      POS_DEAD,    IM, LOG_NORMAL, 1},
    {"immnote",         do_imm_spool,    POS_DEAD,    IM, LOG_NORMAL, 1},
    {"catchup",         do_catchup,      POS_DEAD,     0, LOG_NORMAL, 1},
    {"unread",          do_unread,       POS_DEAD,     0, LOG_NORMAL, 1},

    /*
     * Object manipulation commands.
     */
    {"brandish",        do_brandish,    POS_RESTING,  0, LOG_NORMAL, 1},
    {"close",           do_close,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"drink",           do_drink,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"drop",            do_drop,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"eat",             do_eat,         POS_RESTING,  0, LOG_NORMAL, 1},
    {"envenom",         do_envenom,     POS_RESTING,  0, LOG_NORMAL, 1},
    {"fill",            do_fill,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"give",            do_give,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"heal",            do_heal,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"hold",            do_wear,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"list",            do_list,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"lock",            do_lock,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"open",            do_open,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"pick",            do_pick,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"pour",            do_pour,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"put",             do_put,         POS_RESTING,  0, LOG_NORMAL, 1},
    {"quaff",           do_quaff,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"recite",          do_recite,      POS_RESTING,  0, LOG_NORMAL, 1},
    {"remove",          do_remove,      POS_RESTING,  0, LOG_NORMAL, 1},
    {"sell",            do_sell,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"take",            do_get,         POS_RESTING,  0, LOG_NORMAL, 1},
    {"sacrifice",       do_sacrifice,   POS_RESTING,  0, LOG_NORMAL, 1},
    {"junk",            do_sacrifice,   POS_RESTING,  0, LOG_NORMAL, 0},
    {"value",           do_value,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"wear",            do_wear,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"zap",             do_zap,         POS_RESTING,  0, LOG_NORMAL, 1},

    /*
     * Combat commands.
     */
    {"backstab",        do_backstab,    POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"bash",            do_bash,        POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"bs",              do_backstab,    POS_FIGHTING, 0, LOG_NORMAL, 0},
    {"berserk",         do_berserk,     POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"dirt",            do_dirt,        POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"disarm",          do_disarm,      POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"flee",            do_flee,        POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"kick",            do_kick,        POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"murde",           do_murde,       POS_FIGHTING, 0, LOG_NORMAL, 0},
    {"murder",          do_murder,      POS_FIGHTING, 5, LOG_ALWAYS, 1},
    {"rescue",          do_rescue,      POS_FIGHTING, 0, LOG_NORMAL, 0},
    {"surrender",       do_surrender,   POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"trip",            do_trip,        POS_FIGHTING, 0, LOG_NORMAL, 1},

    /*
     * Mob command interpreter (placed here for faster scan...)
     */
    {"mob",             do_mob,         POS_DEAD,     0, LOG_NEVER, 0},

    /*
     * Miscellaneous commands.
     */
    {"enter",           do_enter,       POS_STANDING, 0, LOG_NORMAL, 1},
    {"follow",          do_follow,      POS_RESTING,  0, LOG_NORMAL, 1},
    {"gain",            do_gain,        POS_STANDING, 0, LOG_NORMAL, 1},
    {"go",              do_enter,       POS_STANDING, 0, LOG_NORMAL, 0},
    {"groups",          do_groups,      POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"hide",            do_hide,        POS_RESTING,  0, LOG_NORMAL, 1},
    {"qui",             do_qui,         POS_DEAD,     0, LOG_NORMAL, 0},
    {"quit",            do_quit,        POS_DEAD,     0, LOG_NORMAL, 1},
    {"recall",          do_recall,      POS_FIGHTING, 0, LOG_NORMAL, 1},
    {"save",            do_save,        POS_DEAD,     0, LOG_NORMAL, 1},
    {"sleep",           do_sleep,       POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"sneak",           do_sneak,       POS_STANDING, 0, LOG_NORMAL, 1},
    {"split",           do_split,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"steal",           do_steal,       POS_STANDING, 0, LOG_NORMAL, 1},
    {"train",           do_train,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"visible",         do_visible,     POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"wake",            do_wake,        POS_SLEEPING, 0, LOG_NORMAL, 1},
    {"where",           do_where,       POS_RESTING,  0, LOG_NORMAL, 1},
    {"reclass",         do_reclass,     POS_SLEEPING, 0, LOG_ALWAYS, 1},

    /*
     * Immortal commands.
     */
    {"advance",         do_advance,     POS_DEAD, ML, LOG_ALWAYS, 1},
    {"copyover",        do_copyover,    POS_DEAD, L2, LOG_ALWAYS, 1},
    {"dump",            do_dump,        POS_DEAD, ML, LOG_ALWAYS, 0},
    {"trust",           do_trust,       POS_DEAD, ML, LOG_ALWAYS, 1},
    {"violate",         do_violate,     POS_DEAD, ML, LOG_ALWAYS, 1},
    {"allow",           do_allow,       POS_DEAD, L2, LOG_ALWAYS, 1},
    {"ban",             do_ban,         POS_DEAD, L2, LOG_ALWAYS, 1},
    {"deny",            do_deny,        POS_DEAD, L1, LOG_ALWAYS, 1},
    {"disconnect",      do_disconnect,  POS_DEAD, L3, LOG_ALWAYS, 1},
    {"flag",            do_flag,        POS_DEAD, L4, LOG_ALWAYS, 1},
    {"freeze",          do_freeze,      POS_DEAD, L4, LOG_ALWAYS, 1},
    {"permban",         do_permban,     POS_DEAD, L1, LOG_ALWAYS, 1},
    {"protect",         do_protect,     POS_DEAD, L1, LOG_ALWAYS, 1},
    {"reboo",           do_reboo,       POS_DEAD, L1, LOG_NORMAL, 0},
    {"reboot",          do_reboot,      POS_DEAD, L1, LOG_ALWAYS, 1},
    {"set",             do_set,         POS_DEAD, L2, LOG_ALWAYS, 1},
    {"shutdow",         do_shutdow,     POS_DEAD, L1, LOG_NORMAL, 0},
    {"shutdown",        do_shutdown,    POS_DEAD, L1, LOG_ALWAYS, 1},
    {"wizlock",         do_wizlock,     POS_DEAD, L2, LOG_ALWAYS, 1},
    {"force",           do_force,       POS_DEAD, L7, LOG_ALWAYS, 1},
    {"load",            do_load,        POS_DEAD, L4, LOG_ALWAYS, 1},
    {"newlock",         do_newlock,     POS_DEAD, L4, LOG_ALWAYS, 1},
    {"nochannels",      do_nochannels,  POS_DEAD, L5, LOG_ALWAYS, 1},
    {"noemote",         do_noemote,     POS_DEAD, L5, LOG_ALWAYS, 1},
    {"noshout",         do_noshout,     POS_DEAD, L5, LOG_ALWAYS, 1},
    {"notell",          do_notell,      POS_DEAD, L5, LOG_ALWAYS, 1},
    {"pecho",           do_pecho,       POS_DEAD, L4, LOG_ALWAYS, 1},
    {"pardon",          do_pardon,      POS_DEAD, L3, LOG_ALWAYS, 1},
    {"purge",           do_purge,       POS_DEAD, L4, LOG_ALWAYS, 1},
    {"restore",         do_restore,     POS_DEAD, L4, LOG_ALWAYS, 1},
    {"sla",             do_sla,         POS_DEAD, L3, LOG_NORMAL, 0},
    {"slay",            do_slay,        POS_DEAD, L3, LOG_ALWAYS, 1},
    {"teleport",        do_transfer,    POS_DEAD, L5, LOG_ALWAYS, 1},
    {"transfer",        do_transfer,    POS_DEAD, L5, LOG_ALWAYS, 1},
    {"poofin",          do_bamfin,      POS_DEAD, L8, LOG_NORMAL, 1},
    {"poofout",         do_bamfout,     POS_DEAD, L8, LOG_NORMAL, 1},
    {"gecho",           do_echo,        POS_DEAD, L4, LOG_ALWAYS, 1},
    {"cecho",           do_cecho,       POS_DEAD, L4, LOG_ALWAYS, 1},
    {"broadcast",       do_broadcast,   POS_DEAD, L4, LOG_ALWAYS, 1},
    {"holylight",       do_holylight,   POS_DEAD, IM, LOG_NORMAL, 1},
    {"incognito",       do_incognito,   POS_DEAD, IM, LOG_NORMAL, 1},
    {"invis",           do_invis,       POS_DEAD, IM, LOG_NORMAL, 0},
    {"log",             do_log,         POS_DEAD, L3, LOG_ALWAYS, 1},
    {"logout",          do_quit,        POS_DEAD,  0, LOG_NORMAL, 1},
    {"memory",          do_memory,      POS_DEAD, IM, LOG_NORMAL, 1},
    {"mwhere",          do_mwhere,      POS_DEAD, IM, LOG_NORMAL, 1},
    {"owhere",          do_owhere,      POS_DEAD, IM, LOG_NORMAL, 1},
    {"peace",           do_peace,       POS_DEAD, L5, LOG_NORMAL, 1},
    {"echo",            do_recho,       POS_DEAD, L6, LOG_ALWAYS, 1},
    {"return",          do_return,      POS_DEAD, L6, LOG_NORMAL, 1},
    {"snoop",           do_snoop,       POS_DEAD, L5, LOG_ALWAYS, 1},
    {"stat",            do_stat,        POS_DEAD, IM, LOG_NORMAL, 1},
    {"stats",           do_stats,       POS_DEAD,  0, LOG_NORMAL, 1},
    {"string",          do_string,      POS_DEAD, L5, LOG_ALWAYS, 1},
    {"switch",          do_switch,      POS_DEAD, L6, LOG_ALWAYS, 1},
    {"wizinvis",        do_invis,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"vnum",            do_vnum,        POS_DEAD, L4, LOG_NORMAL, 1},
    {"zecho",           do_zecho,       POS_DEAD, L4, LOG_ALWAYS, 1},
    {"clone",           do_clone,       POS_DEAD, L5, LOG_ALWAYS, 1},
    {"wiznet",          do_wiznet,      POS_DEAD, IM, LOG_NORMAL, 1},
    {"imotd",           do_imotd,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"smote",           do_smote,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"prefi",           do_prefi,       POS_DEAD, IM, LOG_NORMAL, 0},
    {"prefix",          do_prefix,      POS_DEAD, IM, LOG_NORMAL, 1},
    {"mpdump",          do_mpdump,      POS_DEAD, IM, LOG_NEVER,  1},
    {"mpstat",          do_mpstat,      POS_DEAD, IM, LOG_NEVER,  1},
    {"debug",           do_debug,       POS_DEAD, ML, LOG_NEVER,  0},
    {"forcetick",       do_forcetick,   POS_DEAD, ML, LOG_ALWAYS, 1},
    {"rename",          do_rename,      POS_DEAD, L6, LOG_ALWAYS, 1},
    {"vnumgap",         do_vnumgap,     POS_DEAD, L1, LOG_NEVER,  1},
    {"exlist",          do_exlist,      POS_DEAD, IM, LOG_NEVER,  1},
    {"pathfind",        do_pathfind,    POS_DEAD, IM, LOG_ALWAYS, 1},
    {"test",            do_test,        POS_DEAD, IM, LOG_NORMAL, 1},

    /*
     * OLC
     */
    {"edit",            do_olc,         POS_DEAD, IM, LOG_NORMAL, 1},
    {"asave",           do_asave,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"alist",           do_alist,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"resets",          do_resets,      POS_DEAD, IM, LOG_NORMAL, 1},
    {"redit",           do_redit,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"medit",           do_medit,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"aedit",           do_aedit,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"oedit",           do_oedit,       POS_DEAD, IM, LOG_NORMAL, 1},
    {"mpedit",          do_mpedit,      POS_DEAD, IM, LOG_NORMAL, 1},
    {"hedit",           do_hedit,       POS_DEAD, IM, LOG_NORMAL, 1},

    /*
     * End of list.
     */
    {"", 0, POS_DEAD, 0, LOG_NORMAL, 0}
};


/*
 * The main entry point for executing commands.
 * Can be recursively called from 'at', 'order', 'force'.
 */
void interpret (CHAR_DATA * ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char logline[MAX_INPUT_LENGTH];
    int cmd;
    int trust;
    bool found;

    /*
     * Strip leading spaces.
     */
    while (isspace (*argument))
        argument++;
    if (argument[0] == '\0')
        return;

    /*
     * No hiding.
     */
    REMOVE_BIT (ch->affected_by, AFF_HIDE);

    /*
     * Implement freeze command.
     */
    if (!IS_NPC (ch) && IS_SET (ch->act, PLR_FREEZE))
    {
        send_to_char ("You're totally frozen!\n\r", ch);
        return;
    }

    /*
     * Grab the command word.
     * Special parsing so ' can be a command,
     * also no spaces needed after punctuation.
     */
    strcpy (logline, argument);
    if (!isalpha (argument[0]) && !isdigit (argument[0]))
    {
        command[0] = argument[0];
        command[1] = '\0';
        argument++;
        while (isspace (*argument))
            argument++;
    }
    else
    {
        argument = one_argument (argument, command);
    }

    /*
     * Look for command in command table.
     */
    found = FALSE;
    trust = get_trust (ch);
    for (cmd = 0; cmd_table[cmd].name[0] != '\0'; cmd++)
    {
        if (command[0] == cmd_table[cmd].name[0]
            && !str_prefix (command, cmd_table[cmd].name)
            && cmd_table[cmd].level <= trust)
        {
            found = TRUE;
            break;
        }
    }

    /*
     * Log and snoop.
     */
    smash_dollar(logline);

    if (cmd_table[cmd].log == LOG_NEVER)
        strcpy (logline, "");

    /* Replaced original block of code with fix from Edwin
     * to prevent crashes due to dollar signs in logstrings.
     * I threw in the above call to smash_dollar() just for
     * the sake of overkill :) JR -- 10/15/00
     */
    if ( ( !IS_NPC(ch) && IS_SET(ch->act, PLR_LOG) )
        ||   fLogAll
        ||   cmd_table[cmd].log == LOG_ALWAYS )
    {
        char    s[2*MAX_INPUT_LENGTH],*ps;
        int     i;

        ps=s;
        sprintf( log_buf, "Log %s: %s", ch->name, logline );
        /* Make sure that was is displayed is what is typed */
        for (i=0;log_buf[i];i++)
        {
            *ps++=log_buf[i];
            if (log_buf[i]=='$')
                *ps++='$';
            if (log_buf[i]=='{')
                *ps++='{';
        }
        *ps=0;
        wiznet(s,ch,NULL,WIZ_SECURE,0,get_trust(ch));
        log_string( log_buf );
    }

    if (ch->desc != NULL && ch->desc->snoop_by != NULL)
    {
        write_to_buffer (ch->desc->snoop_by, "% ", 2);
        write_to_buffer (ch->desc->snoop_by, logline, 0);
        write_to_buffer (ch->desc->snoop_by, "\n\r", 2);
    }

    if (!found)
    {
        /*
         * Look for command in socials table.
         */
        if (!check_social (ch, command, argument))
            send_to_char ("Huh?\n\r", ch);
        return;
    }

    /*
     * Character not in position for command?
     */
    if (ch->position < cmd_table[cmd].position)
    {
        switch (ch->position)
        {
            case POS_DEAD:
                send_to_char ("Lie still; you are DEAD.\n\r", ch);
                break;

            case POS_MORTAL:
            case POS_INCAP:
                send_to_char ("You are hurt far too bad for that.\n\r", ch);
                break;

            case POS_STUNNED:
                send_to_char ("You are too stunned to do that.\n\r", ch);
                break;

            case POS_SLEEPING:
                send_to_char ("In your dreams, or what?\n\r", ch);
                break;

            case POS_RESTING:
                send_to_char ("Nah... You feel too relaxed...\n\r", ch);
                break;

            case POS_SITTING:
                send_to_char ("Better stand up first.\n\r", ch);
                break;

            case POS_FIGHTING:
                send_to_char ("No way!  You are still fighting!\n\r", ch);
                break;

        }
        return;
    }

    /*
     * Dispatch the command.
     */
    (*cmd_table[cmd].do_fun) (ch, argument);

    tail_chain ();
    return;
}

/* function to keep argument safe in all commands -- no static strings */
void do_function (CHAR_DATA * ch, DO_FUN * do_fun, char *argument)
{
    char *command_string;

    /* copy the string */
    command_string = str_dup (argument);

    /* dispatch the command */
    (*do_fun) (ch, command_string);

    /* free the string */
    free_string (command_string);
}

bool check_social (CHAR_DATA * ch, char *command, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int cmd;
    bool found;

    found = FALSE;
    for (cmd = 0; social_table[cmd].name[0] != '\0'; cmd++)
    {
        if (command[0] == social_table[cmd].name[0]
            && !str_prefix (command, social_table[cmd].name))
        {
            found = TRUE;
            break;
        }
    }

    if (!found)
        return FALSE;

    if (!IS_NPC (ch) && IS_SET (ch->comm, COMM_NOEMOTE))
    {
        send_to_char ("You are anti-social!\n\r", ch);
        return TRUE;
    }

    switch (ch->position)
    {
        case POS_DEAD:
            send_to_char ("Lie still; you are DEAD.\n\r", ch);
            return TRUE;

        case POS_INCAP:
        case POS_MORTAL:
            send_to_char ("You are hurt far too bad for that.\n\r", ch);
            return TRUE;

        case POS_STUNNED:
            send_to_char ("You are too stunned to do that.\n\r", ch);
            return TRUE;

        case POS_SLEEPING:
            /*
             * I just know this is the path to a 12" 'if' statement.  :(
             * But two players asked for it already!  -- Furey
             */
            if (!str_cmp (social_table[cmd].name, "snore"))
                break;
            send_to_char ("In your dreams, or what?\n\r", ch);
            return TRUE;

    }

    one_argument (argument, arg);
    victim = NULL;
    if (arg[0] == '\0')
    {
        act (social_table[cmd].others_no_arg, ch, NULL, victim, TO_ROOM);
        act (social_table[cmd].char_no_arg, ch, NULL, victim, TO_CHAR);
    }
    else if ((victim = get_char_room (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
    }
    else if (victim == ch)
    {
        act (social_table[cmd].others_auto, ch, NULL, victim, TO_ROOM);
        act (social_table[cmd].char_auto, ch, NULL, victim, TO_CHAR);
    }
    else
    {
        act (social_table[cmd].others_found, ch, NULL, victim, TO_NOTVICT);
        act (social_table[cmd].char_found, ch, NULL, victim, TO_CHAR);
        act (social_table[cmd].vict_found, ch, NULL, victim, TO_VICT);

        if (!IS_NPC (ch) && IS_NPC (victim)
            && !IS_AFFECTED (victim, AFF_CHARM)
            && IS_AWAKE (victim) && victim->desc == NULL)
        {
            switch (number_bits (4))
            {
                case 0:

                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                    act (social_table[cmd].others_found,
                         victim, NULL, ch, TO_NOTVICT);
                    act (social_table[cmd].char_found, victim, NULL, ch,
                         TO_CHAR);
                    act (social_table[cmd].vict_found, victim, NULL, ch,
                         TO_VICT);
                    break;

                case 9:
                case 10:
                case 11:
                case 12:
                    act ("$n slaps $N.", victim, NULL, ch, TO_NOTVICT);
                    act ("You slap $N.", victim, NULL, ch, TO_CHAR);
                    act ("$n slaps you.", victim, NULL, ch, TO_VICT);
                    break;
            }
        }
    }

    return TRUE;
}



/*
 * Return true if an argument is completely numeric.
 */
bool is_number (char *arg)
{

    if (*arg == '\0')
        return FALSE;

    if (*arg == '+' || *arg == '-')
        arg++;

    for (; *arg != '\0'; arg++)
    {
        if (!isdigit (*arg))
            return FALSE;
    }

    return TRUE;
}



/*
 * Given a string like 14.foo, return 14 and 'foo'
 */
int number_argument (char *argument, char *arg)
{
    char *pdot;
    int number;

    for (pdot = argument; *pdot != '\0'; pdot++)
    {
        if (*pdot == '.')
        {
            *pdot = '\0';
            number = atoi (argument);
            *pdot = '.';
            strcpy (arg, pdot + 1);
            return number;
        }
    }

    strcpy (arg, argument);
    return 1;
}

/*
 * Given a string like 14*foo, return 14 and 'foo'
 */
int mult_argument (char *argument, char *arg)
{
    char *pdot;
    int number;

    for (pdot = argument; *pdot != '\0'; pdot++)
    {
        if (*pdot == '*')
        {
            *pdot = '\0';
            number = atoi (argument);
            *pdot = '*';
            strcpy (arg, pdot + 1);
            return number;
        }
    }

    strcpy (arg, argument);
    return 1;
}

/*
 * Pick off one argument from a string and return the rest.
 * Understands quotes.
 */
char *one_argument (char *argument, char *arg_first)
{
    char cEnd;

    while (isspace (*argument))
        argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"')
        cEnd = *argument++;

    while (*argument != '\0')
    {
        if (*argument == cEnd)
        {
            argument++;
            break;
        }
        *arg_first = LOWER (*argument);
        arg_first++;
        argument++;
    }
    *arg_first = '\0';

    while (isspace (*argument))
        argument++;

    return argument;
}

/*
 * Contributed by Alander.
 */
void do_commands (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int cmd;
    int col;

    col = 0;
    for (cmd = 0; cmd_table[cmd].name[0] != '\0'; cmd++)
    {
        if (cmd_table[cmd].level <= LEVEL_HERO
            && cmd_table[cmd].level <= get_trust (ch) && cmd_table[cmd].show)
        {
            sprintf (buf, "%-12s", cmd_table[cmd].name);
            send_to_char (buf, ch);
            if (++col % 6 == 0)
                send_to_char ("\n\r", ch);
        }
    }

    if (col % 6 != 0)
        send_to_char ("\n\r", ch);
    return;
}

/*
 * Shows all immortal commands available
 */
void do_wizhelp (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int cmd;
    int col;

    col = 0;
    for (cmd = 0; cmd_table[cmd].name[0] != '\0'; cmd++)
    {
        if (cmd_table[cmd].level > LEVEL_HERO
            && cmd_table[cmd].level <= get_trust (ch) && cmd_table[cmd].show)
        {
            sprintf (buf, "%-12s", cmd_table[cmd].name);
            send_to_char (buf, ch);
            if (++col % 6 == 0)
                send_to_char ("\n\r", ch);
        }
    }

    if (col % 6 != 0)
        send_to_char ("\n\r", ch);
    return;
}

/*
 * Begin Alias Functions and Methods
 */

/*
 * Performs the aliasing and other fun stuff.
 */
void substitute_alias (DESCRIPTOR_DATA * d, char *argument)
{
    CHAR_DATA *ch;
    char buf[MAX_STRING_LENGTH], prefix[MAX_INPUT_LENGTH],
        name[MAX_INPUT_LENGTH];
    char *point;
    int alias;

    ch = d->original ? d->original : d->character;

    /* check for prefix */
    if (ch->prefix[0] != '\0' && str_prefix ("prefix", argument))
    {
        if (strlen (ch->prefix) + strlen (argument) > MAX_INPUT_LENGTH - 2)
            send_to_char ("Line to long, prefix not processed.\r\n", ch);
        else
        {
            sprintf (prefix, "%s %s", ch->prefix, argument);
            argument = prefix;
        }
    }

    if (IS_NPC (ch) || ch->pcdata->alias[0] == NULL
        || !str_prefix ("alias", argument) || !str_prefix ("una", argument)
        || !str_prefix ("prefix", argument))
    {
        interpret (d->character, argument);
        return;
    }

    strcpy (buf, argument);

    for (alias = 0; alias < MAX_ALIAS; alias++)
    {                            /* go through the aliases */
        if (ch->pcdata->alias[alias] == NULL)
            break;

        if (!str_prefix (ch->pcdata->alias[alias], argument))
        {
            point = one_argument (argument, name);
            if (!strcmp (ch->pcdata->alias[alias], name))
            {
                /* More Edwin inspired fixes. JR -- 10/15/00 */
                buf[0] = '\0';
                strcat(buf,ch->pcdata->alias_sub[alias]);
                if (point[0])
                {
                    strcat(buf," ");
                    strcat(buf,point);
                }

                if (strlen (buf) > MAX_INPUT_LENGTH - 1)
                {
                    send_to_char
                        ("Alias substitution too long. Truncated.\r\n", ch);
                    buf[MAX_INPUT_LENGTH - 1] = '\0';
                }
                break;
            }
        }
    }
    interpret (d->character, buf);
} // end void substitute_alias

/*
 * Function to force the player to fully type out alias.
 */
void do_alia (CHAR_DATA * ch, char *argument)
{
    send_to_char ("I'm sorry, alias must be entered in full.\n\r", ch);
    return;
} // end do_alia

/*
 * Command to allow a player to set one of their aliases.
 */
void do_alias (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *rch;
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    int pos;

    smash_tilde (argument);

    if (ch->desc == NULL)
        rch = ch;
    else
        rch = ch->desc->original ? ch->desc->original : ch;

    if (IS_NPC (rch))
        return;

    argument = one_argument (argument, arg);


    if (arg[0] == '\0')
    {

        if (rch->pcdata->alias[0] == NULL)
        {
            send_to_char ("You have no aliases defined.\n\r", ch);
            return;
        }
        send_to_char ("Your current aliases are:\n\r", ch);

        for (pos = 0; pos < MAX_ALIAS; pos++)
        {
            if (rch->pcdata->alias[pos] == NULL
                || rch->pcdata->alias_sub[pos] == NULL)
                break;

            sprintf (buf, "    %s:  %s\n\r", rch->pcdata->alias[pos],
                     rch->pcdata->alias_sub[pos]);
            send_to_char (buf, ch);
        }
        return;
    }

    if (!str_prefix ("una", arg) || !str_cmp ("alias", arg))
    {
        send_to_char ("Sorry, that word is reserved.\n\r", ch);
        return;
    }

    /* More Edwin-inspired fixes. JR -- 10/15/00 */
    if (strchr(arg,' ')||strchr(arg,'"')||strchr(arg,'\''))
    {
        send_to_char("The word to be aliased should not contain a space, "
            "a tick or a double-quote.\n\r",ch);
        return;
    }

    if (argument[0] == '\0')
    {
        for (pos = 0; pos < MAX_ALIAS; pos++)
        {
            if (rch->pcdata->alias[pos] == NULL
                || rch->pcdata->alias_sub[pos] == NULL)
                break;

            if (!str_cmp (arg, rch->pcdata->alias[pos]))
            {
                sprintf (buf, "%s aliases to '%s'.\n\r",
                         rch->pcdata->alias[pos],
                         rch->pcdata->alias_sub[pos]);
                send_to_char (buf, ch);
                return;
            }
        }

        send_to_char ("That alias is not defined.\n\r", ch);
        return;
    }

    if (!str_prefix (argument, "delete") || !str_prefix (argument, "prefix"))
    {
        send_to_char ("That shall not be done!\n\r", ch);
        return;
    }

    for (pos = 0; pos < MAX_ALIAS; pos++)
    {
        if (rch->pcdata->alias[pos] == NULL)
            break;

        if (!str_cmp (arg, rch->pcdata->alias[pos]))
        {                        /* redefine an alias */
            free_string (rch->pcdata->alias_sub[pos]);
            rch->pcdata->alias_sub[pos] = str_dup (argument);
            sprintf (buf, "%s is now realiased to '%s'.\n\r", arg, argument);
            send_to_char (buf, ch);
            return;
        }
    }

    if (pos >= MAX_ALIAS)
    {
        send_to_char ("Sorry, you have reached the alias limit.\n\r", ch);
        return;
    }

    /* make a new alias */
    rch->pcdata->alias[pos] = str_dup (arg);
    rch->pcdata->alias_sub[pos] = str_dup (argument);
    sprintf (buf, "%s is now aliased to '%s'.\n\r", arg, argument);
    send_to_char (buf, ch);
} // end do_alias

/*
 * Command to allow a player to remove one of their aliases
 */
void do_unalias (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *rch;
    char arg[MAX_INPUT_LENGTH];
    int pos;
    bool found = FALSE;

    if (ch->desc == NULL)
        rch = ch;
    else
        rch = ch->desc->original ? ch->desc->original : ch;

    if (IS_NPC (rch))
        return;

    argument = one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Unalias what?\n\r", ch);
        return;
    }

    for (pos = 0; pos < MAX_ALIAS; pos++)
    {
        if (rch->pcdata->alias[pos] == NULL)
            break;

        if (found)
        {
            rch->pcdata->alias[pos - 1] = rch->pcdata->alias[pos];
            rch->pcdata->alias_sub[pos - 1] = rch->pcdata->alias_sub[pos];
            rch->pcdata->alias[pos] = NULL;
            rch->pcdata->alias_sub[pos] = NULL;
            continue;
        }

        if (!strcmp (arg, rch->pcdata->alias[pos]))
        {
            send_to_char ("Alias removed.\n\r", ch);
            free_string (rch->pcdata->alias[pos]);
            free_string (rch->pcdata->alias_sub[pos]);
            rch->pcdata->alias[pos] = NULL;
            rch->pcdata->alias_sub[pos] = NULL;
            found = TRUE;
        }
    }

    if (!found)
        send_to_char ("No alias of that name to remove.\n\r", ch);
} // end do_unalias
