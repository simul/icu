/*
**********************************************************************
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   05/23/00    aliu        Creation.
**********************************************************************
*/
#include "unicode/utypes.h"
#include "unicode/translit.h"
#include "unicode/rbt.h"
#include "unicode/uniset.h"
#include "unicode/unicode.h"
#include "unicode/normlzr.h"
#include "unicode/uchar.h"
#include "transrt.h"
#include "testutil.h"

#define CASE(id,test) case id:                          \
                          name = #test;                 \
                          if (exec) {                   \
                              logln(#test "---");       \
                              logln((UnicodeString)""); \
                              test();                   \
                          }                             \
                          break

void
TransliteratorRoundTripTest::runIndexedTest(int32_t index, UBool exec,
                                   const char* &name, char* /*par*/) {
    switch (index) {
        CASE(0,TestHiragana);
        CASE(1,TestKatakana);
        //CASE(2,TestArabic);
        //CASE(3,TestHebrew);
        CASE(2,TestGreek);
        CASE(3,TestCyrillic);
        /*
        CASE(7,TestJamo);
        CASE(8,TestJamoHangul);
        */
        default: name = ""; break;
    }
}

//--------------------------------------------------------------------
// BitSet
//--------------------------------------------------------------------

/**
 * Tiny and incomplete BitSet.  Hardcoded to support 0..FFFF.
 */
class BitSet {
    int32_t bits[65536/32];

public:
    BitSet();
    ~BitSet();
    void clear();
    void set(int32_t x);
    UBool get(int32_t x) const;
};

BitSet::BitSet() {
    clear();
}

BitSet::~BitSet() {
}

void BitSet::clear() {
    int32_t *limit = bits + 65536/32;
    int32_t *p = bits;
    while (p < limit) *p++ = 0;
}

void BitSet::set(int32_t x) {
    x &= 0xFFFF;
    int32_t i = x / 32;
    int32_t bit = 1L << (x & 31);
    bits[i] |= bit;
}

UBool BitSet::get(int32_t x) const {
    x &= 0xFFFF;
    int32_t i = x / 32;
    int32_t bit = 1L << (x & 31);
    return (bits[i] & bit) != 0L;
}

//--------------------------------------------------------------------
// Legal
//--------------------------------------------------------------------

class Legal {
public:
    Legal() {}
    virtual ~Legal() {}
    virtual UBool is(const UnicodeString& sourceString) const {return TRUE;}
};

class LegalGreek : public Legal {
public:
    LegalGreek() {}
    virtual ~LegalGreek() {}

    virtual UBool is(const UnicodeString& sourceString) const;

    static UBool isVowel(UChar c);
    
    static UBool isRho(UChar c);
};

UBool LegalGreek::is(const UnicodeString& sourceString) const { 
    // Legal greek has breathing marks IFF there is a vowel or RHO at the start
    // IF it has them, it has exactly one.
    // IF it starts with a RHO, then the breathing mark must come before the second letter.
    // Since there are no surrogates in greek, don't worry about them
    UnicodeString decomp;
    UErrorCode ec = U_ZERO_ERROR;
    Normalizer::decompose(sourceString, FALSE, 0, decomp, ec);
    UBool firstIsVowel = FALSE;
    UBool firstIsRho = FALSE;
    UBool noLetterYet = TRUE;
    int32_t breathingCount = 0;
    int32_t letterCount = 0;
    for (int32_t i = 0; i < decomp.length(); ++i) {
        UChar c = decomp.charAt(i);
        if (u_isalpha(c)) {
            ++letterCount;
            if (noLetterYet) {
                noLetterYet =  FALSE;
                firstIsVowel = isVowel(c);
                firstIsRho = isRho(c);
            }
            if (firstIsRho && letterCount == 2 && breathingCount == 0) return FALSE;
        }
        if (c == 0x0313 || c == 0x0314) {
            ++breathingCount;
        }
    }
    
    if (firstIsVowel || firstIsRho) return breathingCount == 1;
    return breathingCount == 0;
}

UBool LegalGreek::isVowel(UChar c) {
    switch (c) {
    case 0x03B1:
    case 0x03B5:
    case 0x03B7:
    case 0x03B9:
    case 0x03BF:
    case 0x03C5:
    case 0x03C9:
    case 0x0391:
    case 0x0395:
    case 0x0397:
    case 0x0399:
    case 0x039F:
    case 0x03A5:
    case 0x03A9:
        return TRUE;
    }
    return FALSE;
}

UBool LegalGreek::isRho(UChar c) {
    switch (c) {
    case 0x03C1:
    case 0x03A1:
        return TRUE;
    }
    return FALSE;
}

class LegalDeleter {
    Legal* obj;
    Legal*& zeroMe;
public:
    LegalDeleter(Legal* adopted, Legal*& ptrToClean) :
        obj(adopted),
        zeroMe(ptrToClean) {}
    ~LegalDeleter() { delete obj; zeroMe = NULL; }
};

//--------------------------------------------------------------------
// RTTest Interface
//--------------------------------------------------------------------

class RTTest {

    // PrintWriter out;

    UnicodeString transliteratorID; 
    int8_t sourceScript;
    int8_t targetScript;
    int32_t errorLimit;
    int32_t errorCount;
    int32_t pairLimit;
    UnicodeSet sourceRange;
    UnicodeSet targetRange;
    IntlTest* log;
    Legal* legalSource; // NOT owned

public:

    /*
     * create a test for the given script transliterator.
     */
    RTTest(const UnicodeString& transliteratorIDStr, 
           int8_t sourceScriptVal, int8_t targetScriptVal);

    virtual ~RTTest();

    void setErrorLimit(int32_t limit);

    void setPairLimit(int32_t limit);

    void test(const UnicodeString& sourceRange,
              const UnicodeString& targetRange, IntlTest* log,
              Legal* adoptedLegal);

private:

    void test2();

    void logWrongScript(const UnicodeString& label,
                        const UnicodeString& from,
                        const UnicodeString& to);
    void logRoundTripFailure(const UnicodeString& from,
                             const UnicodeString& to,
                             const UnicodeString& back);

protected:

    /*
     * Characters to filter for source-target mapping completeness
     * Typically is base alphabet, minus extended characters
     * Default is ASCII letters for Latin
     */
    virtual UBool isSource(UChar c);

    /*
     * Characters to check for target back to source mapping.
     * Typically the same as the target script, plus punctuation
     */
    inline UBool isReceivingSource(UChar c);

    /*
     * Characters to filter for target-source mapping
     * Typically is base alphabet, minus extended characters
     */
    inline UBool isTarget(UChar c);

    /*
     * Characters to check for target-source mapping
     * Typically the same as the source script, plus punctuation
     */
    inline UBool isReceivingTarget(UChar c);

    UBool isSource(const UnicodeString& s);
    UBool isTarget(const UnicodeString& s);
    UBool isReceivingSource(const UnicodeString& s);
    UBool isReceivingTarget(const UnicodeString& s);
};

//--------------------------------------------------------------------
// RTTest Implementation
//--------------------------------------------------------------------

/*
 * create a test for the given script transliterator.
 */
RTTest::RTTest(const UnicodeString& transliteratorIDStr, 
               int8_t sourceScriptVal, int8_t targetScriptVal) {
    this->transliteratorID = transliteratorIDStr;
    this->sourceScript = sourceScriptVal;
    this->targetScript = targetScriptVal;
    legalSource = NULL;
    errorLimit = (int32_t)0x7FFFFFFFL;
    errorCount = 0;
    pairLimit  = 0x10000;
}

RTTest::~RTTest() {
}

void RTTest::setErrorLimit(int32_t limit) {
    errorLimit = limit;
}

void RTTest::setPairLimit(int32_t limit) {
    pairLimit = limit;
}

void RTTest::test(const UnicodeString& sourceRangeVal,
                  const UnicodeString& targetRangeVal, IntlTest* logVal,
                  Legal* adoptedLegal) {

    UErrorCode status = U_ZERO_ERROR;

    this->log = logVal;
    this->legalSource = adoptedLegal;
    LegalDeleter cleaner(adoptedLegal, this->legalSource);

    if (sourceRangeVal.length() > 0) {
        this->sourceRange.applyPattern(sourceRangeVal, status);
        if (U_FAILURE(status)) {
            log->errln("FAIL: UnicodeSet::applyPattern(" +
                       sourceRangeVal + ")");
            return;
        }
    } else {
        this->sourceRange.applyPattern("[a-zA-Z]", status);
        if (U_FAILURE(status)) {
            log->errln("FAIL: UnicodeSet::applyPattern([a-z])");
            return;
        }
    }
    this->targetRange.clear();
    if (targetRangeVal.length() > 0) {
        this->targetRange.applyPattern(targetRangeVal, status);
        if (U_FAILURE(status)) {
            log->errln("FAIL: UnicodeSet::applyPattern(" +
                       targetRangeVal + ")");
            return;
        }
    }

    test2();

    if (errorCount > 0) {
        log->errln(transliteratorID + " errors: " + errorCount); // + ", see " + logFileName);
    } else {
        log->logln(transliteratorID + " ok");
    }
}

void RTTest::logWrongScript(const UnicodeString& label,
                            const UnicodeString& from,
                            const UnicodeString& to) {
    log->errln((UnicodeString)"Fail " +
               label + ": " +
               from + "(" + TestUtility::hex(from) + ") => " +
               to + "(" + TestUtility::hex(to) + ")");
    ++errorCount;
}

void RTTest::logRoundTripFailure(const UnicodeString& from,
                                 const UnicodeString& to,
                                 const UnicodeString& back) {
    if (!legalSource->is(from)) return; // skip illegals

    log->errln((UnicodeString)"Fail Roundtrip: " +
               from + "(" + TestUtility::hex(from) + ") => " +
               to + "(" + TestUtility::hex(to) + ") => " +
               back + "(" + TestUtility::hex(back) + ") => ");
    ++errorCount;
}

/*
 * Characters to filter for source-target mapping completeness
 * Typically is base alphabet, minus extended characters
 * Default is ASCII letters for Latin
 */
UBool RTTest::isSource(UChar c) {
    return (TestUtility::getScript(c) == sourceScript && u_isalpha(c)
        && sourceRange.contains(c));
}

/*
 * Characters to check for target back to source mapping.
 * Typically the same as the target script, plus punctuation
 */
inline UBool
RTTest::isReceivingSource(UChar c) {
    int8_t script = TestUtility::getScript(c);
    return (script == sourceScript || script == TestUtility::COMMON_SCRIPT);
}

/*
 * Characters to filter for target-source mapping
 * Typically is base alphabet, minus extended characters
 */
inline UBool
RTTest::isTarget(UChar c) {
    return (TestUtility::getScript(c) == targetScript && u_isalpha(c)
        && (targetRange.isEmpty() || targetRange.contains(c)));
}

/*
 * Characters to check for target-source mapping
 * Typically the same as the source script, plus punctuation
 */
inline UBool
RTTest::isReceivingTarget(UChar c) {
    int8_t script = TestUtility::getScript(c);
    return (script == targetScript || script == TestUtility::COMMON_SCRIPT);
}

UBool RTTest::isSource(const UnicodeString& s) {
    int32_t length = s.length();
    for (int32_t i = 0; i < length; ++i) {
        if (!isSource(s.charAt(i)))
            return FALSE;
    }
    return TRUE;
}

UBool RTTest::isTarget(const UnicodeString& s) {
    int32_t length = s.length();
    for (int32_t i = 0; i < length; ++i) {
        if (!isTarget(s.charAt(i)))
            return FALSE;
    }
    return TRUE;
}

UBool RTTest::isReceivingSource(const UnicodeString& s) {
    int32_t length = s.length();
    for (int32_t i = 0; i < length; ++i) {
        if (!isReceivingSource(s.charAt(i)))
            return FALSE;
    }
    return TRUE;
}

UBool RTTest::isReceivingTarget(const UnicodeString& s) {
    int32_t length = s.length();
    for (int32_t i = 0; i < length; ++i) {
        if (!isReceivingTarget(s.charAt(i)))
            return FALSE;
    }
    return TRUE;
}

//--------------------------------------------------------------------
// Specific Tests
//--------------------------------------------------------------------

void TransliteratorRoundTripTest::TestHiragana() {
    RTTest test("Latin-Hiragana",
                TestUtility::LATIN_SCRIPT, TestUtility::HIRAGANA_SCRIPT);
    test.test("[a-z]", UnicodeString("[\\u3040-\\u3094]", ""), this, new Legal());
}

void TransliteratorRoundTripTest::TestKatakana() {
    RTTest test("Latin-Katakana", 
                TestUtility::LATIN_SCRIPT, TestUtility::KATAKANA_SCRIPT);
    test.test("[a-z]", UnicodeString("[\\u30A1-\\u30FA\\u30FC]", ""), this, new Legal());
}

void TransliteratorRoundTripTest::TestArabic() {
//  RTTest test("Latin-Arabic", 
//              TestUtility::LATIN_SCRIPT, TestUtility::ARABIC_SCRIPT);
//  test.test("[a-z]", UnicodeString("[\\u0620-\\u065F-[\\u0640]]", ""), this, new Legal());
}

void TransliteratorRoundTripTest::TestHebrew() {
//  RTTest test("Latin-Hebrew", 
//              TestUtility::LATIN_SCRIPT, TestUtility::HEBREW_SCRIPT);
//  test.test("", UnicodeString("[\\u05D0-\\u05EF]", ""), this, new Legal());
}

void TransliteratorRoundTripTest::TestJamo() {
    RTTest t("Latin-Jamo", 
             TestUtility::LATIN_SCRIPT, TestUtility::JAMO_SCRIPT);
    t.setErrorLimit(200); // Don't run full test -- too long
    t.test("", "", this, new Legal());
}

void TransliteratorRoundTripTest::TestJamoHangul() {
    RTTest t("Latin-Hangul", 
             TestUtility::LATIN_SCRIPT, TestUtility::HANGUL_SCRIPT);
    t.setErrorLimit(50); // Don't run full test -- too long
    t.test("", "", this, new Legal());
}

void TransliteratorRoundTripTest::TestGreek() {
    RTTest test("Latin-Greek", 
                TestUtility::LATIN_SCRIPT, TestUtility::GREEK_SCRIPT);
    test.test("", UnicodeString("[\\u003B\\u00B7[:Greek:]-[\\u03D7-\\u03EF]]", ""), this, new LegalGreek());
}

void TransliteratorRoundTripTest::TestCyrillic() {
    RTTest test("Latin-Cyrillic", 
                TestUtility::LATIN_SCRIPT, TestUtility::CYRILLIC_SCRIPT);
    test.test("", UnicodeString("[\\u0400-\\u045F]", ""), this, new Legal());
}

void RTTest::test2() {

    UChar c;
    UnicodeString cs, targ, reverse;
    int8_t *type = new int8_t[0xFFFF];
    UParseError parseError;
    UErrorCode status = U_ZERO_ERROR;
    Transliterator* sourceToTarget = Transliterator::createInstance(transliteratorID, UTRANS_FORWARD, parseError, status);
    if (sourceToTarget == NULL) {
        log->errln("Fail: createInstance(" + transliteratorID +
                   ") returned NULL");
        return;
    }
    Transliterator* targetToSource = sourceToTarget->createInverse(status);
    if (targetToSource == NULL) {
        log->errln("Fail: " + transliteratorID +
                   ".createInverse() returned NULL");
        delete sourceToTarget;
        return;
    }

    log->logln("Initializing type array");

    for (c = 0; c < 0xFFFF; ++c) {
        type[c] = u_charType(c);
    }

    BitSet failSourceTarg;

    log->logln("Checking that all source characters convert to target - Singles");

    for (c = 0; c < 0xFFFF; ++c) {
        if (type[c] == U_UNASSIGNED || !isSource(c))
            continue;
        cs.remove();
        cs.append(c);
        targ = cs;
        sourceToTarget->transliterate(targ);
        if (!isReceivingTarget(targ)) {
            logWrongScript("Source-Target", cs, targ);
            failSourceTarg.set(c);
            if (errorCount >= errorLimit)
                return;
        }
    }

    log->logln("Checking that all source characters convert to target - Doubles");

    for (c = 0; c < 0xFFFF; ++c) {
        if (type[c] == U_UNASSIGNED ||
            !isSource(c)) continue;
        if (failSourceTarg.get(c)) continue;

        for (UChar d = 0; d < 0xFFFF; ++d) {
            if (type[d] == U_UNASSIGNED || !isSource(d))
                continue;
            if (failSourceTarg.get(d)) continue;

            cs.remove();
            cs.append(c).append(d);
            targ = cs;
            sourceToTarget->transliterate(targ);
            if (!isReceivingTarget(targ)) {
                logWrongScript("Source-Target", cs, targ);
                if (errorCount >= errorLimit)
                    return;
            }
        }
    }

    log->logln("Checking that target characters convert to source and back - Singles");

    BitSet failTargSource;
    BitSet failRound;

    for (c = 0; c < 0xFFFF; ++c) {
        if (type[c] == U_UNASSIGNED || !isTarget(c))
            continue;
        cs.remove();
        cs.append(c);
        targ = cs;
        targetToSource->transliterate(targ);
        reverse = targ;
        sourceToTarget->transliterate(reverse);
        if (!isReceivingSource(targ)) {
            logWrongScript("Target-Source", cs, targ);
            failTargSource.set(c);
            if (errorCount >= errorLimit)
                return;
        } else if (cs != reverse) {
            logRoundTripFailure(cs, targ, reverse);
            failRound.set(c);
            if (errorCount >= errorLimit)
                return;
        }
    }

    log->logln("Checking that target characters convert to source and back - Doubles");
    int32_t count = 0;
    cs = UNICODE_STRING("aa", 2);
    for (c = 0; c < 0xFFFF; ++c) {
        if (type[c] == U_UNASSIGNED || !isTarget(c))
            continue;
        if (++count > pairLimit) {
            //throw new TestTruncated("Test truncated at " + pairLimit + " x 64k pairs");
            log->logln("");
            log->logln((UnicodeString)"Test truncated at " + pairLimit + " x 64k pairs");
            return;
        }
        cs.setCharAt(0, c);
        log->log(TestUtility::hex(c));
        for (UChar d = 0; d < 0xFFFF; ++d) {
            if (type[d] == U_UNASSIGNED || !isTarget(d))
                continue;
            cs.setCharAt(1, d);
            targ = cs;
            targetToSource->transliterate(targ);
            reverse = targ;
            sourceToTarget->transliterate(reverse);
            if (!isReceivingSource(targ) && !failTargSource.get(c) && !failTargSource.get(d)) {
                logWrongScript("Target-Source", cs, targ);
                if (errorCount >= errorLimit)
                    return;
            } else if (cs.caseCompare(reverse, U_FOLD_CASE_DEFAULT) != 0 && !failRound.get(c) && !failRound.get(d)) {
                logRoundTripFailure(cs, targ, reverse);
                if (errorCount >= errorLimit)
                    return;
            }
        }
    }
    log->logln("");
    delete []type;
    delete sourceToTarget;
    delete targetToSource;
}

