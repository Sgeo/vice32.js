/*
 * archdep_os2.c - Miscellaneous system-specific stuff.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#define INCL_DOSQUEUES     /* Queue commands     */
#define INCL_DOSSESMGR     /* DosStartSession    */
#define INCL_DOSMEMMGR     /* DosFreeMem         */
#define INCL_DOSPROFILE    /* DosTmrQueryTime    */
#define INCL_DOSPROCESS    /* DosGetInfoBlock    */
#define INCL_DOSMODULEMGR  /* DosQueryModuleName */
#define INCL_DOSSEMAPHORES /* Dos-*-MutexSem     */

#define INCL_DOSMISC
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS

#include <os2.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <tchar.h>

#ifndef HAVE_GETTIMEOFDAY
#include <sys/timeb.h>
#include <sys/time.h>
#endif

#include "ui.h"

#ifdef HAVE_DIR_H
#include <dir.h>
#endif

#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if defined(HAVE_IO_H)
#include <io.h>
#endif

#ifdef HAVE_PROCESS_H
#include <process.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archdep.h"
#include "ioutil.h"
#include "kbd.h"
#include "keyboard.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "util.h"
#include "resources.h"
#include "vsyncapi.h"


/** \brief  Tokens that are illegal in a path/filename
 *
 * FIXME: taken from win32
 */
static const char *illegal_name_tokens = "/\\?*:|\"<>";


static char *argv0;

static char *program_name = NULL;

char *archdep_program_name(void)
{
    if (program_name == NULL) {
        char *s, *e;
        int len;

        s = strrchr(argv0, '\\');
        if (s == NULL) {
            s = argv0;
        } else {
            s++;
        }
        e = strchr(s, '.');
        if (e == NULL) {
            e = argv0 + strlen(argv0);
        }
        len = (int)(e - s + 1);
        program_name = lib_malloc(len);
        memcpy(program_name, s, len - 1);
        program_name[len - 1] = 0;
    }

    return program_name;
}

const char *archdep_boot_path(void)
{
    static char *boot_path;

    if (boot_path == NULL) {
        util_fname_split(argv0, &boot_path, NULL);

        /* This should not happen, but you never know...  */
        if (boot_path == NULL) {
            boot_path = lib_stralloc("./");
        }
    }

    return boot_path;
}

char *archdep_default_sysfile_pathlist(const char *emu_id)
{
    static char *pathlist = NULL;

    if (!pathlist) {
        pathlist = util_concat(emu_id, ARCHDEP_FINDPATH_SEPARATOR_STRING, "DRIVES", ARCHDEP_FINDPATH_SEPARATOR_STRING, "PRINTER", NULL);
    }
    return pathlist;
}

/* Return a malloc'ed backup file name for file `fname'.  */
char *archdep_make_backup_filename(const char *fname)
{
    return util_concat(fname, "~", NULL);
}

char *archdep_default_save_resource_file_name(void)
{
    return archdep_default_resource_file_name();
}

char *archdep_default_resource_file_name(void)
{
    return util_concat(archdep_boot_path(), "\\sdl-vice.ini", NULL);
}


/** \brief  Get path to VICE session file
 *
 * The 'session file' is a file that is used to store settings between VICE
 * runs, storing things like the last used directory.
 *
 * \return  path to session file
 */
char *archdep_default_session_file_name(void)
{
    return util_concat(archdep_boot_path(), "\\sdl-vice-session.ini", NULL);
}



char *archdep_default_fliplist_file_name(void)
{
    return util_concat(archdep_boot_path(), "\\fliplist-", machine_get_name(), ".vfl", NULL);
}

char *archdep_default_rtc_file_name(void)
{
    return util_concat(archdep_boot_path(), "\\sdl-vice.rtc", NULL);
}

char *archdep_default_autostart_disk_image_file_name(void)
{
    const char *home;

    home = archdep_boot_path();
    return util_concat(home, "\\autostart-", machine_get_name(), ".d64", NULL);
}

char *archdep_default_hotkey_file_name(void)
{
    return util_concat(archdep_boot_path(), "\\sdl-hotkey-", machine_get_name(), ".vkm", NULL);
}

char *archdep_default_joymap_file_name(void)
{
    return util_concat(archdep_boot_path(), "\\sdl-joymap-", machine_get_name(), ".vjm", NULL);
}

FILE *archdep_open_default_log_file(void)
{
    char *fname;
    FILE *f;

    fname = util_concat(archdep_boot_path(), "\\vice.log", NULL);
    f = fopen(fname, "wt");
    lib_free(fname);

    return f;
}

int archdep_default_logger(const char *lvl, const char *txt)
{
    if (fputs(lvl, stdout) == EOF || fprintf(stdout, txt) < 0 || fputc ('\n', stdout) == EOF) {
        return -1;
    }
    return 0;
}

