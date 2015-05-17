/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik Strfeldt, Tom Madsen, and Katja Nyboe.    *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Thanks to abaddon for proof-reading our comm.c and pointing out bugs.  *
 *  Any remaining bugs are, of course, our work, not his.  :)              *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*    ROM 2.4 is copyright 1993-1998 Russ Taylor                             *
*    ROM has been brought to you by the ROM consortium                      *
*        Russ Taylor (rtaylor@hypercube.org)                                *
*        Gabrielle Taylor (gtaylor@hypercube.org)                           *
*        Brian Moore (zump@rom.org)                                         *
*    By using this code, you have agreed to follow the terms of the         *
*    ROM license, in the file Rom24/doc/rom.license                         *
****************************************************************************/

/*
 * This file contains all of the OS-dependent stuff:
 *   startup, signals, BSD sockets for tcp/ip, i/o, timing.
 *
 * The data flow for input is:
 *    Game_loop ---> Read_from_descriptor ---> Read
 *    Game_loop ---> Read_from_buffer
 *
 * The data flow for output is:
 *    Game_loop ---> Process_Output ---> Write_to_descriptor -> Write
 *
 * The OS-dependent functions are Read_from_descriptor and Write_to_descriptor.
 * -- Furey  26 Jan 1993
 */

// System Specific Includes
#if defined(__APPLE__)
    #include <types.h>
    #include <time.h>
    #include <unistd.h>                /* OLC -- for close read write etc */
#elif defined(_WIN32)
    #include <sys/types.h>
    #include <time.h>
    #include <io.h>
    //#include <winsock.h>
    #include <WinSock2.h>
    #include "telnet.h"
    #include <sys/timeb.h> 
#else
    #include <sys/types.h>
    #include <sys/time.h>
    #include <unistd.h>                /* OLC -- for close read write etc */
    #include <time.h>
#endif

// General Includes
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>                /* printf_to_char */
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"

/*
 * Malloc debugging stuff.
 */

#if defined(MALLOC_DEBUG)
    #include <malloc.h>
    extern int malloc_debug args ((int));
    extern int malloc_verify args ((void));
#endif

#if defined(unix) || defined(_WIN32)
    #include <signal.h>
#endif

/*
 * Socket and TCP/IP stuff.
 */
#if defined(_WIN32) || defined (__APPLE__)
    const	char	echo_off_str[] = { IAC, WILL, TELOPT_ECHO, '\0' };
    const	char	echo_on_str[] = { IAC, WONT, TELOPT_ECHO, '\0' };
    const	char 	go_ahead_str	[] = { IAC, GA, '\0' };
#endif

#if    defined(unix)
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include "telnet.h"
    const char echo_off_str[] = { IAC, WILL, TELOPT_ECHO, '\0' };
    const char echo_on_str[] = { IAC, WONT, TELOPT_ECHO, '\0' };
    const char go_ahead_str[] = { IAC, GA, '\0' };
#endif


/*
 * OS-dependent declarations.
 */
#if    defined(interactive)
    #include <net/errno.h>
    #include <sys/fnctl.h>
#endif

#if    defined(linux)
    /* 
        Linux shouldn't need these. If you have a problem compiling, try
        uncommenting these functions.
    */
    /*
    int    accept        args( ( int s, struct sockaddr *addr, int *addrlen ) );
    int    bind        args( ( int s, struct sockaddr *name, int namelen ) );
    int    getpeername    args( ( int s, struct sockaddr *name, int *namelen ) );
    int    getsockname    args( ( int s, struct sockaddr *name, int *namelen ) );
    int    listen        args( ( int s, int backlog ) );
    */

    int close args ((int fd));
    int gettimeofday args ((struct timeval * tp, struct timezone * tzp));
    /* int    read        args( ( int fd, char *buf, int nbyte ) ); */
    int select args ((int width, fd_set * readfds, fd_set * writefds,
                      fd_set * exceptfds, struct timeval * timeout));
    int socket args ((int domain, int type, int protocol));
    /* int    write        args( ( int fd, char *buf, int nbyte ) ); *//* read,write in unistd.h */
#endif

#if    defined(__APPLE__)
    #include <console.h>
    #include <fcntl.h>
    #include <unix.h>
    struct timeval {
        time_t tv_sec;
        time_t tv_usec;
    };
    #if    !defined(isascii)
    #define    isascii(c)        ( (c) < 0200 )
    #endif
    static long theKeys[4];

    int gettimeofday args ((struct timeval * tp, void *tzp));
#endif

#if defined(_WIN32)
    void gettimeofday args((struct timeval *tp, void *tzp));
#endif

/*
 * Global variables.
 */
DESCRIPTOR_DATA *descriptor_list;    /* All open descriptors     */
DESCRIPTOR_DATA *d_next;        /* Next descriptor in loop  */
FILE *fpReserve;                /* Reserved file handle     */
bool merc_down;                    /* Shutdown         */
bool wizlock;                    /* Game is wizlocked        */
bool newlock;                    /* Game is newlocked        */
char str_boot_time[MAX_INPUT_LENGTH];
time_t current_time;            /* time of this pulse */
bool MOBtrigger = TRUE;            /* act() switch                 */


/*
 * OS-dependent local functions.
 */
#if defined(unix) || defined(_WIN32) || defined (__APPLE__)
    void game_loop args ((int control));
    int init_socket args ((int port));
    void init_descriptor args ((int control));
    bool read_from_descriptor args ((DESCRIPTOR_DATA * d));
    bool write_to_descriptor args ((int desc, char *txt, int length));
#endif

/*
 * Other local functions (OS-independent).
 */
bool check_parse_name args ((char *name));
bool check_reconnect args ((DESCRIPTOR_DATA * d, char *name, bool fConn));
bool check_playing args ((DESCRIPTOR_DATA * d, char *name));
int main args ((int argc, char **argv));
void nanny args ((DESCRIPTOR_DATA * d, char *argument));
bool process_output args ((DESCRIPTOR_DATA * d, bool fPrompt));
void read_from_buffer args ((DESCRIPTOR_DATA * d));
void stop_idling args ((CHAR_DATA * ch));
void bust_a_prompt args ((CHAR_DATA * ch));

/* Needs to be global because of do_copyover */
int port, control;

/* Put global mud config values here. Look at qmconfig command for clues.     */
/*   -- JR 09/23/2000                                                         */
/* Set values for all but IP address in ../area/config.txt file.             */
/*   -- JR 05/10/2001                                                         */
int mud_ansiprompt, mud_ansicolor, mud_telnetga;

/* Set this to the IP address you want to listen on (127.0.0.1 is good for    */
/* paranoid types who don't want the 'net at large peeking at their MUD)      */
char *mud_ipaddress = "0.0.0.0";

