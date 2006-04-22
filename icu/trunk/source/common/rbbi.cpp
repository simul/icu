/*
***************************************************************************
*   Copyright (C) 1999-2006 International Business Machines Corporation   *
*   and others. All rights reserved.                                      *
***************************************************************************
*/
//
//  file:  rbbi.c    Contains the implementation of the rule based break iterator
//                   runtime engine and the API implementation for
//                   class RuleBasedBreakIterator
//

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/rbbi.h"
#include "unicode/schriter.h"
#include "unicode/udata.h"
#include "unicode/uclean.h"
#include "rbbidata.h"
#include "rbbirb.h"
#include "cmemory.h"
#include "cstring.h"
#include "mutex.h"
#include "ucln_cmn.h"
#include "brkeng.h"

#include "uassert.h"
#include "uvector.h"

U_NAMESPACE_BEGIN


static const int16_t START_STATE = 1;     // The state number of the starting state
static const int16_t STOP_STATE  = 0;     // The state-transition value indicating "stop"


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RuleBasedBreakIterator)





//-------------------------------------------------------------------------------
//
//  UText functions      As a temporary implementation, create a type of CharacterIterator
//                       that works over UText, and let the RBBI engine continue to
//                       work on CharacterIterator, which it always has.
//
//                       The permanent solution is to rework the RBBI engine to use
//                       UText directly, which will be more efficient for all input
//                       sources.
//
//                       This CharacterIterator implementation over UText is not complete,
//                       it has only what is needed for RBBI, and is not intended
//                       to ever become public.
//
//        TODO:  Move this to some other file.
//
//-------------------------------------------------------------------------------


class CharacterIteratorUT: public CharacterIterator {
public:
    CharacterIteratorUT(UText  *ut);
    virtual ~CharacterIteratorUT();

    virtual CharacterIterator *clone() const;
    virtual UBool       operator==(const ForwardCharacterIterator& that) const;
    virtual UChar       setIndex(int32_t position);
    virtual UChar32     previous32(void);
    virtual UChar32     next32(void);
    virtual UBool       hasNext();
    virtual UChar32     current32(void) const;
    virtual UBool       hasPrevious();
    virtual int32_t     move(int32_t delta, EOrigin origin);
    virtual int32_t     move32(int32_t, EOrigin);
    virtual void        getText(UnicodeString  &);
    static  UClassID    getStaticClassID(void);
    virtual UClassID    getDynamicClassID(void) const;

    UText   *fText;
    virtual void        resetTo(const UText *ut, UErrorCode *status);

private:
    CharacterIteratorUT();

    // The following functions are not needed by RBBI,
    //   but are pure virtual in CharacterIterator, so must be defined.
    //   Only stubs are provided in this implementation.
    //   TODO:  need them all if we are going to really support RBBI::getText().
    virtual int32_t       hashCode(void) const                   {U_ASSERT(FALSE); return 0;};
    virtual UChar         nextPostInc(void)                      {U_ASSERT(FALSE); return 0;};
    virtual UChar32       next32PostInc(void)                    {U_ASSERT(FALSE); return 0;};
    virtual UChar         first(void)                            {U_ASSERT(FALSE); return 0;};
    virtual UChar32       first32(void)                          {U_ASSERT(FALSE); return 0;};
    virtual UChar         last(void)                             {U_ASSERT(FALSE); return 0;};
    virtual UChar32       last32(void)                           {U_ASSERT(FALSE); return 0;};
    virtual UChar32       setIndex32(int32_t)                    {U_ASSERT(FALSE); return 0;};
    virtual UChar         current(void) const                    {U_ASSERT(FALSE); return 0;};
    virtual UChar         next(void)                             {U_ASSERT(FALSE); return 0;};
    virtual UChar         previous(void)                         {U_ASSERT(FALSE); return 0;};
};



//
//  The following fields are inherited from CharacterIterator.
//  This implementation __MUST__ keep them current because of non-virtual inline
//  functions defined in CharacterIterator.
//    int32_t  textLength;       // length of the text.
//    int32_t  pos;              // current index position
//    int32_t  begin;            // starting index.  Always 0 for us.
//    int32_t  end;              // ending index
//
// CharacterIterator was designed assuming that utf-16 indexing would be used,
// but native indexing will pass through OK.  This partial implementation only
// provides the '32' flavored code point access, not UChar access.
//

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CharacterIteratorUT)

CharacterIteratorUT::CharacterIteratorUT(UText *ut) {
    fText      = 0;
    textLength = 0;
    pos        = 0;
    begin      = 0;
    end        = 0;
    if (ut == NULL) {
        return;
    }

    UErrorCode status = U_ZERO_ERROR;
    fText = utext_clone(NULL, ut, FALSE, TRUE, &status);  // Shallow, Read-only clone.
    if (fText != NULL) {
        // Set the inherited CharacterItertor fields
        textLength = (int32_t)utext_nativeLength(ut);
        end = textLength;
    }
}

CharacterIteratorUT::CharacterIteratorUT() {
    fText     = NULL;
    textLength = 0;
    pos        = 0;
    begin      = 0;
    end        = 0;
}

CharacterIteratorUT::~CharacterIteratorUT() {
    utext_close(fText);
}


CharacterIterator *CharacterIteratorUT::clone() const {
    CharacterIteratorUT *result = new CharacterIteratorUT(this->fText);
    return result;
}

UBool CharacterIteratorUT::operator==(const ForwardCharacterIterator& that) const {
    if (this->getDynamicClassID() != that.getDynamicClassID()) {
        return FALSE;
    }
    const CharacterIteratorUT *realThat = (const CharacterIteratorUT *)&that;
    UBool result = this->fText->context == realThat->fText->context;
    return result;
}

UChar CharacterIteratorUT::setIndex(int32_t position) {
    pos = position;
    if (pos < 0) {
        pos = 0;
    } else if (pos > end) {
        pos = end;
    }
    utext_setNativeIndex(fText, pos);
    pos = (int32_t)utext_getNativeIndex(fText);  // because utext snaps to code point boundary.
    return 0x0000ffff;  // RBBI doesn't use return value, and UText can't return a UChar easily.
}

UChar32 CharacterIteratorUT::previous32(void) {
    UChar32 result = UTEXT_PREVIOUS32(fText);
    pos = (int32_t)utext_getNativeIndex(fText);  // TODO:  maybe optimize common case?
    if (result < 0) {
        result = 0x0000ffff;
    }
    return result;
}

UChar32 CharacterIteratorUT::next32(void) {
    // TODO: optimize.
    UTEXT_NEXT32(fText);
    pos = (int32_t)utext_getNativeIndex(fText);
    UChar32 result = UTEXT_NEXT32(fText);
    if (result < 0) {
        result = 0x0000ffff;
    } else {
        UTEXT_PREVIOUS32(fText);
    }
    return result;
}

UBool CharacterIteratorUT::hasNext() {
    // What would really be best for RBBI is a hasNext32()
    UBool result = TRUE;
    if (pos >= end) { 
        result  = FALSE;
    }
    return result;
}

UChar32 CharacterIteratorUT::current32(void) const {
    UChar32 result = utext_current32(fText);
    if (result < 0) {
        result = 0x0000ffff;
    }
    return result;
}

UBool CharacterIteratorUT::hasPrevious() {
    UBool result = pos > 0;
    return result;
}

