/* @(#)Copyright (C) 1996-2012 H.Shirouzu		tapi32ex.h	Ver0.99 */
/* ========================================================================
    Project  Name			: Win32 Lightweight  Class Library Test
    Module Name				: Main Header
    Create					: 2005-04-10(Sun)
    Update					: 2012-04-02(Mon)
    port update             : 2016-02-28
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef TAPI32EX_H
#define TAPI32EX_H

#include <QCryptographicHash>
#include <xxhash.h>
#include <bsd/stdlib.h>

#define SHA2_256SIZE 32
#define SHA2_512SIZE 64
#define SHA3_256SIZE 32
#define SHA3_512SIZE 64
#define SHA1_SIZE 20
#define MD5_SIZE  16
#define XX_SIZE_32 4
#define XX_SIZE_64 8

u_int MakeHash(const void *data, int size, DWORD iv=0);

class TDigest {
protected:
    QCryptographicHash *hHash;	//for MD5 and SHA1
    XXH64_state_t	   *xxHash;	//for xxHash
    _int64		updateSize;

public:
    enum Type { SHA1, MD5 ,XX,SHA2_256,SHA2_512,SHA3_256,SHA3_512 } type;

    TDigest();
    ~TDigest();
    BOOL Init(Type _type=SHA1);
    BOOL Reset();
    BOOL Update(void *data, int size);
    BOOL GetVal(void *data);
    int  GetDigestSize() { return type == MD5 ? MD5_SIZE :
                                  type == SHA1 ? SHA1_SIZE :
                                  type == XX ? XX_SIZE_64 :
                                  type == SHA2_256 ? SHA2_256SIZE :
                                  type == SHA2_512 ? SHA2_512SIZE :
                                  type == SHA3_256 ? SHA3_256SIZE :
                                  type == SHA3_512 ? SHA3_512SIZE :
                                  XX_SIZE_64;}
    void GetEmptyVal(void *data);
};

BOOL TGenRandom(void *buf, int len);

#endif
