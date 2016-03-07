/* @(#)Copyright (C) 1996-2011 H.Shirouzu		tlib.h	Ver0.99 */
/* ========================================================================
    Project  Name			: Win32 Lightweight  Class Library Test
    Module Name				: Main Header
    Create					: 1996-06-01(Sat)
    Update					: 2011-05-23(Mon)
    port update             : 2016-02-28
    Copyright				: H.Shirouzu, Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef TLIBMISC_H
#define TLIBMISC_H

#include <sys/types.h>
#include "osl.h"
#include <QString>
#include <QHash>
#include <QThread>
#include <QMutex>

class THashTbl;

class THashObj {
public:
    THashObj	*priorHash;
    THashObj	*nextHash;
    u_int		hashId;

public:
    THashObj() { priorHash = nextHash = NULL; hashId = 0; }
    virtual ~THashObj() { if (priorHash && priorHash != this) UnlinkHash(); }

    virtual bool LinkHash(THashObj *top);
    virtual bool UnlinkHash();
    friend class THashTbl;
};

class THashTbl {
protected:
    THashObj	*hashTbl;
    int			hashNum;
    int			registerNum;
    bool		isDeleteObj;

    virtual bool	IsSameVal(THashObj *, const void *val) = 0;

public:
    THashTbl(int _hashNum=0, bool _isDeleteObj=true);
    virtual ~THashTbl();
    virtual bool	Init(int _hashNum);
    virtual void	UnInit();
    virtual void	Register(THashObj *obj, u_int hash_id);
    virtual void	UnRegister(THashObj *obj);
    virtual THashObj *Search(const void *data, u_int hash_id);
    virtual int		GetRegisterNum() { return registerNum; }
//	virtual u_int	MakeHashId(const void *data) = 0;
};

struct TResHashObj : THashObj {
    void	*val;
    TResHashObj(UINT _resId, void *_val) { hashId = _resId; val = _val; }
    ~TResHashObj() { free(val); }

};

class TResHash : public THashTbl {
protected:
    virtual bool IsSameVal(THashObj *obj, const void *val) {
        return obj->hashId == *(u_int *)val;
    }

public:
    TResHash(int _hashNum) : THashTbl(_hashNum) {}
    TResHashObj	*Search(UINT resId) { return (TResHashObj *)THashTbl::Search(&resId, resId); }
    void		Register(TResHashObj *obj) { THashTbl::Register(obj, obj->hashId); }
};

class Condition {
protected:
    enum WaitEvent { CLEAR_EVENT=0, DONE_EVENT, WAIT_EVENT };
    QMutex				cs;
    QMutex				*hEvents;
    WaitEvent			*waitEvents;
    int					max_threads;
    int					waitCnt;

public:
    Condition(void);
    ~Condition();

    bool Initialize(int _max_threads);
    void UnInitialize(void);

    void Lock(void)		{ cs.lock();}
    void UnLock(void)	{ cs.unlock();}

    // ロックを取得してから利用すること
    int  WaitThreads()	{ return waitCnt; }
    int  IsWait()		{ return waitCnt ? true : FALSE; }
    void DetachThread() { max_threads--; }
    int  MaxThreads()   { return max_threads; }

    bool Wait(DWORD);
    void Notify(void);
};

#define PAGE_SIZE	(4 * 1024)

class VBuf {
protected:
    BYTE	*buf;
    VBuf	*borrowBuf;
    int		size;
    int		usedSize;
    int		maxSize;
    void	Init();

public:
    VBuf(int _size=0, int _max_size=0, VBuf *_borrowBuf=NULL);
    ~VBuf();
    bool	AllocBuf(int _size, int _max_size=0, VBuf *_borrowBuf=NULL);
    bool	LockBuf();
    void	FreeBuf();
    bool	Grow(int grow_size);
    operator BYTE *() { return buf; }
    BYTE	*Buf() { return	buf; }
    WCHAR	*WBuf() { return (WCHAR *)buf; }
    int		Size() { return size; }
    int		MaxSize() { return maxSize; }
    int		UsedSize() { return usedSize; }
    void	SetUsedSize(int _used_size) { usedSize = _used_size; }
    int		AddUsedSize(int _used_size) { return usedSize += _used_size; }
    int		RemainSize(void) { return	size - usedSize; }
};

char* GetLoadStrA(UINT resId);

// WindowsのLoadString()の代替固定文字列構造体
// LoadStringをだますために使う
struct StringTable {
    UINT    resId;
    QString str;
};

int MakePath(char *dest, const char *dir, const char *file);
WCHAR lGetCharIncA(const char **str);
int bin2hexstr(const BYTE *bindata, int len, char *buf);

#endif