int32_t CharacterIteratorUT::move(int32_t delta, EOrigin origin) {
    // only needed for the inherited inline implementation of setToStart().
    int32_t result = pos;
    switch (origin) {
case kStart:
    result = delta;
    break;
case kCurrent: 
    result = pos + delta;
    break;
case kEnd:
    result = end + delta;
    break;
default:
    U_ASSERT(FALSE);
    }
    utext_setNativeIndex(fText, result);
    pos = (int32_t)utext_getNativeIndex(fText);  // align to cp boundary
    return result;
}

int32_t  CharacterIteratorUT::move32(int32_t amt, EOrigin origin) {
    switch (origin) {
case kCurrent: 
    utext_moveIndex32(fText, amt);
    break;
default:
    // don't bother with kStart, kEnd.  Not Used by break iteration.
    U_ASSERT(FALSE);
    }
    pos = (int32_t)utext_getNativeIndex(fText);
    return pos;
}


void  CharacterIteratorUT::resetTo(const UText *ut, UErrorCode *status) {
    // Reset this CharacterIteratorUT to use a new UText.
    fText = utext_clone(fText, ut, FALSE, TRUE, status);
    utext_setNativeIndex(fText, 0);
    textLength = (int32_t)utext_nativeLength(fText);
    pos = 0;
    end = textLength;
}

void  CharacterIteratorUT::getText(UnicodeString  &dest) {
    dest.truncate(0);
    for (UChar32 c=utext_next32From(fText, 0); c!=U_SENTINEL; c=utext_next32(fText)) {
        dest.append(c);
    }
}



//=======================================================================
// constructors
//=======================================================================

/**
 * Constructs a RuleBasedBreakIterator that uses the already-created
 * tables object that is passed in as a parameter.
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(RBBIDataHeader* data, UErrorCode &status)
{
    init();
    fData = new RBBIDataWrapper(data, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}

//-------------------------------------------------------------------------------
//
//   Constructor   from a UDataMemory handle to precompiled break rules
//                 stored in an ICU data file.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(UDataMemory* udm, UErrorCode &status)
{
    init();
    fData = new RBBIDataWrapper(udm, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}



//-------------------------------------------------------------------------------
//
//   Constructor       from a set of rules supplied as a string.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator( const UnicodeString  &rules,
                                                UParseError          &parseError,
                                                UErrorCode           &status)
{
    u_init(&status);      // Just in case ICU is not yet initialized
    init();
    if (U_FAILURE(status)) {return;}
    RuleBasedBreakIterator *bi = (RuleBasedBreakIterator *)
        RBBIRuleBuilder::createRuleBasedBreakIterator(rules, parseError, status);
    // Note:  This is a bit awkward.  The RBBI ruleBuilder has a factory method that
    //        creates and returns a complete RBBI.  From here, in a constructor, we
    //        can't just return the object created by the builder factory, hence
    //        the assignment of the factory created object to "this".
    if (U_SUCCESS(status)) {
        *this = *bi;
        delete bi;
    }
}


//-------------------------------------------------------------------------------
//
// Default Constructor.      Create an empty shell that can be set up later.
//                           Used when creating a RuleBasedBreakIterator from a set
//                           of rules.
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator() {
    init();
}


//-------------------------------------------------------------------------------
//
//   Copy constructor.  Will produce a break iterator with the same behavior,
//                      and which iterates over the same text, as the one passed in.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(const RuleBasedBreakIterator& other)
: BreakIterator(other)
{
    this->init();
    *this = other;
}


/**
 * Destructor
 */
RuleBasedBreakIterator::~RuleBasedBreakIterator() {
    delete fCharIter;
    fCharIter = NULL;
    utext_close(fText);

    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    if (fCachedBreakPositions) {
        uprv_free(fCachedBreakPositions);
        fCachedBreakPositions = NULL;
    }
    if (fLanguageBreakEngines) {
        delete fLanguageBreakEngines;
        fLanguageBreakEngines = NULL;
    }
    if (fUnhandledBreakEngine) {
        delete fUnhandledBreakEngine;
        fUnhandledBreakEngine = NULL;
    }
}

/**
 * Assignment operator.  Sets this iterator to have the same behavior,
 * and iterate over the same text, as the one passed in.
 */
RuleBasedBreakIterator&
RuleBasedBreakIterator::operator=(const RuleBasedBreakIterator& that) {
    if (this == &that) {
        return *this;
    }
    reset();    // Delete break cache information
    fBreakType = that.fBreakType;
    if (fLanguageBreakEngines != NULL) {
        delete fLanguageBreakEngines;
        fLanguageBreakEngines = NULL;   // Just rebuild for now
    }
    // TODO: clone fLanguageBreakEngines from "that"
    UErrorCode status = U_ZERO_ERROR;
    fText = utext_clone(fText, that.fText, FALSE, TRUE, &status);
    if (that.fCharIter != NULL) {
        fCharIter = that.fCharIter->clone();
    }


    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    if (that.fData != NULL) {
        fData = that.fData->addReference();
    }
    fTrace = that.fTrace;

    return *this;
}



//-----------------------------------------------------------------------------
//
//    init()      Shared initialization routine.   Used by all the constructors.
//                Initializes all fields, leaving the object in a consistent state.
//
//-----------------------------------------------------------------------------
UBool RuleBasedBreakIterator::fTrace = FALSE;
void RuleBasedBreakIterator::init() {
    UErrorCode  status    = U_ZERO_ERROR;
    fText                 = utext_openUChars(NULL, NULL, 0, &status);
    fCharIter             = NULL;
    fData                 = NULL;
    fLastRuleStatusIndex  = 0;
    fLastStatusIndexValid = TRUE;
    fDictionaryCharCount  = 0;
    fBreakType            = -1;

    fCachedBreakPositions    = NULL;
    fLanguageBreakEngines    = NULL;
    fUnhandledBreakEngine    = NULL;
    fNumCachedBreakPositions = 0;
    fPositionInCache         = 0;

#ifdef RBBI_DEBUG
    static UBool debugInitDone = FALSE;
    if (debugInitDone == FALSE) {
        char *debugEnv = getenv("U_RBBIDEBUG");
        if (debugEnv && uprv_strstr(debugEnv, "trace")) {
            fTrace = TRUE;
        }
        debugInitDone = TRUE;
    }
#endif
}



//-----------------------------------------------------------------------------
//
//    clone - Returns a newly-constructed RuleBasedBreakIterator with the same
//            behavior, and iterating over the same text, as this one.
//            Virtual function: does the right thing with subclasses.
//
//-----------------------------------------------------------------------------
BreakIterator*
RuleBasedBreakIterator::clone(void) const {
    return new RuleBasedBreakIterator(*this);
}

/**
 * Equality operator.  Returns TRUE if both BreakIterators are of the
 * same class, have the same behavior, and iterate over the same text.
 */
UBool
RuleBasedBreakIterator::operator==(const BreakIterator& that) const {
    if (that.getDynamicClassID() != getDynamicClassID()) {
        return FALSE;
    }

    const RuleBasedBreakIterator& that2 = (const RuleBasedBreakIterator&) that;

    if (!utext_equals(fText, that2.fText)) {
        // The two break iterators are operating on different text,
        //   or have a different interation position.
        return FALSE;
    };

    // TODO:  need a check for when in a dictionary region at different offsets.

    if (that2.fData == fData ||
        (fData != NULL && that2.fData != NULL && *that2.fData == *fData)) {
            // The two break iterators are using the same rules.
            return TRUE;
        }
    return FALSE;
}

/**
 * Compute a hash code for this BreakIterator
 * @return A hash code
 */
int32_t
RuleBasedBreakIterator::hashCode(void) const {
    int32_t   hash = 0;
    if (fData != NULL) {
        hash = fData->hashCode();
    }
    return hash;
}


