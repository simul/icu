/*
*******************************************************************************
* Copyright (C) 1998-1999, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/uchriter.h"
#include "uhash.h"

UCharCharacterIterator::UCharCharacterIterator()
  : CharacterIterator(),
  text(0)
{
    // never default construct!
}

UCharCharacterIterator::UCharCharacterIterator(const UChar* text,
                                               int32_t textLength)
 : CharacterIterator(text != 0 ? textLength : 0),
  text(text)
{
}

UCharCharacterIterator::UCharCharacterIterator(const UChar* text,
                                               int32_t textLength,
                                               UTextOffset pos)
  : CharacterIterator(text != 0 ? textLength : 0, pos),
  text(text)
{
}

UCharCharacterIterator::UCharCharacterIterator(const UChar* text,
                                               int32_t textLength,
                                               UTextOffset begin,
                                               UTextOffset end,
                                               UTextOffset pos)
  : CharacterIterator(text != 0 ? textLength : 0, begin, end, pos),
  text(text)
{
}

UCharCharacterIterator::UCharCharacterIterator(const UCharCharacterIterator& that)
: CharacterIterator(that),
  text(that.text)
{
}

UCharCharacterIterator&
UCharCharacterIterator::operator=(const UCharCharacterIterator& that) {
    CharacterIterator::operator=(that);
    text = that.text;
    return *this;
}

UCharCharacterIterator::~UCharCharacterIterator() {
}

UBool
UCharCharacterIterator::operator==(const ForwardCharacterIterator& that) const {
    if (this == &that) {
        return TRUE;
    }
    
    if (getDynamicClassID() != that.getDynamicClassID()) {
        return FALSE;
    }

    UCharCharacterIterator&    realThat = (UCharCharacterIterator&)that;

    return text == realThat.text
        && textLength == realThat.textLength
        && pos == realThat.pos
        && begin == realThat.begin
        && end == realThat.end;
}

int32_t
UCharCharacterIterator::hashCode() const {
    return uhash_hashUCharsN(text, textLength) ^ pos ^ begin ^ end;
}

CharacterIterator*
UCharCharacterIterator::clone() const {
    return new UCharCharacterIterator(*this);
}

UChar
UCharCharacterIterator::first() {
    pos = begin;
    if(pos < end) {
        return text[pos];
    } else {
        return DONE;
    }
}

UChar
UCharCharacterIterator::firstPostInc() {
    pos = begin;
    if(pos < end) {
        return text[pos++];
    } else {
        return DONE;
    }
}

UChar
UCharCharacterIterator::last() {
    pos = end;
    if(pos > begin) {
        return text[--pos];
    } else {
        return DONE;
    }
}

UChar
UCharCharacterIterator::setIndex(UTextOffset pos) {
    if(pos < begin) {
        pos = begin;
    } else if(pos > end) {
        pos = end;
    }
    this->pos = pos;
    if(pos < end) {
        return text[pos];
    } else {
        return DONE;
    }
}

UChar
UCharCharacterIterator::current() const {
    if (pos >= begin && pos < end) {
        return text[pos];
    } else {
        return DONE;
    }
}

UChar
UCharCharacterIterator::next() {
    if (pos + 1 < end) {
        return text[++pos];
    } else {
        /* make current() return DONE */
        pos = end;
        return DONE;
    }
}

UChar
UCharCharacterIterator::nextPostInc() {
    if (pos < end) {
        return text[pos++];
    } else {
        return DONE;
    }
}

UBool
UCharCharacterIterator::hasNext() {
    return pos < end ? TRUE : FALSE;
}

UChar
UCharCharacterIterator::previous() {
    if (pos > begin) {
        return text[--pos];
    } else {
        return DONE;
    }
}

UBool
UCharCharacterIterator::hasPrevious() {
    return pos > begin ? TRUE : FALSE;
}

