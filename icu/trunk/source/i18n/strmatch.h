/*
* Copyright (C) 2001, International Business Machines Corporation and others. All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   07/23/01    aliu        Creation.
**********************************************************************
*/
#ifndef STRMATCH_H
#define STRMATCH_H

#include "unicode/unistr.h"
#include "unicode/unifunct.h"
#include "unicode/unimatch.h"
#include "unicode/unirepl.h"

U_NAMESPACE_BEGIN

class TransliterationRuleData;

/**
 * An object that matches a fixed input string, implementing the
 * UnicodeMatcher API.  This object also implements the
 * UnicodeReplacer API, allowing it to emit the matched text as
 * output.  Since the match text may contain flexible match elements,
 * such as UnicodeSets, the emitted text is not the match pattern, but
 * instead a substring of the actual matched text.  Following
 * convention, the output text is the leftmost match seen up to this
 * point.
 *
 * A StringMatcher may represent a segment, in which case it has a
 * positive segment number.  This affects how the matcher converts
 * itself to a pattern but does not otherwise affect its function.
 *
 * A StringMatcher that is not a segment should not be used as a
 * UnicodeReplacer.
 */
class StringMatcher : public UnicodeFunctor, public UnicodeMatcher, public UnicodeReplacer {

 public:

    /**
     * Construct a matcher that matches the given pattern string.
     * @param theString the pattern to be matched, possibly containing
     * stand-ins that represent nested UnicodeMatcher objects.
     * @param segmentNum the segment number from 1..n, or 0 if this is
     * not a segment.
     * @param theData context object mapping stand-ins to
     * UnicodeMatcher objects.
     */
    StringMatcher(const UnicodeString& string,
                  int32_t start,
                  int32_t limit,
                  int32_t segmentNum,
                  const TransliterationRuleData& data);

    StringMatcher(const StringMatcher& o);
        
    /**
     * Destructor
     */
    virtual ~StringMatcher();

    /**
     * Implement UnicodeFunctor
     */
    virtual UnicodeFunctor* clone() const;

    /**
     * UnicodeFunctor API.  Cast 'this' to a UnicodeMatcher* pointer
     * and return the pointer.
     */
    virtual UnicodeMatcher* toMatcher() const;

    /**
     * UnicodeFunctor API.  Cast 'this' to a UnicodeReplacer* pointer
     * and return the pointer.
     */
    virtual UnicodeReplacer* toReplacer() const;

    /**
     * Implement UnicodeMatcher
     */
    virtual UMatchDegree matches(const Replaceable& text,
                                 int32_t& offset,
                                 int32_t limit,
                                 UBool incremental);

    /**
     * Implement UnicodeMatcher
     */
    virtual UnicodeString& toPattern(UnicodeString& result,
                                     UBool escapeUnprintable = FALSE) const;

    /**
     * Implement UnicodeMatcher
     */
    virtual UBool matchesIndexValue(uint8_t v) const;

    /**
     * Replace characters in 'text' from 'start' to 'limit' with the
     * output text of this object.  Update the 'cursor' parameter to
     * give the cursor position and return the length of the
     * replacement text.
     *
     * @param text the text to be matched
     * @param start inclusive start index of text to be replaced
     * @param limit exclusive end index of text to be replaced;
     * must be greater than or equal to start
     * @param cursor output parameter for the cursor position.
     * Not all replacer objects will update this, but in a complete
     * tree of replacer objects, representing the entire output side
     * of a transliteration rule, at least one must update it.
     * @return the number of 16-bit code units in the text replacing
     * the characters at offsets start..(limit-1) in text
     */
    virtual int32_t replace(Replaceable& text,
                            int32_t start,
                            int32_t limit,
                            int32_t& cursor);

    /**
     * Returns a string representation of this replacer.  If the
     * result of calling this function is passed to the appropriate
     * parser, typically TransliteratorParser, it will produce another
     * replacer that is equal to this one.
     * @param result the string to receive the pattern.  Previous
     * contents will be deleted.
     * @param escapeUnprintable if TRUE then convert unprintable
     * character to their hex escape representations, \\uxxxx or
     * \\Uxxxxxxxx.  Unprintable characters are defined by
     * Utility.isUnprintable().
     * @return a reference to 'result'.
     */
    virtual UnicodeString& toReplacerPattern(UnicodeString& result,
                                             UBool escapeUnprintable) const;

    /**
     * Remove any match data.  This must be called before performing a
     * set of matches with this segment.
     */
    void resetMatch();

 private:

    /**
     * The text to be matched.
     */
    UnicodeString pattern;

    /**
     * Context object that maps stand-ins to matcher and replacer
     * objects.
     */
    const TransliterationRuleData& data;

    /**
     * The segment number, 1-based, or 0 if not a segment.
     */
    int32_t segmentNumber;

    /**
     * Start offset, in the match text, of the <em>rightmost</em>
     * match.
     */
    int32_t matchStart;

    /**
     * Limit offset, in the match text, of the <em>rightmost</em>
     * match.
     */
    int32_t matchLimit;
};

U_NAMESPACE_END

#endif
