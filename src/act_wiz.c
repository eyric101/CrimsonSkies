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
    #include <unistd.h>                /* For execl in copyover() */
#elif defined(_WIN32)
    #include <sys/types.h>
    #include <time.h>
    #include <io.h>
#else
    #include <sys/types.h>
    #include <sys/time.h>
    #include <unistd.h>                /* For execl in copyover() */
    #include <time.h>
#endif

// General Includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "lookup.h"
#include <assert.h>  // For assert in area_name

/*
 * Local functions.
 */
ROOM_INDEX_DATA *find_location args ((CHAR_DATA * ch, char *arg));
CHAR_DATA       *copyover_ch;

void raw_kill          args((CHAR_DATA * victim)); // for do_slay
void save_game_objects args((void)); // for do_copyover
void wizbless          args((CHAR_DATA * victim)); // for do_wizbless
void do_mload          args((CHAR_DATA *ch, char *argument, int number)); // for do_load
void do_oload          args((CHAR_DATA *ch, char *argument, int number)); // for do_load

void do_wiznet (CHAR_DATA * ch, char *argument)
{
    int flag;
    char buf[MAX_STRING_LENGTH];

    if (argument[0] == '\0')
    {
        if (IS_SET (ch->wiznet, WIZ_ON))
        {
            send_to_char ("Signing off of Wiznet.\n\r", ch);
            REMOVE_BIT (ch->wiznet, WIZ_ON);
        }
        else
        {
            send_to_char ("Welcome to Wiznet!\n\r", ch);
            SET_BIT (ch->wiznet, WIZ_ON);
        }
        return;
    }

    if (!str_prefix (argument, "on"))
    {
        send_to_char ("Welcome to Wiznet!\n\r", ch);
        SET_BIT (ch->wiznet, WIZ_ON);
        return;
    }

    if (!str_prefix (argument, "off"))
    {
        send_to_char ("Signing off of Wiznet.\n\r", ch);
        REMOVE_BIT (ch->wiznet, WIZ_ON);
        return;
    }

    // Wiznet "status" and "show" combined together.
    /* show wiznet status */
    if (!str_prefix(argument,"status") || !str_prefix(argument,"show"))
    {
        bool lf = FALSE;
        buf[0] = '\0';

        sprintf(buf, "{WWiznet{x is toggled %s{x.\n\r\n\r", IS_SET(ch->wiznet,WIZ_ON) ? "{GON" : "{ROFF");
        send_to_char( buf, ch);

        for (flag = 1; wiznet_table[flag].name != NULL; flag++)
        {
            if (ch->level < wiznet_table[flag].level)
                continue;

            sprintf(buf,"{W%-10s    %6s   ",
            capitalize(wiznet_table[flag].name),
            IS_SET(ch->wiznet,wiznet_table[flag].flag) ? "{GON" : "{ROFF");
            send_to_char(buf,ch);

            if (lf)
            {
                lf= FALSE;
                send_to_char("\n\r{x", ch);
            }
            else
            {
                lf = TRUE;
            }
        }

        send_to_char("\n\r{x",ch);
        return;
    }

    flag = wiznet_lookup(argument);

    if (flag == -1 || get_trust(ch) < wiznet_table[flag].level)
    {
    send_to_char("No such option.\n\r",ch);
    return;
    }


    flag = wiznet_lookup (argument);

    if (flag == -1 || get_trust (ch) < wiznet_table[flag].level)
    {
        send_to_char ("No such option.\n\r", ch);
        return;
    }

    if (IS_SET (ch->wiznet, wiznet_table[flag].flag))
    {
        sprintf (buf, "You will no longer see %s on wiznet.\n\r",
                 wiznet_table[flag].name);
        send_to_char (buf, ch);
        REMOVE_BIT (ch->wiznet, wiznet_table[flag].flag);
        return;
    }
    else
    {
        sprintf (buf, "You will now see %s on wiznet.\n\r",
                 wiznet_table[flag].name);
        send_to_char (buf, ch);
        SET_BIT (ch->wiznet, wiznet_table[flag].flag);
        return;
    }

}

void wiznet (char *string, CHAR_DATA * ch, OBJ_DATA * obj,
             long flag, long flag_skip, int min_level)
{
    DESCRIPTOR_DATA *d;

    for (d = descriptor_list; d != NULL; d = d->next)
    {
        if (d->connected == CON_PLAYING && IS_IMMORTAL (d->character)
            && IS_SET (d->character->wiznet, WIZ_ON)
            && (!flag || IS_SET (d->character->wiznet, flag))
            && (!flag_skip || !IS_SET (d->character->wiznet, flag_skip))
            && get_trust (d->character) >= min_level && d->character != ch)
        {
            if (IS_SET (d->character->wiznet, WIZ_PREFIX))
            {
                send_to_char ("--> ", d->character);
            }

            act_new (string, d->character, obj, ch, TO_CHAR, POS_DEAD);
        }
    }

    return;
}

/*
 * Adds a character to a clan.
 */
void do_guild (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    int clan;

    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char ("Syntax: guild <character> <clan name>\n\r", ch);
        return;
    }
    if ((victim = get_char_world (ch, arg1)) == NULL)
    {
        send_to_char ("They aren't playing.\n\r", ch);
        return;
    }

    if (!str_prefix (arg2, "none"))
    {
        send_to_char ("They are now clanless.\n\r", ch);
        send_to_char ("You are now a member of no clan!\n\r", victim);
        victim->clan = 0;
        return;
    }

    if ((clan = clan_lookup (arg2)) == 0)
    {
        send_to_char ("No such clan exists.\n\r", ch);
        return;
    }

    if (clan_table[clan].independent)
    {
        sprintf (buf, "They are now a %s.\n\r", clan_table[clan].name);
        send_to_char (buf, ch);
        sprintf (buf, "You are now a %s.\n\r", clan_table[clan].name);
        send_to_char (buf, victim);
    }
    else
    {
        sprintf (buf, "They are now a member of clan %s.\n\r",
                 capitalize (clan_table[clan].name));
        send_to_char (buf, ch);
        sprintf (buf, "You are now a member of clan %s.\n\r",
                 capitalize (clan_table[clan].name));
    }

    victim->clan = clan;
} // end do_guild