void RuleBasedBreakIterator::setText(UText *ut, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    reset();
    fText = utext_clone(fText, ut, FALSE, TRUE, &status);
    delete fCharIter;    // A CharacterIterator will be lazily created if user asks.
    fCharIter = NULL;    //   It's obsolete, usually wont be needed.
    this->first();
}


UText *RuleBasedBreakIterator::getUText(UText *fillIn, UErrorCode &status) const {
    UText *result = utext_clone(fillIn, fText, FALSE, TRUE, &status);  
    return result;
}



/**
 * Returns the description used to create this iterator
 */
const UnicodeString&
RuleBasedBreakIterator::getRules() const {
    if (fData != NULL) {
        return fData->getRuleSourceString();
    } else {
        static const UnicodeString *s;
        if (s == NULL) {
            // TODO:  something more elegant here.
            //        perhaps API should return the string by value.
            //        Note:  thread unsafe init & leak are semi-ok, better than
            //               what was before.  Sould be cleaned up, though.
            s = new UnicodeString;
        }
        return *s;
    }
}

//=======================================================================
// BreakIterator overrides
//=======================================================================

/**
 * Return a CharacterIterator over the text being analyzed.  
 * @return An iterator over the text being analyzed.
 */
CharacterIterator&
RuleBasedBreakIterator::getText() const {
    RuleBasedBreakIterator* nonConstThis = (RuleBasedBreakIterator*)this;

    //
    if (nonConstThis->fCharIter == NULL) {
        nonConstThis->fCharIter = new CharacterIteratorUT(fText);
    }
    return *nonConstThis->fCharIter;
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::adoptText(CharacterIterator* newText) {
    delete fCharIter;
    fCharIter = newText;
    UErrorCode status = U_ZERO_ERROR;
    reset();
    if (newText==NULL || newText->startIndex() != 0) {   
        // startIndex !=0 wants to be an error, but there's no way to report it.
        // Make the iterator text be an empty string.
        fText = utext_openUChars(fText, NULL, 0, &status);
    } else {
        fText = utext_openCharacterIterator(fText, newText, &status);
    }
    this->first();
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::setText(const UnicodeString& newText) {
    UErrorCode status = U_ZERO_ERROR;
    reset();
    fText = utext_openConstUnicodeString(fText, &newText, &status);
    delete fCharIter;
    fCharIter = NULL;
    this->first();
}



/**
 * Sets the current iteration position to the beginning of the text.
 * @return The offset of the beginning of the text.
 */
int32_t RuleBasedBreakIterator::first(void) {
    reset();
    fLastRuleStatusIndex  = 0;
    fLastStatusIndexValid = TRUE;
    //if (fText == NULL)
    //    return BreakIterator::DONE;

    utext_setNativeIndex(fText, 0);
    return 0;
}

/**
 * Sets the current iteration position to the end of the text.
 * @return The text's past-the-end offset.
 */
int32_t RuleBasedBreakIterator::last(void) {
    reset();
    if (fText == NULL) {
        fLastRuleStatusIndex  = 0;
        fLastStatusIndexValid = TRUE;
        return BreakIterator::DONE;
    }

    fLastStatusIndexValid = FALSE;
    int32_t pos = (int32_t)utext_nativeLength(fText);
    utext_setNativeIndex(fText, pos);
    return pos;
}

/**
 * Advances the iterator either forward or backward the specified number of steps.
 * Negative values move backward, and positive values move forward.  This is
 * equivalent to repeatedly calling next() or previous().
 * @param n The number of steps to move.  The sign indicates the direction
 * (negative is backwards, and positive is forwards).
 * @return The character offset of the boundary position n boundaries away from
 * the current one.
 */
int32_t RuleBasedBreakIterator::next(int32_t n) {
    int32_t result = current();
    while (n > 0) {
        result = next();
        --n;
    }
    while (n < 0) {
        result = previous();
        ++n;
    }
    return result;
}

/**
 * Advances the iterator to the next boundary position.
 * @return The position of the first boundary after this one.
 */
int32_t RuleBasedBreakIterator::next(void) {
    // if we have cached break positions and we're still in the range
    // covered by them, just move one step forward in the cache
    if (fCachedBreakPositions != NULL) {
        if (fPositionInCache < fNumCachedBreakPositions - 1) {
            ++fPositionInCache;
            int32_t pos = fCachedBreakPositions[fPositionInCache];
            utext_setNativeIndex(fText, pos);
            return pos;
        }
        else {
            reset();
        }
    }

    int32_t startPos = current();
    int32_t result = handleNext(fData->fForwardTable);
    if (fDictionaryCharCount > 0) {
        result = checkDictionary(startPos, result, FALSE);
    }
    return result;
}

/**
 * Advances the iterator backwards, to the last boundary preceding this one.
 * @return The position of the last boundary position preceding this one.
 */
int32_t RuleBasedBreakIterator::previous(void) {
    int32_t result;
    int32_t startPos;

    // if we have cached break positions and we're still in the range
    // covered by them, just move one step backward in the cache
    if (fCachedBreakPositions != NULL) {
        if (fPositionInCache > 0) {
            --fPositionInCache;
            int32_t pos = fCachedBreakPositions[fPositionInCache];
            utext_setNativeIndex(fText, pos);
            return pos;
        }
        else {
            reset();
        }
    }

    // if we're already sitting at the beginning of the text, return DONE
    if (fText == NULL || (startPos = current()) == 0) {
        fLastRuleStatusIndex  = 0;
        fLastStatusIndexValid = TRUE;
        return BreakIterator::DONE;
    }

    if (fData->fSafeRevTable != NULL || fData->fSafeFwdTable != NULL) {
        result = handlePrevious(fData->fReverseTable);
        if (fDictionaryCharCount > 0) {
            result = checkDictionary(result, startPos, TRUE);
        }
        return result;
    }

    // old rule syntax
    // set things up.  handlePrevious() will back us up to some valid
    // break position before the current position (we back our internal
    // iterator up one step to prevent handlePrevious() from returning
    // the current position), but not necessarily the last one before

    // where we started

    int32_t start = current();

    utext_previous32(fText);
    int32_t lastResult    = handlePrevious(fData->fReverseTable);
    if (lastResult == UBRK_DONE) {
        lastResult = 0;
        utext_setNativeIndex(fText, 0);
    }
    result = lastResult;
    int32_t lastTag       = 0;
    UBool   breakTagValid = FALSE;

    // iterate forward from the known break position until we pass our
    // starting point.  The last break position before the starting
    // point is our return value

    for (;;) {
        result         = next();
        if (result == BreakIterator::DONE || result >= start) {
            break;
        }
        lastResult     = result;
        lastTag        = fLastRuleStatusIndex;
        breakTagValid  = TRUE;
    }

    // fLastBreakTag wants to have the value for section of text preceding
    // the result position that we are to return (in lastResult.)  If
    // the backwards rules overshot and the above loop had to do two or more
    // next()s to move up to the desired return position, we will have a valid
    // tag value. But, if handlePrevious() took us to exactly the correct result positon,
    // we wont have a tag value for that position, which is only set by handleNext().

    // set the current iteration position to be the last break position
    // before where we started, and then return that value
    utext_setNativeIndex(fText, lastResult);
    fLastRuleStatusIndex  = lastTag;       // for use by getRuleStatus()
    fLastStatusIndexValid = breakTagValid;

    // No need to check the dictionary; it will have been handled by
    // next()

    return lastResult;
}

/**
 * Sets the iterator to refer to the first boundary position following
 * the specified position.
 * @offset The position from which to begin searching for a break position.
 * @return The position of the first break after the current position.
 */
int32_t RuleBasedBreakIterator::following(int32_t offset) {
    // if we have cached break positions and offset is in the range
    // covered by them, use them
    // TODO: could use binary search
    // TODO: what if offset is outside range, but break is not?
    if (fCachedBreakPositions != NULL) {
        if (offset >= fCachedBreakPositions[0]
                && offset < fCachedBreakPositions[fNumCachedBreakPositions - 1]) {
            fPositionInCache = 0;
            // We are guaranteed not to leave the array due to range test above
            while (offset >= fCachedBreakPositions[fPositionInCache]) {
                ++fPositionInCache;
            }
            int32_t pos = fCachedBreakPositions[fPositionInCache];
            utext_setNativeIndex(fText, pos);
            return pos;
        }
        else {
            reset();
        }
    }

    // if the offset passed in is already past the end of the text,
    // just return DONE; if it's before the beginning, return the
    // text's starting offset
    fLastRuleStatusIndex  = 0;
    fLastStatusIndexValid = TRUE;
    if (fText == NULL || offset >= utext_nativeLength(fText)) {
        last();
        return next();
    }
    else if (offset < 0) {
        return first();
    }

    // otherwise, set our internal iteration position (temporarily)
    // to the position passed in.  If this is the _beginning_ position,
    // then we can just use next() to get our return value

    int32_t result = 0;

    if (fData->fSafeRevTable != NULL) {
        // new rule syntax
        utext_setNativeIndex(fText, offset);
        // move forward one codepoint to prepare for moving back to a
        // safe point.
        // this handles offset being between a supplementary character
        utext_next32(fText);
        // handlePrevious will move most of the time to < 1 boundary away
        handlePrevious(fData->fSafeRevTable);
        int32_t result = next();
        while (result <= offset) {
            result = next();
        }
        return result;
    }
    if (fData->fSafeFwdTable != NULL) {
        // backup plan if forward safe table is not available
        utext_setNativeIndex(fText, offset);
        utext_previous32(fText);
        // handle next will give result >= offset
        handleNext(fData->fSafeFwdTable);
        // previous will give result 0 or 1 boundary away from offset,
        // most of the time
        // we have to
        int32_t oldresult = previous();
        while (oldresult > offset) {
            int32_t result = previous();
            if (result <= offset) {
                return oldresult;
            }
            oldresult = result;
        }
        int32_t result = next();
        if (result <= offset) {
            return next();
        }
        return result;
    }
    // otherwise, we have to sync up first.  Use handlePrevious() to back
    // up to a known break position before the specified position (if
    // we can determine that the specified position is a break position,
    // we don't back up at all).  This may or may not be the last break
    // position at or before our starting position.  Advance forward
    // from here until we've passed the starting position.  The position
    // we stop on will be the first break position after the specified one.
    // old rule syntax

    utext_setNativeIndex(fText, offset);
    if (offset == 0) {
        return next();
    }
    result = previous();

    while (result != BreakIterator::DONE && result <= offset) {
        result = next();
    }

    return result;
}

/**
 * Sets the iterator to refer to the last boundary position before the
 * specified position.
 * @offset The position to begin searching for a break from.
 * @return The position of the last boundary before the starting position.
 */
int32_t RuleBasedBreakIterator::preceding(int32_t offset) {
    // if we have cached break positions and offset is in the range
    // covered by them, use them
    if (fCachedBreakPositions != NULL) {
        // TODO: binary search?
        // TODO: What if offset is outside range, but break is not?
        if (offset > fCachedBreakPositions[0]
                && offset <= fCachedBreakPositions[fNumCachedBreakPositions - 1]) {
            fPositionInCache = 0;
            while (fPositionInCache < fNumCachedBreakPositions
                   && offset > fCachedBreakPositions[fPositionInCache])
                ++fPositionInCache;
            --fPositionInCache;
            utext_setNativeIndex(fText, fCachedBreakPositions[fPositionInCache]);
            return fCachedBreakPositions[fPositionInCache];
        }
        else {
            reset();
        }
    }

    // if the offset passed in is already past the end of the text,
    // just return DONE; if it's before the beginning, return the
    // text's starting offset
    if (fText == NULL || offset > utext_nativeLength(fText)) {
        // return BreakIterator::DONE;
        return last();
    }
    else if (offset < 0) {
        return first();
    }

    // if we start by updating the current iteration position to the
    // position specified by the caller, we can just use previous()
    // to carry out this operation

    if (fData->fSafeFwdTable != NULL) {
        // new rule syntax
        utext_setNativeIndex(fText, offset);
        int32_t newOffset = (int32_t)utext_getNativeIndex(fText);
        if (newOffset != offset) {
            // Will come here if specified offset was not a code point boundary AND
            //   the underlying implmentation is using UText, which snaps any non-code-point-boundary
            //   indices to the containing code point.
            // For breakitereator::preceding only, these non-code-point indices need to be moved
            //   up to refer to the following codepoint.
            utext_next32(fText);
            offset = (int32_t)utext_getNativeIndex(fText);
        }

        // TODO:  (synwee) would it be better to just check for being in the middle of a surrogate pair,
        //        rather than adjusting the position unconditionally?
        //        (Change would interact with safe rules.)
        // TODO:  change RBBI behavior for off-boundary indices to match that of UText?
        //        affects only preceding(), seems cleaner, but is slightly different.
        utext_previous32(fText);
        handleNext(fData->fSafeFwdTable);
        int32_t result = (int32_t)utext_getNativeIndex(fText);
        while (result >= offset) {
            result = previous();
        }
        return result;
    }
    if (fData->fSafeRevTable != NULL) {
        // backup plan if forward safe table is not available
        utext_setNativeIndex(fText, offset);
        utext_next32(fText);
        // handle previous will give result <= offset
        handlePrevious(fData->fSafeRevTable);

        // next will give result 0 or 1 boundary away from offset,
        // most of the time
        // we have to
        int32_t oldresult = next();
        while (oldresult < offset) {
            int32_t result = next();
            if (result >= offset) {
                return oldresult;
            }
            oldresult = result;
        }
        int32_t result = previous();
        if (result >= offset) {
            return previous();
        }
        return result;
    }

    // old rule syntax
    utext_setNativeIndex(fText, offset);
    return previous();
}

/**
 * Returns true if the specfied position is a boundary position.  As a side
 * effect, leaves the iterator pointing to the first boundary position at
 * or after "offset".
 * @param offset the offset to check.
 * @return True if "offset" is a boundary position.
 */
UBool RuleBasedBreakIterator::isBoundary(int32_t offset) {
    // the beginning index of the iterator is always a boundary position by definition
    if (offset == 0) {
        first();       // For side effects on current position, tag values.
        return TRUE;
    }

    if (offset == (int32_t)utext_nativeLength(fText)) {
        last();       // For side effects on current position, tag values.
        return TRUE;
    }

    // out-of-range indexes are never boundary positions
    if (offset < 0) {
        first();       // For side effects on current position, tag values.
        return FALSE;
    }

    if (offset > utext_nativeLength(fText)) {
        last();        // For side effects on current position, tag values.
        return FALSE;
    }

    // otherwise, we can use following() on the position before the specified
    // one and return true if the position we get back is the one the user
    // specified
    utext_previous32From(fText, offset);
    int32_t backOne = (int32_t)utext_getNativeIndex(fText);
    UBool    result  = following(backOne) == offset;
    return result;
}

/**
 * Returns the current iteration position.
 * @return The current iteration position.
 */
int32_t RuleBasedBreakIterator::current(void) const {
    int32_t  pos = (int32_t)utext_getNativeIndex(fText);
    return pos;
}
 
//=======================================================================
// implementation
//=======================================================================

//
// RBBIRunMode  -  the state machine runs an extra iteration at the beginning and end
//                 of user text.  A variable with this enum type keeps track of where we
//                 are.  The state machine only fetches user input while in the RUN mode.
//
enum RBBIRunMode {
    RBBI_START,     // state machine processing is before first char of input
    RBBI_RUN,       // state machine processing is in the user text
    RBBI_END        // state machine processing is after end of user text.
};


//-----------------------------------------------------------------------------------
//
//  handleNext(stateTable)
//     This method is the actual implementation of the rbbi next() method. 
//     It is not overridden by dictionary based break iterators.
//     This method initializes the state machine to state 1
//     and advances through the text character by character until we reach the end
//     of the text or the state machine transitions to state 0.  We update our return
//     value every time the state machine passes through an accepting state.
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handleNext(const RBBIStateTable *statetable) {
    int32_t             state;
    int16_t             category        = 0;
    RBBIRunMode         mode;
    
    RBBIStateTableRow  *row;
    UChar32             c;
    int32_t             lookaheadStatus = 0;
    int32_t             lookaheadTagIdx = 0;
    int32_t             result          = 0;
    int32_t             initialPosition = 0;
    int32_t             lookaheadResult = 0;
    UBool               lookAheadHardBreak = (statetable->fFlags & RBBI_LOOKAHEAD_HARD_BREAK) != 0;

    if (fTrace) {
        RBBIDebugPuts("Handle Next   pos   char  state category");
    }

    // No matter what, handleNext alway correctly sets the break tag value.
    fLastStatusIndexValid = TRUE;
    fLastRuleStatusIndex = 0;

    // if we're already at the end of the text, return DONE.
    c = utext_current32(fText);
    if (fData == NULL || c==U_SENTINEL) {
        return BreakIterator::DONE;
    }

    //  Set up the starting char.
    initialPosition = (int32_t)utext_getNativeIndex(fText); 
    result          = initialPosition;

    //  Set the initial state for the state machine
    state = START_STATE;
    row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));
    category = 3;
    mode     = RBBI_RUN;
    if (statetable->fFlags & RBBI_BOF_REQUIRED) {
        category = 2;
        mode     = RBBI_START;
    }


    // loop until we reach the end of the text or transition to state 0
    //
    for (;;) {
        if (utext_current32(fText)==U_SENTINEL) {
            // Reached end of input string.
            if (mode == RBBI_END) {
                // We have already run the loop one last time with the 
                //   character set to the psueudo {eof} value.  Now it is time
                //   to unconditionally bail out.
                if (lookaheadResult > result) {
                    // We ran off the end of the string with a pending look-ahead match.
                    // Treat this as if the look-ahead condition had been met, and return
                    //  the match at the / position from the look-ahead rule.
                    result               = lookaheadResult;
                    fLastRuleStatusIndex = lookaheadTagIdx;
                    lookaheadStatus = 0;
                } else if (result == initialPosition) {
                    // Ran off end, no match found.
                    // move forward one
                    utext_setNativeIndex(fText, initialPosition);
                    utext_next32(fText);
                }
                break;
            }
            // Run the loop one last time with the fake end-of-input character category.
            mode = RBBI_END;
            category = 1;
        }

        //
        // Get the char category.  An incoming category of 1 or 2 means that
        //      we are preset for doing the beginning or end of input, and
        //      that we shouldn't get a category from an actual text input character.
        //
        if (mode == RBBI_RUN) {
            // look up the current character's character category, which tells us
            // which column in the state table to look at.
            // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
            //        not the size of the character going in, which is a UChar32.
            //
            UTRIE_GET16(&fData->fTrie, c, category);

            // Check the dictionary bit in the character's category.
            //    Counter is only used by dictionary based iterators (subclasses).
            //    Chars that need to be handled by a dictionary have a flag bit set
            //    in their category values.
            //
            if ((category & 0x4000) != 0)  {
                fDictionaryCharCount++;
                //  And off the dictionary flag bit.
                category &= ~0x4000;
            }
        }

        #ifdef RBBI_DEBUG
            if (fTrace) {
                RBBIDebugPrintf("             %4d   ", utext_getNativeIndex(fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //
        state = row->fNextState[category];
        row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));

        // Advance to the next character.  
        // If this is a beginning-of-input loop iteration, don't advance
        //    the input position.  The next iteration will be processing the
        //    first real input character.
        if (mode == RBBI_RUN) {
            //  TODO:  rework loop to use UText's prefered posincrement style of operation.
            utext_next32(fText);
            c = utext_current32(fText);
        } else {
            if (mode == RBBI_START) {
                mode = RBBI_RUN;
            }
        }

        if (row->fAccepting == -1) {
            // Match found, common case.
            result = (int32_t)utext_getNativeIndex(fText);
            fLastRuleStatusIndex = row->fTagIdx;   // Remember the break status (tag) values.
        }

        if (row->fLookAhead != 0) {
            if (lookaheadStatus != 0
                && row->fAccepting == lookaheadStatus) {
                // Lookahead match is completed.  
                result               = lookaheadResult;
                fLastRuleStatusIndex = lookaheadTagIdx;
                lookaheadStatus      = 0;
                // TODO:  make a standalone hard break in a rule work.
                if (lookAheadHardBreak) {
                    utext_setNativeIndex(fText, result);
                    return result;
                }
                // Look-ahead completed, but other rules may match further.  Continue on
                //  TODO:  junk this feature?  I don't think it's used anywhwere.
                goto continueOn;
            }

            int32_t  r = (int32_t)utext_getNativeIndex(fText);
            lookaheadResult = r;
            lookaheadStatus = row->fLookAhead;
            lookaheadTagIdx = row->fTagIdx;
            goto continueOn;
        }


        if (row->fAccepting != 0) {
            // Because this is an accepting state, any in-progress look-ahead match
            //   is no longer relavant.  Clear out the pending lookahead status.
            lookaheadStatus = 0;           // clear out any pending look-ahead match.
        }

continueOn:
        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // We have advanced through the string until it is certain that no
            //   longer match is possible, no matter what characters follow.
            break;
        }

    }

    // The state machine is done.  Check whether it found a match...

    // If the iterator failed to advance in the match engine, force it ahead by one.
    //   (This really indicates a defect in the break rules.  They should always match
    //    at least one character.)
    if (result == initialPosition) {
        utext_setNativeIndex(fText, initialPosition);
        utext_next32(fText);
        result = (int32_t)utext_getNativeIndex(fText);
    }

    // Leave the iterator at our result position.
    utext_setNativeIndex(fText, result);
    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}



