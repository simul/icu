/*
 **********************************************************************
 *   Copyright (C) 2005-2006, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __CSMATCH_H
#define __CSMATCH_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

class InputText;
class CharsetRecognizer;

struct UCharsetMatch : public UMemory
{
 private:
    CharsetRecognizer *csr;
    InputText *textIn;
    int32_t confidence;

 public:
    UCharsetMatch();

    void set(InputText *input, CharsetRecognizer *cr, int32_t conf);

    const char *getName()const;

    const char *getLanguage()const;

    int32_t getConfidence()const;

    int32_t getUChars(UChar *buf, int32_t cap, UErrorCode *status) const;
};

U_NAMESPACE_END

#endif /* __CSMATCH_H */