/* RT nochannels command, for those spammers */
void do_nochannels (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Nochannel whom?", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (get_trust (victim) >= get_trust (ch))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    if (IS_SET (victim->comm, COMM_NOCHANNELS))
    {
        REMOVE_BIT (victim->comm, COMM_NOCHANNELS);
        send_to_char ("The gods have restored your channel priviliges.\n\r",
                      victim);
        send_to_char ("NOCHANNELS removed.\n\r", ch);
        sprintf (buf, "$N restores channels to %s", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }
    else
    {
        SET_BIT (victim->comm, COMM_NOCHANNELS);
        send_to_char ("The gods have revoked your channel priviliges.\n\r",
                      victim);
        send_to_char ("NOCHANNELS set.\n\r", ch);
        sprintf (buf, "$N revokes %s's channels.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }

    return;
}


void do_smote (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *vch;
    char *letter, *name;
    char last[MAX_INPUT_LENGTH], temp[MAX_STRING_LENGTH];
    int matches = 0;

    if (!IS_NPC (ch) && IS_SET (ch->comm, COMM_NOEMOTE))
    {
        send_to_char ("You can't show your emotions.\n\r", ch);
        return;
    }

    if (argument[0] == '\0')
    {
        send_to_char ("Emote what?\n\r", ch);
        return;
    }

    if (strstr (argument, ch->name) == NULL)
    {
        send_to_char ("You must include your name in an smote.\n\r", ch);
        return;
    }

    send_to_char (argument, ch);
    send_to_char ("\n\r", ch);

    for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
    {
        if (vch->desc == NULL || vch == ch)
            continue;

        if ((letter = strstr (argument, vch->name)) == NULL)
        {
            send_to_char (argument, vch);
            send_to_char ("\n\r", vch);
            continue;
        }

        strcpy (temp, argument);
        temp[strlen (argument) - strlen (letter)] = '\0';
        last[0] = '\0';
        name = vch->name;

        for (; *letter != '\0'; letter++)
        {
            if (*letter == '\'' && matches == strlen (vch->name))
            {
                strcat (temp, "r");
                continue;
            }

            if (*letter == 's' && matches == strlen (vch->name))
            {
                matches = 0;
                continue;
            }

            if (matches == strlen (vch->name))
            {
                matches = 0;
            }

            if (*letter == *name)
            {
                matches++;
                name++;
                if (matches == strlen (vch->name))
                {
                    strcat (temp, "you");
                    last[0] = '\0';
                    name = vch->name;
                    continue;
                }
                strncat (last, letter, 1);
                continue;
            }

            matches = 0;
            strcat (temp, last);
            strncat (temp, letter, 1);
            last[0] = '\0';
            name = vch->name;
        }

        send_to_char (temp, vch);
        send_to_char ("\n\r", vch);
    }

    return;
}

void do_bamfin (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (!IS_NPC (ch))
    {
        smash_tilde (argument);

        if (argument[0] == '\0')
        {
            sprintf (buf, "Your poofin is %s\n\r", ch->pcdata->bamfin);
            send_to_char (buf, ch);
            return;
        }

        if (strstr (argument, ch->name) == NULL)
        {
            send_to_char ("You must include your name.\n\r", ch);
            return;
        }

        free_string (ch->pcdata->bamfin);
        ch->pcdata->bamfin = str_dup (argument);

        sprintf (buf, "Your poofin is now %s\n\r", ch->pcdata->bamfin);
        send_to_char (buf, ch);
    }
    return;
}

void do_bamfout (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (!IS_NPC (ch))
    {
        smash_tilde (argument);

        if (argument[0] == '\0')
        {
            sprintf (buf, "Your poofout is %s\n\r", ch->pcdata->bamfout);
            send_to_char (buf, ch);
            return;
        }

        if (strstr (argument, ch->name) == NULL)
        {
            send_to_char ("You must include your name.\n\r", ch);
            return;
        }

        free_string (ch->pcdata->bamfout);
        ch->pcdata->bamfout = str_dup (argument);

        sprintf (buf, "Your poofout is now %s\n\r", ch->pcdata->bamfout);
        send_to_char (buf, ch);
    }
    return;
}



void do_deny (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char ("Deny whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (IS_NPC (victim))
    {
        send_to_char ("Not on NPC's.\n\r", ch);
        return;
    }

    if (get_trust (victim) >= get_trust (ch))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    SET_BIT (victim->act, PLR_DENY);
    send_to_char ("You are denied access!\n\r", victim);
    sprintf (buf, "$N denies access to %s", victim->name);
    wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    send_to_char ("OK.\n\r", ch);
    save_char_obj (victim);
    stop_fighting (victim, TRUE);
    do_function (victim, &do_quit, "");

    return;
}



void do_disconnect (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;

    one_argument (argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char ("Disconnect whom?\n\r", ch);
        return;
    }

    if (is_number (arg))
    {
        int desc;

        desc = atoi (arg);
        for (d = descriptor_list; d != NULL; d = d->next)
        {
            if (d->descriptor == desc)
            {
                close_socket (d);
                send_to_char ("Ok.\n\r", ch);
                return;
            }
        }
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (victim->desc == NULL)
    {
        act ("$N doesn't have a descriptor.", ch, NULL, victim, TO_CHAR);
        return;
    }

    for (d = descriptor_list; d != NULL; d = d->next)
    {
        if (d == victim->desc)
        {
            close_socket (d);
            send_to_char ("Ok.\n\r", ch);
            return;
        }
    }

    bug ("Do_disconnect: desc not found.", 0);
    send_to_char ("Descriptor not found!\n\r", ch);
    return;
}



void do_pardon (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument (argument, arg1);

    if (arg1[0] == '\0')
    {
        send_to_char ("Syntax: pardon <character>\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg1)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (IS_NPC (victim))
    {
        send_to_char ("Not on NPC's.\n\r", ch);
        return;
    }

    REMOVE_BIT (victim->act, PLR_WANTED);
    send_to_char ("Wanted flag removed.\n\r", ch);
    send_to_char ("You are no longer ({RWANTED{x).\n\r", victim);

    return;
} // end do_pardon

/*
 * gecho command to send a global echo to all connected players.
 */
void do_echo (CHAR_DATA * ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if (argument[0] == '\0')
    {
        send_to_char ("Global echo what?\n\r", ch);
        return;
    }

    for (d = descriptor_list; d; d = d->next)
    {
        if (d->connected == CON_PLAYING)
        {
            if (get_trust (d->character) >= get_trust (ch))
                send_to_char ("global> ", d->character);
            send_to_char (argument, d->character);
            send_to_char ("\n\r", d->character);
        }
    }

    return;
} // end do_echo

/*
 * Continent echo - sends an echo to all players on the current continent.
 */
void do_cecho (CHAR_DATA * ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if (argument[0] == '\0')
    {
        send_to_char ("Continent echo what?\n\r", ch);
        return;
    }

    for (d = descriptor_list; d; d = d->next)
    {
        if (d == NULL)
            continue;

        // Send to just playing characters on the same continent.
        if (d->connected == CON_PLAYING && d->character->in_room->area->continent == ch->in_room->area->continent)
        {
            if (get_trust (d->character) >= get_trust (ch))
                send_to_char ("continent> ", d->character);
            send_to_char (argument, d->character);
            send_to_char ("\n\r", d->character);
        }
    }

    return;
} // end do_cecho

void do_recho (CHAR_DATA * ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if (argument[0] == '\0')
    {
        send_to_char ("Local echo what?\n\r", ch);

        return;
    }

    for (d = descriptor_list; d; d = d->next)
    {
        if (d->connected == CON_PLAYING
            && d->character->in_room == ch->in_room)
        {
            if (get_trust (d->character) >= get_trust (ch))
                send_to_char ("local> ", d->character);
            send_to_char (argument, d->character);
            send_to_char ("\n\r", d->character);
        }
    }

    return;
}

void do_zecho (CHAR_DATA * ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if (argument[0] == '\0')
    {
        send_to_char ("Zone echo what?\n\r", ch);
        return;
    }

    for (d = descriptor_list; d; d = d->next)
    {
        if (d->connected == CON_PLAYING
            && d->character->in_room != NULL && ch->in_room != NULL
            && d->character->in_room->area == ch->in_room->area)
        {
            if (get_trust (d->character) >= get_trust (ch))
                send_to_char ("zone> ", d->character);
            send_to_char (argument, d->character);
            send_to_char ("\n\r", d->character);
        }
    }
}

void do_pecho (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument (argument, arg);

    if (argument[0] == '\0' || arg[0] == '\0')
    {
        send_to_char ("Personal echo what?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("Target not found.\n\r", ch);
        return;
    }

    if (get_trust (victim) >= get_trust (ch) && get_trust (ch) != MAX_LEVEL)
        send_to_char ("personal> ", victim);

    send_to_char (argument, victim);
    send_to_char ("\n\r", victim);
    send_to_char ("personal> ", ch);
    send_to_char (argument, ch);
    send_to_char ("\n\r", ch);
}


ROOM_INDEX_DATA *find_location (CHAR_DATA * ch, char *arg)
{
    CHAR_DATA *victim;
    OBJ_DATA *obj;

    if (is_number (arg))
        return get_room_index (atoi (arg));

    if ((victim = get_char_world (ch, arg)) != NULL)
        return victim->in_room;

    if ((obj = get_obj_world (ch, arg)) != NULL)
        return obj->in_room;

    return NULL;
}



void do_transfer (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    ROOM_INDEX_DATA *location;
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;

    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);

    if (arg1[0] == '\0')
    {
        send_to_char ("Transfer whom (and where)?\n\r", ch);
        return;
    }

    if (!str_cmp (arg1, "all"))
    {
        for (d = descriptor_list; d != NULL; d = d->next)
        {
            if (d->connected == CON_PLAYING
                && d->character != ch
                && d->character->in_room != NULL
                && can_see (ch, d->character))
            {
                char buf[MAX_STRING_LENGTH];
                sprintf (buf, "%s %s", d->character->name, arg2);
                do_function (ch, &do_transfer, buf);
            }
        }
        return;
    }

    /*
     * Thanks to Grodyn for the optional location parameter.
     */
    if (arg2[0] == '\0')
    {
        location = ch->in_room;
    }
    else
    {
        if ((location = find_location (ch, arg2)) == NULL)
        {
            send_to_char ("No such location.\n\r", ch);
            return;
        }

        if (!is_room_owner (ch, location) && room_is_private (location)
            && get_trust (ch) < MAX_LEVEL)
        {
            send_to_char ("That room is private right now.\n\r", ch);
            return;
        }
    }

    if ((victim = get_char_world (ch, arg1)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (victim->in_room == NULL)
    {
        send_to_char ("They are in limbo.\n\r", ch);
        return;
    }

    if (victim->fighting != NULL)
        stop_fighting (victim, TRUE);
    act ("$n disappears in a mushroom cloud.", victim, NULL, NULL, TO_ROOM);
    char_from_room (victim);
    char_to_room (victim, location);
    act ("$n arrives from a puff of smoke.", victim, NULL, NULL, TO_ROOM);
    if (ch != victim)
        act ("$n has transferred you.", ch, NULL, victim, TO_VICT);
    do_function (victim, &do_look, "auto");
    send_to_char ("Ok.\n\r", ch);
}



void do_at (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    ROOM_INDEX_DATA *location;
    ROOM_INDEX_DATA *original;
    OBJ_DATA *on;
    CHAR_DATA *wch;

    argument = one_argument (argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
        send_to_char ("At where what?\n\r", ch);
        return;
    }

    if ((location = find_location (ch, arg)) == NULL)
    {
        send_to_char ("No such location.\n\r", ch);
        return;
    }

    if (!is_room_owner (ch, location) && room_is_private (location)
        && get_trust (ch) < MAX_LEVEL)
    {
        send_to_char ("That room is private right now.\n\r", ch);
        return;
    }

    original = ch->in_room;
    on = ch->on;
    char_from_room (ch);
    char_to_room (ch, location);
    interpret (ch, argument);

    /*
     * See if 'ch' still exists before continuing!
     * Handles 'at XXXX quit' case.
     */
    for (wch = char_list; wch != NULL; wch = wch->next)
    {
        if (wch == ch)
        {
            char_from_room (ch);
            char_to_room (ch, original);
            ch->on = on;
            break;
        }
    }

    return;
}



void do_goto (CHAR_DATA * ch, char *argument)
{
    ROOM_INDEX_DATA *location;
    CHAR_DATA *rch;
    int count = 0;

    if (argument[0] == '\0')
    {
        send_to_char ("Goto where?\n\r", ch);
        return;
    }

    if ((location = find_location (ch, argument)) == NULL)
    {
        send_to_char ("No such location.\n\r", ch);
        return;
    }

    count = 0;
    for (rch = location->people; rch != NULL; rch = rch->next_in_room)
        count++;

    if (!is_room_owner (ch, location) && room_is_private (location)
        && (count > 1 || get_trust (ch) < MAX_LEVEL))
    {
        send_to_char ("That room is private right now.\n\r", ch);
        return;
    }

    if (ch->fighting != NULL)
        stop_fighting (ch, TRUE);

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (get_trust (rch) >= ch->invis_level)
        {
            if (ch->pcdata != NULL && ch->pcdata->bamfout[0] != '\0')
                act ("$t", ch, ch->pcdata->bamfout, rch, TO_VICT);
            else
                act ("$n leaves in a swirling mist.", ch, NULL, rch, TO_VICT);
        }
    }

    char_from_room (ch);
    char_to_room (ch, location);


    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (get_trust (rch) >= ch->invis_level)
        {
            if (ch->pcdata != NULL && ch->pcdata->bamfin[0] != '\0')
                act ("$t", ch, ch->pcdata->bamfin, rch, TO_VICT);
            else
                act ("$n appears in a swirling mist.", ch, NULL, rch,
                     TO_VICT);
        }
    }

    do_function (ch, &do_look, "auto");
    return;
}

void do_violate (CHAR_DATA * ch, char *argument)
{
    ROOM_INDEX_DATA *location;
    CHAR_DATA *rch;

    if (argument[0] == '\0')
    {
        send_to_char ("Goto where?\n\r", ch);
        return;
    }

    if ((location = find_location (ch, argument)) == NULL)
    {
        send_to_char ("No such location.\n\r", ch);
        return;
    }

    if (!room_is_private (location))
    {
        send_to_char ("That room isn't private, use goto.\n\r", ch);
        return;
    }

    if (ch->fighting != NULL)
        stop_fighting (ch, TRUE);

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (get_trust (rch) >= ch->invis_level)
        {
            if (ch->pcdata != NULL && ch->pcdata->bamfout[0] != '\0')
                act ("$t", ch, ch->pcdata->bamfout, rch, TO_VICT);
            else
                act ("$n leaves in a swirling mist.", ch, NULL, rch, TO_VICT);
        }
    }

    char_from_room (ch);
    char_to_room (ch, location);


    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (get_trust (rch) >= ch->invis_level)
        {
            if (ch->pcdata != NULL && ch->pcdata->bamfin[0] != '\0')
                act ("$t", ch, ch->pcdata->bamfin, rch, TO_VICT);
            else
                act ("$n appears in a swirling mist.", ch, NULL, rch,
                     TO_VICT);
        }
    }

    do_function (ch, &do_look, "auto");
    return;
}

/* RT to replace the 3 stat commands */

void do_stat (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *string;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *location;
    CHAR_DATA *victim;

    string = one_argument (argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  stat <name>\n\r", ch);
        send_to_char ("  stat obj <name>\n\r", ch);
        send_to_char ("  stat mob <name>\n\r", ch);
        send_to_char ("  stat room <number>\n\r", ch);
        return;
    }

    if (!str_cmp (arg, "room"))
    {
        do_function (ch, &do_rstat, string);
        return;
    }

    if (!str_cmp (arg, "obj"))
    {
        do_function (ch, &do_ostat, string);
        return;
    }

    if (!str_cmp (arg, "char") || !str_cmp (arg, "mob"))
    {
        do_function (ch, &do_mstat, string);
        return;
    }

    /* do it the old way */

    obj = get_obj_world (ch, argument);
    if (obj != NULL)
    {
        do_function (ch, &do_ostat, argument);
        return;
    }

    victim = get_char_world (ch, argument);
    if (victim != NULL)
    {
        do_function (ch, &do_mstat, argument);
        return;
    }

    location = find_location (ch, argument);
    if (location != NULL)
    {
        do_function (ch, &do_rstat, argument);
        return;
    }

    send_to_char ("Nothing by that name found anywhere.\n\r", ch);
}

void do_rstat (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    ROOM_INDEX_DATA *location;
    OBJ_DATA *obj;
    CHAR_DATA *rch;
    int door;

    one_argument (argument, arg);
    location = (arg[0] == '\0') ? ch->in_room : find_location (ch, arg);
    if (location == NULL)
    {
        send_to_char ("No such location.\n\r", ch);
        return;
    }

    if (!is_room_owner (ch, location) && ch->in_room != location
        && room_is_private (location) && !IS_TRUSTED (ch, IMPLEMENTOR))
    {
        send_to_char ("That room is private right now.\n\r", ch);
        return;
    }

    sprintf (buf, "Name: '%s'\n\rArea: '%s'\n\r",
             location->name, location->area->name);
    send_to_char (buf, ch);

    sprintf (buf,
             "Vnum: %d  Sector: %d  Light: %d  Healing: %d  Mana: %d\n\r",
             location->vnum,
             location->sector_type,
             location->light, location->heal_rate, location->mana_rate);
    send_to_char (buf, ch);

    sprintf (buf,
             "Room flags: %d.\n\rDescription:\n\r%s",
             location->room_flags, location->description);
    send_to_char (buf, ch);

    if (location->extra_descr != NULL)
    {
        EXTRA_DESCR_DATA *ed;

        send_to_char ("Extra description keywords: '", ch);
        for (ed = location->extra_descr; ed; ed = ed->next)
        {
            send_to_char (ed->keyword, ch);
            if (ed->next != NULL)
                send_to_char (" ", ch);
        }
        send_to_char ("'.\n\r", ch);
    }

    send_to_char ("Characters:", ch);
    for (rch = location->people; rch; rch = rch->next_in_room)
    {
        if (can_see (ch, rch))
        {
            send_to_char (" ", ch);
            one_argument (rch->name, buf);
            send_to_char (buf, ch);
        }
    }

    send_to_char (".\n\rObjects:   ", ch);
    for (obj = location->contents; obj; obj = obj->next_content)
    {
        send_to_char (" ", ch);
        one_argument (obj->name, buf);
        send_to_char (buf, ch);
    }
    send_to_char (".\n\r", ch);

    for (door = 0; door < MAX_DIR; door++)
    {
        EXIT_DATA *pexit;

        if ((pexit = location->exit[door]) != NULL)
        {
            sprintf (buf,
                     "Door: %d.  To: %d.  Key: %d.  Exit flags: %d.\n\rKeyword: '%s'.  Description: %s",
                     door,
                     (pexit->u1.to_room ==
                      NULL ? -1 : pexit->u1.to_room->vnum), pexit->key,
                     pexit->exit_info, pexit->keyword,
                     pexit->description[0] !=
                     '\0' ? pexit->description : "(none).\n\r");
            send_to_char (buf, ch);
        }
    }

    return;
}



void do_ostat (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    AFFECT_DATA *paf;
    OBJ_DATA *obj;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Stat what?\n\r", ch);
        return;
    }

    if ((obj = get_obj_world (ch, argument)) == NULL)
    {
        send_to_char ("Nothing like that in hell, earth, or heaven.\n\r", ch);
        return;
    }

    sprintf (buf, "Name(s): %s\n\r", obj->name);
    send_to_char (buf, ch);

    sprintf (buf, "Vnum: %d  Type: %s  Resets: %d\n\r",
             obj->pIndexData->vnum,
             item_name (obj->item_type), obj->pIndexData->reset_num);
    send_to_char (buf, ch);

    sprintf (buf, "Short description: %s\n\rLong description: %s\n\r",
             obj->short_descr, obj->description);
    send_to_char (buf, ch);

    sprintf (buf, "Wear bits: %s\n\rExtra bits: %s\n\r",
             wear_bit_name (obj->wear_flags),
             extra_bit_name (obj->extra_flags));
    send_to_char (buf, ch);

    if (obj->enchanted_by != NULL)
    {
        sprintf(buf, "Enchanted by: %s\n\r", obj->enchanted_by);
        send_to_char(buf, ch);
    }

    if (obj->wizard_mark != NULL)
    {
        sprintf(buf, "Wizard mark: %s\n\r", obj->wizard_mark);
        send_to_char(buf, ch);
    }

    sprintf (buf, "Number: %d/%d  Weight: %d/%d/%d (10th pounds)\n\r",
             1, get_obj_number (obj),
             obj->weight, get_obj_weight (obj), get_true_weight (obj));
    send_to_char (buf, ch);

    sprintf (buf, "Level: %d  Cost: %d  Condition: %d  Timer: %d\n\r",
             obj->level, obj->cost, obj->condition, obj->timer);
    send_to_char (buf, ch);

    sprintf (buf,
             "In room: %d  In object: %s  Carried by: %s  Wear_loc: %d\n\r",
             obj->in_room == NULL ? 0 : obj->in_room->vnum,
             obj->in_obj == NULL ? "(none)" : obj->in_obj->short_descr,
             obj->carried_by == NULL ? "(none)" :
             can_see (ch, obj->carried_by) ? obj->carried_by->name
             : "someone", obj->wear_loc);
    send_to_char (buf, ch);

    sprintf (buf, "Values: %d %d %d %d %d\n\r",
             obj->value[0], obj->value[1], obj->value[2], obj->value[3],
             obj->value[4]);
    send_to_char (buf, ch);

    /* now give out vital statistics as per identify */

    switch (obj->item_type)
    {
        case ITEM_SCROLL:
        case ITEM_POTION:
        case ITEM_PILL:
            sprintf (buf, "Level %d spells of:", obj->value[0]);
            send_to_char (buf, ch);

            if (obj->value[1] >= 0 && obj->value[1] < top_sn)
            {
                send_to_char (" '", ch);
                send_to_char (skill_table[obj->value[1]]->name, ch);
                send_to_char ("'", ch);
            }

            if (obj->value[2] >= 0 && obj->value[2] < top_sn)
            {
                send_to_char (" '", ch);
                send_to_char (skill_table[obj->value[2]]->name, ch);
                send_to_char ("'", ch);
            }

            if (obj->value[3] >= 0 && obj->value[3] < top_sn)
            {
                send_to_char (" '", ch);
                send_to_char (skill_table[obj->value[3]]->name, ch);
                send_to_char ("'", ch);
            }

            if (obj->value[4] >= 0 && obj->value[4] < top_sn)
            {
                send_to_char (" '", ch);
                send_to_char (skill_table[obj->value[4]]->name, ch);
                send_to_char ("'", ch);
            }

            send_to_char (".\n\r", ch);
            break;

        case ITEM_WAND:
        case ITEM_STAFF:
            sprintf (buf, "Has %d(%d) charges of level %d",
                     obj->value[1], obj->value[2], obj->value[0]);
            send_to_char (buf, ch);

            if (obj->value[3] >= 0 && obj->value[3] < top_sn)
            {
                send_to_char (" '", ch);
                send_to_char (skill_table[obj->value[3]]->name, ch);
                send_to_char ("'", ch);
            }

            send_to_char (".\n\r", ch);
            break;

        case ITEM_DRINK_CON:
            sprintf (buf, "It holds %s-colored %s.\n\r",
                     liq_table[obj->value[2]].liq_color,
                     liq_table[obj->value[2]].liq_name);
            send_to_char (buf, ch);
            break;


        case ITEM_WEAPON:
            send_to_char ("Weapon type is ", ch);
            switch (obj->value[0])
            {
                case (WEAPON_EXOTIC):
                    send_to_char ("exotic\n\r", ch);
                    break;
                case (WEAPON_SWORD):
                    send_to_char ("sword\n\r", ch);
                    break;
                case (WEAPON_DAGGER):
                    send_to_char ("dagger\n\r", ch);
                    break;
                case (WEAPON_SPEAR):
                    send_to_char ("spear/staff\n\r", ch);
                    break;
                case (WEAPON_MACE):
                    send_to_char ("mace/club\n\r", ch);
                    break;
                case (WEAPON_AXE):
                    send_to_char ("axe\n\r", ch);
                    break;
                case (WEAPON_FLAIL):
                    send_to_char ("flail\n\r", ch);
                    break;
                case (WEAPON_WHIP):
                    send_to_char ("whip\n\r", ch);
                    break;
                case (WEAPON_POLEARM):
                    send_to_char ("polearm\n\r", ch);
                    break;
                default:
                    send_to_char ("unknown\n\r", ch);
                    break;
            }
            sprintf (buf, "Damage is %dd%d (average %d)\n\r",
                        obj->value[1], obj->value[2],
                        (1 + obj->value[2]) * obj->value[1] / 2);
            send_to_char (buf, ch);

            sprintf (buf, "Damage noun is %s.\n\r",
                     (obj->value[3] > 0
                      && obj->value[3] <
                      MAX_DAMAGE_MESSAGE) ? attack_table[obj->value[3]].noun :
                     "undefined");
            send_to_char (buf, ch);

            if (obj->value[4])
            {                    /* weapon flags */
                sprintf (buf, "Weapons flags: %s\n\r",
                         weapon_bit_name (obj->value[4]));
                send_to_char (buf, ch);
            }
            break;

        case ITEM_ARMOR:
            sprintf (buf,
                     "Armor class is %d pierce, %d bash, %d slash, and %d vs. magic\n\r",
                     obj->value[0], obj->value[1], obj->value[2],
                     obj->value[3]);
            send_to_char (buf, ch);
            break;

        case ITEM_CONTAINER:
            sprintf (buf, "Capacity: %d#  Maximum weight: %d#  flags: %s\n\r",
                     obj->value[0], obj->value[3],
                     cont_bit_name (obj->value[1]));
            send_to_char (buf, ch);
            if (obj->value[4] != 100)
            {
                sprintf (buf, "Weight multiplier: %d%%\n\r", obj->value[4]);
                send_to_char (buf, ch);
            }
            break;
    }


    if (obj->extra_descr != NULL || obj->pIndexData->extra_descr != NULL)
    {
        EXTRA_DESCR_DATA *ed;

        send_to_char ("Extra description keywords: '", ch);

        for (ed = obj->extra_descr; ed != NULL; ed = ed->next)
        {
            send_to_char (ed->keyword, ch);
            if (ed->next != NULL)
                send_to_char (" ", ch);
        }

        for (ed = obj->pIndexData->extra_descr; ed != NULL; ed = ed->next)
        {
            send_to_char (ed->keyword, ch);
            if (ed->next != NULL)
                send_to_char (" ", ch);
        }

        send_to_char ("'\n\r", ch);
    }

    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
        sprintf (buf, "Affects %s by %d, level %d",
                 affect_loc_name (paf->location), paf->modifier, paf->level);
        send_to_char (buf, ch);
        if (paf->duration > -1)
            sprintf (buf, ", %d hours.\n\r", paf->duration);
        else
            sprintf (buf, ".\n\r");
        send_to_char (buf, ch);
        if (paf->bitvector)
        {
            switch (paf->where)
            {
                case TO_AFFECTS:
                    sprintf (buf, "Adds %s affect.\n",
                             affect_bit_name (paf->bitvector));
                    break;
                case TO_WEAPON:
                    sprintf (buf, "Adds %s weapon flags.\n",
                             weapon_bit_name (paf->bitvector));
                    break;
                case TO_OBJECT:
                    sprintf (buf, "Adds %s object flag.\n",
                             extra_bit_name (paf->bitvector));
                    break;
                case TO_IMMUNE:
                    sprintf (buf, "Adds immunity to %s.\n",
                             imm_bit_name (paf->bitvector));
                    break;
                case TO_RESIST:
                    sprintf (buf, "Adds resistance to %s.\n\r",
                             imm_bit_name (paf->bitvector));
                    break;
                case TO_VULN:
                    sprintf (buf, "Adds vulnerability to %s.\n\r",
                             imm_bit_name (paf->bitvector));
                    break;
                default:
                    sprintf (buf, "Unknown bit %d: %d\n\r",
                             paf->where, paf->bitvector);
                    break;
            }
            send_to_char (buf, ch);
        }
    }

    if (!obj->enchanted)
        for (paf = obj->pIndexData->affected; paf != NULL; paf = paf->next)
        {
            sprintf (buf, "Affects %s by %d, level %d.\n\r",
                     affect_loc_name (paf->location), paf->modifier,
                     paf->level);
            send_to_char (buf, ch);
            if (paf->bitvector)
            {
                switch (paf->where)
                {
                    case TO_AFFECTS:
                        sprintf (buf, "Adds %s affect.\n",
                                 affect_bit_name (paf->bitvector));
                        break;
                    case TO_OBJECT:
                        sprintf (buf, "Adds %s object flag.\n",
                                 extra_bit_name (paf->bitvector));
                        break;
                    case TO_IMMUNE:
                        sprintf (buf, "Adds immunity to %s.\n",
                                 imm_bit_name (paf->bitvector));
                        break;
                    case TO_RESIST:
                        sprintf (buf, "Adds resistance to %s.\n\r",
                                 imm_bit_name (paf->bitvector));
                        break;
                    case TO_VULN:
                        sprintf (buf, "Adds vulnerability to %s.\n\r",
                                 imm_bit_name (paf->bitvector));
                        break;
                    default:
                        sprintf (buf, "Unknown bit %d: %d\n\r",
                                 paf->where, paf->bitvector);
                        break;
                }
                send_to_char (buf, ch);
            }
        }

    return;
}



void do_mstat (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    AFFECT_DATA *paf;
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Stat whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, argument)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    sprintf (buf, "Name: %s\n\r", victim->name);
    send_to_char (buf, ch);

    sprintf (buf,
             "Vnum: %d  Race: %s  Group: %d  Sex: %s  Room: %d\n\r",
             IS_NPC (victim) ? victim->pIndexData->vnum : 0,
             race_table[victim->race].name,
             IS_NPC (victim) ? victim->group : 0, sex_table[victim->sex].name,
             victim->in_room == NULL ? 0 : victim->in_room->vnum);
    send_to_char (buf, ch);

    if (IS_NPC (victim))
    {
        sprintf (buf, "Count: %d  Killed: %d\n\r",
                 victim->pIndexData->count, victim->pIndexData->killed);
        send_to_char (buf, ch);
    }

    sprintf (buf,
             "Str: %d(%d)  Int: %d(%d)  Wis: %d(%d)  Dex: %d(%d)  Con: %d(%d)\n\r",
             victim->perm_stat[STAT_STR],
             get_curr_stat (victim, STAT_STR),
             victim->perm_stat[STAT_INT],
             get_curr_stat (victim, STAT_INT),
             victim->perm_stat[STAT_WIS],
             get_curr_stat (victim, STAT_WIS),
             victim->perm_stat[STAT_DEX],
             get_curr_stat (victim, STAT_DEX),
             victim->perm_stat[STAT_CON], get_curr_stat (victim, STAT_CON));
    send_to_char (buf, ch);

    sprintf (buf, "Hp: %d/%d  Mana: %d/%d  Move: %d/%d  Practices: %d\n\r",
             victim->hit, victim->max_hit,
             victim->mana, victim->max_mana,
             victim->move, victim->max_move,
             IS_NPC (ch) ? 0 : victim->practice);
    send_to_char (buf, ch);

    sprintf (buf,
             "Lv: %d  Class: %s  Align: %d  Gold: %ld  Silver: %ld  Exp: %d\n\r",
             victim->level,
             IS_NPC (victim) ? "mobile" : class_table[victim->class]->name,
             victim->alignment, victim->gold, victim->silver, victim->exp);
    send_to_char (buf, ch);

    sprintf (buf, "Armor: pierce: %d  bash: %d  slash: %d  magic: %d\n\r",
             GET_AC (victim, AC_PIERCE), GET_AC (victim, AC_BASH),
             GET_AC (victim, AC_SLASH), GET_AC (victim, AC_EXOTIC));
    send_to_char (buf, ch);

    sprintf (buf,
             "Hit: %d  Dam: %d  Saves: %d  Size: %s  Position: %s  Wimpy: %d\n\r",
             GET_HITROLL (victim), GET_DAMROLL (victim), victim->saving_throw,
             size_table[victim->size].name,
             position_table[victim->position].name, victim->wimpy);
    send_to_char (buf, ch);

    if (IS_NPC (victim))
    {
        sprintf (buf, "Damage: %dd%d  Message:  %s\n\r",
                 victim->damage[DICE_NUMBER], victim->damage[DICE_TYPE],
                 attack_table[victim->dam_type].noun);
        send_to_char (buf, ch);
    }
    sprintf (buf, "Fighting: %s\n\r",
             victim->fighting ? victim->fighting->name : "(none)");
    send_to_char (buf, ch);

    if (!IS_NPC (victim))
    {
        sprintf (buf,
                 "Thirst: %d  Hunger: %d  Full: %d  Drunk: %d\n\r",
                 victim->pcdata->condition[COND_THIRST],
                 victim->pcdata->condition[COND_HUNGER],
                 victim->pcdata->condition[COND_FULL],
                 victim->pcdata->condition[COND_DRUNK]);
        send_to_char (buf, ch);
    }

    sprintf (buf, "Carry number: %d  Carry weight: %ld\n\r",
             victim->carry_number, get_carry_weight (victim) / 10);
    send_to_char (buf, ch);


    if (!IS_NPC (victim))
    {
        sprintf (buf,
                 "Age: %d  Played: %d  Last Level: %d  Timer: %d\n\r",
                 get_age (victim),
                 (int) (victim->played + current_time - victim->logon) / 3600,
                 victim->pcdata->last_level, victim->timer);
        send_to_char (buf, ch);
    }

    sprintf (buf, "Act: %s\n\r", act_bit_name (victim->act));
    send_to_char (buf, ch);

    if (victim->comm)
    {
        sprintf (buf, "Comm: %s\n\r", comm_bit_name (victim->comm));
        send_to_char (buf, ch);
    }

    if (IS_NPC (victim) && victim->off_flags)
    {
        sprintf (buf, "Offense: %s\n\r", off_bit_name (victim->off_flags));
        send_to_char (buf, ch);
    }

    if (victim->imm_flags)
    {
        sprintf (buf, "Immune: %s\n\r", imm_bit_name (victim->imm_flags));
        send_to_char (buf, ch);
    }

    if (victim->res_flags)
    {
        sprintf (buf, "Resist: %s\n\r", imm_bit_name (victim->res_flags));
        send_to_char (buf, ch);
    }

    if (victim->vuln_flags)
    {
        sprintf (buf, "Vulnerable: %s\n\r",
                 imm_bit_name (victim->vuln_flags));
        send_to_char (buf, ch);
    }

    sprintf (buf, "Form: %s\n\rParts: %s\n\r",
             form_bit_name (victim->form), part_bit_name (victim->parts));
    send_to_char (buf, ch);

    if (victim->affected_by)
    {
        sprintf (buf, "Affected by %s\n\r",
                 affect_bit_name (victim->affected_by));
        send_to_char (buf, ch);
    }

    sprintf (buf, "Master: %s  Leader: %s  Pet: %s\n\r",
             victim->master ? victim->master->name : "(none)",
             victim->leader ? victim->leader->name : "(none)",
             victim->pet ? victim->pet->name : "(none)");
    send_to_char (buf, ch);

    if (!IS_NPC (victim))
    {
        sprintf (buf, "Security: %d.\n\r", victim->pcdata->security);    /* OLC */
        send_to_char (buf, ch);    /* OLC */
    }

    sprintf (buf, "Short description: %s\n\rLong  description: %s",
             victim->short_descr,
             victim->long_descr[0] !=
             '\0' ? victim->long_descr : "(none)\n\r");
    send_to_char (buf, ch);

    if (IS_NPC (victim) && victim->spec_fun != 0)
    {
        sprintf (buf, "Mobile has special procedure %s.\n\r",
                 spec_name (victim->spec_fun));
        send_to_char (buf, ch);
    }

    for (paf = victim->affected; paf != NULL; paf = paf->next)
    {
        sprintf (buf,
                 "Spell: '%s' modifies %s by %d for %d hours with bits %s, level %d.\n\r",
                 skill_table[(int) paf->type]->name,
                 affect_loc_name (paf->location),
                 paf->modifier,
                 paf->duration, affect_bit_name (paf->bitvector), paf->level);
        send_to_char (buf, ch);
    }

    return;
}

/* ofind and mfind replaced with vnum, vnum skill also added */

void do_vnum (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *string;

    string = one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  vnum obj <name>\n\r", ch);
        send_to_char ("  vnum mob <name>\n\r", ch);
        send_to_char ("  vnum skill <skill or spell>\n\r", ch);
        return;
    }

    if (!str_cmp (arg, "obj"))
    {
        do_function (ch, &do_ofind, string);
        return;
    }

    if (!str_cmp (arg, "mob") || !str_cmp (arg, "char"))
    {
        do_function (ch, &do_mfind, string);
        return;
    }

    if (!str_cmp (arg, "skill") || !str_cmp (arg, "spell"))
    {
        do_function (ch, &do_slookup, string);
        return;
    }
    /* do both */
    do_function (ch, &do_mfind, argument);
    do_function (ch, &do_ofind, argument);
}


void do_mfind (CHAR_DATA * ch, char *argument)
{
    extern int top_mob_index;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    MOB_INDEX_DATA *pMobIndex;
    int vnum;
    int nMatch;
    bool fAll;
    bool found;

    one_argument (argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char ("Find whom?\n\r", ch);
        return;
    }

    fAll = FALSE;                /* !str_cmp( arg, "all" ); */
    found = FALSE;
    nMatch = 0;

    /*
     * Yeah, so iterating over all vnum's takes 10,000 loops.
     * Get_mob_index is fast, and I don't feel like threading another link.
     * Do you?
     * -- Furey
     */
    for (vnum = 0; nMatch < top_mob_index; vnum++)
    {
        if ((pMobIndex = get_mob_index (vnum)) != NULL)
        {
            nMatch++;
            if (fAll || is_name (argument, pMobIndex->player_name))
            {
                found = TRUE;
                sprintf (buf, "[%5d] %s\n\r",
                         pMobIndex->vnum, pMobIndex->short_descr);
                send_to_char (buf, ch);
            }
        }
    }

    if (!found)
        send_to_char ("No mobiles by that name.\n\r", ch);

    return;
}



void do_ofind (CHAR_DATA * ch, char *argument)
{
    extern int top_obj_index;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    OBJ_INDEX_DATA *pObjIndex;
    int vnum;
    int nMatch;
    bool fAll;
    bool found;

    one_argument (argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char ("Find what?\n\r", ch);
        return;
    }

    fAll = FALSE;                /* !str_cmp( arg, "all" ); */
    found = FALSE;
    nMatch = 0;

    /*
     * Yeah, so iterating over all vnum's takes 10,000 loops.
     * Get_obj_index is fast, and I don't feel like threading another link.
     * Do you?
     * -- Furey
     */
    for (vnum = 0; nMatch < top_obj_index; vnum++)
    {
        if ((pObjIndex = get_obj_index (vnum)) != NULL)
        {
            nMatch++;
            if (fAll || is_name (argument, pObjIndex->name))
            {
                found = TRUE;
                sprintf (buf, "[%5d] %s\n\r",
                         pObjIndex->vnum, pObjIndex->short_descr);
                send_to_char (buf, ch);
            }
        }
    }

    if (!found)
        send_to_char ("No objects by that name.\n\r", ch);

    return;
}


void do_owhere (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_INPUT_LENGTH];
    BUFFER *buffer;
    OBJ_DATA *obj;
    OBJ_DATA *in_obj;
    bool found;
    int number = 0, max_found;

    found = FALSE;
    number = 0;
    max_found = 200;

    buffer = new_buf ();

    if (argument[0] == '\0')
    {
        send_to_char ("Find what?\n\r", ch);
        return;
    }

    for (obj = object_list; obj != NULL; obj = obj->next)
    {
        if (!can_see_obj (ch, obj) || !is_name (argument, obj->name)
            || ch->level < obj->level)
            continue;

        found = TRUE;
        number++;

        for (in_obj = obj; in_obj->in_obj != NULL; in_obj = in_obj->in_obj);

        if (in_obj->carried_by != NULL && can_see (ch, in_obj->carried_by)
            && in_obj->carried_by->in_room != NULL)
            sprintf (buf, "%3d) %s is carried by %s [Room %d]\n\r",
                     number, obj->short_descr, PERS (in_obj->carried_by, ch),
                     in_obj->carried_by->in_room->vnum);
        else if (in_obj->in_room != NULL
                 && can_see_room (ch, in_obj->in_room)) sprintf (buf,
                                                                 "%3d) %s is in %s [Room %d]\n\r",
                                                                 number,
                                                                 obj->short_descr,
                                                                 in_obj->in_room->name,
                                                                 in_obj->in_room->vnum);
        else
            sprintf (buf, "%3d) %s is somewhere\n\r", number,
                     obj->short_descr);

        buf[0] = UPPER (buf[0]);
        add_buf (buffer, buf);

        if (number >= max_found)
            break;
    }

    if (!found)
        send_to_char ("Nothing like that in heaven or earth.\n\r", ch);
    else
        page_to_char (buf_string (buffer), ch);

    free_buf (buffer);
}