int main (int argc, char **argv)
{
    struct timeval now_time;
    bool fCopyOver = FALSE;

    /*
     * Memory debugging if needed.
     */
#if defined(MALLOC_DEBUG)
    malloc_debug (2);
#endif

    /*
     * Init time.
     */
    gettimeofday (&now_time, NULL);
    current_time = (time_t) now_time.tv_sec;
    strcpy (str_boot_time, ctime (&current_time));

    /*
     * Macintosh console initialization.
     */
#if defined(__APPLE__)
    console_options.nrows = 31;
    cshow (stdout);
    csetmode (C_RAW, stdin);
    cecho2file ("log file", 1, stderr);
#endif

    /*
     * Reserve one channel for our use.
     */
    if ((fpReserve = fopen (NULL_FILE, "r")) == NULL)
    {
        perror (NULL_FILE);
        exit (1);
    }

    /*
     * Get the port number.
     */
    port = 4000;
    if (argc > 1)
    {
        if (!is_number (argv[1]))
        {
            fprintf (stderr, "Usage: %s [port #]\n", argv[0]);
            exit (1);
        }
        else if ((port = atoi (argv[1])) <= 1024)
        {
            fprintf (stderr, "Port number must be above 1024.\n");
            exit (1);
        }

        /* Are we recovering from a copyover? */
        if (argv[2] && argv[2][0])
        {
            fCopyOver = TRUE;
            control = atoi (argv[3]);
        }
        else
            fCopyOver = FALSE;

    }

    /*
     * Run the game.
     */
#if defined(unix) || defined(_WIN32) || defined(__APPLE__)

    qmconfig_read(); /* Here so we can set the IP adress. -- JR 05/06/01 */
    if (!fCopyOver)
        control = init_socket (port);

    boot_db ();
    log_f ("Crimson Skies is ready to rock on port %d (%s).", port, mud_ipaddress);

    if (fCopyOver)
        copyover_recover ();

    game_loop (control);

#if defined(_WIN32)
    closesocket(control);
    WSACleanup();
#else
    close(control);
#endif

#endif

    /*
     * That's all, folks.
     */
    log_string ("Normal termination of game.");
    exit (0);
    return 0;
}



#if defined(unix) || defined(_WIN32) || defined(__APPLE__)
int init_socket (int port)
{
    static struct sockaddr_in sa_zero;
    struct sockaddr_in sa;
    int x = 1;
    int fd;

#if !defined(_WIN32)
    if ((fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror ("Init_socket: socket");
        exit (1);
    }
#else
    WORD    wVersionRequested = MAKEWORD(1, 1);
    WSADATA wsaData;
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        perror("No useable WINSOCK.DLL");
        exit(1);
    }

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Init_socket: socket");
        exit(1);
    }
#endif

    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
                    (char *) &x, sizeof (x)) < 0)
    {
        perror ("Init_socket: SO_REUSEADDR");
        
#if defined(_WIN32)
        closesocket(fd);
#else
        close (fd);
#endif

        exit (1);
    }

#if defined(SO_DONTLINGER) && !defined(SYSV)
    {
        struct linger ld;

        ld.l_onoff = 1;
        ld.l_linger = 1000;

        if (setsockopt (fd, SOL_SOCKET, SO_DONTLINGER,
                        (char *) &ld, sizeof (ld)) < 0)
        {
            perror ("Init_socket: SO_DONTLINGER");

#if defined(_WIN32)
            closesocket(fd);
#else
            close (fd);
#endif

            exit (1);
        }
    }
#endif

    sa = sa_zero;

#if !defined(_WIN32)
    sa.sin_family = AF_INET;
#else
    sa.sin_family = PF_INET;
#endif

    sa.sin_port = htons (port);
    sa.sin_addr.s_addr = inet_addr( mud_ipaddress );
    log_f("Set IP address to %s", mud_ipaddress);

    if (bind (fd, (struct sockaddr *) &sa, sizeof (sa)) < 0)
    {
        perror ("Init socket: bind");

#if defined(_WIN32)
        closesocket(fd);
#else
        close(fd);
#endif

        exit (1);
    }


    if (listen (fd, 3) < 0)
    {
        perror ("Init socket: listen");
        
#if defined(_WIN32)
        closesocket(fd);
#else
        close(fd);
#endif

        exit (1);
    }

    return fd;
}
#endif


#if defined(unix) || defined(_WIN32) || defined(__APPLE__)
void game_loop (int control)
{
    static struct timeval null_time;
    struct timeval last_time;

#if !defined(_WIN32)
    signal (SIGPIPE, SIG_IGN);
#endif
    
    gettimeofday (&last_time, NULL);
    current_time = (time_t) last_time.tv_sec;

    /* Main loop */
    while (!merc_down)
    {
        fd_set in_set;
        fd_set out_set;
        fd_set exc_set;
        DESCRIPTOR_DATA *d;
        int maxdesc;

#if defined(MALLOC_DEBUG)
        if (malloc_verify () != 1)
            abort ();
#endif

        /*
         * Poll all active descriptors.
         */
        FD_ZERO (&in_set);
        FD_ZERO (&out_set);
        FD_ZERO (&exc_set);
        FD_SET (control, &in_set);
        maxdesc = control;
        for (d = descriptor_list; d; d = d->next)
        {
            maxdesc = UMAX (maxdesc, d->descriptor);
            FD_SET (d->descriptor, &in_set);
            FD_SET (d->descriptor, &out_set);
            FD_SET (d->descriptor, &exc_set);
        }

        if (select (maxdesc + 1, &in_set, &out_set, &exc_set, &null_time) < 0)
        {
            perror ("Game_loop: select: poll");
            exit (1);
        }

        /*
         * New connection?
         */
        if (FD_ISSET (control, &in_set))
            init_descriptor (control);

        /*
         * Kick out the freaky folks.
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;
            if (FD_ISSET (d->descriptor, &exc_set))
            {
                FD_CLR (d->descriptor, &in_set);
                FD_CLR (d->descriptor, &out_set);
                if (d->character && d->connected == CON_PLAYING)
                    save_char_obj (d->character);
                d->outtop = 0;
                close_socket (d);
            }
        }

        /*
         * Process input.
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;
            d->fcommand = FALSE;

            if (FD_ISSET (d->descriptor, &in_set))
            {
                if (d->character != NULL)
                    d->character->timer = 0;
                if (!read_from_descriptor (d))
                {
                    FD_CLR (d->descriptor, &out_set);
                    if (d->character != NULL && d->connected == CON_PLAYING)
                        save_char_obj (d->character);
                    d->outtop = 0;
                    close_socket (d);
                    continue;
                }
            }

            if (d->character != NULL && d->character->daze > 0)
                --d->character->daze;

            if (d->character != NULL && d->character->wait > 0)
            {
                --d->character->wait;
                continue;
            }

            read_from_buffer (d);
            if (d->incomm[0] != '\0')
            {
                d->fcommand = TRUE;
                stop_idling (d->character);

                /* OLC */
                if (d->showstr_point)
                    show_string (d, d->incomm);
                else if (d->pString)
                    string_add (d->character, d->incomm);
                else
                    switch (d->connected)
                    {
                        case CON_PLAYING:
                            if (!run_olc_editor (d))
                                substitute_alias (d, d->incomm);
                            break;
                        default:
                            nanny (d, d->incomm);
                            break;
                    }

                d->incomm[0] = '\0';
            }
        }

        /*
         * Autonomous game motion.
         */
        update_handler (FALSE);

        /*
         * Output.
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;

            if ((d->fcommand || d->outtop > 0)
                && FD_ISSET (d->descriptor, &out_set))
            {
                if (!process_output (d, TRUE))
                {
                    if (d->character != NULL && d->connected == CON_PLAYING)
                        save_char_obj (d->character);
                    d->outtop = 0;
                    close_socket (d);
                }
            }
        }

        /*
         * Synchronize to a clock.
         * Sleep( last_time + 1/PULSE_PER_SECOND - now ).
         * Careful here of signed versus unsigned arithmetic.
         */