//-----------------------------------------------------------------------------------
//
//  handlePrevious()
//
//      Iterate backwards, according to the logic of the reverse rules.
//      This version handles the exact style backwards rules.
//
//      The logic of this function is very similar to handleNext(), above.
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handlePrevious(const RBBIStateTable *statetable) {
    int32_t             state;
    int16_t             category        = 0;
    RBBIRunMode         mode;
    RBBIStateTableRow  *row;
    UChar32             c;
    int32_t             lookaheadStatus = 0;
    int32_t             result          = 0;
    int32_t             initialPosition = 0;
    int32_t             lookaheadResult = 0;
    UBool               lookAheadHardBreak = (statetable->fFlags & RBBI_LOOKAHEAD_HARD_BREAK) != 0;

    if (fTrace) {
        RBBIDebugPuts("Handle Previous   pos   char  state category");
    }

    // handlePrevious() never gets the rule status.
    // Flag the status as invalid; if the user ever asks for status, we will need
    // to back up, then re-find the break position using handleNext(), which does
    // get the status value.
    fLastStatusIndexValid = FALSE;
    fLastRuleStatusIndex = 0;

    // if we're already at the start of the text, return DONE.
    if (fText == NULL || fData == NULL || utext_getNativeIndex(fText)==0) {
        return BreakIterator::DONE;
    }

    //  Set up the starting char.
    initialPosition = (int32_t)utext_getNativeIndex(fText);
    result          = initialPosition;
    c               = utext_previous32(fText);

    //  Set the initial state for the state machine
    state = START_STATE;
    row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));
    category = 3;
    mode     = RBBI_RUN;
    if (statetable->fFlags & RBBI_BOF_REQUIRED) {
        category = 2;
        mode     = RBBI_START;
    }


    // loop until we reach the start of the text or transition to state 0
    //
    for (;;) {
        if (c == U_SENTINEL) {
            // Reached end of input string.
            if (mode == RBBI_END || 
                *(int32_t *)fData->fHeader->fFormatVersion == 1 ) {
                // We have already run the loop one last time with the 
                //   character set to the psueudo {eof} value.  Now it is time
                //   to unconditionally bail out.
                //  (Or we have an old format binary rule file that does not support {eof}.)
                if (lookaheadResult < result) {
                    // We ran off the end of the string with a pending look-ahead match.
                    // Treat this as if the look-ahead condition had been met, and return
                    //  the match at the / position from the look-ahead rule.
                    result               = lookaheadResult;
                    lookaheadStatus = 0;
                } else if (result == initialPosition) {
                    // Ran off start, no match found.
                    // move one index one (towards the start, since we are doing a previous())
                    utext_setNativeIndex(fText, initialPosition);
                    utext_previous32(fText);   // TODO:  shouldn't be necessary.  We're already at beginning.  Check.
                }
                break;
            }
            // Run the loop one last time with the fake end-of-input character category.
            mode = RBBI_END;
            category = 1;
        }

        //
        // Get the char category.  An incoming category of 1 or 2 means that
        //      we are preset for doing the beginning or end of input, and
        //      that we shouldn't get a category from an actual text input character.
        //
        if (mode == RBBI_RUN) {
            // look up the current character's character category, which tells us
            // which column in the state table to look at.
            // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
            //        not the size of the character going in, which is a UChar32.
            //
            UTRIE_GET16(&fData->fTrie, c, category);

            // Check the dictionary bit in the character's category.
            //    Counter is only used by dictionary based iterators (subclasses).
            //    Chars that need to be handled by a dictionary have a flag bit set
            //    in their category values.
            //
            if ((category & 0x4000) != 0)  {
                fDictionaryCharCount++;
                //  And off the dictionary flag bit.
                category &= ~0x4000;
            }
        }

        #ifdef RBBI_DEBUG
            if (fTrace) {
                RBBIDebugPrintf("             %4d   ", (int32_t)utext_getNativeIndex(fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //
        state = row->fNextState[category];
        row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));

        if (row->fAccepting == -1) {
            // Match found, common case.
            result = (int32_t)utext_getNativeIndex(fText);
        }

        if (row->fLookAhead != 0) {
            if (lookaheadStatus != 0
                && row->fAccepting == lookaheadStatus) {
                // Lookahead match is completed.  
                result               = lookaheadResult;
                lookaheadStatus      = 0;
                // TODO:  make a standalone hard break in a rule work.
                if (lookAheadHardBreak) {
                    utext_setNativeIndex(fText, result);
                    return result;
                }
                // Look-ahead completed, but other rules may match further.  Continue on
                //  TODO:  junk this feature?  I don't think it's used anywhwere.
                goto continueOn;
            }

            int32_t  r = (int32_t)utext_getNativeIndex(fText);
            lookaheadResult = r;
            lookaheadStatus = row->fLookAhead;
            goto continueOn;
        }


        if (row->fAccepting != 0) {
            // Because this is an accepting state, any in-progress look-ahead match
            //   is no longer relavant.  Clear out the pending lookahead status.
            lookaheadStatus = 0;    
        }

continueOn:
        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // We have advanced through the string until it is certain that no
            //   longer match is possible, no matter what characters follow.
            break;
        }

        // Move (backwards) to the next character to process.  
        // If this is a beginning-of-input loop iteration, don't advance
        //    the input position.  The next iteration will be processing the
        //    first real input character.
        if (mode == RBBI_RUN) {
            c = utext_previous32(fText);
        } else {            
            if (mode == RBBI_START) {
                mode = RBBI_RUN;
            }
        }
    }

    // The state machine is done.  Check whether it found a match...

    // If the iterator failed to advance in the match engine, force it ahead by one.
    //   (This really indicates a defect in the break rules.  They should always match
    //    at least one character.)
    if (result == initialPosition) {
        utext_setNativeIndex(fText, initialPosition);
        utext_previous32(fText);
        result = (int32_t)utext_getNativeIndex(fText);
    }

    // Leave the iterator at our result position.
    utext_setNativeIndex(fText, result);
    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}


