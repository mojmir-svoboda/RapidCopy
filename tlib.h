/* @(#)Copyright (C) 1996-2012 H.Shirouzu		tlib.h	Ver0.99 */
/* ========================================================================
    Project  Name			: Win32 Lightweight  Class Library Test
    Module Name				: Main Header
    Create					: 1996-06-01(Sat)
    Update					: 2012-04-02(Mon)
    port update             : 2016-02-28
    Copyright				: H.Shirouzu, Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef TLIB_H
#define TLIB_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "tmisc.h"
#include "tapi32ex.h"
#include "tapi32v.h"
#include "osl.h"

#define ALIGN_SIZE(all_size, block_size) (((all_size) + (block_size) -1) \
                                         / (block_size) * (block_size))
//ファイル名最大長は<sys/syslimits.h>に従う。
#define MAX_FNAME_LEN	NAME_MAX

struct TListObj {
    TListObj *prior, *next;
};

class TList {
protected:
    TListObj	top;
    int			num;

public:
    TList(void);
    void		Init(void);
    void		AddObj(TListObj *obj);
    void		DelObj(TListObj *obj);
    TListObj	*TopObj(void);
    TListObj	*EndObj(void);
    TListObj	*NextObj(TListObj *obj);
    TListObj	*PriorObj(TListObj *obj);
    bool		IsEmpty() { return	top.next == &top; }
    void		MoveList(TList *from_list);
    int			Num() { return num; }
};

#endif