#if !defined(_WIN32)
        {
            struct timeval now_time;
            long secDelta;
            long usecDelta;

            gettimeofday (&now_time, NULL);
            usecDelta = ((int) last_time.tv_usec) - ((int) now_time.tv_usec)
                + 1000000 / PULSE_PER_SECOND;
            secDelta = ((int) last_time.tv_sec) - ((int) now_time.tv_sec);
            while (usecDelta < 0)
            {
                usecDelta += 1000000;
                secDelta -= 1;
            }

            while (usecDelta >= 1000000)
            {
                usecDelta -= 1000000;
                secDelta += 1;
            }

            if (secDelta > 0 || (secDelta == 0 && usecDelta > 0))
            {
                struct timeval stall_time;

                stall_time.tv_usec = usecDelta;
                stall_time.tv_sec = secDelta;
                if (select (0, NULL, NULL, NULL, &stall_time) < 0)
                {
                    perror ("Game_loop: select: stall");
                    exit (1);
                }
            }
        }
#else
    {
        int times_up;
        int nappy_time;
        struct _timeb start_time;
        struct _timeb end_time;
        _ftime(&start_time);
        times_up = 0;

        while (times_up == 0)
        {
            _ftime(&end_time);
            if ((nappy_time =
                (int)(1000 *
                (double)((end_time.time - start_time.time) +
                ((double)(end_time.millitm -
                start_time.millitm) /
                1000.0)))) >=
                (double)(1000 / PULSE_PER_SECOND))
                times_up = 1;
            else
            {
                Sleep((int)((double)(1000 / PULSE_PER_SECOND) -
                    (double)nappy_time));
                times_up = 1;
            }
        }
    }

#endif
        gettimeofday (&last_time, NULL);
        current_time = (time_t) last_time.tv_sec;
    }

    return;
}
#endif



#if defined(unix) || defined(_WIN32) || defined(__APPLE__)

void init_descriptor (int control)
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *dnew;
    struct sockaddr_in sock;
    struct hostent *from;
    
#if defined(_WIN32)
    int size;
    int desc;
    size = sizeof(sock);
#else
    size_t desc;
    socklen_t size;
#endif

    getsockname (control, (struct sockaddr *) &sock, &size);
    if ((desc = accept (control, (struct sockaddr *) &sock, &size)) < 0)
    {
        perror ("New_descriptor: accept");
        return;
    }

#if !defined(FNDELAY)
#define FNDELAY O_NDELAY
#endif

#if !defined(_WIN32)
    if (fcntl (desc, F_SETFL, FNDELAY) == -1)
    {
        perror ("New_descriptor: fcntl: FNDELAY");
        return;
    }
#endif

    /*
     * Cons a new descriptor.
     */
    dnew = new_descriptor ();

    dnew->descriptor = desc;
    if (!mud_ansiprompt)
        dnew->connected = CON_GET_NAME;
    else
        dnew->connected = CON_ANSI;
    dnew->ansi = mud_ansicolor;
    dnew->showstr_head = NULL;
    dnew->showstr_point = NULL;
    dnew->outsize = 2000;
    dnew->pEdit = NULL;            /* OLC */
    dnew->pString = NULL;        /* OLC */
    dnew->editor = 0;            /* OLC */
    dnew->outbuf = alloc_mem (dnew->outsize);

    size = sizeof (sock);
    if (getpeername (desc, (struct sockaddr *) &sock, &size) < 0)
    {
        perror ("New_descriptor: getpeername");
        dnew->host = str_dup ("(unknown)");
    }
    else
    {
        /*
         * Would be nice to use inet_ntoa here but it takes a struct arg,
         * which ain't very compatible between gcc and system libraries.
         */
        int addr;

        addr = ntohl (sock.sin_addr.s_addr);
        sprintf (buf, "%d.%d.%d.%d",
                 (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
                 (addr >> 8) & 0xFF, (addr) & 0xFF);
        sprintf (log_buf, "Sock.sinaddr:  %s", buf);
        log_string (log_buf);

#if !defined(_WIN32)
        from = gethostbyaddr ((char *) &sock.sin_addr,
                              sizeof (sock.sin_addr), AF_INET);
#else
        from = gethostbyaddr((char *)&sock.sin_addr,
            sizeof(sock.sin_addr), PF_INET);
#endif

        dnew->host = str_dup (from ? from->h_name : buf);
    }

    /*
     * Swiftest: I added the following to ban sites.  I don't
     * endorse banning of sites, but Copper has few descriptors now
     * and some people from certain sites keep abusing access by
     * using automated 'autodialers' and leaving connections hanging.
     *
     * Furey: added suffix check by request of Nickel of HiddenWorlds.
     */
    if (check_ban (dnew->host, BAN_ALL))
    {
        write_to_descriptor (desc,
                             "Your site has been banned from this mud.\n\r",
                             0);

#if defined(_WIN32)
        closesocket(desc);
#else
        close (desc);
#endif

        free_descriptor (dnew);

        return;
    }
    /*
     * Init descriptor data.
     */
    dnew->next = descriptor_list;
    descriptor_list = dnew;

    /*
     * First Contact!
     */
    if (!mud_ansiprompt)
    {
        extern char * help_greeting;
        if (help_greeting[0] == '.')
            send_to_desc(help_greeting + 1, dnew);
        else
            send_to_desc(help_greeting, dnew);
    }
    else
        write_to_descriptor(desc, "\n\rDo you want color? (Y/N) -> ", 0);

    return;
}
#endif



void close_socket (DESCRIPTOR_DATA * dclose)
{
    CHAR_DATA *ch;

    if (dclose->outtop > 0)
        process_output (dclose, FALSE);

    if (dclose->snoop_by != NULL)
    {
        write_to_buffer (dclose->snoop_by,
                         "Your victim has left the game.\n\r", 0);
    }

    {
        DESCRIPTOR_DATA *d;

        for (d = descriptor_list; d != NULL; d = d->next)
        {
            if (d->snoop_by == dclose)
                d->snoop_by = NULL;
        }
    }

    if ((ch = dclose->character) != NULL)
    {
        sprintf (log_buf, "Closing link to %s.", ch->name);
        log_string (log_buf);
        /* cut down on wiznet spam when rebooting */
        /* If ch is writing note or playing, just lose link otherwise clear char */
        if ((dclose->connected == CON_PLAYING && !merc_down)
                || ((dclose->connected >= CON_NOTE_TO)
                        && (dclose->connected <= CON_NOTE_FINISH)))
        {
            act ("$n has lost $s link.", ch, NULL, NULL, TO_ROOM);
            wiznet ("Net death has claimed $N.", ch, NULL, WIZ_LINKS, 0, 0);
            ch->desc = NULL;
        }
        else
        {
            free_char (dclose->original ? dclose->original :
                       dclose->character);
        }
    }

    if (d_next == dclose)
        d_next = d_next->next;

    if (dclose == descriptor_list)
    {
        descriptor_list = descriptor_list->next;
    }
    else
    {
        DESCRIPTOR_DATA *d;

        for (d = descriptor_list; d && d->next != dclose; d = d->next);
        if (d != NULL)
            d->next = dclose->next;
        else
            bug ("Close_socket: dclose not found.", 0);
    }

#if defined(_WIN32)
    closesocket(dclose->descriptor);
#else
    close (dclose->descriptor);
#endif

    free_descriptor (dclose);
#if defined(__APPLE__)
    exit (1);
#endif
    return;
}