int archdep_path_is_relative(const char *path)
{
    return !(isalpha(path[0]) && path[1] == ':' && (path[2] == '/' || path[2] == '\\') || (path[0] == '/' || path[0] == '\\'));
}

static int archdep_search_path(const char *name, char *pBuf, int lBuf)
{
    const int flags = SEARCH_CUR_DIRECTORY|SEARCH_IGNORENETERRS;
    char *path = "";        /* PATH environment variable */
    char *pgmName = util_concat(name, ".exe", NULL);

    // Search the program in the path
    DosScanEnv("PATH", &path);


    if (DosSearchPath(flags, path, pgmName, pBuf, lBuf)) {
        return -1;
    }
    lib_free(pgmName);

    return 0;
}

static char *archdep_cmdline(const char *name, char **argv, const char *sout, const char *serr)
{
    char *res;
    int length = 0;
    int i = 0;

    while (argv[++i]) {
        length += strlen(argv[i]);
    }

    length += i + 1 + (name ? strlen(name) : 0) + 3 + (sout ? strlen(sout) : 0) + 5 +(serr ? strlen(serr) : 0) + 6; // need space for the spaces
    res = lib_calloc(1, length);

    strcat(strcpy(res,"/c "), name);

    i = 0;
    while (argv[++i]) {
        strcat(strcat(res," "), argv[i]);
    }

    if (sout) {
        strcat(strcat(strcat(res,  " > \""), sout), "\"");
    }

    if (serr) {
        strcat(strcat(strcat(res, " 2> \""), serr), "\"");
    }

    return res;
}

static void archdep_create_mutex_sem(HMTX *hmtx, const char *pszName, int fState)
{
    APIRET rc;

    char *sem = lib_malloc(13+strlen(pszName) + 5 + 1);

    sprintf(sem, "\\SEM32\\VICE2\\%s_%04x", pszName, vsyncarch_gettime()&0xffff);

    rc = DosCreateMutexSem(sem, hmtx, 0, fState);
}

static HMTX hmtxSpawn;

/* Launch program `name' (searched via the PATH environment variable)
   passing `argv' as the parameters, wait for it to exit and return its
   exit status. If `stdout_redir' or `stderr_redir' are != NULL,
   redirect stdout or stderr to the corresponding file.  */
int archdep_spawn(const char *name, char **argv, char **pstdout_redir, const char *stderr_redir)
{
    // how to redirect stdout & stderr??
    typedef struct _CHILDINFO {  /* Define a structure for the queue data */
        USHORT usSessionID;
        USHORT usReturn;
    } CHILDINFO;

    UCHAR fqName[256] = ""; /* Result of PATH search             */
    HQUEUE hqQueue;         /* Queue handle                      */
    REQUESTDATA rdRequest;  /* Request data for the queue        */
    ULONG ulSzData;         /* Size of the queue data            */
    BYTE bPriority;         /* For the queue                     */
    PVOID pvData;           /* Pointer to the queue data         */
    STARTDATA sd;           /* Start Data for DosStartSession    */
    PID pid;                /* PID for the started child session */
    ULONG ulSession;        /* Session ID for the child session  */
    APIRET rc;              /* Return code from API's            */
    char *cmdline;
    char *stdout_redir = NULL;

    if (pstdout_redir != NULL) {
        if (*pstdout_redir == NULL) {
            *pstdout_redir = archdep_tmpnam();
        }
        stdout_redir = *pstdout_redir;
    }

    if (archdep_search_path(name, fqName, sizeof(fqName))) {
        return -1;
    }

    // Make the needed command string
    cmdline = archdep_cmdline(fqName, argv, stdout_redir, stderr_redir);

    memset(&sd, 0, sizeof(STARTDATA));
    sd.Length = sizeof(STARTDATA);
    sd.FgBg = SSF_FGBG_BACK;      /* Start session in background */
    sd.Related = SSF_RELATED_CHILD;  /* Start a child session       */
    sd.PgmName = "cmd.exe";
    sd.PgmInputs = cmdline;
    sd.PgmControl = SSF_CONTROL_INVISIBLE;
    sd.TermQ = "\\QUEUES\\VICE2\\CHILD.QUE";
    sd.InheritOpt = SSF_INHERTOPT_SHELL;

    /* Start a child process and return it's session ID.
     Wait for the session to end and get it's session ID
     from the termination queue */

    // this prevents you from closing Vice while a child is running
    if (DosRequestMutexSem(hmtxSpawn, SEM_INDEFINITE_WAIT)) {
        return 0;
    }

    if (!(rc = DosCreateQueue(&hqQueue, QUE_FIFO|QUE_CONVERT_ADDRESS, sd.TermQ))) {
        if (!(rc = DosStartSession(&sd, &ulSession, &pid)))  {
            if (!(rc = DosReadQueue(hqQueue, &rdRequest, &ulSzData, &pvData, 0, DCWW_WAIT, &bPriority, 0))) {
                rc = ((CHILDINFO*)pvData)->usReturn;

                DosFreeMem(pvData); /* Free the memory of the queue data element read */
            }
        }
        DosCloseQueue(hqQueue);
    }

    DosReleaseMutexSem(hmtxSpawn);

    lib_free(cmdline);
    return rc;
}