void do_mwhere (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    BUFFER *buffer;
    CHAR_DATA *victim;
    bool found;
    int count = 0;

    if (argument[0] == '\0')
    {
        DESCRIPTOR_DATA *d;

        /* show characters logged */

        buffer = new_buf ();
        for (d = descriptor_list; d != NULL; d = d->next)
        {
            if (d->character != NULL && d->connected == CON_PLAYING
                && d->character->in_room != NULL && can_see (ch, d->character)
                && can_see_room (ch, d->character->in_room))
            {
                victim = d->character;
                count++;
                if (d->original != NULL)
                    sprintf (buf,
                             "%3d) %s (in the body of %s) is in %s [%d]\n\r",
                             count, d->original->name, victim->short_descr,
                             victim->in_room->name, victim->in_room->vnum);
                else
                    sprintf (buf, "%3d) %s is in %s [%d]\n\r", count,
                             victim->name, victim->in_room->name,
                             victim->in_room->vnum);
                add_buf (buffer, buf);
            }
        }

        page_to_char (buf_string (buffer), ch);
        free_buf (buffer);
        return;
    }

    found = FALSE;
    buffer = new_buf ();
    for (victim = char_list; victim != NULL; victim = victim->next)
    {
        if (victim->in_room != NULL && is_name (argument, victim->name))
        {
            found = TRUE;
            count++;
            sprintf (buf, "%3d) [%5d] %-28s [%5d] %s\n\r", count,
                     IS_NPC (victim) ? victim->pIndexData->vnum : 0,
                     IS_NPC (victim) ? victim->short_descr : victim->name,
                     victim->in_room->vnum, victim->in_room->name);
            add_buf (buffer, buf);
        }
    }

    if (!found)
        act ("You didn't find any $T.", ch, NULL, argument, TO_CHAR);
    else
        page_to_char (buf_string (buffer), ch);

    free_buf (buffer);

    return;
}



void do_reboo (CHAR_DATA * ch, char *argument)
{
    send_to_char ("If you want to REBOOT, spell it out.\n\r", ch);
    return;
}



void do_reboot (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    extern bool merc_down;
    DESCRIPTOR_DATA *d, *d_next;
    CHAR_DATA *vch;

    if (ch->invis_level < LEVEL_HERO)
    {
        sprintf (buf, "{RReboot{x by %s.", ch->name);
        do_function (ch, &do_echo, buf);
    }

    // Let's restore the world as part of the reboot as a thank you to the players.
    do_restore(ch, "all");

    merc_down = TRUE;
    for (d = descriptor_list; d != NULL; d = d_next)
    {
        d_next = d->next;
        vch = d->original ? d->original : d->character;
        if (vch != NULL)
            save_char_obj (vch);
        close_socket (d);
    }

    // Save special items like donation pits and corpses
    save_game_objects();

    return;
}

void do_shutdow (CHAR_DATA * ch, char *argument)
{
    send_to_char ("If you want to SHUTDOWN, spell it out.\n\r", ch);
    return;
}

void do_shutdown (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    extern bool merc_down;
    DESCRIPTOR_DATA *d, *d_next;
    CHAR_DATA *vch;

    if (ch->invis_level < LEVEL_HERO)
        sprintf (buf, "{RShutdown{x by %s.", ch->name);
    append_file (ch, SHUTDOWN_FILE, buf);
    strcat (buf, "\n\r");
    if (ch->invis_level < LEVEL_HERO)
    {
        do_function (ch, &do_echo, buf);
    }

    // Let's restore the world as part of the shutdown as a thank you to the players.
    do_restore(ch, "all");

    merc_down = TRUE;
    for (d = descriptor_list; d != NULL; d = d_next)
    {
        d_next = d->next;
        vch = d->original ? d->original : d->character;
        if (vch != NULL)
            save_char_obj (vch);
        close_socket (d);
    }

    // Save special items like donation pits and corpses
    save_game_objects();

    return;
}