bool read_from_descriptor (DESCRIPTOR_DATA * d)
{
    int iStart;

    /* Hold horses if pending command already. */
    if (d->incomm[0] != '\0')
        return TRUE;

    /* Check for overflow. */
    iStart = strlen (d->inbuf);
    if (iStart >= sizeof (d->inbuf) - 10)
    {
        sprintf (log_buf, "%s input overflow!", d->host);
        log_string (log_buf);
        write_to_descriptor (d->descriptor,
                             "\n\r*** PUT A LID ON IT!!! ***\n\r", 0);
        return FALSE;
    }

    /* Snarf input. */
#if defined(__APPLE__)
    for (;;)
    {
        int c;
        c = getc (stdin);
        if (c == '\0' || c == EOF)
            break;
        putc (c, stdout);
        if (c == '\r')
            putc ('\n', stdout);
        d->inbuf[iStart++] = c;
        if (iStart > sizeof (d->inbuf) - 10)
            break;
    }
#endif

#if defined(unix) || defined(_WIN32)
    for (;;)
    {
        int nRead;

#if !defined(_WIN32)
        nRead = read (d->descriptor, d->inbuf + iStart, sizeof (d->inbuf) - 10 - iStart);
#else
        nRead = recv(d->descriptor, d->inbuf + iStart, sizeof(d->inbuf) - 10 - iStart, 0);
#endif

        if (nRead > 0)
        {
            iStart += nRead;
            if (d->inbuf[iStart - 1] == '\n' || d->inbuf[iStart - 1] == '\r')
                break;
        }
        else if (nRead == 0)
        {
            log_string ("EOF encountered on read.");
            return FALSE;
        }
#if defined( WIN32 )
        else if (WSAGetLastError() == WSAEWOULDBLOCK || errno == EAGAIN)
            break;
#endif
        else if (errno == EWOULDBLOCK)
            break;
        else
        {
            perror ("Read_from_descriptor");
            return FALSE;
        }
    }
#endif

    d->inbuf[iStart] = '\0';
    return TRUE;
}



/*
 * Transfer one line from input buffer to input line.
 */
void read_from_buffer (DESCRIPTOR_DATA * d)
{
    int i, j, k;

    /*
     * Hold horses if pending command already.
     */
    if (d->incomm[0] != '\0')
        return;

    /*
     * Look for at least one new line.
     */
    for (i = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++)
    {
        if (d->inbuf[i] == '\0')
            return;
    }

    /*
     * Canonical input processing.
     */
    for (i = 0, k = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++)
    {
        if (k >= MAX_INPUT_LENGTH - 2)
        {
            write_to_descriptor (d->descriptor, "Line too long.\n\r", 0);

            /* skip the rest of the line */
            for (; d->inbuf[i] != '\0'; i++)
            {
                if (d->inbuf[i] == '\n' || d->inbuf[i] == '\r')
                    break;
            }
            d->inbuf[i] = '\n';
            d->inbuf[i + 1] = '\0';
            break;
        }

        if (d->inbuf[i] == '\b' && k > 0)
            --k;
        else if (isascii (d->inbuf[i]) && isprint (d->inbuf[i]))
            d->incomm[k++] = d->inbuf[i];
    }

    /*
     * Finish off the line.
     */
    if (k == 0)
        d->incomm[k++] = ' ';
    d->incomm[k] = '\0';

    /*
     * Deal with bozos with #repeat 1000 ...
     */

    if (k > 1 || d->incomm[0] == '!')
    {
        if (d->incomm[0] != '!' && strcmp (d->incomm, d->inlast))
        {
            d->repeat = 0;
        }
        else
        {
            if (++d->repeat >= 25 && d->character
                && d->connected == CON_PLAYING)
            {
                sprintf (log_buf, "%s input spamming!", d->host);
                log_string (log_buf);
                wiznet ("Spam spam spam $N spam spam spam spam spam!",
                        d->character, NULL, WIZ_SPAM, 0,
                        get_trust (d->character));
                if (d->incomm[0] == '!')
                    wiznet (d->inlast, d->character, NULL, WIZ_SPAM, 0,
                            get_trust (d->character));
                else
                    wiznet (d->incomm, d->character, NULL, WIZ_SPAM, 0,
                            get_trust (d->character));

                d->repeat = 0;
/*
        write_to_descriptor( d->descriptor,
            "\n\r*** PUT A LID ON IT!!! ***\n\r", 0 );
        strcpy( d->incomm, "quit" );
*/
            }
        }
    }


    /*
     * Do '!' substitution.
     */
    if (d->incomm[0] == '!')
        strcpy (d->incomm, d->inlast);
    else
        strcpy (d->inlast, d->incomm);

    /*
     * Shift the input buffer.
     */
    while (d->inbuf[i] == '\n' || d->inbuf[i] == '\r')
        i++;
    for (j = 0; (d->inbuf[j] = d->inbuf[i + j]) != '\0'; j++);
    return;
}



/*
 * Low level output function.
 */
bool process_output (DESCRIPTOR_DATA * d, bool fPrompt)
{
    extern bool merc_down;

    /*
     * Bust a prompt.
     */
    if (!merc_down)
    {
        if (d->showstr_point)
            write_to_buffer (d, "[Hit Return to continue]\n\r", 0);
        else if (fPrompt && d->pString && d->connected == CON_PLAYING)
            write_to_buffer (d, "> ", 2);
        else if (fPrompt && d->connected == CON_PLAYING)
        {
            CHAR_DATA *ch;
            CHAR_DATA *victim;

            ch = d->character;

            /* battle prompt */
            if ((victim = ch->fighting) != NULL && can_see (ch, victim))
            {
                int percent;
                char wound[100];
                char *pbuff;
                char buf[MSL];
                char buffer[MSL*2];

                if (victim->max_hit > 0)
                    percent = victim->hit * 100 / victim->max_hit;
                else
                    percent = -1;

                if (percent >= 100)
                    sprintf (wound, "is in excellent condition.");
                else if (percent >= 90)
                    sprintf (wound, "has a few scratches.");
                else if (percent >= 75)
                    sprintf (wound, "has some small wounds and bruises.");
                else if (percent >= 50)
                    sprintf (wound, "has quite a few wounds.");
                else if (percent >= 30)
                    sprintf (wound,
                             "has some big nasty wounds and scratches.");
                else if (percent >= 15)
                    sprintf (wound, "looks pretty hurt.");
                else if (percent >= 0)
                    sprintf (wound, "is in awful condition.");
                else
                    sprintf (wound, "is {rbleeding{x to death.");

                sprintf (buf, "%s %s \n\r",
                         IS_NPC (victim) ? victim->short_descr : victim->name,
                         wound);
                buf[0] = UPPER (buf[0]);
                pbuff = buffer;
                colorconv (pbuff, buf, CH(d));
                write_to_buffer (d, buffer, 0);
            }


            ch = d->original ? d->original : d->character;
            if (!IS_SET (ch->comm, COMM_COMPACT))
                write_to_buffer (d, "\n\r", 2);


            if (IS_SET (ch->comm, COMM_PROMPT))
                bust_a_prompt (d->character);

            if (IS_SET (ch->comm, COMM_TELNET_GA))
                write_to_buffer (d, go_ahead_str, 0);
        }
    }

    /*
     * Short-circuit if nothing to write.
     */
    if (d->outtop == 0)
        return TRUE;

    /*
     * Snoop-o-rama.
     */
    if (d->snoop_by != NULL)
    {
        if (d->character != NULL)
            write_to_buffer (d->snoop_by, d->character->name, 0);
        write_to_buffer (d->snoop_by, "> ", 2);
        write_to_buffer (d->snoop_by, d->outbuf, d->outtop);
    }

    /*
     * OS-dependent output.
     */
    if (!write_to_descriptor (d->descriptor, d->outbuf, d->outtop))
    {
        d->outtop = 0;
        return FALSE;
    }
    else
    {
        d->outtop = 0;
        return TRUE;
    }
}

