static char *regexp_id =
    "@(#)Copyright (C) 2005-2006 H.Shirouzu		regexp.cpp	ver1.84";
/* ========================================================================
    Project  Name			: Regular Expression / Wild Card Match Library
    Create					: 2005-11-03(The)
    Update					: 2006-01-31(Tue)
    ported update           : 2016-02-28
    Copyright				: H.Shirouzu Kengo.Sawatsu
    Summary                 : 独自の正規表現、ワイルドカードライブラリだったが
                            : Qtベースの機能に移行。
    Reference				:
    ======================================================================== */

#include "tlib.h"
#include "regexp.h"

RegExp_q::RegExp_q()
{
    exp_list = new QStringList();
    reg = new QRegExp();
}

RegExp_q::~RegExp_q()
{
    delete exp_list;
    delete reg;
}

void RegExp_q::Init(char* def_pattern)
{
    hasdef_pattern = false;

    if(exp_list){
        delete exp_list;
    }
    exp_list = new QStringList();
    //初期パターン値設定要求有り？
    if(def_pattern){
        exp_list->append(def_pattern);
        hasdef_pattern = true;
    }

    if(reg){
        delete reg;
    }
    reg = new QRegExp();
    reg->setPatternSyntax(QRegExp::WildcardUnix);
}

BOOL RegExp_q::RegisterWildCard(const void *wild_str, RegExp_q::CaseSense cs)
{
    if(cs == RegExp_q::CASE_SENSE){
        reg->setCaseSensitivity(Qt::CaseSensitive);
    }
    else{
        reg->setCaseSensitivity(Qt::CaseInsensitive);
    }
    reg->setPattern((char*)wild_str);
    if(!reg->isValid()){
        return false;
    }

    if(!hasdef_pattern){
        exp_list->append((char*)wild_str);
    }
    //初期値持ちの場合は初期値をクリアしてから追加
    else{
        //初期値をクリア
        exp_list->clear();
        exp_list->append((char*)wild_str);
        hasdef_pattern = false;
    }
    return	true;
}

BOOL RegExp_q::IsMatch(const void *target)
{
    for(int i=0;i < exp_list->size();i++){
        reg->setPattern(exp_list->at(i));
        if(reg->exactMatch((char*)target)){
            return true;
        }
    }
    return false;
}

BOOL RegExp_q::IsRegistered()
{
    return true;
}

void RegExp_q::RemoveDuplicateEntry(){

    int remove_entry = 0;
    remove_entry = exp_list->removeDuplicates();
    if(remove_entry){
        qDebug("remove entry num = %d",remove_entry);
    }
}