UChar32
UCharCharacterIterator::first32() {
    pos = begin;
    if(pos < end) {
        UTextOffset i = pos;
        UChar32 c;
        UTF_NEXT_CHAR(text, i, end, c);
        return c;
    } else {
        return DONE;
    }
}

UChar32
UCharCharacterIterator::first32PostInc() {
    pos = begin;
    if(pos < end) {
        UChar32 c;
        UTF_NEXT_CHAR(text, pos, end, c);
        return c;
    } else {
        return DONE;
    }
}

UChar32
UCharCharacterIterator::last32() {
    pos = end;
    if(pos > begin) {
        UChar32 c;
        UTF_PREV_CHAR(text, begin, pos, c);
        return c;
    } else {
        return DONE;
    }
}

UChar32
UCharCharacterIterator::setIndex32(UTextOffset pos) {
    if(pos < begin) {
        pos = begin;
    } else if(pos > end) {
        pos = end;
    }
    if(pos < end) {
        UTF_SET_CHAR_START(text, begin, pos);
        UTextOffset i = this->pos = pos;
        UChar32 c;
        UTF_NEXT_CHAR(text, i, end, c);
        return c;
    } else {
        this->pos = pos;
        return DONE;
    }
}

UChar32
UCharCharacterIterator::current32() const {
    if (pos >= begin && pos < end) {
        UChar32 c;
        UTF_GET_CHAR(text, begin, pos, end, c);
        return c;
    } else {
        return DONE;
    }
}

UChar32
UCharCharacterIterator::next32() {
    if (pos < end) {
        UTF_FWD_1(text, pos, end);
        if(pos < end) {
            UTextOffset i = pos;
            UChar32 c;
            UTF_NEXT_CHAR(text, i, end, c);
            return c;
        }
    }
    /* make current() return DONE */
    pos = end;
    return DONE;
}

UChar32
UCharCharacterIterator::next32PostInc() {
    if (pos < end) {
        UChar32 c;
        UTF_NEXT_CHAR(text, pos, end, c);
        return c;
    } else {
        return DONE;
    }
}

UChar32
UCharCharacterIterator::previous32() {
    if (pos > begin) {
        UChar32 c;
        UTF_PREV_CHAR(text, begin, pos, c);
        return c;
    } else {
        return DONE;
    }
}

UTextOffset
UCharCharacterIterator::move(int32_t delta, CharacterIterator::EOrigin origin) {
    switch(origin) {
    case kStart:
        pos = begin + delta;
        break;
    case kCurrent:
        pos += delta;
        break;
    case kEnd:
        pos = end + delta;
        break;
    default:
        break;
    }

    if(pos < begin) {
        pos = begin;
    } else if(pos > end) {
        pos = end;
    }

    return pos;
}

UTextOffset
UCharCharacterIterator::move32(int32_t delta, CharacterIterator::EOrigin origin) {
    // this implementation relies on the "safe" version of the UTF macros
    // (or the trustworthiness of the caller)
    switch(origin) {
    case kStart:
        pos = begin;
        if(delta > 0) {
            UTF_FWD_N(text, pos, end, delta);
        }
        break;
    case kCurrent:
        if(delta > 0) {
            UTF_FWD_N(text, pos, end, delta);
        } else {
            UTF_BACK_N(text, pos, end, -delta);
        }
        break;
    case kEnd:
        pos = end;
        if(delta < 0) {
            UTF_BACK_N(text, pos, end, -delta);
        }
        break;
    default:
        break;
    }

    return pos;
}

void UCharCharacterIterator::setText(const UChar* newText,
                                     int32_t      newTextLength) {
    text = newText;
    if(newText == 0 || newTextLength < 0) {
        newTextLength = 0;
    }
    end = textLength = newTextLength;
    pos = begin = 0;
}

void
UCharCharacterIterator::getText(UnicodeString& result) {
    result = UnicodeString(text, textLength);
}

char UCharCharacterIterator::fgClassID = 0;