/*
 * Bust a prompt (player settable prompt)
 * coded by Morgenes for Aldara Mud
 */
void bust_a_prompt (CHAR_DATA * ch)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    const char *str;
    const char *i;
    char *point;
    char *pbuff;
    char buffer[MAX_STRING_LENGTH * 2];
    char doors[MAX_INPUT_LENGTH];
    EXIT_DATA *pexit;
    bool found;
    const char *dir_name[] = { "N", "E", "S", "W", "U", "D", "-NE", "-NW", "-SE", "-SW" };
    int door;

    point = buf;
    str = ch->prompt;

    // Rhien, 04/10/2015 If the prompt is null, give them the default prompt
    if (str == NULL || str[0] == '\0')
    {
        ch->prompt = str_dup("<%hhp %mm %vmv {g%r {x({c%e{x)>{x  ");
    }

    if (IS_SET (ch->comm, COMM_AFK))
    {
        send_to_char ("{c<AFK>{x ", ch);
        return;
    }

    while (*str != '\0')
    {
        if (*str != '%')
        {
            *point++ = *str++;
            continue;
        }
        ++str;
        switch (*str)
        {
            default:
                i = " ";
                break;
            case 'e':
                found = FALSE;
                doors[0] = '\0';
                for (door = 0; door < MAX_DIR; door++)
                {
                    if ((pexit = ch->in_room->exit[door]) != NULL
                        && pexit->u1.to_room != NULL
                        && (can_see_room (ch, pexit->u1.to_room)
                            || (IS_AFFECTED (ch, AFF_INFRARED)
                                && !IS_AFFECTED (ch, AFF_BLIND)))
                        && !IS_SET (pexit->exit_info, EX_CLOSED))
                    {
                        found = TRUE;
                        strcat (doors, dir_name[door]);
                    }
                }
                if (!found)
                    strcat (doors, "none");
                sprintf (buf2, "%s", doors);
                i = buf2;
                break;
            case 'c':
                sprintf (buf2, "%s", "\n\r");
                i = buf2;
                break;
            case 'h':
        {
        // Rhien,  4/10/2015, color indicators for vitals, 
        // statement needs wrapped in braces to declare variables
        // in here.
        int percent;
        percent = ch->hit * 100 / ch->max_hit;

            if (percent > 70) {
                    sprintf(buf2, "{W%d{x", ch->hit);
            }
            else if (percent >= 30 && percent <= 70) {
                    sprintf(buf2, "{Y%d{x", ch->hit);
            }
            else {
                    sprintf(buf2, "{R%d{x", ch->hit);
            }

                i = buf2;
                break;
            }
            case 'H':
                sprintf (buf2, "%d", ch->max_hit);
                i = buf2;
                break;
            case 'm':
            {
                // Rhien,  4/10/2015, color indicators for vitals
                int percent;
                percent = ch->mana * 100 / ch->max_mana;

                if (percent > 70) {
                        sprintf(buf2, "{W%d{x", ch->mana);
                }
                else if (percent >= 30 && percent <= 70) {
                        sprintf(buf2, "{Y%d{x", ch->mana);
                }
                else {
                        sprintf(buf2, "{R%d{x", ch->mana);
                }

                i = buf2;
                break;
            }
            case 'M':
                sprintf (buf2, "%d", ch->max_mana);
                i = buf2;
                break;
            case 'v':
            {
                // Rhien,  4/10/2015, color indicators for vitals
                int percent;
                percent = ch->move * 100 / ch->max_move;

                if (percent > 70) {
                        sprintf(buf2, "{W%d{x", ch->move);
                }
                else if (percent > 30 && percent < 70) {
                        sprintf(buf2, "{Y%d{x", ch->move);
                }
                else {
                        sprintf(buf2, "{R%d{x", ch->move);
                }

                i = buf2;
                break;
        }
            case 'V':
                sprintf (buf2, "%d", ch->max_move);
                i = buf2;
                break;
            case 'x':
                sprintf (buf2, "%d", ch->exp);
                i = buf2;
                break;
            case 'X':
                sprintf (buf2, "%d", IS_NPC (ch) ? 0 :
                         (ch->level + 1) * exp_per_level (ch,
                                                          ch->pcdata->
                                                          points) - ch->exp);
                i = buf2;
                break;
            case 'g':
                sprintf (buf2, "%ld", ch->gold);
                i = buf2;
                break;
            case 's':
                sprintf (buf2, "%ld", ch->silver);
                i = buf2;
                break;
            case 'a':
                if (ch->level > 9)
                    sprintf (buf2, "%d", ch->alignment);
                else
                    sprintf (buf2, "%s",
                             IS_GOOD (ch) ? "good" : IS_EVIL (ch) ? "evil" :
                             "neutral");
                i = buf2;
                break;
            case 'r':
                if (ch->in_room != NULL)
                    sprintf (buf2, "%s",
                             ((!IS_NPC
                               (ch) && IS_SET (ch->act, PLR_HOLYLIGHT))
                              || (!IS_AFFECTED (ch, AFF_BLIND)
                                  && !room_is_dark (ch->
                                                    in_room))) ? ch->in_room->
                             name : "darkness");
                else
                    sprintf (buf2, " ");
                i = buf2;
                break;
            case 'R':
                if (IS_IMMORTAL (ch) && ch->in_room != NULL)
                    sprintf (buf2, "%d", ch->in_room->vnum);
                else
                    sprintf (buf2, " ");
                i = buf2;
                break;
            case 'z':
                if (IS_IMMORTAL (ch) && ch->in_room != NULL)
                    sprintf (buf2, "%s", ch->in_room->area->name);
                else
                    sprintf (buf2, " ");
                i = buf2;
                break;
            case '%':
                sprintf (buf2, "%%");
                i = buf2;
                break;
            case 'o':
                sprintf (buf2, "%s", olc_ed_name (ch));
                i = buf2;
                break;
            case 'O':
                sprintf (buf2, "%s", olc_ed_vnum (ch));
                i = buf2;
                break;
        }
        ++str;
        while ((*point = *i) != '\0')
            ++point, ++i;
    }
    *point = '\0';
    pbuff = buffer;
    colorconv (pbuff, buf, ch);
    send_to_char ("{p", ch);
    write_to_buffer (ch->desc, buffer, 0);
    send_to_char ("{x", ch);

    if (ch->prefix[0] != '\0')
        write_to_buffer (ch->desc, ch->prefix, 0);
    return;
}



/*
 * Append onto an output buffer.
 */
void write_to_buffer (DESCRIPTOR_DATA * d, const char *txt, int length)
{
    /*
     * Find length in case caller didn't.
     */
    if (length <= 0)
        length = strlen (txt);

    /*
     * Initial \n\r if needed.
     */
    if (d->outtop == 0 && !d->fcommand)
    {
        d->outbuf[0] = '\n';
        d->outbuf[1] = '\r';
        d->outtop = 2;
    }

    /*
     * Expand the buffer as needed.
     */
    while (d->outtop + length >= d->outsize)
    {
        char *outbuf;

        if (d->outsize >= 32000)
        {
            bug ("Buffer overflow. Closing.\n\r", 0);
            close_socket (d);
            return;
        }
        outbuf = alloc_mem (2 * d->outsize);
        strncpy (outbuf, d->outbuf, d->outtop);
        free_mem (d->outbuf, d->outsize);
        d->outbuf = outbuf;
        d->outsize *= 2;
    }

    /*
     * Copy.
     */
    strncpy (d->outbuf + d->outtop, txt, length);
    d->outtop += length;
    return;
}