void
RuleBasedBreakIterator::reset()
{
    if (fCachedBreakPositions) {
        uprv_free(fCachedBreakPositions);
    }
    fCachedBreakPositions = NULL;
    fNumCachedBreakPositions = 0;
    fDictionaryCharCount = 0;
    fPositionInCache = 0;
}



//-------------------------------------------------------------------------------
//
//   getRuleStatus()   Return the break rule tag associated with the current
//                     iterator position.  If the iterator arrived at its current
//                     position by iterating forwards, the value will have been
//                     cached by the handleNext() function.
//
//                     If no cached status value is available, the status is
//                     found by doing a previous() followed by a next(), which
//                     leaves the iterator where it started, and computes the
//                     status while doing the next().
//
//-------------------------------------------------------------------------------
void RuleBasedBreakIterator::makeRuleStatusValid() {
    if (fLastStatusIndexValid == FALSE) {
        //  No cached status is available.
        if (fText == NULL || current() == 0) {
            //  At start of text, or there is no text.  Status is always zero.
            fLastRuleStatusIndex = 0;
            fLastStatusIndexValid = TRUE;
        } else {
            //  Not at start of text.  Find status the tedious way.
            int32_t pa = current();
            previous();
            if (fNumCachedBreakPositions > 0) {
                reset();                // Blow off the dictionary cache
            }
            int32_t pb = next();
            if (pa != pb) {
                // note: the if (pa != pb) test is here only to eliminate warnings for
                //       unused local variables on gcc.  Logically, it isn't needed.
                U_ASSERT(pa == pb);
            }
        }
    }
    U_ASSERT(fLastRuleStatusIndex >= 0  &&  fLastRuleStatusIndex < fData->fStatusMaxIdx);
}


