/*
 **********************************************************************
 *   Copyright (C) 2005-2006, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __INPUTEXT_H
#define __INPUTEXT_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN 

class InputText : public UMemory
{
public:
    InputText();
    ~InputText();

    void setText(const char *in, int32_t len);
    void setDeclaredEncoding(const char *encoding, int32_t len);
    UBool isSet() const; 
    void MungeInput(UBool fStripTags);


    static const int32_t kBufSize = 8192;
    // The text to be checked.  Markup will have been
    //   removed if appropriate.
    uint8_t    *fInputBytes;
    int32_t     fInputLen;          // Length of the byte data in fInputText.
    // byte frequency statistics for the input text.
    //   Value is percent, not absolute.
    //   Value is rounded up, so zero really means zero occurences. 
    int16_t  *fByteStats;
    UBool     fC1Bytes;          // True if any bytes in the range 0x80 - 0x9F are in the input;false by default
    char     *fDeclaredEncoding;

    const uint8_t           *fRawInput;     // Original, untouched input bytes.
    //  If user gave us a byte array, this is it.
    //  If user gave us a stream, it's read to a 
    //   buffer here.
    int32_t                  fRawLength;    // Length of data in fRawInput array.

};

U_NAMESPACE_END

#endif /* __INPUTEXT_H */