/*
 * Rhien 5/15/2015
 * Writes a message to all descriptors using write_to_descriptor which is
 * low level with no color.  This can be used to send a message to every
 * connected socket.  This is useful where no other logic in the loop is 
 * needed.
 */
void write_to_all_desc(char *txt)
{
    DESCRIPTOR_DATA *d;

    for (d = descriptor_list; d != NULL; d = d->next)
    {
        write_to_descriptor(d->descriptor, txt, 0);
    }
}

/*
 * Rhien 5/15/2015
 * Writes a message to all characters through the send_to_char mechansim.
 */
void send_to_all_char(char *txt)
{
    CHAR_DATA *ch;

    for (ch = char_list; ch != NULL; ch = ch->next)
    {
        if (!IS_NPC (ch))
        {
            send_to_char(txt, ch);
        }
    }
}

/*
 * Lowest level output function.
 * Write a block of text to the file descriptor.
 * If this gives errors on very long blocks (like 'ofind all'),
 *   try lowering the max block size.
 */
bool write_to_descriptor (int desc, char *txt, int length)
{
    int iStart;
    int nWrite;
    int nBlock;

#if defined(__APPLE__) 
    if (desc == 0)
        desc = 1;
#endif

    if (length <= 0)
        length = strlen (txt);

    for (iStart = 0; iStart < length; iStart += nWrite)
    {
        nBlock = UMIN (length - iStart, 4096);

#if defined(_WIN32)
    if ( ( nWrite = send( desc, txt + iStart, nBlock , 0) ) < 0 )
        {
            perror ("Write_to_descriptor");
            return FALSE;
        }
#else
        if ((nWrite = write (desc, txt + iStart, nBlock)) < 0)
        {
            perror("Write_to_descriptor");
            return FALSE;
        }
#endif

    }

    return TRUE;
}



void log_f (char *fmt, ...)
{
    char buf[2 * MSL];
    va_list args;
    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    log_string (buf);
}

/*
 * Parse a name for acceptability.
 */
bool check_parse_name (char *name)
{
    int clan;

    /*
     * Reserved words.
     */
    if (is_exact_name (name,
                       "all auto immortal self someone something the you loner none"))
    {
        return FALSE;
    }

    /* check clans */
    for (clan = 0; clan < MAX_CLAN; clan++)
    {
        if (LOWER (name[0]) == LOWER (clan_table[clan].name[0])
            && !str_cmp (name, clan_table[clan].name))
            return FALSE;
    }

    /*
     * Length restrictions.
     */

    if (strlen (name) < 2)
        return FALSE;

#if defined(__APPLE__) || defined(unix) || defined(_WIN32)
    if (strlen (name) > 12)
        return FALSE;
#endif

    /*
     * Alphanumerics only.
     * Lock out IllIll twits.
     */
    {
        char *pc;
        bool fIll, adjcaps = FALSE, cleancaps = FALSE;
        int total_caps = 0;

        fIll = TRUE;
        for (pc = name; *pc != '\0'; pc++)
        {
            if (!isalpha (*pc))
                return FALSE;

            if (isupper (*pc))
            {                    /* ugly anti-caps hack */
                if (adjcaps)
                    cleancaps = TRUE;
                total_caps++;
                adjcaps = TRUE;
            }
            else
                adjcaps = FALSE;

            if (LOWER (*pc) != 'i' && LOWER (*pc) != 'l')
                fIll = FALSE;
        }

        if (fIll)
            return FALSE;

        if (cleancaps
            || (total_caps > (strlen (name)) / 2
                && strlen (name) < 3)) return FALSE;
    }

    /*
     * Prevent players from naming themselves after mobs.
     */
    {
        extern MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
        MOB_INDEX_DATA *pMobIndex;
        int iHash;

        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (pMobIndex = mob_index_hash[iHash];
                 pMobIndex != NULL; pMobIndex = pMobIndex->next)
            {
                if (is_name (name, pMobIndex->player_name))
                    return FALSE;
            }
        }
    }

    /*
     * Edwin's been here too. JR -- 10/15/00
     * 
     * Check names of people playing. Yes, this is necessary for multiple
     * newbies with the same name (thanks Saro)
     */
    if (descriptor_list)
    {
        int count=0;
        DESCRIPTOR_DATA *d, *dnext;

        for (d = descriptor_list; d != NULL; d = dnext)
        {
            dnext=d->next;
            if (d->connected!=CON_PLAYING&&d->character&&d->character->name
                && d->character->name[0] && !str_cmp(d->character->name,name))
            {
                count++;
                close_socket(d);
            }
        }
        if (count)
        {
            sprintf(log_buf,"Double newbie alert (%s)",name);
            wiznet(log_buf,NULL,NULL,WIZ_LOGINS,0,0);

            return FALSE;
        }
    }

    return TRUE;
}



/*
 * Look for link-dead player to reconnect.
 */
bool check_reconnect (DESCRIPTOR_DATA * d, char *name, bool fConn)
{
    CHAR_DATA *ch;

    for (ch = char_list; ch != NULL; ch = ch->next)
    {
        if (!IS_NPC (ch)
            && (!fConn || ch->desc == NULL)
            && !str_cmp (d->character->name, ch->name))
        {
            if (fConn == FALSE)
            {
                free_string (d->character->pcdata->pwd);
                d->character->pcdata->pwd = str_dup (ch->pcdata->pwd);
            }
            else
            {
                free_char (d->character);
                d->character = ch;
                ch->desc = d;
                ch->timer = 0;
                send_to_char
                    ("Reconnecting. Type replay to see missed tells.\n\r",
                     ch);
                act ("$n has reconnected.", ch, NULL, NULL, TO_ROOM);

                sprintf (log_buf, "%s@%s reconnected.", ch->name, d->host);
                log_string (log_buf);
                wiznet ("$N groks the fullness of $S link.",
                        ch, NULL, WIZ_LINKS, 0, 0);
                d->connected = CON_PLAYING;
                /* Inform the character of a note in progress and the possbility
                 * of continuation!
                 */
                if (ch->pcdata->in_progress)
                    send_to_char ("You have a note in progress. Type NWRITE to continue it.\n\r", ch);
            }
            return TRUE;
        }
    }

    return FALSE;
}



/*
 * Check if already playing.
 */
bool check_playing (DESCRIPTOR_DATA * d, char *name)
{
    DESCRIPTOR_DATA *dold;

    for (dold = descriptor_list; dold; dold = dold->next)
    {
        if (dold != d
            && dold->character != NULL
            && dold->connected != CON_GET_NAME
            && dold->connected != CON_GET_OLD_PASSWORD
            && !str_cmp (name, dold->original
                         ? dold->original->name : dold->character->name))
        {
            write_to_buffer (d, "That character is already playing.\n\r", 0);
            write_to_buffer (d, "Do you wish to connect anyway (Y/N)?", 0);
            d->connected = CON_BREAK_CONNECT;
            return TRUE;
        }
    }

    return FALSE;
}