int32_t  RuleBasedBreakIterator::getRuleStatus() const {
    RuleBasedBreakIterator *nonConstThis  = (RuleBasedBreakIterator *)this;
    nonConstThis->makeRuleStatusValid();

    // fLastRuleStatusIndex indexes to the start of the appropriate status record
    //                                                 (the number of status values.)
    //   This function returns the last (largest) of the array of status values.
    int32_t  idx = fLastRuleStatusIndex + fData->fRuleStatusTable[fLastRuleStatusIndex];
    int32_t  tagVal = fData->fRuleStatusTable[idx];

    return tagVal;
}




int32_t RuleBasedBreakIterator::getRuleStatusVec(
             int32_t *fillInVec, int32_t capacity, UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return 0;
    }

    RuleBasedBreakIterator *nonConstThis  = (RuleBasedBreakIterator *)this;
    nonConstThis->makeRuleStatusValid();
    int32_t  numVals = fData->fRuleStatusTable[fLastRuleStatusIndex];
    int32_t  numValsToCopy = numVals;
    if (numVals > capacity) {
        status = U_BUFFER_OVERFLOW_ERROR;
        numValsToCopy = capacity;
    }
    int i;
    for (i=0; i<numValsToCopy; i++) {
        fillInVec[i] = fData->fRuleStatusTable[fLastRuleStatusIndex + i + 1];
    }
    return numVals;
}