/* return malloc'd version of full pathname of orig_name */
int archdep_expand_path(char **return_path, const char *filename)
{
    if (filename[0] == '\\' || filename[1] == ':') {
        *return_path = lib_stralloc(filename);
    } else {
        char *p = (char *)malloc(512);
        while (getcwd(p, 512) == NULL) {
            return 0;
        }

        *return_path = util_concat(p, "\\", filename, NULL);
        lib_free(p);
    }
    return 0;
}

void archdep_startup_log_error(const char *format, ...)
{
    char *tmp;
    va_list args;

    va_start(args, format);
    tmp = lib_mvsprintf(format, args);
    va_end(args);

    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, tmp, "SDLVICE/2 Startup Error", 0, MB_OK);
    lib_free(tmp);
}

char *archdep_quote_parameter(const char *name)
{
    return util_concat("\"", name, "\"", NULL);
}

char *archdep_filename_parameter(const char *name)
{
    char *exp;
    char *a;

    archdep_expand_path(&exp, name);
    a = archdep_quote_parameter(exp);
    lib_free(exp);
    return a;
}

char *archdep_tmpnam(void)
{
    return lib_stralloc(tmpnam(NULL));
}

FILE *archdep_mkstemp_fd(char **filename, const char *mode)
{
    char *tmp;
    FILE *fd;

    tmp = lib_stralloc(tmpnam(NULL));

    fd = fopen(tmp, mode);

    if (fd == NULL) {
        return NULL;
    }

    *filename = tmp;

    return fd;
}


int archdep_mkdir(const char *pathname, int mode)
{
    return mkdir((char*)pathname);
}

int archdep_rmdir(const char *pathname)
{
    return rmdir(pathname);
}

int archdep_stat(const char *file_name, unsigned int *len, unsigned int *isdir)
{
    struct stat statbuf;

    if (stat(file_name, &statbuf) < 0) {
        return -1;
    }

    *len = statbuf.st_size;
    *isdir = statbuf.st_mode & S_IFDIR;

    return 0;
}

/* set permissions of given file to rw, respecting current umask */
int archdep_fix_permissions(const char *file_name)
{
    return 0;
}

int archdep_file_is_blockdev(const char *name)
{
    return 0;
}

int archdep_file_is_chardev(const char *name)
{
    if (strcmp(name, "/dev/cbm") == 0) {
        return 1;
    }

    return 0;
}

#ifdef SDL_CHOOSE_DRIVES
char **archdep_list_drives(void)
{
    char **result, **p;
    ULONG dn = 0;
    FSINFO buffer = {0};
    APIRET rc = NO_ERROR;
    int drive_count = 1;
    int i;
    int drives[26];

    drives[0] = 0;
    drives[1] = 0;
    for (i = 3; i <= 26; ++i) {
        dn = (ULONG)i;
        rc = DosQueryFSInfo(dn, FSIL_VOLSER, &buffer, sizeof(FSINFO));
        if (rc == NO_ERROR) {
            drives[i - 1] = 1;
            ++drive_count;
        } else {
            drives[i - 1] = 0;
        }
    }

    result = lib_malloc(sizeof(char*) * drive_count);
    p = result;

    for (i = 2; i < 26; ++i) {
        if (drives[i]) {
            char buf[16];
            sprintf(buf, "%c:/", 'a' + i);
            *p++ = lib_stralloc(buf);
        }
    }
    *p = NULL;

    return result;
}

char *archdep_get_current_drive(void)
{
    char *p = ioutil_current_dir();
    char *p2 = strchr(p, '\\');
    p2[0] = '/';
    p2[1] = '\0';
    return p;
}

void archdep_set_current_drive(const char *drive)
{
    if (_chdir(drive)) {
        ui_error("Failed to change drive to %s", drive);
    }
}
#endif

int archdep_require_vkbd(void)
{
    return 0;
}

int archdep_rename(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
}

#ifndef HAVE_GETTIMEOFDAY
int archdep_rtc_get_centisecond(void)
{
    struct timeb tb;

    __ftime(&tb);

    return (int)tb.millitm / 10;
}
#endif

/* returns host keyboard mapping. used to initialize the keyboard map when
   starting with a black (default) config, so an educated guess works good
   enough most of the time :)

   FIXME: add more languages, constants are defined in winnt.h

   https://msdn.microsoft.com/en-us/library/windows/desktop/dd318693%28v=vs.85%29.aspx
*/
int kbd_arch_get_host_mapping(void)
{
    return KBD_MAPPING_US;
}

static int archdep_init_extra(int *argc, char **argv)
{
    argv0 = lib_stralloc(argv[0]);
    return 0;
}

static void archdep_shutdown_extra(void)
{
}
