/* @(#)Copyright (C) 2005-2006 H.Shirouzu   regexp.cpp	ver1.84 */
/* ========================================================================
    Project  Name			: Regular Expression / Wild Card Match Library
    Create					: 2005-11-03(The)
    Update					: 2006-01-31(Tue)
    port update             : 2016-02-28
    Copyright				: H.Shirouzu,Kengo Sawatsu
    SUmmary                 : 正規表現クラス
    Reference				:
    ======================================================================== */
#include <QString>
#include <QStringList>
#include <QRegExp>

//Qtライブラリでワイルドカードするためのクラス
class RegExp_q {
public:
    RegExp_q();
    ~RegExp_q();

    enum CaseSense { CASE_SENSE, CASE_INSENSE };

    void	Init(char* def_pattern=NULL);
    BOOL	RegisterWildCard(const void *wild_str, CaseSense cs=CASE_SENSE);
    BOOL	IsMatch(const void *target);
    BOOL	IsRegistered(void);
    void	RemoveDuplicateEntry();

protected:
    QStringList *exp_list;		//フィルタ文字列配列のリスト。
    QRegExp *reg;
    bool hasdef_pattern;

};