//-------------------------------------------------------------------------------
//
//   getBinaryRules        Access to the compiled form of the rules,
//                         for use by build system tools that save the data
//                         for standard iterator types.
//
//-------------------------------------------------------------------------------
const uint8_t  *RuleBasedBreakIterator::getBinaryRules(uint32_t &length) {
    const uint8_t  *retPtr = NULL;
    length = 0;

    if (fData != NULL) {
        retPtr = (const uint8_t *)fData->fHeader;
        length = fData->fHeader->fLength;
    }
    return retPtr;
}




//-------------------------------------------------------------------------------
//
//  BufferClone       TODO:  In my (Andy) opinion, this function should be deprecated.
//                    Saving one heap allocation isn't worth the trouble.
//                    Cloning shouldn't be done in tight loops, and
//                    making the clone copy involves other heap operations anyway.
//                    And the application code for correctly dealing with buffer
//                    size problems and the eventual object destruction is ugly.
//
//-------------------------------------------------------------------------------
BreakIterator *  RuleBasedBreakIterator::createBufferClone(void *stackBuffer,
                                   int32_t &bufferSize,
                                   UErrorCode &status)
{
    if (U_FAILURE(status)){
        return NULL;
    }

    //
    //  If user buffer size is zero this is a preflight operation to
    //    obtain the needed buffer size, allowing for worst case misalignment.
    //
    if (bufferSize == 0) {
        bufferSize = sizeof(RuleBasedBreakIterator) + U_ALIGNMENT_OFFSET_UP(0);
        return NULL;
    }


    //
    //  Check the alignment and size of the user supplied buffer.
    //  Allocate heap memory if the user supplied memory is insufficient.
    //
    char    *buf   = (char *)stackBuffer;
    uint32_t s      = bufferSize;

    if (stackBuffer == NULL) {
        s = 0;   // Ignore size, force allocation if user didn't give us a buffer.
    }
    if (U_ALIGNMENT_OFFSET(stackBuffer) != 0) {
        uint32_t offsetUp = (uint32_t)U_ALIGNMENT_OFFSET_UP(buf);
        s   -= offsetUp;
        buf += offsetUp;
    }
    if (s < sizeof(RuleBasedBreakIterator)) {
        buf = (char *) new RuleBasedBreakIterator;
        if (buf == 0) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        status = U_SAFECLONE_ALLOCATED_WARNING;
    }

    //
    //  Clone the object.
    //    TODO:  using an overloaded operator new to directly initialize the
    //           copy in the user's buffer would be better, but it doesn't seem
    //           to get along with namespaces.  Investigate why.
    //
    //           The memcpy is only safe with an empty (default constructed)
    //           break iterator.  Use on others can screw up reference counts
    //           to data.  memcpy-ing objects is not really a good idea...
    //
    RuleBasedBreakIterator localIter;        // Empty break iterator, source for memcpy
    RuleBasedBreakIterator *clone = (RuleBasedBreakIterator *)buf;
    uprv_memcpy(clone, &localIter, sizeof(RuleBasedBreakIterator)); // clone = empty, but initialized, iterator.
    *clone = *this;                          // clone = the real one we want.
    if (status != U_SAFECLONE_ALLOCATED_WARNING) {
        clone->fBufferClone = TRUE;
    }

    return clone;
}


//-------------------------------------------------------------------------------
//
//  isDictionaryChar      Return true if the category lookup for this char
//                        indicates that it is in the set of dictionary lookup
//                        chars.
//
//                        This function is intended for use by dictionary based
//                        break iterators.
//
//-------------------------------------------------------------------------------
UBool RuleBasedBreakIterator::isDictionaryChar(UChar32   c) {
    if (fData == NULL) {
        return FALSE;
    }
    uint16_t category;
    UTRIE_GET16(&fData->fTrie, c, category);
    return (category & 0x4000) != 0;
}


//-------------------------------------------------------------------------------
//
//  checkDictionary       This function handles all processing of characters in
//                        the "dictionary" set. It will determine the appropriate
//                        course of action, and possibly set up a cache in the
//                        process.
//
//-------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::checkDictionary(int32_t startPos,
                            int32_t endPos,
                            UBool reverse) {
    // Reset the old break cache first.
    uint32_t dictionaryCount = fDictionaryCharCount;
    reset();

    if (dictionaryCount <= 1 || (endPos - startPos) <= 1) {
        return (reverse ? startPos : endPos);
    }
    
    // Starting from the starting point, scan towards the proposed result,
    // looking for the first dictionary character (which may be the one
    // we're on, if we're starting in the middle of a range).
    utext_setNativeIndex(fText, reverse ? endPos : startPos);
    if (reverse) {
        utext_previous32(fText);
    }
    
    int32_t rangeStart = startPos;
    int32_t rangeEnd = endPos;

    uint16_t    category;
    int32_t     current;
    UErrorCode  status = U_ZERO_ERROR;
    UStack      breaks(status);
    int32_t     foundBreakCount = 0;
    UChar32     c = utext_current32(fText);

    UTRIE_GET16(&fData->fTrie, c, category);
    
    // Is the character we're starting on a dictionary character? If so, we
    // need to back up to include the entire run; otherwise the results of
    // the break algorithm will differ depending on where we start. Since
    // the result is cached and there is typically a non-dictionary break
    // within a small number of words, there should be little performance impact.
    if (category & 0x4000) {
        if (reverse) {
            do {
                utext_next32(fText);          // TODO:  recast to work directly with postincrement.
                c = utext_current32(fText);
                UTRIE_GET16(&fData->fTrie, c, category);
            } while (c != U_SENTINEL && (category & 0x4000));
            // Back up to the last dictionary character
            rangeEnd = (int32_t)utext_getNativeIndex(fText);
            if (c == U_SENTINEL) {
                // c = fText->last32();
                //   TODO:  why was this if needed?
                c = utext_previous32(fText);
            }
            else {
                c = utext_previous32(fText);
            }
        }
        else {
            do {
                c = utext_previous32(fText);
                UTRIE_GET16(&fData->fTrie, c, category);
            }
            while (c != U_SENTINEL && (category & 0x4000));
            // Back up to the last dictionary character
            if (c == U_SENTINEL) {
                // c = fText->first32();
                c = utext_current32(fText);
            }
            else {
                utext_next32(fText);
                c = utext_current32(fText);
            }
            rangeStart = (int32_t)utext_getNativeIndex(fText);;
        }
        UTRIE_GET16(&fData->fTrie, c, category);
    }
    
    // Loop through the text, looking for ranges of dictionary characters.
    // For each span, find the appropriate break engine, and ask it to find
    // any breaks within the span.
    while(U_SUCCESS(status)) {
        if (reverse) {
            while((current = (int32_t)utext_getNativeIndex(fText)) > rangeStart && (category & 0x4000) == 0) {
                c = utext_previous32(fText);
                UTRIE_GET16(&fData->fTrie, c, category);
            }
            if (current <= rangeStart) {
                break;
            }
        }
        else {
            while((current = (int32_t)utext_getNativeIndex(fText)) < rangeEnd && (category & 0x4000) == 0) {
                utext_next32(fText);           // TODO:  tweak for post-increment operation
                c = utext_current32(fText);
                UTRIE_GET16(&fData->fTrie, c, category);
            }
            if (current >= rangeEnd) {
                break;
            }
        }
        
        // We now have a dictionary character. Get the appropriate language object
        // to deal with it.
        const LanguageBreakEngine *lbe = getLanguageBreakEngine(c);
        
        // Ask the language object if there are any breaks. It will leave the text
        // pointer on the other side of its range, ready to search for the next one.
        if (lbe != NULL) {
            foundBreakCount += lbe->findBreaks(fText, rangeStart, rangeEnd, reverse, fBreakType, breaks);
        }
        
        // Reload the loop variables for the next go-round
        c = utext_current32(fText);
        UTRIE_GET16(&fData->fTrie, c, category);
    }
    
    // If we found breaks, build a new break cache. The first and last entries must
    // be the original starting and ending position.
    if (foundBreakCount > 0) {
        int32_t totalBreaks = foundBreakCount;
        if (startPos < breaks.elementAti(0)) {
            totalBreaks += 1;
        }
        if (endPos > breaks.peeki()) {
            totalBreaks += 1;
        }
        fCachedBreakPositions = (int32_t *)uprv_malloc(totalBreaks * sizeof(int32_t));
        if (fCachedBreakPositions != NULL) {
            int32_t out = 0;
            fNumCachedBreakPositions = totalBreaks;
            if (startPos < breaks.elementAti(0)) {
                fCachedBreakPositions[out++] = startPos;
            }
            for (int32_t i = 0; i < foundBreakCount; ++i) {
                fCachedBreakPositions[out++] = breaks.elementAti(i);
            }
            if (endPos > fCachedBreakPositions[out-1]) {
                fCachedBreakPositions[out] = endPos;
            }
            // If there are breaks, then by definition, we are replacing the original
            // proposed break by one of the breaks we found. Use following() and
            // preceding() to do the work. They should never recurse in this case.
            if (reverse) {
                return preceding(endPos - 1);
            }
            else {
                return following(startPos);
            }
        }
        // If the allocation failed, just fall through to the "no breaks found" case.
    }

    // If we get here, there were no language-based breaks. As a result, the
    // text pointer should be back to where it started, but set it just to
    // make sure.
    utext_setNativeIndex(fText, reverse ? startPos : endPos);
    return (reverse ? startPos : endPos);
}

