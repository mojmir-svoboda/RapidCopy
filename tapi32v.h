/* @(#)Copyright (C) 1996-2010 H.Shirouzu		tapi32v.h	Ver0.99 */
/* ========================================================================
    Project  Name			: Win32 Lightweight  Class Library Test
    Module Name				: Main Header
    Create					: 2005-04-10(Sun)
    Update					: 2010-05-09(Sun)
    port update             : 2016-02-28
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef TAPI32V_H
#define TAPI32V_H

#include "osl.h"

//Windowsのwide対応用のV関数、本当は不要なんだけど。。その内ネイティブに置き換えよう。。
extern int (*lstrcmpiV)(const void *str1, const void *str2);
extern int (*lstrlenV)(const void *str);
extern int (*strcmpV)(const void *str1, const void *str2);
extern int (*strnicmpV)(const void *str1, const void *str2, int len);
extern int (*strlenV)(const void *path);
extern void *(*strcpyV)(void *dst, const void *src);
extern void *(*strdupV)(const void *src);
extern void *(*strchrV)(const void *src, int);
extern void *(*strrchrV)(const void *src, int);
extern long (*strtolV)(const void *, const void **, int base);
extern u_long (*strtoulV)(const void *, const void **, int base);
extern int (*sprintfV)(void *buf, const void *format,...);
extern int (*MakePathV)(void *dest, const void *dir, const void *file);

extern char *ASTERISK_V;		// "*"
extern char *FMT_CAT_ASTER_V;	// "%s\\*"
extern char *FMT_STR_V;			// "%s"
extern char *FMT_STRSTR_V;			// "%s%s"
extern char *FMT_QUOTE_STR_V;	// "\"%s\""
extern const char *FMT_INT_STR_V;		// "%d"
extern char *BACK_SLASH_V;		// "\\"
extern char *SEMICOLON_V;		// ";"
extern char *UNICODE_PARAGRAPH; // u2029
extern char *SEMICLN_SPC_V;		// "; "
extern char *NEWLINE_STR_V;		// "\n"
extern char *EMPTY_STR_V;		// ""
extern char *DOT_V;				// "."
extern char *DOTDOT_V;			// ".."
extern char *QUOTE_V;			// "\""
extern char *TRUE_V;			// "true"
extern char *FALSE_V;			// "false"
extern int CHAR_LEN_V;			// 2(WCHAR) or 1(char)
extern int MAX_PATHLEN_V;

inline void *MakeAddr(const void *addr, int len) { return (BYTE *)addr + len * CHAR_LEN_V; }

inline void SetChar(void *addr, int offset, int val) {
    (*(char *)MakeAddr(addr, offset) = val);
}
inline char GetChar(const void *addr, int offset) {
    return *(char *)MakeAddr(addr, offset);
}
inline int DiffLen(const void *high, const void *low) {
    return (int)((char *)high - (char *)low);
}

void *GetLoadStrV(UINT resId);

BOOL TLibInit_Win32V();

#endif