void stop_idling (CHAR_DATA * ch)
{
    if (ch == NULL
        || ch->desc == NULL
        || ch->desc->connected != CON_PLAYING
        || ch->was_in_room == NULL
        || ch->in_room != get_room_index (ROOM_VNUM_LIMBO)) return;

    ch->timer = 0;
    char_from_room (ch);
    char_to_room (ch, ch->was_in_room);
    ch->was_in_room = NULL;
    act ("$n has returned from the void.", ch, NULL, NULL, TO_ROOM);
    return;
}



/*
 * Write to one char.
 */
void send_to_char_bw (const char *txt, CHAR_DATA * ch)
{
    if (txt != NULL && ch->desc != NULL)
        write_to_buffer (ch->desc, txt, strlen (txt));
    return;
}

/*
 * Send a page to one char.
 */
void page_to_char_bw (const char *txt, CHAR_DATA * ch)
{
    if (txt == NULL || ch->desc == NULL)
        return;

    if (ch->lines == 0)
    {
        send_to_char_bw (txt, ch);
        return;
    }

    ch->desc->showstr_head = alloc_mem (strlen (txt) + 1);
    strcpy (ch->desc->showstr_head, txt);
    ch->desc->showstr_point = ch->desc->showstr_head;
    show_string (ch->desc, "");
}

/*
 * Page to one char, new color version, by Lope.
 */
void send_to_char (const char *txt, CHAR_DATA * ch)
{
    const char *point;
    char *point2;
    char buf[MAX_STRING_LENGTH * 4];
    int skip = 0;

    buf[0] = '\0';
    point2 = buf;
    if (txt && ch->desc)
    {
        if (IS_SET (ch->act, PLR_COLOR))
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    skip = color (*point, ch, point2);
                    while (skip-- > 0)
                        ++point2;
                    continue;
                }
                *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            write_to_buffer (ch->desc, buf, point2 - buf);
        }
        else
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    continue;
                }
                *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            write_to_buffer (ch->desc, buf, point2 - buf);
        }
    }
    return;
}

/*
 * Page to one descriptor using Lope's color.
 */
void send_to_desc (const char *txt, DESCRIPTOR_DATA * d)
{
    const char *point;
    char *point2;
    char buf[MAX_STRING_LENGTH * 4];
    int skip = 0;

    buf[0] = '\0';
    point2 = buf;
    if (txt && d)
    {
        if (d->ansi == TRUE)
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    skip = color (*point, NULL, point2);
                    while (skip-- > 0)
                        ++point2;
                    continue;
                }
                *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            write_to_buffer (d, buf, point2 - buf);
        }
        else
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    continue;
                }
                *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            write_to_buffer (d, buf, point2 - buf);
        }
    }
    return;
}

void page_to_char (const char *txt, CHAR_DATA * ch)
{
    const char *point;
    char *point2;
    char buf[MAX_STRING_LENGTH * 4];
    int skip = 0;

#if defined(__APPLE__)
    send_to_char (txt, ch);
#else
    buf[0] = '\0';
    point2 = buf;
    if (txt && ch->desc)
    {
        if (IS_SET (ch->act, PLR_COLOR))
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    skip = color (*point, ch, point2);
                    while (skip-- > 0)
                        ++point2;
                    continue;
                }
                *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            ch->desc->showstr_head = alloc_mem (strlen (buf) + 1);
            strcpy (ch->desc->showstr_head, buf);
            ch->desc->showstr_point = ch->desc->showstr_head;
            show_string (ch->desc, "");
        }
        else
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    continue;
                }
                *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            ch->desc->showstr_head = alloc_mem (strlen (buf) + 1);
            strcpy (ch->desc->showstr_head, buf);
            ch->desc->showstr_point = ch->desc->showstr_head;
            show_string (ch->desc, "");
        }
    }
#endif
    return;
}

