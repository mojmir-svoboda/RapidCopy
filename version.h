/*static char *version_id =
    "@(#)Copyright (C) 20004-2012 H.Shirouzu   version.cpp	ver2.10"; */
/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Module Name				: Version
    Create					: 2010-06-13(Sun)
    Update					: 2014-04-16(Wed)
    port update             : 2016-02-28
    Copyright				: H.Shirouzu, Kengo Sawatsu
    Reference				:
    ======================================================================== */
#include "osl.h"

#ifndef VERSION_H
#define VERSION_H

#define RAPIDCOPY_VER_DEFINE_NUM 3		//x.x.x
#define RAPIDCOPY_VER_MAJOR		 0		//?.x.x
#define RAPIDCOPY_VER_MINOR		 1		//x.?.x
#define RAPIDCOPY_VER_PATCH		 2		//x.x.?

void SetVersionStr(BOOL is_admin=FALSE);
const char *GetVersionStr();
const char *GetVerAdminStr();
const char *GetCopyrightStr(void);
const char *GetOrganizeStr();
const char *GetVersionStr2();

#endif