void do_protect (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;

    if (argument[0] == '\0')
    {
        send_to_char ("Protect whom from snooping?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, argument)) == NULL)
    {
        send_to_char ("You can't find them.\n\r", ch);
        return;
    }

    if (IS_SET (victim->comm, COMM_SNOOP_PROOF))
    {
        act_new ("$N is no longer snoop-proof.", ch, NULL, victim, TO_CHAR,
                 POS_DEAD);
        send_to_char ("Your snoop-proofing was just removed.\n\r", victim);
        REMOVE_BIT (victim->comm, COMM_SNOOP_PROOF);
    }
    else
    {
        act_new ("$N is now snoop-proof.", ch, NULL, victim, TO_CHAR,
                 POS_DEAD);
        send_to_char ("You are now immune to snooping.\n\r", victim);
        SET_BIT (victim->comm, COMM_SNOOP_PROOF);
    }
}



void do_snoop (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;
    char buf[MAX_STRING_LENGTH];

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Snoop whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (victim->desc == NULL)
    {
        send_to_char ("No descriptor to snoop.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char ("Cancelling all snoops.\n\r", ch);
        wiznet ("$N stops being such a snoop.",
                ch, NULL, WIZ_SNOOPS, WIZ_SECURE, get_trust (ch));
        for (d = descriptor_list; d != NULL; d = d->next)
        {
            if (d->snoop_by == ch->desc)
                d->snoop_by = NULL;
        }
        return;
    }

    if (victim->desc->snoop_by != NULL)
    {
        send_to_char ("Busy already.\n\r", ch);
        return;
    }

    if (!is_room_owner (ch, victim->in_room) && ch->in_room != victim->in_room
        && room_is_private (victim->in_room) && !IS_TRUSTED (ch, IMPLEMENTOR))
    {
        send_to_char ("That character is in a private room.\n\r", ch);
        return;
    }

    if (get_trust (victim) >= get_trust (ch)
        || IS_SET (victim->comm, COMM_SNOOP_PROOF))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    if (ch->desc != NULL)
    {
        for (d = ch->desc->snoop_by; d != NULL; d = d->snoop_by)
        {
            if (d->character == victim || d->original == victim)
            {
                send_to_char ("No snoop loops.\n\r", ch);
                return;
            }
        }
    }

    victim->desc->snoop_by = ch->desc;
    sprintf (buf, "$N starts snooping on %s",
             (IS_NPC (ch) ? victim->short_descr : victim->name));
    wiznet (buf, ch, NULL, WIZ_SNOOPS, WIZ_SECURE, get_trust (ch));
    send_to_char ("Ok.\n\r", ch);
    return;
}



void do_switch (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Switch into whom?\n\r", ch);
        return;
    }

    if (ch->desc == NULL)
        return;

    if (ch->desc->original != NULL)
    {
        send_to_char ("You are already switched.\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char ("Ok.\n\r", ch);
        return;
    }

    if (!IS_NPC (victim))
    {
        send_to_char ("You can only switch into mobiles.\n\r", ch);
        return;
    }

    if (!is_room_owner (ch, victim->in_room) && ch->in_room != victim->in_room
        && room_is_private (victim->in_room) && !IS_TRUSTED (ch, IMPLEMENTOR))
    {
        send_to_char ("That character is in a private room.\n\r", ch);
        return;
    }

    if (victim->desc != NULL)
    {
        send_to_char ("Character in use.\n\r", ch);
        return;
    }

    sprintf (buf, "$N switches into %s", victim->short_descr);
    wiznet (buf, ch, NULL, WIZ_SWITCHES, WIZ_SECURE, get_trust (ch));

    ch->desc->character = victim;
    ch->desc->original = ch;
    victim->desc = ch->desc;
    ch->desc = NULL;
    /* change communications to match */
    if (ch->prompt != NULL)
        victim->prompt = str_dup (ch->prompt);
    victim->comm = ch->comm;
    victim->lines = ch->lines;
    send_to_char ("Ok.\n\r", victim);
    return;
}



void do_return (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (ch->desc == NULL)
        return;

    if (ch->desc->original == NULL)
    {
        send_to_char ("You aren't switched.\n\r", ch);
        return;
    }

    send_to_char
        ("You return to your original body. Type replay to see any missed tells.\n\r",
         ch);
    if (ch->prompt != NULL)
    {
        free_string (ch->prompt);
        ch->prompt = NULL;
    }

    sprintf (buf, "$N returns from %s.", ch->short_descr);
    wiznet (buf, ch->desc->original, 0, WIZ_SWITCHES, WIZ_SECURE,
            get_trust (ch));
    ch->desc->character = ch->desc->original;
    ch->desc->original = NULL;
    ch->desc->character->desc = ch->desc;
    ch->desc = NULL;
    return;
}

/*
 * Trust levels for load and clone.  I've set this to immortal, clone and
 * load should probably be logged anyway.
 */
bool obj_check (CHAR_DATA * ch, OBJ_DATA * obj)
{
    if (IS_IMMORTAL(ch))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/* for clone, to insure that cloning goes many levels deep */
void recursive_clone (CHAR_DATA * ch, OBJ_DATA * obj, OBJ_DATA * clone)
{
    OBJ_DATA *c_obj, *t_obj;


    for (c_obj = obj->contains; c_obj != NULL; c_obj = c_obj->next_content)
    {
        if (obj_check (ch, c_obj))
        {
            t_obj = create_object (c_obj->pIndexData, 0);
            clone_object (c_obj, t_obj);
            obj_to_obj (t_obj, clone);
            recursive_clone (ch, c_obj, t_obj);
        }
    }
}

/* command that is similar to load */
void do_clone (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *rest;
    CHAR_DATA *mob;
    OBJ_DATA *obj;

    rest = one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Clone what?\n\r", ch);
        return;
    }

    if (!str_prefix (arg, "object"))
    {
        mob = NULL;
        obj = get_obj_here (ch, rest);
        if (obj == NULL)
        {
            send_to_char ("You don't see that here.\n\r", ch);
            return;
        }
    }
    else if (!str_prefix (arg, "mobile") || !str_prefix (arg, "character"))
    {
        obj = NULL;
        mob = get_char_room (ch, rest);
        if (mob == NULL)
        {
            send_to_char ("You don't see that here.\n\r", ch);
            return;
        }
    }
    else
    {                            /* find both */

        mob = get_char_room (ch, argument);
        obj = get_obj_here (ch, argument);
        if (mob == NULL && obj == NULL)
        {
            send_to_char ("You don't see that here.\n\r", ch);
            return;
        }
    }

    /* clone an object */
    if (obj != NULL)
    {
        OBJ_DATA *clone;

        if (!obj_check (ch, obj))
        {
            send_to_char
                ("Your powers are not great enough for such a task.\n\r", ch);
            return;
        }

        clone = create_object (obj->pIndexData, 0);
        clone_object (obj, clone);
        if (obj->carried_by != NULL)
            obj_to_char (clone, ch);
        else
            obj_to_room (clone, ch->in_room);
        recursive_clone (ch, obj, clone);

        act ("$n has created $p.", ch, clone, NULL, TO_ROOM);
        act ("You clone $p.", ch, clone, NULL, TO_CHAR);
        wiznet ("$N clones $p.", ch, clone, WIZ_LOAD, WIZ_SECURE,
                get_trust (ch));
        return;
    }
    else if (mob != NULL)
    {
        CHAR_DATA *clone;
        OBJ_DATA *new_obj;
        char buf[MAX_STRING_LENGTH];

        if (!IS_NPC (mob))
        {
            send_to_char ("You can only clone mobiles.\n\r", ch);
            return;
        }

        clone = create_mobile (mob->pIndexData);
        clone_mobile (mob, clone);

        for (obj = mob->carrying; obj != NULL; obj = obj->next_content)
        {
            if (obj_check (ch, obj))
            {
                new_obj = create_object (obj->pIndexData, 0);
                clone_object (obj, new_obj);
                recursive_clone (ch, obj, new_obj);
                obj_to_char (new_obj, clone);
                new_obj->wear_loc = obj->wear_loc;
            }
        }
        char_to_room (clone, ch->in_room);
        act ("$n has created $N.", ch, NULL, clone, TO_ROOM);
        act ("You clone $N.", ch, NULL, clone, TO_CHAR);
        sprintf (buf, "$N clones %s.", clone->short_descr);
        wiznet (buf, ch, NULL, WIZ_LOAD, WIZ_SECURE, get_trust (ch));
        return;
    }
}

/*
 * Allows an immortal to load one or more objects or mobs.
 */
void do_load(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int number = 0;
    argument = one_argument(argument,arg);

    if (arg[0] == '\0')
    {
        send_to_char("Syntax:\n\r",ch);
        send_to_char("  load mob <vnum>\n\r",ch);
        send_to_char("  load mob <quantity>*<vnum>\n\r", ch);
        send_to_char("  load obj <vnum>\n\r",ch);
        send_to_char("  load obj <quantity>*<vnum>\n\r\n\r", ch);
        send_to_char("  * Max quantity allowed is 200 at a time.\n\r", ch);
        return;
    }

    number = mult_argument(argument, arg2);

    if (number > 200)
    {
        send_to_char("That's a bit excessive isn't it?\n\r", ch);
        return;
    }

    if (number < 1)
    {
        number = 1;
    }
    if (!str_prefix(arg, "mob") || !str_cmp(arg, "char"))
    {
        do_mload(ch, arg2, number);
    }
    else if (!str_prefix(arg, "obj"))
    {
        do_oload(ch, arg2, number);
    }
    else
    {
        // echo syntax
        do_load(ch, "");
    }

} // end do_load

/*
 * Loads a mob (mobile).  This is called from the do_load command.
 */
void do_mload(CHAR_DATA *ch, char *argument, int number)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    MOB_INDEX_DATA *pMobIndex;
    CHAR_DATA *victim;
    int n = 0;

    one_argument( argument, arg );

    if ( arg[0] == '\0' || !is_number(arg) || number < 1)
    {
        do_load(ch, "");
        return;
    }

    if ((pMobIndex = get_mob_index(atoi(arg))) == NULL)
    {
        send_to_char( "No mob has that vnum.\n\r", ch );
        return;
    }

    n = 0;

    do
    {
        n++;
        victim = create_mobile( pMobIndex );
        char_to_room( victim, ch->in_room );
    } while (n < number);

    sprintf(buf, "$n has created [{R%d{x] $N!", number);
    act(buf, ch, NULL, victim, TO_ROOM );
    sprintf(buf, "$N loads [{R%d{x] %s.", number, victim->short_descr);
    wiznet(buf, ch, NULL, WIZ_LOAD, WIZ_SECURE, get_trust(ch));
    sprintf(buf, "You load [{R%d{x] $N.", number);
    act(buf, ch, NULL, victim, TO_CHAR);
    return;

} // end do_mload

/*
 * Loads an object.  This is called by the do_load command.
 */
void do_oload( CHAR_DATA *ch, char *argument, int number)
{
    char arg1[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    OBJ_INDEX_DATA *pObjIndex;
    OBJ_DATA *obj;
    int n = 0;

    argument = one_argument( argument, arg1 );

    if (arg1[0] == '\0' || number < 1 || !is_number(arg1))
    {
        do_load(ch, "");
        return;
    }

    if ((pObjIndex = get_obj_index(atoi(arg1))) == NULL)
    {
        send_to_char( "No object has that vnum.\n\r", ch );
        return;
    }

    n = 0;

    do
    {
        n++;
        obj = create_object(pObjIndex, 0);

        if (CAN_WEAR(obj, ITEM_TAKE))
        {
            obj_to_char( obj, ch );
        }
        else
        {
            obj_to_room(obj, ch->in_room);
        }
    } while (n < number);

    sprintf(buf, "$n has created [{R%d{x] $p!", number);
    act( buf, ch, obj, NULL, TO_ROOM );
    sprintf(buf, "$N loads [{R%d{x] $p.", number);
    wiznet(buf,ch,obj,WIZ_LOAD,WIZ_SECURE,get_trust(ch));
    sprintf(buf, "You load [{R%d{x] $p.", number);
    act(buf, ch, obj, NULL, TO_CHAR);

   return;
} // end do_oload

void do_purge (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[100];
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    DESCRIPTOR_DATA *d;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        /* 'purge' */
        CHAR_DATA *vnext;
        OBJ_DATA *obj_next;

        for (victim = ch->in_room->people; victim != NULL; victim = vnext)
        {
            vnext = victim->next_in_room;
            if (IS_NPC (victim) && !IS_SET (victim->act, ACT_NOPURGE)
                && victim != ch /* safety precaution */ )
                extract_char (victim, TRUE);
        }

        for (obj = ch->in_room->contents; obj != NULL; obj = obj_next)
        {
            obj_next = obj->next_content;
            if (!IS_OBJ_STAT (obj, ITEM_NOPURGE))
                extract_obj (obj);
        }

        act ("$n purges the room!", ch, NULL, NULL, TO_ROOM);
        send_to_char ("Ok.\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (!IS_NPC (victim))
    {

        if (ch == victim)
        {
            send_to_char ("Ho ho ho.\n\r", ch);
            return;
        }

        if (get_trust (ch) <= get_trust (victim))
        {
            send_to_char ("Maybe that wasn't a good idea...\n\r", ch);
            sprintf (buf, "%s tried to purge you!\n\r", ch->name);
            send_to_char (buf, victim);
            return;
        }

        act ("$n disintegrates $N.", ch, 0, victim, TO_NOTVICT);

        if (victim->level > 1)
            save_char_obj (victim);
        d = victim->desc;
        extract_char (victim, TRUE);
        if (d != NULL)
            close_socket (d);

        return;
    }

    act ("$n purges $N.", ch, NULL, victim, TO_NOTVICT);
    extract_char (victim, TRUE);
    return;
}



void do_advance (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int level;
    int iLevel;

    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0' || !is_number (arg2))
    {
        send_to_char ("Syntax: advance <char> <level>.\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg1)) == NULL)
    {
        send_to_char ("That player is not here.\n\r", ch);
        return;
    }

    if (IS_NPC (victim))
    {
        send_to_char ("Not on NPC's.\n\r", ch);
        return;
    }

    if ((level = atoi (arg2)) < 1 || level > MAX_LEVEL)
    {
        sprintf (buf, "Level must be 1 to %d.\n\r", MAX_LEVEL);
        send_to_char (buf, ch);
        return;
    }

    if (level > get_trust (ch))
    {
        send_to_char ("Limited to your trust level.\n\r", ch);
        return;
    }

    /*
     * Lower level:
     *   Reset to level 1.
     *   Then raise again.
     *   Currently, an imp can lower another imp.
     *   -- Swiftest
     */
    if (level <= victim->level)
    {
        int temp_prac;

        send_to_char ("Lowering a player's level!\n\r", ch);
        send_to_char ("**** OOOOHHHHHHHHHH  NNNNOOOO ****\n\r", victim);
        temp_prac = victim->practice;
        victim->level = 1;
        victim->exp = exp_per_level (victim, victim->pcdata->points);
        victim->max_hit = 10;
        victim->max_mana = 100;
        victim->max_move = 100;
        victim->practice = 0;
        victim->hit = victim->max_hit;
        victim->mana = victim->max_mana;
        victim->move = victim->max_move;
        advance_level (victim, TRUE);
        victim->practice = temp_prac;
    }
    else
    {
        send_to_char ("Raising a player's level!\n\r", ch);
        send_to_char ("**** OOOOHHHHHHHHHH  YYYYEEEESSS ****\n\r", victim);
    }

    for (iLevel = victim->level; iLevel < level; iLevel++)
    {
        victim->level += 1;
        advance_level (victim, TRUE);
    }
    sprintf (buf, "You are now level %d.\n\r", victim->level);
    send_to_char (buf, victim);
    victim->exp = exp_per_level (victim, victim->pcdata->points)
        * UMAX (1, victim->level);
    victim->trust = 0;
    save_char_obj (victim);
    return;
}



void do_trust (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    int level;

    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0' || !is_number (arg2))
    {
        send_to_char ("Syntax: trust <char> <level>.\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg1)) == NULL)
    {
        send_to_char ("That player is not here.\n\r", ch);
        return;
    }

    if ((level = atoi (arg2)) < 0 || level > MAX_LEVEL)
    {
        sprintf (buf, "Level must be 0 (reset) or 1 to %d.\n\r", MAX_LEVEL);
        send_to_char (buf, ch);
        return;
    }

    if (level > get_trust (ch))
    {
        send_to_char ("Limited to your trust.\n\r", ch);
        return;
    }

    victim->trust = level;
    return;
}



void do_restore (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *vch;
    DESCRIPTOR_DATA *d;

    one_argument (argument, arg);
    if (arg[0] == '\0' || !str_cmp (arg, "room"))
    {
        /* cure room */

        for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
        {
            affect_strip (vch, gsn_plague);
            affect_strip (vch, gsn_poison);
            affect_strip (vch, gsn_blindness);
            affect_strip (vch, gsn_sleep);
            affect_strip (vch, gsn_curse);

            vch->hit = vch->max_hit;
            vch->mana = vch->max_mana;
            vch->move = vch->max_move;
            update_pos (vch);
            act ("$n has restored you.", ch, NULL, vch, TO_VICT);
        }

        sprintf (buf, "$N restored room %d.", ch->in_room->vnum);
        wiznet (buf, ch, NULL, WIZ_RESTORE, WIZ_SECURE, get_trust (ch));

        send_to_char ("Room restored.\n\r", ch);
        return;

    }

    if (get_trust (ch) >= MAX_LEVEL - 1 && !str_cmp (arg, "all"))
    {
        /* cure all */

        for (d = descriptor_list; d != NULL; d = d->next)
        {
            victim = d->character;

            if (victim == NULL || IS_NPC (victim))
                continue;

            affect_strip (victim, gsn_plague);
            affect_strip (victim, gsn_poison);
            affect_strip (victim, gsn_blindness);
            affect_strip (victim, gsn_sleep);
            affect_strip (victim, gsn_curse);

            victim->hit = victim->max_hit;
            victim->mana = victim->max_mana;
            victim->move = victim->max_move;
            update_pos (victim);
            if (victim->in_room != NULL)
                act ("$n has restored you.", ch, NULL, victim, TO_VICT);
        }
        send_to_char ("All active players restored.\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    affect_strip (victim, gsn_plague);
    affect_strip (victim, gsn_poison);
    affect_strip (victim, gsn_blindness);
    affect_strip (victim, gsn_sleep);
    affect_strip (victim, gsn_curse);
    victim->hit = victim->max_hit;
    victim->mana = victim->max_mana;
    victim->move = victim->max_move;
    update_pos (victim);
    act ("$n has restored you.", ch, NULL, victim, TO_VICT);
    sprintf (buf, "$N restored %s",
             IS_NPC (victim) ? victim->short_descr : victim->name);
    wiznet (buf, ch, NULL, WIZ_RESTORE, WIZ_SECURE, get_trust (ch));
    send_to_char ("Ok.\n\r", ch);
    return;
}


void do_freeze (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Freeze whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (IS_NPC (victim))
    {
        send_to_char ("Not on NPC's.\n\r", ch);
        return;
    }

    if (get_trust (victim) >= get_trust (ch))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    if (IS_SET (victim->act, PLR_FREEZE))
    {
        REMOVE_BIT (victim->act, PLR_FREEZE);
        send_to_char ("You can play again.\n\r", victim);
        send_to_char ("FREEZE removed.\n\r", ch);
        sprintf (buf, "$N thaws %s.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }
    else
    {
        SET_BIT (victim->act, PLR_FREEZE);
        send_to_char ("You can't do ANYthing!\n\r", victim);
        send_to_char ("FREEZE set.\n\r", ch);
        sprintf (buf, "$N puts %s in the deep freeze.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }

    save_char_obj (victim);

    return;
}



void do_log (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Log whom?\n\r", ch);
        return;
    }

    if (!str_cmp (arg, "all"))
    {
        if (fLogAll)
        {
            fLogAll = FALSE;
            send_to_char ("Log ALL off.\n\r", ch);
        }
        else
        {
            fLogAll = TRUE;
            send_to_char ("Log ALL on.\n\r", ch);
        }
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (IS_NPC (victim))
    {
        send_to_char ("Not on NPC's.\n\r", ch);
        return;
    }

    /*
     * No level check, gods can log anyone.
     */
    if (IS_SET (victim->act, PLR_LOG))
    {
        REMOVE_BIT (victim->act, PLR_LOG);
        send_to_char ("LOG removed.\n\r", ch);
    }
    else
    {
        SET_BIT (victim->act, PLR_LOG);
        send_to_char ("LOG set.\n\r", ch);
    }

    return;
}



void do_noemote (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Noemote whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }


    if (get_trust (victim) >= get_trust (ch))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    if (IS_SET (victim->comm, COMM_NOEMOTE))
    {
        REMOVE_BIT (victim->comm, COMM_NOEMOTE);
        send_to_char ("You can emote again.\n\r", victim);
        send_to_char ("NOEMOTE removed.\n\r", ch);
        sprintf (buf, "$N restores emotes to %s.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }
    else
    {
        SET_BIT (victim->comm, COMM_NOEMOTE);
        send_to_char ("You can't emote!\n\r", victim);
        send_to_char ("NOEMOTE set.\n\r", ch);
        sprintf (buf, "$N revokes %s's emotes.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }

    return;
}



void do_noshout (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Noshout whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (IS_NPC (victim))
    {
        send_to_char ("Not on NPC's.\n\r", ch);
        return;
    }

    if (get_trust (victim) >= get_trust (ch))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    if (IS_SET (victim->comm, COMM_NOSHOUT))
    {
        REMOVE_BIT (victim->comm, COMM_NOSHOUT);
        send_to_char ("You can shout again.\n\r", victim);
        send_to_char ("NOSHOUT removed.\n\r", ch);
        sprintf (buf, "$N restores shouts to %s.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }
    else
    {
        SET_BIT (victim->comm, COMM_NOSHOUT);
        send_to_char ("You can't shout!\n\r", victim);
        send_to_char ("NOSHOUT set.\n\r", ch);
        sprintf (buf, "$N revokes %s's shouts.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }

    return;
}



void do_notell (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Notell whom?", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (get_trust (victim) >= get_trust (ch))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    if (IS_SET (victim->comm, COMM_NOTELL))
    {
        REMOVE_BIT (victim->comm, COMM_NOTELL);
        send_to_char ("You can tell again.\n\r", victim);
        send_to_char ("NOTELL removed.\n\r", ch);
        sprintf (buf, "$N restores tells to %s.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }
    else
    {
        SET_BIT (victim->comm, COMM_NOTELL);
        send_to_char ("You can't tell!\n\r", victim);
        send_to_char ("NOTELL set.\n\r", ch);
        sprintf (buf, "$N revokes %s's tells.", victim->name);
        wiznet (buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
    }

    return;
}



void do_peace (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *rch;

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (rch->fighting != NULL)
            stop_fighting (rch, TRUE);
        if (IS_NPC (rch) && IS_SET (rch->act, ACT_AGGRESSIVE))
            REMOVE_BIT (rch->act, ACT_AGGRESSIVE);
    }

    send_to_char ("Ok.\n\r", ch);
    return;
}

void do_wizlock (CHAR_DATA * ch, char *argument)
{
    extern bool wizlock;
    wizlock = !wizlock;

    if (wizlock)
    {
        wiznet ("$N has wizlocked the game.", ch, NULL, 0, 0, 0);
        send_to_char ("Game wizlocked.\n\r", ch);
    }
    else
    {
        wiznet ("$N removes wizlock.", ch, NULL, 0, 0, 0);
        send_to_char ("Game un-wizlocked.\n\r", ch);
    }

    return;
}

/* RT anti-newbie code */

void do_newlock (CHAR_DATA * ch, char *argument)
{
    extern bool newlock;
    newlock = !newlock;

    if (newlock)
    {
        wiznet ("$N locks out new characters.", ch, NULL, 0, 0, 0);
        send_to_char ("New characters have been locked out.\n\r", ch);
    }
    else
    {
        wiznet ("$N allows new characters back in.", ch, NULL, 0, 0, 0);
        send_to_char ("Newlock removed.\n\r", ch);
    }

    return;
}

// do_flag code merged in from flags.c since it's a wiz command
int flag_lookup
args ((const char *name, const struct flag_type * flag_table));

void do_flag (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH],
        arg3[MAX_INPUT_LENGTH];
    char word[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    long *flag, old = 0, new = 0, marked = 0, pos;
    char type;
    const struct flag_type *flag_table;

    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);
    argument = one_argument (argument, arg3);

    type = argument[0];

    if (type == '=' || type == '-' || type == '+')
        argument = one_argument (argument, word);

    if (arg1[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  flag mob  <name> <field> <flags>\n\r", ch);
        send_to_char ("  flag char <name> <field> <flags>\n\r", ch);
        send_to_char ("  mob  flags: act,aff,off,imm,res,vuln,form,part\n\r",
                      ch);
        send_to_char ("  char flags: plr,comm,aff,imm,res,vuln,\n\r", ch);
        send_to_char ("  +: add flag, -: remove flag, = set equal to\n\r",
                      ch);
        send_to_char ("  otherwise flag toggles the flags listed.\n\r", ch);
        return;
    }

    if (arg2[0] == '\0')
    {
        send_to_char ("What do you wish to set flags on?\n\r", ch);
        return;
    }

    if (arg3[0] == '\0')
    {
        send_to_char ("You need to specify a flag to set.\n\r", ch);
        return;
    }

    if (argument[0] == '\0')
    {
        send_to_char ("Which flags do you wish to change?\n\r", ch);
        return;
    }

    if (!str_prefix (arg1, "mob") || !str_prefix (arg1, "char"))
    {
        victim = get_char_world (ch, arg2);
        if (victim == NULL)
        {
            send_to_char ("You can't find them.\n\r", ch);
            return;
        }

        /* select a flag to set */
        if (!str_prefix (arg3, "act"))
        {
            if (!IS_NPC (victim))
            {
                send_to_char ("Use plr for PCs.\n\r", ch);
                return;
            }

            flag = &victim->act;
            flag_table = act_flags;
        }

        else if (!str_prefix (arg3, "plr"))
        {
            if (IS_NPC (victim))
            {
                send_to_char ("Use act for NPCs.\n\r", ch);
                return;
            }

            flag = &victim->act;
            flag_table = plr_flags;
        }

        else if (!str_prefix (arg3, "aff"))
        {
            flag = &victim->affected_by;
            flag_table = affect_flags;
        }

        else if (!str_prefix (arg3, "immunity"))
        {
            flag = &victim->imm_flags;
            flag_table = imm_flags;
        }

        else if (!str_prefix (arg3, "resist"))
        {
            flag = &victim->res_flags;
            flag_table = imm_flags;
        }

        else if (!str_prefix (arg3, "vuln"))
        {
            flag = &victim->vuln_flags;
            flag_table = imm_flags;
        }

        else if (!str_prefix (arg3, "form"))
        {
            if (!IS_NPC (victim))
            {
                send_to_char ("Form can't be set on PCs.\n\r", ch);
                return;
            }

            flag = &victim->form;
            flag_table = form_flags;
        }

        else if (!str_prefix (arg3, "parts"))
        {
            if (!IS_NPC (victim))
            {
                send_to_char ("Parts can't be set on PCs.\n\r", ch);
                return;
            }

            flag = &victim->parts;
            flag_table = part_flags;
        }

        else if (!str_prefix (arg3, "comm"))
        {
            if (IS_NPC (victim))
            {
                send_to_char ("Comm can't be set on NPCs.\n\r", ch);
                return;
            }

            flag = &victim->comm;
            flag_table = comm_flags;
        }

        else
        {
            send_to_char ("That's not an acceptable flag.\n\r", ch);
            return;
        }

        old = *flag;
        victim->zone = NULL;

        if (type != '=')
            new = old;

        /* mark the words */
        for (;;)
        {
            argument = one_argument (argument, word);

            if (word[0] == '\0')
                break;

            pos = flag_lookup (word, flag_table);

            if (pos == NO_FLAG)
            {
                send_to_char ("That flag doesn't exist!\n\r", ch);
                return;
            }
            else
                SET_BIT (marked, pos);
        }

        for (pos = 0; flag_table[pos].name != NULL; pos++)
        {
            if (!flag_table[pos].settable
                && IS_SET (old, flag_table[pos].bit))
            {
                SET_BIT (new, flag_table[pos].bit);
                continue;
            }

            if (IS_SET (marked, flag_table[pos].bit))
            {
                switch (type)
                {
                    case '=':
                    case '+':
                        SET_BIT (new, flag_table[pos].bit);
                        break;
                    case '-':
                        REMOVE_BIT (new, flag_table[pos].bit);
                        break;
                    default:
                        if (IS_SET (new, flag_table[pos].bit))
                            REMOVE_BIT (new, flag_table[pos].bit);
                        else
                            SET_BIT (new, flag_table[pos].bit);
                }
            }
        }
        *flag = new;
        return;
    }
}

void do_slookup (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int sn;

    one_argument (argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char ("Lookup which skill or spell?  Choose 'all' to list all skills/spells.\n\r", ch);
        return;
    }

    if (!str_cmp (arg, "all"))
    {
        for (sn = 0; sn < top_sn; sn++)
        {
            if (skill_table[sn]->name == NULL)
                break;
            sprintf (buf, "Sn: %3d  Skill/spell: '%s'\n\r", sn, skill_table[sn]->name);
            send_to_char (buf, ch);
        }
    }
    else
    {
        if ((sn = skill_lookup (arg)) < 0)
        {
            send_to_char ("No such skill or spell.\n\r", ch);
            return;
        }

        sprintf (buf, "Sn: %3d  Skill/spell: '%s'\n\r", sn, skill_table[sn]->name);
        send_to_char (buf, ch);
    }

    return;
}

/* RT set replaces sset, mset, oset, and rset */

void do_set (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    argument = one_argument (argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  set mob       <name> <field> <value>\n\r", ch);
        send_to_char ("  set character <name> <field> <value>\n\r", ch);
        send_to_char ("  set obj       <name> <field> <value>\n\r", ch);
        send_to_char ("  set room      <room> <field> <value>\n\r", ch);
        send_to_char ("  set skill     <name> <spell or skill> <value>\n\r", ch);
        return;
    }

    if (!str_prefix (arg, "mobile") || !str_prefix (arg, "character"))
    {
        do_function (ch, &do_mset, argument);
        return;
    }

    if (!str_prefix (arg, "skill") || !str_prefix (arg, "spell"))
    {
        do_function (ch, &do_sset, argument);
        return;
    }

    if (!str_prefix (arg, "object"))
    {
        do_function (ch, &do_oset, argument);
        return;
    }

    if (!str_prefix (arg, "room"))
    {
        do_function (ch, &do_rset, argument);
        return;
    }
    /* echo syntax */
    do_function (ch, &do_set, "");
}


void do_sset (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int value;
    int sn;
    bool fAll;

    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);
    argument = one_argument (argument, arg3);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  set skill <name> <spell or skill> <value>\n\r", ch);
        send_to_char ("  set skill <name> all <value>\n\r", ch);
        send_to_char ("   (use the name of the skill, not the number)\n\r",
                      ch);
        return;
    }

    if ((victim = get_char_world (ch, arg1)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (IS_NPC (victim))
    {
        send_to_char ("Not on NPC's.\n\r", ch);
        return;
    }

    fAll = !str_cmp (arg2, "all");
    sn = 0;
    if (!fAll && (sn = skill_lookup (arg2)) < 0)
    {
        send_to_char ("No such skill or spell.\n\r", ch);
        return;
    }

    /*
     * Snarf the value.
     */
    if (!is_number (arg3))
    {
        send_to_char ("Value must be numeric.\n\r", ch);
        return;
    }

    value = atoi (arg3);
    if (value < 0 || value > 100)
    {
        send_to_char ("Value range is 0 to 100.\n\r", ch);
        return;
    }

    if (fAll)
    {
        for (sn = 0; sn < top_sn; sn++)
        {
            if (skill_table[sn]->name != NULL)
                victim->pcdata->learned[sn] = value;
        }
    }
    else
    {
        victim->pcdata->learned[sn] = value;
    }

    return;
}


void do_mset (CHAR_DATA * ch, char *argument)
{
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    int value;
    extern int top_class;

    smash_tilde (argument);
    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);
    strcpy (arg3, argument);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  set char <name> <field> <value>\n\r", ch);
        send_to_char ("  Field being one of:\n\r", ch);
        send_to_char ("    str int wis dex con sex class level\n\r", ch);
        send_to_char ("    race group gold silver hp mana move prac\n\r", ch);
        send_to_char ("    align train thirst hunger drunk full\n\r", ch);
        send_to_char ("    security hours wanted[on|off] tester[on|off]\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, arg1)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    /* clear zones for mobs */
    victim->zone = NULL;

    /*
     * Snarf the value (which need not be numeric).
     */
    value = is_number (arg3) ? atoi (arg3) : -1;

    /*
     * Set something.
     */
    if (!str_cmp (arg2, "str"))
    {
        if (value < 3 || value > get_max_train (victim, STAT_STR))
        {
            sprintf (buf,
                     "Strength range is 3 to %d\n\r.",
                     get_max_train (victim, STAT_STR));
            send_to_char (buf, ch);
            return;
        }

        victim->perm_stat[STAT_STR] = value;
        return;
    }

    if (!str_cmp (arg2, "security"))
    {                            /* OLC */
        if (IS_NPC (ch))
        {
            send_to_char ("NPC's can't set this value.\n\r", ch);
            return;
        }

        if (IS_NPC (victim))
        {
            send_to_char ("Not on NPC's.\n\r", ch);
            return;
        }

        if (value > ch->pcdata->security || value < 0)
        {
            if (ch->pcdata->security != 0)
            {
                sprintf (buf, "Valid security is 0-%d.\n\r",
                         ch->pcdata->security);
                send_to_char (buf, ch);
            }
            else
            {
                send_to_char ("Valid security is 0 only.\n\r", ch);
            }
            return;
        }
        victim->pcdata->security = value;
        return;
    }

    if (!str_cmp (arg2, "int"))
    {
        if (value < 3 || value > get_max_train (victim, STAT_INT))
        {
            sprintf (buf,
                     "Intelligence range is 3 to %d.\n\r",
                     get_max_train (victim, STAT_INT));
            send_to_char (buf, ch);
            return;
        }

        victim->perm_stat[STAT_INT] = value;
        return;
    }

    if (!str_cmp (arg2, "wis"))
    {
        if (value < 3 || value > get_max_train (victim, STAT_WIS))
        {
            sprintf (buf,
                     "Wisdom range is 3 to %d.\n\r", get_max_train (victim,
                                                                    STAT_WIS));
            send_to_char (buf, ch);
            return;
        }

        victim->perm_stat[STAT_WIS] = value;
        return;
    }

    if (!str_cmp (arg2, "dex"))
    {
        if (value < 3 || value > get_max_train (victim, STAT_DEX))
        {
            sprintf (buf,
                     "Dexterity range is 3 to %d.\n\r",
                     get_max_train (victim, STAT_DEX));
            send_to_char (buf, ch);
            return;
        }

        victim->perm_stat[STAT_DEX] = value;
        return;
    }

    if (!str_cmp (arg2, "con"))
    {
        if (value < 3 || value > get_max_train (victim, STAT_CON))
        {
            sprintf (buf,
                     "Constitution range is 3 to %d.\n\r",
                     get_max_train (victim, STAT_CON));
            send_to_char (buf, ch);
            return;
        }

        victim->perm_stat[STAT_CON] = value;
        return;
    }

    if (!str_prefix (arg2, "sex"))
    {
        if (value < 0 || value > 2)
        {
            send_to_char ("Sex range is 0 to 2.\n\r", ch);
            return;
        }
        victim->sex = value;
        if (!IS_NPC (victim))
            victim->pcdata->true_sex = value;
        return;
    }

    if (!str_prefix (arg2, "class"))
    {
        int class;

        if (IS_NPC (victim))
        {
            send_to_char ("Mobiles have no class.\n\r", ch);
            return;
        }

        class = class_lookup (arg3);
        if (class == -1)
        {
            char buf[MAX_STRING_LENGTH];

            strcpy (buf, "Possible classes are: ");
            for (class = 0; class < top_class; class++)
            {
                if (class > 0)
                    strcat (buf, " ");
                strcat (buf, class_table[class]->name);
            }
            strcat (buf, ".\n\r");

            send_to_char (buf, ch);
            return;
        }

        victim->class = class;
        return;
    }

    if (!str_prefix (arg2, "level"))
    {
        if (!IS_NPC (victim))
        {
            send_to_char ("Not on PC's.\n\r", ch);
            return;
        }

        if (value < 0 || value > MAX_LEVEL)
        {
            sprintf (buf, "Level range is 0 to %d.\n\r", MAX_LEVEL);
            send_to_char (buf, ch);
            return;
        }
        victim->level = value;
        return;
    }

    if (!str_prefix (arg2, "gold"))
    {
        victim->gold = value;
        return;
    }

    if (!str_prefix (arg2, "silver"))
    {
        victim->silver = value;
        return;
    }

    if (!str_prefix (arg2, "hp"))
    {
        if (value < -10 || value > 30000)
        {
            send_to_char ("Hp range is -10 to 30,000 hit points.\n\r", ch);
            return;
        }
        victim->max_hit = value;
        if (!IS_NPC (victim))
            victim->pcdata->perm_hit = value;
        return;
    }

    if (!str_prefix (arg2, "mana"))
    {
        if (value < 0 || value > 30000)
        {
            send_to_char ("Mana range is 0 to 30,000 mana points.\n\r", ch);
            return;
        }
        victim->max_mana = value;
        if (!IS_NPC (victim))
            victim->pcdata->perm_mana = value;
        return;
    }

    if (!str_prefix (arg2, "move"))
    {
        if (value < 0 || value > 30000)
        {
            send_to_char ("Move range is 0 to 30,000 move points.\n\r", ch);
            return;
        }
        victim->max_move = value;
        if (!IS_NPC (victim))
            victim->pcdata->perm_move = value;
        return;
    }

    if (!str_prefix (arg2, "practice"))
    {
        if (value < 0 || value > 250)
        {
            send_to_char ("Practice range is 0 to 250 sessions.\n\r", ch);
            return;
        }
        victim->practice = value;
        return;
    }

    if (!str_prefix (arg2, "train"))
    {
        if (value < 0 || value > 50)
        {
            send_to_char ("Training session range is 0 to 50 sessions.\n\r",
                          ch);
            return;
        }
        victim->train = value;
        return;
    }

    if (!str_prefix (arg2, "align"))
    {
        if (value < 1 || value > 3)
        {
            send_to_char ("Alignment range is 1 for evil, 2 for neutral, 3 for good.\n\r", ch);
            return;
        }
        victim->alignment = value;
        return;
    }

    if (!str_prefix (arg2, "thirst"))
    {
        if (IS_NPC (victim))
        {
            send_to_char ("Not on NPC's.\n\r", ch);
            return;
        }

        if (value < -1 || value > 100)
        {
            send_to_char ("Thirst range is -1 to 100.\n\r", ch);
            return;
        }

        victim->pcdata->condition[COND_THIRST] = value;
        return;
    }

    if (!str_prefix (arg2, "drunk"))
    {
        if (IS_NPC (victim))
        {
            send_to_char ("Not on NPC's.\n\r", ch);
            return;
        }

        if (value < -1 || value > 100)
        {
            send_to_char ("Drunk range is -1 to 100.\n\r", ch);
            return;
        }

        victim->pcdata->condition[COND_DRUNK] = value;
        return;
    }

    if (!str_prefix (arg2, "full"))
    {
        if (IS_NPC (victim))
        {
            send_to_char ("Not on NPC's.\n\r", ch);
            return;
        }

        if (value < -1 || value > 100)
        {
            send_to_char ("Full range is -1 to 100.\n\r", ch);
            return;
        }

        victim->pcdata->condition[COND_FULL] = value;
        return;
    }

    if (!str_prefix (arg2, "hunger"))
    {
        if (IS_NPC (victim))
        {
            send_to_char ("Not on NPC's.\n\r", ch);
            return;
        }

        if (value < -1 || value > 100)
        {
            send_to_char ("Full range is -1 to 100.\n\r", ch);
            return;
        }

        victim->pcdata->condition[COND_HUNGER] = value;
        return;
    }

    if (!str_prefix (arg2, "race"))
    {
        int race;

        race = race_lookup (arg3);

        if (race == 0)
        {
            send_to_char ("That is not a valid race.\n\r", ch);
            return;
        }

        if (!IS_NPC (victim) && !race_table[race].pc_race)
        {
            send_to_char ("That is not a valid player race.\n\r", ch);
            return;
        }

        victim->race = race;
        return;
    }

    if (!str_prefix (arg2, "group"))
    {
        if (!IS_NPC (victim))
        {
            send_to_char ("Only on NPCs.\n\r", ch);
            return;
        }
        victim->group = value;
        return;
    }

    // Toggle wanted
    if (!str_prefix(arg2, "wanted"))
    {
        if (IS_NPC(victim))
        {
            send_to_char("Not on NPC's\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "on"))
        {
            SET_BIT (victim->act, PLR_WANTED);
            send_to_char ("({RWANTED{x) flag added.\n\r", ch);
        }
        else if (!str_prefix(arg3, "off"))
        {
            REMOVE_BIT (victim->act, PLR_WANTED);
            send_to_char ("({RWANTED{x) flag removed.\n\r", ch);
        }

        return;
    }

    if (!str_prefix(arg2, "tester"))
    {
        if (IS_NPC(victim))
        {
            send_to_char("Not on NPC's\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "on"))
        {
            SET_BIT (victim->act, PLR_TESTER);
            send_to_char ("({WTester{x) flag added.\n\r", ch);

            if (ch != victim)
            {
                sprintf(buf, "%s has set you as a ({WTester{x).\n\r", ch->name);
                send_to_char (buf, victim);
            }

        }
        else if (!str_prefix(arg3, "off"))
        {
            REMOVE_BIT (victim->act, PLR_TESTER);
            send_to_char ("({WTester{x) flag removed.\n\r", ch);

            if (ch != victim)
            {
                sprintf(buf, "%s has removed your ({WTester{x) flag.\n\r", ch->name);
                send_to_char (buf, victim);
            }
        }

        return;
    }

    if (!str_prefix (arg2, "hours"))
    {
        if (IS_NPC (victim))
        {
            send_to_char ("Not on NPC's.\n\r", ch);
            return;
        }

        if (!is_number (arg3))
        {
            send_to_char ("Value must be numeric.\n\r", ch);
            return;
        }

        value = atoi (arg3);

        if (value < 0 || value > 99999)
        {
            send_to_char ("Value must be between 0 and 99,999.\n\r", ch);
            return;
        }

        victim->played = ( value * 3600 );
        printf_to_char(ch, "%s's hours set to %d.\n\r", victim->name, value);

        return;
    }

    /*
     * Generate usage message.
     */
    do_function (ch, &do_mset, "");
    return;
}

void do_string (CHAR_DATA * ch, char *argument)
{
    char type[MAX_INPUT_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *obj;

    smash_tilde (argument);
    argument = one_argument (argument, type);
    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);
    strcpy (arg3, argument);

    if (type[0] == '\0' || arg1[0] == '\0' || arg2[0] == '\0'
        || arg3[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  string char <name> <field> <string>\n\r", ch);
        send_to_char ("    fields: name short long desc title spec\n\r", ch);
        send_to_char ("  string obj  <name> <field> <string>\n\r", ch);
        send_to_char ("    fields: name short long extended\n\r", ch);
        return;
    }

    if (!str_prefix (type, "character") || !str_prefix (type, "mobile"))
    {
        if ((victim = get_char_world (ch, arg1)) == NULL)
        {
            send_to_char ("They aren't here.\n\r", ch);
            return;
        }

        /* clear zone for mobs */
        victim->zone = NULL;

        /* string something */

        if (!str_prefix (arg2, "name"))
        {
            if (!IS_NPC (victim))
            {
                send_to_char ("Not on PC's.\n\r", ch);
                return;
            }
            free_string (victim->name);
            victim->name = str_dup (arg3);
            return;
        }

        if (!str_prefix (arg2, "description"))
        {
            free_string (victim->description);
            victim->description = str_dup (arg3);
            return;
        }

        if (!str_prefix (arg2, "short"))
        {
            free_string (victim->short_descr);
            victim->short_descr = str_dup (arg3);
            return;
        }

        if (!str_prefix (arg2, "long"))
        {
            free_string (victim->long_descr);
            strcat (arg3, "\n\r");
            victim->long_descr = str_dup (arg3);
            return;
        }

        if (!str_prefix (arg2, "title"))
        {
            if (IS_NPC (victim))
            {
                send_to_char ("Not on NPC's.\n\r", ch);
                return;
            }

            set_title (victim, arg3);
            return;
        }

        if (!str_prefix (arg2, "spec"))
        {
            if (!IS_NPC (victim))
            {
                send_to_char ("Not on PC's.\n\r", ch);
                return;
            }

            if ((victim->spec_fun = spec_lookup (arg3)) == 0)
            {
                send_to_char ("No such spec fun.\n\r", ch);
                return;
            }

            return;
        }
    }

    if (!str_prefix (type, "object"))
    {
        /* string an obj */

        if ((obj = get_obj_world (ch, arg1)) == NULL)
        {
            send_to_char ("Nothing like that in heaven or earth.\n\r", ch);
            return;
        }

        if (!str_prefix (arg2, "name"))
        {
            free_string (obj->name);
            obj->name = str_dup (arg3);
            return;
        }

        if (!str_prefix (arg2, "short"))
        {
            free_string (obj->short_descr);
            obj->short_descr = str_dup (arg3);
            return;
        }

        if (!str_prefix (arg2, "long"))
        {
            free_string (obj->description);
            obj->description = str_dup (arg3);
            return;
        }

        if (!str_prefix (arg2, "ed") || !str_prefix (arg2, "extended"))
        {
            EXTRA_DESCR_DATA *ed;

            argument = one_argument (argument, arg3);
            if (argument == NULL)
            {
                send_to_char
                    ("Syntax: oset <object> ed <keyword> <string>\n\r", ch);
                return;
            }

            strcat (argument, "\n\r");

            ed = new_extra_descr ();

            ed->keyword = str_dup (arg3);
            ed->description = str_dup (argument);
            ed->next = obj->extra_descr;
            obj->extra_descr = ed;
            return;
        }
    }


    /* echo bad use message */
    do_function (ch, &do_string, "");
}



void do_oset (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int value;

    smash_tilde (argument);
    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);
    strcpy (arg3, argument);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  set obj <object> <field> <value>\n\r", ch);
        send_to_char ("  Field being one of:\n\r", ch);
        send_to_char ("    value0 value1 value2 value3 value4 (v1-v4)\n\r",
                      ch);
        send_to_char ("    extra wear level weight cost timer\n\r", ch);
        return;
    }

    if ((obj = get_obj_world (ch, arg1)) == NULL)
    {
        send_to_char ("Nothing like that in heaven or earth.\n\r", ch);
        return;
    }

    /*
     * Snarf the value (which need not be numeric).
     */
    value = atoi (arg3);

    /*
     * Set something.
     */
    if (!str_cmp (arg2, "value0") || !str_cmp (arg2, "v0"))
    {
        obj->value[0] = UMIN (50, value);
        return;
    }

    if (!str_cmp (arg2, "value1") || !str_cmp (arg2, "v1"))
    {
        obj->value[1] = value;
        return;
    }

    if (!str_cmp (arg2, "value2") || !str_cmp (arg2, "v2"))
    {
        obj->value[2] = value;
        return;
    }

    if (!str_cmp (arg2, "value3") || !str_cmp (arg2, "v3"))
    {
        obj->value[3] = value;
        return;
    }

    if (!str_cmp (arg2, "value4") || !str_cmp (arg2, "v4"))
    {
        obj->value[4] = value;
        return;
    }

    if (!str_prefix (arg2, "extra"))
    {
        obj->extra_flags = value;
        return;
    }

    if (!str_prefix (arg2, "wear"))
    {
        obj->wear_flags = value;
        return;
    }

    if (!str_prefix (arg2, "level"))
    {
        obj->level = value;
        return;
    }

    if (!str_prefix (arg2, "weight"))
    {
        obj->weight = value;
        return;
    }

    if (!str_prefix (arg2, "cost"))
    {
        obj->cost = value;
        return;
    }

    if (!str_prefix (arg2, "timer"))
    {
        obj->timer = value;
        return;
    }

    /*
     * Generate usage message.
     */
    do_function (ch, &do_oset, "");
    return;
}



void do_rset (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    ROOM_INDEX_DATA *location;
    int value;

    smash_tilde (argument);
    argument = one_argument (argument, arg1);
    argument = one_argument (argument, arg2);
    strcpy (arg3, argument);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
        send_to_char ("Syntax:\n\r", ch);
        send_to_char ("  set room <location> <field> <value>\n\r", ch);
        send_to_char ("  Field being one of:\n\r", ch);
        send_to_char ("    flags sector\n\r", ch);
        return;
    }

    if ((location = find_location (ch, arg1)) == NULL)
    {
        send_to_char ("No such location.\n\r", ch);
        return;
    }

    if (!is_room_owner (ch, location) && ch->in_room != location
        && room_is_private (location) && !IS_TRUSTED (ch, IMPLEMENTOR))
    {
        send_to_char ("That room is private right now.\n\r", ch);
        return;
    }

    /*
     * Snarf the value.
     */
    if (!is_number (arg3))
    {
        send_to_char ("Value must be numeric.\n\r", ch);
        return;
    }
    value = atoi (arg3);

    /*
     * Set something.
     */
    if (!str_prefix (arg2, "flags"))
    {
        location->room_flags = value;
        return;
    }

    if (!str_prefix (arg2, "sector"))
    {
        location->sector_type = value;
        return;
    }

    /*
     * Generate usage message.
     */
    do_function (ch, &do_rset, "");
    return;
}



void do_sockets (CHAR_DATA * ch, char *argument)
{
    char buf[2 * MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char state[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d;
    int count;

    count = 0;
    buf[0] = '\0';

    one_argument (argument, arg);
    for (d = descriptor_list; d != NULL; d = d->next)
    {
         switch (d->connected)
         {
            default:
                sprintf(state, "Unknown");
                break;
            case -15:
                sprintf(state, "Get Email");
                break;
            case -14:
                sprintf(state, "Get Name");
                break;
            case -13:
                sprintf(state, "Get Old Password");
                break;
            case -12:
                sprintf(state, "Confirm New Name");
                break;
            case -11:
                sprintf(state, "Get New Password");
                break;
            case -10:
                sprintf(state, "Confirm New Password");
                break;
            case -9:
                sprintf(state, "Color Prompt");
                break;
            case -8:
                sprintf(state, "TelnetGA Prompt");
                break;
            case -7:
                sprintf(state, "Get Race");
                break;
            case -6:
                sprintf(state, "Get Gender");
                break;
            case -5:
                sprintf(state, "Get Class");
                break;
            case -4:
                sprintf(state, "Get Name");
                break;
            case -3:
                sprintf(state, "Default Choice");
                break;
            case -2:
                sprintf(state, "Get Groups");
                break;
            case -1:
                sprintf(state, "Get Weapon");
                break;
            case 0:
                sprintf(state, "Playing");
                break;
            case 1:
                sprintf(state, "Reading IMOTD");
                break;
            case 2:
                sprintf(state, "Reading MOTD");
                break;
            case 3:
                sprintf(state, "Reconnect Prompt");
                break;
            case 4:
                sprintf(state, "Copyover Recovery");
                break;
        }

        if (d->character != NULL && can_see (ch, d->character)
            && (arg[0] == '\0' || is_name (arg, d->character->name)
                || (d->original && is_name (arg, d->original->name))))
        {
            count++;
            sprintf (buf + strlen (buf), "{c[{W%3d{c][{W%-25s{c][{W%s@%s{c]{x\n\r",
                     d->descriptor,
                     state,
                     d->original ? d->original->name :
                     d->character ? d->character->name : "(none)",
                     d->host);
        }
        else if (d->character == NULL)
        {
            sprintf (buf + strlen (buf), "{c[{W%3d{c][{W%-25s{c][{Wnobody@%s{c]{x\n\r",
                     d->descriptor,
                     state,
                     d->host);
            count++;
        }
    }
    if (count == 0)
    {
        send_to_char ("No one by that name is connected.\n\r", ch);
        return;
    }

    sprintf (buf2, "\n\r{c%d user%s{x\n\r", count, count == 1 ? "" : "s");
    strcat (buf, buf2);

    send_to_char("{c--------------------------------------------------------------------------------{x\n\r", ch);
    send_to_char("{c[{WDes{c][{WConnected State{x          {c][{WCharacter@IP Address{x                          {c]{x\n\r", ch);
    send_to_char("{c--------------------------------------------------------------------------------{x\n\r", ch);

    page_to_char (buf, ch);
    return;
}



/*
 * Thanks to Grodyn for pointing out bugs in this function.
 */
void do_force (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];

    argument = one_argument (argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
        send_to_char ("Force whom to do what?\n\r", ch);
        return;
    }

    one_argument (argument, arg2);

    if (!str_cmp (arg2, "delete") || !str_prefix (arg2, "mob"))
    {
        send_to_char ("That will NOT be done.\n\r", ch);
        return;
    }

    sprintf (buf, "$n forces you to '%s'.", argument);

    /* Replaced original block with code by Edwin to keep from
     * corrupting pfiles in certain pet-infested situations.
     * JR -- 10/15/00
     */
    if ( !str_cmp( arg, "all" ) )
    {
        DESCRIPTOR_DATA *desc,*desc_next;

        if (get_trust(ch) < MAX_LEVEL - 3)
        {
            send_to_char("Not at your level!\n\r",ch);
            return;
        }

        for ( desc = descriptor_list; desc != NULL; desc = desc_next )
        {
            desc_next = desc->next;

            if (desc->connected==CON_PLAYING &&
                get_trust( desc->character ) < get_trust( ch ) )
            {
                act( buf, ch, NULL, desc->character, TO_VICT );
                interpret( desc->character, argument );
            }
        }
    }
    else if (!str_cmp (arg, "players"))
    {
        CHAR_DATA *vch;
        CHAR_DATA *vch_next;

        if (get_trust (ch) < MAX_LEVEL - 2)
        {
            send_to_char ("Not at your level!\n\r", ch);
            return;
        }

        for (vch = char_list; vch != NULL; vch = vch_next)
        {
            vch_next = vch->next;

            if (!IS_NPC (vch) && get_trust (vch) < get_trust (ch)
                && vch->level < LEVEL_HERO)
            {
                act (buf, ch, NULL, vch, TO_VICT);
                interpret (vch, argument);
            }
        }
    }
    else if (!str_cmp (arg, "gods"))
    {
        CHAR_DATA *vch;
        CHAR_DATA *vch_next;

        if (get_trust (ch) < MAX_LEVEL - 2)
        {
            send_to_char ("Not at your level!\n\r", ch);
            return;
        }

        for (vch = char_list; vch != NULL; vch = vch_next)
        {
            vch_next = vch->next;

            if (!IS_NPC (vch) && get_trust (vch) < get_trust (ch)
                && vch->level >= LEVEL_HERO)
            {
                act (buf, ch, NULL, vch, TO_VICT);
                interpret (vch, argument);
            }
        }
    }
    else
    {
        CHAR_DATA *victim;

        if ((victim = get_char_world (ch, arg)) == NULL)
        {
            send_to_char ("They aren't here.\n\r", ch);
            return;
        }

        if (victim == ch)
        {
            send_to_char ("Aye aye, right away!\n\r", ch);
            return;
        }

        if (!is_room_owner (ch, victim->in_room)
            && ch->in_room != victim->in_room
            && room_is_private (victim->in_room)
            && !IS_TRUSTED (ch, IMPLEMENTOR))
        {
            send_to_char ("That character is in a private room.\n\r", ch);
            return;
        }

        if (get_trust (victim) >= get_trust (ch))
        {
            send_to_char ("Do it yourself!\n\r", ch);
            return;
        }

        if (!IS_NPC (victim) && get_trust (ch) < MAX_LEVEL - 3)
        {
            send_to_char ("Not at your level!\n\r", ch);
            return;
        }

        act (buf, ch, NULL, victim, TO_VICT);
        interpret (victim, argument);
    }

    send_to_char ("Ok.\n\r", ch);
    return;
}



/*
 * New routines by Dionysos.
 */
void do_invis (CHAR_DATA * ch, char *argument)
{
    int level;
    char arg[MAX_STRING_LENGTH];

    /* RT code for taking a level argument */
    one_argument (argument, arg);

    if (arg[0] == '\0')
        /* take the default path */

        if (ch->invis_level)
        {
            ch->invis_level = 0;
            act ("$n slowly fades into existence.", ch, NULL, NULL, TO_ROOM);
            send_to_char ("You slowly fade back into existence.\n\r", ch);
        }
        else
        {
            ch->invis_level = get_trust (ch);
            act ("$n slowly fades into thin air.", ch, NULL, NULL, TO_ROOM);
            send_to_char ("You slowly vanish into thin air.\n\r", ch);
        }
    else
        /* do the level thing */
    {
        level = atoi (arg);
        if (level < 2 || level > get_trust (ch))
        {
            send_to_char ("Invis level must be between 2 and your level.\n\r",
                          ch);
            return;
        }
        else
        {
            ch->reply = NULL;
            ch->invis_level = level;
            act ("$n slowly fades into thin air.", ch, NULL, NULL, TO_ROOM);
            send_to_char ("You slowly vanish into thin air.\n\r", ch);
        }
    }

    return;
}


void do_incognito (CHAR_DATA * ch, char *argument)
{
    int level;
    char arg[MAX_STRING_LENGTH];

    /* RT code for taking a level argument */
    one_argument (argument, arg);

    if (arg[0] == '\0')
        /* take the default path */

        if (ch->incog_level)
        {
            ch->incog_level = 0;
            act ("$n is no longer cloaked.", ch, NULL, NULL, TO_ROOM);
            send_to_char ("You are no longer cloaked.\n\r", ch);
        }
        else
        {
            ch->incog_level = get_trust (ch);
            act ("$n cloaks $s presence.", ch, NULL, NULL, TO_ROOM);
            send_to_char ("You cloak your presence.\n\r", ch);
        }
    else
        /* do the level thing */
    {
        level = atoi (arg);
        if (level < 2 || level > get_trust (ch))
        {
            send_to_char ("Incog level must be between 2 and your level.\n\r",
                          ch);
            return;
        }
        else
        {
            ch->reply = NULL;
            ch->incog_level = level;
            act ("$n cloaks $s presence.", ch, NULL, NULL, TO_ROOM);
            send_to_char ("You cloak your presence.\n\r", ch);
        }
    }

    return;
}



void do_holylight (CHAR_DATA * ch, char *argument)
{
    if (IS_NPC (ch))
        return;

    if (IS_SET (ch->act, PLR_HOLYLIGHT))
    {
        REMOVE_BIT (ch->act, PLR_HOLYLIGHT);
        send_to_char ("Holy light mode off.\n\r", ch);
    }
    else
    {
        SET_BIT (ch->act, PLR_HOLYLIGHT);
        send_to_char ("Holy light mode on.\n\r", ch);
    }

    return;
}

/* prefix command: it will put the string typed on each line typed */

void do_prefi (CHAR_DATA * ch, char *argument)
{
    send_to_char ("You cannot abbreviate the prefix command.\r\n", ch);
    return;
}

void do_prefix (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_INPUT_LENGTH];

    if (argument[0] == '\0')
    {
        if (ch->prefix[0] == '\0')
        {
            send_to_char ("You have no prefix to clear.\r\n", ch);
            return;
        }

        send_to_char ("Prefix removed.\r\n", ch);
        free_string (ch->prefix);
        ch->prefix = str_dup ("");
        return;
    }

    if (ch->prefix[0] != '\0')
    {
        sprintf (buf, "Prefix changed to %s.\r\n", argument);
        free_string (ch->prefix);
    }
    else
    {
        sprintf (buf, "Prefix set to %s.\r\n", argument);
    }

    ch->prefix = str_dup (argument);
}

/* This file holds the copyover data */
#define COPYOVER_FILE "copyover.data"

/* This is the executable file */
#define EXE_FILE      "../area/cs-mud"

bool is_copyover = FALSE;
int copyover_timer = 0;

/*  Copyover - Original idea: Fusion of MUD++
 *  Adapted to Diku by Erwin S. Andreasen, <erwin@andreasen.org> (http://www.andreasen.org)
 *  Modifications made by Rhien.
 */
void do_copyover (CHAR_DATA * ch, char *argument)
{
    FILE *fp;
    DESCRIPTOR_DATA *d, *d_next;
    char buf[500], buf2[100], buf3[100];
    char arg1[MSL], arg2[MSL];
    extern int port, control;    /* db.c */

    if (ch == NULL)
    {
        send_to_all_char("{RWARNING{x: auto-{R{*copyover{x cancelled.");
        return;
    }

    // Check for the required argument
    if (argument[0] == '\0')
    {
        send_to_char("\n\rSyntax:  copyover [now|delay <ticks>|cancel|<reason>]\n\r\n\r", ch);
        send_to_char("Sample Usage:  copyover now\n\r", ch);
        send_to_char("               copyover now <reason>\n\r", ch);
        send_to_char("               copyover delay 3\n\r", ch);
        send_to_char("               copyover delay 3 <reason>\n\r", ch);
        send_to_char("               copyover cancel\n\r", ch);
        return;
    }

    argument = one_argument( argument, arg1 );

    // Verify the first argument is valid
    if (str_cmp( arg1, "now") && str_cmp(arg1, "delay") && str_cmp(arg1, "cancel"))
    {
        send_to_char("\n\rSyntax:  copyover [now|delay <ticks>|cancel|<reason>]\n\r\n\r", ch);
        send_to_char("Sample Usage:  copyover now\n\r", ch);
        send_to_char("               copyover delay 3\n\r", ch);
        send_to_char("               copyover delay 3 Implement new code\n\r", ch);
        send_to_char("               copyover cancel\n\r", ch);
       return;
    }

    if (!str_cmp( arg1, "delay"))
    {
        argument = one_argument( argument, arg2);

        if (is_number(arg2))
        {
            // Set how many ticks the copyover should occur in and send a message to all
            // connected sockets.
            copyover_timer = atoi(arg2);
            is_copyover = TRUE;

            // With or without a reason
            if (argument[0] == '\0')
            {
                sprintf (buf, "\n\r{RWARNING: auto-{R{*copyover{x by {B%s{x will occur in {B%d{x tick(s).\n\r",
                     ch->name, copyover_timer);
            }
            else
            {
                sprintf (buf, "\n\r{RWARNING{x: auto-{R{*copyover{x by {B%s{x will occur in {B%d{x tick(s).\n\r{WReason{x: %s\n\r",
                     ch->name, copyover_timer, argument);
            }

            // Set the pointer back to the person calling it so we can reference them
            // in the timer call back that's in update.c
            copyover_ch = ch;

            send_to_all_char(buf);
        }
        else
        {
            send_to_char("\n\rSyntax:  copyover delay <ticks>\n\r", ch);
            return;
        }

    }
    else if (!str_cmp( arg1, "cancel"))
    {
        is_copyover = FALSE;
        copyover_timer = 0;

        sprintf (buf, "\n\r{RWARNING{x: auto-{R{*copyover{x cancelled by {B%s{x.\n\r", ch->name);

        copyover_ch = NULL;

        send_to_all_char(buf);
    }
    else if (!str_cmp( arg1, "now" ))
    {
        // Let's restore the world as part of the copyover as a thank you to the players.
        do_restore(ch, "all");

        // This is where the actual copyover is initiated and where the timer should
        // call when it's up
        fp = fopen (COPYOVER_FILE, "w");

        if (!fp)
        {
            send_to_char ("Copyover file not writeable, aborted.\n\r", ch);
            log_f ("Could not write to copyover file: %s", COPYOVER_FILE);
            perror ("do_copyover:fopen");
            return;
        }

        /* Consider changing all saved areas here, if you use OLC */
        /* do_asave (NULL, ""); - autosave changed areas */

        // Ability to specify reason to show players
        if (argument[0] == '\0')
        {
            sprintf (buf, "\n\r{RWARNING{x: auto-{R{*copyover{x by {B%s{x - please remain seated!\n\r",
                 ch->name);
        }
        else
        {
            // If it's less than 200 characters, show it, otherwise show default, we don't want to
            // buffer override the buf.
            if (strlen(argument) < 200)
            {
                sprintf (buf, "\n\r{RWARNING{x: auto-{R{*copyover{x by {B%s{x - please remain seated!\n\r{WReason{x: %s\n\r",
                     ch->name, argument);
            }
            else
            {
                sprintf (buf, "\n\r{RWARNING{x: auto-{R{*copyover{x by {B%s{x - please remain seated!\n\r",
                     ch->name);
            }
        }

        /* For each playing descriptor, save its state */
        for (d = descriptor_list; d; d = d_next)
        {
            CHAR_DATA *och = CH (d);
            d_next = d->next;        /* We delete from the list , so need to save this */

            if (!d->character || d->connected < CON_PLAYING)
           {                        /* drop those logging on */
               write_to_descriptor (d->descriptor,
                                     "\n\rSorry, we are rebooting. Come back in a few minutes.\n\r",
                                     0, d);
                close_socket (d);    /* throw'em out */
            }
            else
            {
                fprintf (fp, "%d %s %s\n", d->descriptor, och->name, d->host);
                save_char_obj (och);
                write_to_descriptor (d->descriptor, buf, 0, d);
            }
        }

        fprintf (fp, "-1\n");
        fclose (fp);

        /* Save the special items like donation pits and player corpses */
        save_game_objects();

        /* Close reserve and other always-open files and release other resources */
        fclose (fpReserve);

        /* exec - descriptors are inherited */
        sprintf (buf, "%d", port);
        sprintf (buf2, "%d", control);
        strncpy( buf3, "-1", 100 );
        execl (EXE_FILE, "cs-mud", buf, "copyover", buf2, buf3, (char *) NULL);

        /* Failed - sucessful exec will not return */
        perror ("do_copyover: execl");
        send_to_char ("Copyover FAILED!\n\r", ch);

        /* Here you might want to reopen fpReserve */
        fpReserve = fopen (NULL_FILE, "r");
    } // end if "now"

} // end do_copyover

/* Recover from a copyover - load players */
void copyover_recover()
{
    DESCRIPTOR_DATA *d;
    FILE *fp;
    char name[100];
    char host[MSL];
    int desc;
    bool fOld;

    log_f ("Copyover recovery initiated");

    fp = fopen (COPYOVER_FILE, "r");

    if (!fp)
    {                            /* there are some descriptors open which will hang forever then ? */
        perror ("copyover_recover:fopen");
        log_f ("Copyover file not found. Exitting.\n\r");
        exit (1);
    }

#if defined(_WIN32)
    _unlink(COPYOVER_FILE);        /* In case something crashes - doesn't prevent reading  */
#else
    unlink (COPYOVER_FILE);        /* In case something crashes - doesn't prevent reading  */
#endif

    for (;;)
    {
        int errorcheck = fscanf (fp, "%d %s %s\n", &desc, name, host);
        if ( errorcheck < 0 )
            break;
        if (desc == -1)
            break;

        /* Write something, and check if it goes error-free */
        if (!write_to_descriptor
            (desc, "\n\rRestoring from copyover...\n\r", 0, NULL))
        {
#if defined(_WIN32)
            _close(desc);        /* nope */
#else
            close (desc);        /* nope */
#endif

            continue;
        }

        d = new_descriptor ();
        d->descriptor = desc;

        d->host = str_dup (host);
        d->next = descriptor_list;
        descriptor_list = d;
        d->connected = CON_COPYOVER_RECOVER;    /* -15, so close_socket frees the char */
        d->ansi = TRUE;

        /* Now, find the pfile */

        fOld = load_char_obj (d, name);

        if (!fOld)
        {                        /* Player file not found?! */
            write_to_descriptor (desc,
                                 "\n\rSomehow, your character was lost in the copyover. Sorry.\n\r",
                                 0, d);
            close_socket (d);
        }
        else
        {                        /* ok! */

            write_to_descriptor (desc, "\n\rCopyover recovery complete.\n\r",
                                 0, d);

            /* Just In Case */
            if (!d->character->in_room)
                d->character->in_room = get_room_index (ROOM_VNUM_TEMPLE);

            /* Insert in the char_list */
            d->character->next = char_list;
            char_list = d->character;

            char_to_room (d->character, d->character->in_room);
            do_look (d->character, "auto");
            act ("$n materializes!", d->character, NULL, NULL, TO_ROOM);
            d->connected = CON_PLAYING;

            if (d->character->pet != NULL)
            {
                char_to_room (d->character->pet, d->character->in_room);
                act ("$n materializes!.", d->character->pet, NULL, NULL,
                     TO_ROOM);
            }
        }

    }
    fclose (fp);

} // end void copyover_recover

// Rhien - 04/13/2015
// This will force a tick to happen which will be an immortal only command on the
// higher levels.  This will allow for testing of tick releated items or to allow
// an IMM to manipulate tick timing for whatever purpose they need it for.s
void do_forcetick(CHAR_DATA * ch, char *argument)
{
    // We're going to show a WIZNET message for this
    char buf[100];
    strcpy(buf, "$N forces a TICK ");

    update_handler(TRUE);

    send_to_char("You have forced a tick.\n\r", ch);
    wiznet(buf, ch, NULL, WIZ_TICKS, 0, 0);
    return;
}

/*
* do_rename renames a player to another name.
* PCs only. Previous file is deleted, if it exists.
* Char is then saved to new file.
* New name is checked against std. checks, existing offline players and
* online players.
* .gz files are checked for too, just in case.
*/

bool check_parse_name(char* name);  /* comm.c */

void do_rename(CHAR_DATA* ch, char* argument)
{
    char old_name[MAX_INPUT_LENGTH],
        new_name[MAX_INPUT_LENGTH],
        strsave[MAX_INPUT_LENGTH];

    CHAR_DATA* victim;
    FILE* file;

    argument = one_argument(argument, old_name); /* find new/old name */
    one_argument(argument, new_name);

    /* Trivial checks */
    if (!old_name[0])
    {
        send_to_char("Rename who?\n\r", ch);
        return;
    }

    victim = get_char_world(ch, old_name);

    if (!victim)
    {
        send_to_char("There is no such a person online.\n\r", ch);
        return;
    }

    if (IS_NPC(victim))
    {
        send_to_char("You cannot use Rename on NPCs.\n\r", ch);
        return;
    }

    /* allow rename self new_name,but otherwise only lower level */
    if ((victim != ch) && (get_trust(victim) >= get_trust(ch)))
    {
        send_to_char("You failed.\n\r", ch);
        return;
    }

    if (!victim->desc || (victim->desc->connected != CON_PLAYING))
    {
        send_to_char("This player has lost his link or is inside a pager or the like.\n\r", ch);
        return;
    }

    if (!new_name[0])
    {
        send_to_char("Rename to what new name?\n\r", ch);
        return;
    }

    if (!check_parse_name(new_name))
    {
        send_to_char("The new name is illegal.\n\r", ch);
        return;
    }

    /* First, check if there is a player named that off-line */
    sprintf(strsave, "%s%s", PLAYER_DIR, capitalize(new_name));

    fclose(fpReserve); /* close the reserve file */
    file = fopen(strsave, "r"); /* attempt to to open pfile */
    if (file)
    {
        send_to_char("A player with that name already exists!\n\r", ch);
        fclose(file);
        fpReserve = fopen(NULL_FILE, "r"); /* is this really necessary these days? */
        return;
    }
    fpReserve = fopen(NULL_FILE, "r");  /* reopen the extra file */

    sprintf(strsave, "%s%s.gz", PLAYER_DIR, capitalize(new_name));

    fclose(fpReserve); /* close the reserve file */
    file = fopen(strsave, "r"); /* attempt to to open pfile */
    if (file)
    {
        send_to_char("A player with that name already exists in a compressed file!\n\r", ch);
        fclose(file);
        fpReserve = fopen(NULL_FILE, "r");
        return;
    }
    fpReserve = fopen(NULL_FILE, "r");  /* reopen the extra file */

    if (get_char_world(ch, new_name)) /* check for playing level-1 non-saved */
    {
        send_to_char("A player with the name you specified already exists!\n\r", ch);
        return;
    }

    /* Save the filename of the old name */
    sprintf(strsave, "%s%s", PLAYER_DIR, capitalize(victim->name));

    /* Rename the character and save him to a new file */
    /* NOTE: Players who are level 1 do NOT get saved under a new name */
    free_string(victim->name);
    victim->name = str_dup(capitalize(new_name));

    save_char_obj(victim);

    /* unlink the old file */
#if !defined(_WIN32)
    unlink(strsave); /* unlink does return a value.. but we do not care */
#else
    _unlink(strsave);
#endif

    /* That's it! */
    send_to_char("Character renamed.\n\r", ch);

    victim->position = POS_STANDING; /* I am laaazy */
    act("$n has renamed you to $N!", ch, NULL, victim, TO_VICT);

} /* do_rename */

// VNUM Utilities from Erwin
// To have VLIST show more than vnum 0 - 32000, change the number below: 
#define MAX_SHOW_VNUM   320  /* show only 1 - 100*100 */
#define NUL '\0'
extern ROOM_INDEX_DATA * room_index_hash[MAX_KEY_HASH]; /* db.c */

/* opposite directions */
const sh_int opposite_dir[10] = { DIR_SOUTH, DIR_WEST, DIR_NORTH, DIR_EAST, DIR_DOWN, DIR_UP, DIR_SOUTHWEST, DIR_SOUTHEAST, DIR_NORTHWEST, DIR_NORTHEAST };

/* get the 'short' name of an area (e.g. MIDGAARD, MIRROR etc. */
/* assumes that the filename saved in the AREA_DATA struct is something like midgaard.are */
char * area_name(AREA_DATA *pArea)
{
    static char buffer[64]; /* short filename */
    char  *period;

    assert(pArea != NULL);

    strncpy(buffer, pArea->file_name, 64); /* copy the filename */
    period = strchr(buffer, '.'); /* find the period (midgaard.are) */
    if (period) /* if there was one */
        *period = '\0'; /* terminate the string there (midgaard) */

    return buffer;
}

typedef enum { exit_from, exit_to, exit_both } exit_status;

/* depending on status print > or < or <> between the 2 rooms */
void room_pair(ROOM_INDEX_DATA* left, ROOM_INDEX_DATA* right, exit_status ex, char *buffer)
{
    char *sExit;

    switch (ex)
    {
    default:
        sExit = "??"; break; /* invalid usage */
    case exit_from:
        sExit = "< "; break;
    case exit_to:
        sExit = " >"; break;
    case exit_both:
        sExit = "<>"; break;
    }

    sprintf(buffer, "%5d %-26.26s %s%5d %-26.26s(%-8.8s)\n\r",
        left->vnum, left->name,
        sExit,
        right->vnum, right->name,
        area_name(right->area)
        );
}

/* for every exit in 'room' which leads to or from pArea but NOT both, print it */
void checkexits(ROOM_INDEX_DATA *room, AREA_DATA *pArea, char* buffer)
{
    char buf[MAX_STRING_LENGTH];
    int i;
    EXIT_DATA *exit;
    ROOM_INDEX_DATA *to_room;

    strcpy(buffer, "");
    for (i = 0; i < MAX_DIR; i++)
    {
        exit = room->exit[i];
        if (!exit)
            continue;
        else
            to_room = exit->u1.to_room;

        if (to_room)  /* there is something on the other side */
        { 
            if ((room->area == pArea) && (to_room->area != pArea))
            { /* an exit from our area to another area */
                /* check first if it is a two-way exit */

                if (to_room->exit[opposite_dir[i]] &&
                    to_room->exit[opposite_dir[i]]->u1.to_room == room)
                    room_pair(room, to_room, exit_both, buf); /* <> */
                else
                    room_pair(room, to_room, exit_to, buf); /* > */

                strcat(buffer, buf);
            }
            else
            {
                if ((room->area != pArea) && (exit->u1.to_room->area == pArea))
                { /* an exit from another area to our area */

                    if (!
                        (to_room->exit[opposite_dir[i]] &&
                        to_room->exit[opposite_dir[i]]->u1.to_room == room)
                        )
                        /* two-way exits are handled in the other if */
                    {
                        room_pair(to_room, room, exit_from, buf);
                        strcat(buffer, buf);
                    }

                } /* if room->area */
            }
        }
    } /* for */

}

/* for now, no arguments, just list the current area */
void do_exlist(CHAR_DATA *ch, char * argument)
{
    AREA_DATA* pArea;
    ROOM_INDEX_DATA* room;
    int i;
    char buffer[MAX_STRING_LENGTH];

    pArea = ch->in_room->area; /* this is the area we want info on */
    for (i = 0; i < MAX_KEY_HASH; i++) /* room index hash table */
        for (room = room_index_hash[i]; room != NULL; room = room->next)
            /* run through all the rooms on the MUD */

        {
            checkexits(room, pArea, buffer);
            send_to_char(buffer, ch);
        }
}

/*
 * Rhien, 4/21/2015
 * Broadcasts a message to all connected descriptors regardless of game state and including
 * the login screen.
 */
void do_broadcast(CHAR_DATA * ch, char *argument)
{
    if (argument[0] == '\0')
    {
        send_to_char("You must enter a broadcast message.\n\r", ch);
        return;
    }

    DESCRIPTOR_DATA *d;
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "\n\r{C[{WBroadcast by %s{C] {W%s{x\n\r", ch->name, argument);

    for (d = descriptor_list; d != NULL; d = d->next)
    {
        send_to_desc(buf, d);
    }

}

/*
 * Rhien, 04/21/2015
 * This will determine the max vnum by looping through the areas (which will get us roughly within 100) and then
 * we will loop over all possible vnums from 0 to that value and print out the ranges of empty.
 */
void do_vnumgap(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  vnumgap room\n\r", ch);
        send_to_char("  vnumgap obj\n\r", ch);
        send_to_char("  vnumgap mob\n\r", ch);
        return;
    }

    // Find the max vnum that could be in any area.  The ceiling will be the same for mobs, rooms and objects.
    AREA_DATA *pArea;
    int vnumCeiling;
    vnumCeiling = 0;

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
        if (pArea->max_vnum > vnumCeiling) {
            vnumCeiling = pArea->max_vnum;
        }
    }

    char buf[MAX_STRING_LENGTH];
    int startVnum, endVnum, lastFoundVnum;

    startVnum = 0;
    endVnum = 0;
    lastFoundVnum = 0;

    if (!str_cmp(arg, "room"))
    {
        ROOM_INDEX_DATA *room;

        // All VNUMs possible
        for (startVnum = 0; startVnum < vnumCeiling; startVnum++)
        {
            room = get_room_index(startVnum);

            if (room == NULL) {
                int x;

                // find out where the end of this range is, then advance to that position
                for (x = startVnum; x < 32767; x++)
                {
                    room = get_room_index(x);

                    if (room != NULL) {
                        endVnum = x - 1; // The last vnum that was used.
                        sprintf(buf, "Open VNUM Range: %d-%d\n\r", startVnum, endVnum);
                        write_to_descriptor(ch->desc->descriptor, buf, 0, ch->desc);

                        // Advance the position
                        startVnum = endVnum;
                        break;
                    }

                }

            }
            else
            {
                lastFoundVnum = startVnum;
            }

        }

        // And the last one...
        sprintf(buf, "Open VNUM Range: %d-%d\n\r", lastFoundVnum, vnumCeiling);
        write_to_descriptor(ch->desc->descriptor, buf, 0, ch->desc);

        return;
    }
    else if (!str_cmp(arg, "obj")) 
    {
        OBJ_INDEX_DATA *obj;

        // All VNUMs possible
        for (startVnum = 0; startVnum < vnumCeiling; startVnum++)
        {
            obj = get_obj_index(startVnum);

            if (obj == NULL) {
                int x;

                // find out where the end of this range is, then advance to that position
                for (x = startVnum; x < vnumCeiling; x++)
                {
                    obj = get_obj_index(x);

                    if (obj != NULL) {
                        endVnum = x - 1; // The last vnum that was used.
                        sprintf(buf, "Open VNUM Range: %d-%d\n\r", startVnum, endVnum);
                        write_to_descriptor(ch->desc->descriptor, buf, 0, ch->desc);

                        // Advance the position
                        startVnum = endVnum;
                        break;
                    }

                }

            }
            else
            {
                lastFoundVnum = startVnum;
            }

        }

        // And the last one...
        sprintf(buf, "Open VNUM Range: %d-%d\n\r", lastFoundVnum, vnumCeiling);
        write_to_descriptor(ch->desc->descriptor, buf, 0, ch->desc);
    }
    else if (!str_cmp(arg, "mob"))
    {
        MOB_INDEX_DATA *mob;

        // All VNUMs possible
        for (startVnum = 0; startVnum < vnumCeiling; startVnum++)
        {
            mob = get_mob_index(startVnum);

            if (mob == NULL) {
                int x;

                // find out where the end of this range is, then advance to that position
                for (x = startVnum; x < vnumCeiling; x++)
                {
                    mob = get_mob_index(x);

                    if (mob != NULL) {
                        endVnum = x - 1; // The last vnum that was used.
                        sprintf(buf, "Open VNUM Range: %d-%d\n\r", startVnum, endVnum);
                        write_to_descriptor(ch->desc->descriptor, buf, 0, ch->desc);

                        // Advance the position
                        startVnum = endVnum;
                        break;
                    }

                }

            }
            else
            {
                lastFoundVnum = startVnum;
            }

        }

        // And the last one...
        sprintf(buf, "Open VNUM Range: %d-%d\n\r", lastFoundVnum, vnumCeiling);
        write_to_descriptor(ch->desc->descriptor, buf, 0, ch->desc);
    }
    else
    {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  vnumgap room\n\r", ch);
        send_to_char("  vnumgap obj\n\r", ch);
        send_to_char("  vnumgap mob\n\r", ch);
        return;
    }

} // end do_vnumgap

/*
 * Function to force the immortal to type out slay.
 */
void do_sla (CHAR_DATA * ch, char *argument)
{
    send_to_char ("If you want to SLAY, spell it out.\n\r", ch);
    return;
} // end do_sla

/*
 * Immortal command to slay an NPC or player.
 */
void do_slay (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;
    char arg[MAX_INPUT_LENGTH];

    one_argument (argument, arg);
    if (arg[0] == '\0')
    {
        send_to_char ("Slay whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_room (ch, arg)) == NULL)
    {
        send_to_char ("They aren't here.\n\r", ch);
        return;
    }

    if (ch == victim)
    {
        send_to_char ("Suicide is a mortal sin.\n\r", ch);
        return;
    }

    if (!IS_NPC (victim) && victim->level >= get_trust (ch))
    {
        send_to_char ("You failed.\n\r", ch);
        return;
    }

    act ("{1You slay $M in cold blood!{x", ch, NULL, victim, TO_CHAR);
    act ("{1$n slays you in cold blood!{x", ch, NULL, victim, TO_VICT);
    act ("{1$n slays $N in cold blood!{x", ch, NULL, victim, TO_NOTVICT);
    raw_kill (victim);
    return;
}

/*
 * This is an immortal command will toggle whether the current
 * user is a tester.  If an immortal needs to set someone as a tester
 * they can use the set command to do that.
 */
void do_test(CHAR_DATA * ch, char *argument)
{
    if (IS_SET(ch->act, PLR_TESTER))
    {
        REMOVE_BIT (ch->act, PLR_TESTER);
        send_to_char ("({WTester{x) flag removed.\n\r", ch);
    }
    else
    {
        SET_BIT (ch->act, PLR_TESTER);
        send_to_char ("({WTester{x) flag added.\n\r", ch);
    }

} // end do_test

/*
 * Immortal command to remove all affects from a character or NPC.
 */
void do_wizcancel(CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;
    char buf[MAX_STRING_LENGTH];

    // This gets rid of a compiler warning about unitialized variables even
    // the logic won't get past the else if statement without victim having
    // something.
    victim = NULL;

    if (argument[0] == '\0')
    {
        victim = ch;
    }
    else if (!victim)
    {
        // If the victim is null that means they entered something.. try
        // to find them.
        if ((victim = get_char_world(ch, argument)) == NULL)
        {
            send_to_char ("They aren't here.\n\r", ch);
            return;
        }
    }

    // This will remove all affects from the victim
    affect_strip_all(victim);

    // Show the user what was done.
    if (ch != victim)
    {
        sprintf(buf, "%s has removed all affects from you.\n\r", ch->name);
        send_to_char(buf, victim);

        sprintf(buf, "%s has had all of their affects removed.\n\r", victim->name);
        send_to_char(buf, ch);
    }
    else
    {
        send_to_char("You have removed all affects from yourself.\n\r", ch);
        return;
    }

    return;
} // end do_wizcancel

/*
 * An immortal command (initial as an implementor command) that will grant an
 * immortal blessing onto a player (or all players).  This should be used as
 * a thank you to active players.  It's not necessarily over powered although
 * it's doing a lot.  It also cannot be cancelled or dispelled.
 */
void do_wizbless(CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;

    if (argument[0] == '\0')
    {
        send_to_char ("Syntax: wizbless <character|all>\n\r", ch);
        return;
    }

    if (!str_cmp (argument, "all"))
    {
        for (victim = char_list; victim != NULL; victim = victim->next)
        {
            if (!IS_NPC(victim))
            {
                wizbless(victim);
                act("$n has granted you an immortal blessing.", ch, NULL, victim, TO_VICT);
            }
        }

        send_to_char("All players have been granted an immortal blessing.\n\r", ch);
        return;
    }

    if ((victim = get_char_world (ch, argument)) == NULL)
    {
        send_to_char ("They aren't playing.\n\r", ch);
        return;
    }

    if (IS_NPC(victim))
    {
        send_to_char("Not on NPC's\n\r", ch);
        return;
    }

    wizbless(victim);
    send_to_char("They have been granted an immortal blessing.\n\r", ch);
    act("$n has granted you an immortal blessing.", ch, NULL, victim, TO_VICT);

} // end do_bless

/*
 * The adding for wizbless has been moved into this function so it can be called
 * from the loop for all in the do function above.
 */
void wizbless(CHAR_DATA * victim)
{
    if (is_affected(victim, gsn_immortal_blessing))
    {
        affect_strip(victim, gsn_immortal_blessing);
    }

    AFFECT_DATA af;

    af.where = TO_AFFECTS;
    af.type = gsn_immortal_blessing;
    af.level = ML;
    af.duration = 120;
    af.location = APPLY_HITROLL;
    af.modifier = 4;
    af.bitvector = 0;
    affect_to_char (victim, &af);

    af.location = APPLY_DAMROLL;
    af.modifier = 4;
    affect_to_char (victim, &af);

    af.location = APPLY_SAVING_SPELL;
    af.modifier = -5;
    affect_to_char (victim, &af);

    af.modifier = -40;
    af.location = APPLY_AC;
    affect_to_char (victim, &af);

    af.location = APPLY_DEX;
    af.modifier = 2;
    af.bitvector = AFF_HASTE;
    affect_to_char (victim, &af);

    af.location = APPLY_CON;
    af.modifier = 3;
    af.bitvector = 0;
    affect_to_char (victim, &af);

    af.location = APPLY_WIS;
    af.modifier = 3;
    af.bitvector = 0;
    affect_to_char (victim, &af);

    af.location = APPLY_INT;
    af.modifier = 3;
    af.bitvector = 0;
    affect_to_char (victim, &af);

    af.location = APPLY_STR;
    af.modifier = 3;
    af.bitvector = 0;
    affect_to_char (victim, &af);

} // end void wizbless

/*
 * Debug function to quickly test code without having to wire something up.
 */
void do_debug(CHAR_DATA * ch, char *argument)
{
    /*CHAR_DATA * rch;
    char buf[MSL];

    for (rch = ch->in_room->people; rch; rch = rch->next_in_room)
    {
        sprintf(buf, "%s is in the room.\n\r", rch->name);
        send_to_char(buf, ch);
    }*/

    send_to_char("Nothing here currently, move along.\r\n", ch);
    return;

} // end do_debug