/* string pager */
void show_string (struct descriptor_data *d, char *input)
{
    char buffer[4 * MAX_STRING_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    register char *scan, *chk;
    int lines = 0, toggle = 1;
    int show_lines;

    one_argument (input, buf);
    if (buf[0] != '\0')
    {
        if (d->showstr_head)
        {
            free_mem (d->showstr_head, strlen (d->showstr_head));
            d->showstr_head = 0;
        }
        d->showstr_point = 0;
        return;
    }

    if (d->character)
        show_lines = d->character->lines;
    else
        show_lines = 0;

    for (scan = buffer;; scan++, d->showstr_point++)
    {
        if (((*scan = *d->showstr_point) == '\n' || *scan == '\r')
            && (toggle = -toggle) < 0)
            lines++;

        else if (!*scan || (show_lines > 0 && lines >= show_lines))
        {
            *scan = '\0';
            write_to_buffer (d, buffer, strlen (buffer));
            for (chk = d->showstr_point; isspace (*chk); chk++);
            {
                if (!*chk)
                {
                    if (d->showstr_head)
                    {
                        free_mem (d->showstr_head, strlen (d->showstr_head));
                        d->showstr_head = 0;
                    }
                    d->showstr_point = 0;
                }
            }
            return;
        }
    }
    return;
}

void act_new (const char *format, CHAR_DATA * ch, const void *arg1,
              const void *arg2, int type, int min_pos)
{
    static char *const he_she[] = { "it", "he", "she" };
    static char *const him_her[] = { "it", "him", "her" };
    static char *const his_her[] = { "its", "his", "her" };

    char buf[MAX_STRING_LENGTH];
    char fname[MAX_INPUT_LENGTH];
    CHAR_DATA *to;
    CHAR_DATA *vch = (CHAR_DATA *) arg2;
    OBJ_DATA *obj1 = (OBJ_DATA *) arg1;
    OBJ_DATA *obj2 = (OBJ_DATA *) arg2;
    const char *str;
    const char *i;
    char *point;
    char *pbuff;
    char buffer[MSL * 2];

    /*
     * Discard null and zero-length messages.
     */
    if (format == NULL || format[0] == '\0')
        return;

    /* discard null rooms and chars */
    if (ch == NULL || ch->in_room == NULL)
        return;

    to = ch->in_room->people;
    if (type == TO_VICT)
    {
        if (vch == NULL)
        {
            bug ("Act: null vch with TO_VICT.", 0);
            return;
        }

        if (vch->in_room == NULL)
            return;

        to = vch->in_room->people;
    }

    for (; to != NULL; to = to->next_in_room)
    {
        if ((!IS_NPC (to) && to->desc == NULL)
            || (IS_NPC (to) && !HAS_TRIGGER (to, TRIG_ACT))
            || to->position < min_pos)
            continue;

        if ((type == TO_CHAR) && to != ch)
            continue;
        if (type == TO_VICT && (to != vch || to == ch))
            continue;
        if (type == TO_ROOM && to == ch)
            continue;
        if (type == TO_NOTVICT && (to == ch || to == vch))
            continue;

        point = buf;
        str = format;
        while (*str != '\0')
        {
            if (*str != '$')
            {
                *point++ = *str++;
                continue;
            }
            ++str;
            i = " <@@@> ";

            if (arg2 == NULL && *str >= 'A' && *str <= 'Z')
            {
                bug ("Act: missing arg2 for code %d.", *str);
                i = " <@@@> ";
            }
            else
            {
                switch (*str)
                {
                    /* Added checking of pointers to each case after
                     * reading about the bug on Edwin's page.
                     * JR -- 10/15/00
                     */
                    default:
                        bug ("Act: bad code %d.", *str);
                        i = " <@@@> ";
                        break;
                        /* Thx alex for 't' idea */
                    case 't':
                        if (arg1)
                            i = (char *) arg1;
                        else
                            bug("Act: bad code $t for 'arg1'",0);
                        break;
                    case 'T':
                        if (arg2)
                            i = (char *) arg2;
                        else
                            bug("Act: bad code $T for 'arg2'",0);
                        break;
                    case 'n':
                        if (ch && to)
                            i = PERS (ch, to);
                        else
                            bug("Act: bad code $n for 'ch' or 'to'",0);
                        break;
                    case 'N':
                        if (vch && to)
                            i = PERS (vch, to);
                        else
                            bug("Act: bad code $N for 'vch' or 'to'",0);
                        break;
                    case 'e':
                        if (ch)
                            i = he_she[URANGE (0, ch->sex, 2)];
                        else
                            bug("Act: bad code $e for 'ch'",0);
                        break;
                    case 'E':
                        if (vch)
                            i = he_she[URANGE (0, vch->sex, 2)];
                        else
                            bug("Act: bad code $E for 'vch'",0);
                        break;
                    case 'm':
                        if (ch)
                            i = him_her[URANGE (0, ch->sex, 2)];
                        else
                            bug("Act: bad code $m for 'ch'",0);
                        break;
                    case 'M':
                        if (vch)
                            i = him_her[URANGE (0, vch->sex, 2)];
                        else
                            bug("Act: bad code $M for 'vch'",0);
                        break;
                    case 's':
                        if (ch)
                            i = his_her[URANGE (0, ch->sex, 2)];
                        else
                            bug("Act: bad code $s for 'ch'",0);
                        break;
                    case 'S':
                        if (vch)
                            i = his_her[URANGE (0, vch->sex, 2)];
                        else
                            bug("Act: bad code $S for 'vch'",0);
                        break;
                    case 'p':
                        if (to && obj1)
                            i = can_see_obj (to, obj1)
                            ? obj1->short_descr : "something";
                        else
                            bug("Act: bad code $p for 'to' or 'obj1'",0);
                        break;
                    case 'P':
                        if (to && obj2)
                            i = can_see_obj (to, obj2)
                            ? obj2->short_descr : "something";
                        else
                            bug("Act: bad code $P for 'to' or 'obj2'",0);
                        break;
                    case 'd':
                        if (arg2 == NULL || ((char *) arg2)[0] == '\0')
                        {
                            i = "door";
                        }
                        else
                        {
                            one_argument ((char *) arg2, fname);
                            i = fname;
                        }
                        break;
                }
            }

            ++str;
            while ((*point = *i) != '\0')
                ++point, ++i;
        }

        *point++ = '\n';
        *point++ = '\r';
        *point = '\0';
        /* Kludge to capitalize first letter of buffer, trying
         * to account for { color codes. -- JR 09/09/00
         */
        if (buf[0] == 123)
            buf[2] = UPPER (buf[2]);
        else
            buf[0] = UPPER (buf[0]);
        pbuff = buffer;
        colorconv (pbuff, buf, to);
        if (to->desc && (to->desc->connected == CON_PLAYING))
            write_to_buffer( to->desc, buffer, 0); /* changed to buffer to reflect prev. fix */
        else if (MOBtrigger)
            mp_act_trigger (buf, to, ch, arg1, arg2, TRIG_ACT);
    }
    return;
}

int color(char type, CHAR_DATA *ch, char *string)
{
    char	code[20];
    char	*p = '\0';

    if (ch && IS_NPC(ch))
        return(0);

    switch (type)
    {
    default:
        sprintf(code, CLEAR);
        break;
    case 'x':
        sprintf(code, CLEAR);
        break;
    case 'b':
        sprintf(code, C_BLUE);
        break;
    case 'c':
        sprintf(code, C_CYAN);
        break;
    case 'g':
        sprintf(code, C_GREEN);
        break;
    case 'm':
        sprintf(code, C_MAGENTA);
        break;
    case 'r':
        sprintf(code, C_RED);
        break;
    case 'w':
        sprintf(code, C_WHITE);
        break;
    case 'y':
        sprintf(code, C_YELLOW);
        break;
    case 'B':
        sprintf(code, C_B_BLUE);
        break;
    case 'C':
        sprintf(code, C_B_CYAN);
        break;
    case 'G':
        sprintf(code, C_B_GREEN);
        break;
    case 'M':
        sprintf(code, C_B_MAGENTA);
        break;
    case 'R':
        sprintf(code, C_B_RED);
        break;
    case 'W':
        sprintf(code, C_B_WHITE);
        break;
    case 'Y':
        sprintf(code, C_B_YELLOW);
        break;
    case 'D':
        sprintf(code, C_D_GREY);
        break;
    case '*':
        sprintf(code, "%c", 007);
        break;
    case '/':
        strcpy(code, "\n\r");
        break;
    case '^':
        strcpy(code, "\x1B[0;5m"); // Blink
        break;
    case '_':
        strcpy(code, "\x1B[0;4m");  // Underline
        break;
        case '&':
                strcpy(code, "\x1B[0;7m");  // Reverse
                break;
    case '-':
        strcpy(code, "~"); 
        break;
    //case '/':
    //	sprintf(code, "%c", 012);
    //	break;
    case '{':
        sprintf(code, "%c", '{');
        break;
    }

    p = code;
    while (*p != '\0')
    {
        *string = *p++;
        *++string = '\0';
    }

    return(strlen(code));
}

void colorconv (char *buffer, const char *txt, CHAR_DATA * ch)
{
    const char *point;
    int skip = 0;

    if (ch && ch->desc && txt)
    {
        if (IS_SET (ch->act, PLR_COLOR))
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    if (*point != '\n') {
                        skip = color (*point, ch, buffer);
                        while (skip-- > 0)
                            ++buffer;
                        continue;
                    }
                }
                *buffer = *point;
                *++buffer = '\0';
            }
            *buffer = '\0';
        }
        else
        {
            for (point = txt; *point; point++)
            {
                if (*point == '{')
                {
                    point++;
                    continue;
                }
                *buffer = *point;
                *++buffer = '\0';
            }
            *buffer = '\0';
        }
    }
    return;
}

/*
 * Macintosh support functions.
 */
#if defined(__APPLE__)
int gettimeofday (struct timeval *tp, void *tzp)
{
    tp->tv_sec = time (NULL);
    tp->tv_usec = 0;
}
#endif

/* source: EOD, by John Booth <???> */
void printf_to_desc (DESCRIPTOR_DATA * d, char *fmt, ...)
{
    char buf[MSL];
    va_list args;
    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    send_to_desc (buf, d);
}

void printf_to_char (CHAR_DATA * ch, char *fmt, ...)
{
    char buf[MAX_STRING_LENGTH];
    va_list args;
    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    send_to_char (buf, ch);
}

void bugf (char *fmt, ...)
{
    char buf[2 * MSL];
    va_list args;
    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    bug (buf, 0);
}

/*
 * Windows support functions
 * (copied from Envy)
 */
#if defined( _WIN32 )
void gettimeofday(struct timeval *tp, void *tzp)
{
    tp->tv_sec = time(NULL);
    tp->tv_usec = 0;
}
#endif

/*
 * Macintosh support functions.
 */
#if defined(__APPLE__)
int gettimeofday(struct timeval *tp, void *tzp)
{
    tp->tv_sec = time(NULL);
    tp->tv_usec = 0;
}
#endif
