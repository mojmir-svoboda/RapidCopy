/* @(#)Copyright (C) 1996-2010 H.Shirouzu		tapi32v.cpp	Ver0.99 */
/* ========================================================================
    Project  Name			: Win32 Lightweight  Class Library Test
    Module Name				: Main Header
    Create					: 2005-04-10(Sun)
    Update					: 2010-05-09(Sun)
    ported update           : 2016-03-01
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Summary                 : Win32APIのANSI/UNICODE振り分け初期化。
                              Mac/Linux版では意味なし関数なので、その内整理、削除予定
    Reference				:
    ======================================================================== */

#include "tlib.h"
#include <stdio.h>
#include "osl.h"

int		( *lstrcmpiV)(const void *str1, const void *str2);
int		(*lstrlenV)(const void *str);

WCHAR	(*lGetCharV)(const void *, int);
void	(*lSetCharV)(void *, int, WCHAR ch);
WCHAR	(*lGetCharIncV)(const void **);
int		(*strcmpV)(const void *str1, const void *str2);	// static ... for qsort
int		(*strnicmpV)(const void *str1, const void *str2, int len);
int		(*strlenV)(const void *path);
void	*(*strcpyV)(void *dst, const void *src);
void	*(*strdupV)(const void *src);
void	*(*strchrV)(const void *src, int);
void	*(*strrchrV)(const void *src, int);
long	(*strtolV)(const void *, const void **, int base);
u_long	(*strtoulV)(const void *, const void **, int base);
int		(*sprintfV)(void *buf, const void *format,...);
int		(*MakePathV)(void *dest, const void *dir, const void *file);

char *ASTERISK_V;		// "*"
char *FMT_CAT_ASTER_V;	// "%s\\*"
char *FMT_STR_V;		// "%s"
char *FMT_STRSTR_V;		// "%s%s"
char *FMT_QUOTE_STR_V;	// "\"%s\""
const char *FMT_INT_STR_V;	// "%d"
char *BACK_SLASH_V;		// "\\"
char *SEMICOLON_V;		// ";"
char *UNICODE_PARAGRAPH; // u2029
char *SEMICLN_SPC_V;	// "; "
char *NEWLINE_STR_V;	// "\n"
char *EMPTY_STR_V;		// ""
char *DOT_V;			// "."
char *DOTDOT_V;			// ".."
char *QUOTE_V;			// "\""
char *TRUE_V;			// "true"
char *FALSE_V;			// "false"
int CHAR_LEN_V;			// 1(char)
int MAX_PATHLEN_V;		// MAX_PATH

void *GetLoadStrV(UINT resId)
{
    return GetLoadStrA(resId);
}

//Windows版ではコールするWin32APIを振り分けるために最初に初期化関数として使うけど、
//Mac/Linuxb版では意味なし関数。。その内削除したい。。
BOOL TLibInit_Win32V()
{
    lstrcmpiV = (int (*)(const void *, const void *))::strcasecmp;
    lstrlenV = (int (*)(const void *))::strlen;

    strcmpV = (int (*)(const void *, const void *))::strcmp;
    strnicmpV = (int (*)(const void *, const void *, int))::strncmp;
    strlenV = (int (*)(const void *))::strlen;
    strcpyV = (void *(*)(void *, const void *))::strcpy;
    strdupV = (void *(*)(const void *))::strdup;
    strchrV = (void *(*)(const void *, int))(const char *(*)(const char *, int))::strchr;
    strrchrV = (void *(*)(const void *, int))(const char *(*)(const char *, int))::strrchr;
    strtolV = (long (*)(const void *, const void **, int base))::strtol;
    strtoulV = (u_long (*)(const void *, const void **, int base))::strtoul;
    sprintfV = (int (*)(void *, const void *,...))(int (*)(char *, const char *,...))::sprintf;
    MakePathV = (int (*)(void *, const void *, const void *))::MakePath;

    CHAR_LEN_V = sizeof(char);
    MAX_PATHLEN_V = MAX_PATH;

    ASTERISK_V = "*";
    FMT_CAT_ASTER_V = "%s/";
    FMT_STR_V = "%s";
    FMT_STRSTR_V = "%s%s";
    FMT_QUOTE_STR_V = "\"%s\"";
    BACK_SLASH_V = "/";
    SEMICOLON_V = ";";
    UNICODE_PARAGRAPH = "\u2029";
    SEMICLN_SPC_V = "; ";
    NEWLINE_STR_V = "\n";
    EMPTY_STR_V = "";
    DOT_V = ".";
    DOTDOT_V = "..";
    QUOTE_V = "\"";
    TRUE_V = "true";
    FALSE_V = "false";
    return	TRUE;
}