static UStack *gLanguageBreakFactories = NULL;

U_NAMESPACE_END

// defined in ucln_cmn.h

/**
 * Release all static memory held by breakiterator.  
 */
U_CDECL_BEGIN
static UBool U_CALLCONV breakiterator_cleanup_dict(void) {
    if (gLanguageBreakFactories) {
        delete gLanguageBreakFactories;
        gLanguageBreakFactories = NULL;
    }
    return TRUE;
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV _deleteFactory(void *obj) {
    delete (LanguageBreakFactory *) obj;
}
U_CDECL_END
U_NAMESPACE_BEGIN

static const LanguageBreakEngine*
getLanguageBreakEngineFromFactory(UChar32 c, int32_t breakType)
{
    UBool       needsInit;
    UErrorCode  status = U_ZERO_ERROR;
    umtx_lock(NULL);
    needsInit = (UBool)(gLanguageBreakFactories == NULL);
    umtx_unlock(NULL);
    
    if (needsInit) {
        UStack  *factories = new UStack(_deleteFactory, NULL, status);
        if (U_SUCCESS(status)) {
            ICULanguageBreakFactory *builtIn = new ICULanguageBreakFactory(status);
            factories->push(builtIn, status);
#ifdef U_LOCAL_SERVICE_HOOK
            LanguageBreakFactory *extra = (LanguageBreakFactory *)uprv_svc_hook("languageBreakFactory", &status);
            if (extra != NULL) {
                factories->push(extra, status);
            }
#endif
        }
        umtx_lock(NULL);
        if (gLanguageBreakFactories == NULL) {
            gLanguageBreakFactories = factories;
            factories = NULL;
            ucln_common_registerCleanup(UCLN_COMMON_BREAKITERATOR_DICT, breakiterator_cleanup_dict);
        }
        umtx_unlock(NULL);
        delete factories;
    }
    
    if (gLanguageBreakFactories == NULL) {
        return NULL;
    }
    
    int32_t i = gLanguageBreakFactories->size();
    const LanguageBreakEngine *lbe = NULL;
    while (--i >= 0) {
        LanguageBreakFactory *factory = (LanguageBreakFactory *)(gLanguageBreakFactories->elementAt(i));
        lbe = factory->getEngineFor(c, breakType);
        if (lbe != NULL) {
            break;
        }
    }
    return lbe;
}


//-------------------------------------------------------------------------------
//
//  getLanguageBreakEngine  Find an appropriate LanguageBreakEngine for the
//                          the characer c.
//
//-------------------------------------------------------------------------------
const LanguageBreakEngine *
RuleBasedBreakIterator::getLanguageBreakEngine(UChar32 c) {
    const LanguageBreakEngine *lbe = NULL;
    UErrorCode status = U_ZERO_ERROR;
    
    if (fLanguageBreakEngines == NULL) {
        fLanguageBreakEngines = new UStack(status);
        if (U_FAILURE(status)) {
            delete fLanguageBreakEngines;
            fLanguageBreakEngines = 0;
            return NULL;
        }
    }
    
    int32_t i = fLanguageBreakEngines->size();
    while (--i >= 0) {
        lbe = (const LanguageBreakEngine *)(fLanguageBreakEngines->elementAt(i));
        if (lbe->handles(c, fBreakType)) {
            return lbe;
        }
    }
    
    // No existing dictionary took the character. See if a factory wants to
    // give us a new LanguageBreakEngine for this character.
    lbe = getLanguageBreakEngineFromFactory(c, fBreakType);
    
    // If we got one, use it and push it on our stack.
    if (lbe != NULL) {
        fLanguageBreakEngines->push((void *)lbe, status);
        // Even if we can't remember it, we can keep looking it up, so
        // return it even if the push fails.
        return lbe;
    }
    
    // No engine is forthcoming for this character. Add it to the
    // reject set. Create the reject break engine if needed.
    if (fUnhandledBreakEngine == NULL) {
        fUnhandledBreakEngine = new UnhandledEngine(status);
        if (U_SUCCESS(status) && fUnhandledBreakEngine == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        // Put it last so that scripts for which we have an engine get tried
        // first.
        fLanguageBreakEngines->insertElementAt(fUnhandledBreakEngine, 0, status);
        // If we can't insert it, or creation failed, get rid of it
        if (U_FAILURE(status)) {
            delete fUnhandledBreakEngine;
            fUnhandledBreakEngine = 0;
            return NULL;
        }
    }
    
    // Tell the reject engine about the character; at its discretion, it may
    // add more than just the one character.
    fUnhandledBreakEngine->handleCharacter(c, fBreakType);
        
    return fUnhandledBreakEngine;
}



int32_t RuleBasedBreakIterator::getBreakType() const {
    return fBreakType;
}

void RuleBasedBreakIterator::setBreakType(int32_t type) {
    fBreakType = type;
    reset();
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
