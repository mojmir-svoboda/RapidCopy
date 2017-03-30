static char *version_id =
    "@(#)Copyright (C) 2004-2012 H.Shirouzu		Version.cpp	ver1.0.0";
static char	*organize_str =
    "L'espace Vision";

static char *version_id_2 =
    "Copyright (C) 2016 Kengo Sawatsu";

/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Module Name				: Version
    Create					: 2010-06-13(Sun)
    Update					: 2014-04-16(Wed)
    ported update           : 2016-03-02
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Reference				:
    ======================================================================== */
#include <string.h>
#include "version.h"

static char version_str[32];
static char copyright_str[128];
static char admin_str[32];

void SetVersionStr(BOOL is_admin)
{
    char *versionstr = "1.2.3";
    sprintf(version_str,"v%s",versionstr);
}

const char *GetVersionStr()
{
    if(version_str[0] == 0)
        SetVersionStr();
    return	version_str;
}

const char *GetVerAdminStr()
{
    return	admin_str;
}

const char *GetCopyrightStr(void)
{
    if(copyright_str[0] == 0){
        char *s = strchr(version_id, 'C');
        char *e = strchr(version_id, '\t');
        if(s && e && s < e){
            sprintf(copyright_str, "%.*s", e-s, s);
        }
    }
    return	copyright_str;
}

const char *GetVersionStr2(void){
    return version_id_2;
}

const char *GetOrganizeStr(void)
{
    return	organize_str;
}


