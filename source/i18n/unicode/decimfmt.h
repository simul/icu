/*
********************************************************************************
*   Copyright (C) 1997-2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File DECIMFMT.H
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/20/97    clhuang     Updated per C++ implementation.
*   04/03/97    aliu        Rewrote parsing and formatting completely, and
*                           cleaned up and debugged.  Actually works now.
*   04/17/97    aliu        Changed DigitCount to int per code review.
*   07/10/97    helena      Made ParsePosition a class and get rid of the function
*                           hiding problems.
*   09/09/97    aliu        Ported over support for exponential formats.
*    07/20/98    stephen        Changed documentation
********************************************************************************
*/
 
#ifndef DECIMFMT_H
#define DECIMFMT_H
 
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dcfmtsym.h"
#include "unicode/numfmt.h"
#include "unicode/locid.h"

U_NAMESPACE_BEGIN

class DigitList;
class ChoiceFormat;

/**
 * DecimalFormat is a concrete subclass of NumberFormat that formats decimal
 * numbers. It has a variety of features designed to make it possible to parse
 * and format numbers in any locale, including support for Western, Arabic, or
 * Indic digits.  It also supports different flavors of numbers, including
 * integers ("123"), fixed-point numbers ("123.4"), scientific notation
 * ("1.23E4"), percentages ("12%"), and currency amounts ("$123").  All of these
 * flavors can be easily localized.
 *
 * <p>To obtain a NumberFormat for a specific locale (including the default
 * locale) call one of NumberFormat's factory methods such as
 * createInstance(). Do not call the DecimalFormat constructors directly, unless
 * you know what you are doing, since the NumberFormat factory methods may
 * return subclasses other than DecimalFormat.
 *
 * <p><strong>Example Usage</strong>
 *
 * \code
 *     // Normally we would have a GUI with a menu for this
 *     int32_t locCount;
 *     const Locale* locales = NumberFormat::getAvailableLocales(locCount);
 * 
 *     double myNumber = -1234.56;
 *     UErrorCode success = U_ZERO_ERROR;
 *     NumberFormat* form;
 * 
 *     // Print out a number with the localized number, currency and percent
 *     // format for each locale.
 *     UnicodeString countryName;
 *     UnicodeString displayName;
 *     UnicodeString str;
 *     UnicodeString pattern;
 *     Formattable fmtable;
 *     for (int32_t j = 0; j < 3; ++j) {
 *         cout << endl << "FORMAT " << j << endl;
 *         for (int32_t i = 0; i < locCount; ++i) {
 *             if (locales[i].getCountry(countryName).size() == 0) {
 *                 // skip language-only
 *                 continue;
 *             }
 *             switch (j) {
 *             case 0:
 *                 form = NumberFormat::createInstance(locales[i], success ); break;
 *             case 1:
 *                 form = NumberFormat::createCurrencyInstance(locales[i], success ); break;
 *             default:
 *                 form = NumberFormat::createPercentInstance(locales[i], success ); break;
 *             }
 *             if (form) {
 *                 str.remove();
 *                 pattern = ((DecimalFormat*)form)->toPattern(pattern);
 *                 cout << locales[i].getDisplayName(displayName) << ": " << pattern;
 *                 cout << "  ->  " << form->format(myNumber,str) << endl;
 *                 form->parse(form->format(myNumber,str), fmtable, success);
 *                 delete form;  
 *             }
 *         }
 *     }
 * \endcode
 *
 * <p><strong>Patterns</strong>
 *
 * <p>A DecimalFormat consists of a <em>pattern</em> and a set of
 * <em>symbols</em>.  The pattern may be set directly using
 * applyPattern(), or indirectly using other API methods which
 * manipulate aspects of the pattern, such as the minimum number of integer
 * digits.  The symbols are stored in a DecimalFormatSymbols
 * object.  When using the NumberFormat factory methods, the
 * pattern and symbols are read from ICU's locale data.
 * 
 * <p><strong>Special Pattern Characters</strong>
 *
 * <p>Many characters in a pattern are taken literally; they are matched during
 * parsing and output unchanged during formatting.  Special characters, on the
 * other hand, stand for other characters, strings, or classes of characters.
 * For example, the '#' character is replaced by a localized digit.  Often the
 * replacement character is the same as the pattern character; in the U.S. locale,
 * the ',' grouping character is replaced by ','.  However, the replacement is
 * still happening, and if the symbols are modified, the grouping character
 * changes.  Some special characters affect the behavior of the formatter by
 * their presence; for example, if the percent character is seen, then the
 * value is multiplied by 100 before being displayed.
 *
 * <p>To insert a special character in a pattern as a literal, that is, without
 * any special meaning, the character must be quoted.  There are some exceptions to
 * this which are noted below.
 *
 * <p>The characters listed here are used in non-localized patterns.  Localized
 * patterns use the corresponding characters taken from this formatter's
 * DecimalFormatSymbols object instead, and these characters lose
 * their special status.  Two exceptions are the currency sign and quote, which
 * are not localized.
 *
 * <table border=0 cellspacing=3 cellpadding=0>
 *   <tr bgcolor="#ccccff">
 *     <td align=left><strong>Symbol</strong>
 *     <td align=left><strong>Location</strong>
 *     <td align=left><strong>Localized?</strong>
 *     <td align=left><strong>Meaning</strong>
 *   <tr valign=top>
 *     <td><code>0</code>
 *     <td>Number
 *     <td>Yes
 *     <td>Digit
 *   <tr valign=top bgcolor="#eeeeff">
 *     <td><code>1-9</code>
 *     <td>Number
 *     <td>Yes
 *     <td>'1' through '9' indicate rounding.
 *   <tr valign=top>
 *     <td><code>#</code>
 *     <td>Number
 *     <td>Yes
 *     <td>Digit, zero shows as absent
 *   <tr valign=top bgcolor="#eeeeff">
 *     <td><code>.</code>
 *     <td>Number
 *     <td>Yes
 *     <td>Decimal separator or monetary decimal separator
 *   <tr valign=top>
 *     <td><code>-</code>
 *     <td>Number
 *     <td>Yes
 *     <td>Minus sign
 *   <tr valign=top bgcolor="#eeeeff">
 *     <td><code>,</code>
 *     <td>Number
 *     <td>Yes
 *     <td>Grouping separator
 *   <tr valign=top>
 *     <td><code>E</code>
 *     <td>Number
 *     <td>Yes
 *     <td>Separates mantissa and exponent in scientific notation.
 *         <em>Need not be quoted in prefix or suffix.</em>
 *   <tr valign=top bgcolor="#eeeeff">
 *     <td><code>+</code>
 *     <td>Exponent
 *     <td>Yes
 *     <td>Prefix positive exponents with localized plus sign.
 *         <em>Need not be quoted in prefix or suffix.</em>
 *   <tr valign=top>
 *     <td><code>;</code>
 *     <td>Subpattern boundary
 *     <td>Yes
 *     <td>Separates positive and negative subpatterns
 *   <tr valign=top bgcolor="#eeeeff">
 *     <td><code>%</code>
 *     <td>Prefix or suffix
 *     <td>Yes
 *     <td>Multiply by 100 and show as percentage
 *   <tr valign=top>
 *     <td><code>\u2030</code>
 *     <td>Prefix or suffix
 *     <td>Yes
 *     <td>Multiply by 1000 and show as per mille
 *   <tr valign=top bgcolor="#eeeeff">
 *     <td><code>&#164;</code> (<code>\u00A4</code>)
 *     <td>Prefix or suffix
 *     <td>No
 *     <td>Currency sign, replaced by currency symbol.  If
 *         doubled, replaced by international currency symbol.
 *         If present in a pattern, the monetary decimal separator
 *         is used instead of the decimal separator.
 *   <tr valign=top>
 *     <td><code>'</code>
 *     <td>Prefix or suffix
 *     <td>No
 *     <td>Used to quote special characters in a prefix or suffix,
 *         for example, <code>"'#'#"</code> formats 123 to
 *         <code>"#123"</code>.  To create a single quote
 *         itself, use two in a row: <code>"# o''clock"</code>.
 *   <tr valign=top bgcolor="#eeeeff">
 *     <td><code>*</code>
 *     <td>Prefix or suffix boundary
 *     <td>Yes
 *     <td>Pad escape, precedes pad character
 * </table>
 *
 * <p>A DecimalFormat pattern contains a postive and negative
 * subpattern, for example, "#,##0.00;(#,##0.00)".  Each subpattern has a
 * prefix, a numeric part, and a suffix.  If there is no explicit negative
 * subpattern, the negative subpattern is the localized minus sign prefixed to the
 * positive subpattern. That is, "0.00" alone is equivalent to "0.00;-0.00".  If there
 * is an explicit negative subpattern, it serves only to specify the negative
 * prefix and suffix; the number of digits, minimal digits, and other
 * characteristics are ignored in the negative subpattern. That means that
 * "#,##0.0#;(#)" has precisely the same result as "#,##0.0#;(#,##0.0#)".
 *
 * <p>The prefixes, suffixes, and various symbols used for infinity, digits,
 * thousands separators, decimal separators, etc. may be set to arbitrary
 * values, and they will appear properly during formatting.  However, care must
 * be taken that the symbols and strings do not conflict, or parsing will be
 * unreliable.  For example, either the positive and negative prefixes or the
 * suffixes must be distinct for parse() to be able
 * to distinguish positive from negative values.  Another example is that the
 * decimal separator and thousands separator should be distinct characters, or
 * parsing will be impossible.
 *
 * <p>The <em>grouping separator</em> is a character that separates clusters of
 * integer digits to make large numbers more legible.  It commonly used for
 * thousands, but in some locales it separates ten-thousands.  The <em>grouping
 * size</em> is the number of digits between the grouping separators, such as 3
 * for "100,000,000" or 4 for "1 0000 0000". There are actually two different
 * grouping sizes: One used for the least significant integer digits, the
 * <em>primary grouping size</em>, and one used for all others, the
 * <em>secondary grouping size</em>.  In most locales these are the same, but
 * sometimes they are different. For example, if the primary grouping interval
 * is 3, and the secondary is 2, then this corresponds to the pattern
 * "#,##,##0", and the number 123456789 is formatted as "12,34,56,789".  If a
 * pattern contains multiple grouping separators, the interval between the last
 * one and the end of the integer defines the primary grouping size, and the
 * interval between the last two defines the secondary grouping size. All others
 * are ignored, so "#,##,###,####" == "###,###,####" == "##,#,###,####".
 *
 * <p>Illegal patterns, such as "#.#.#" or "#.###,###", will cause
 * DecimalFormat to set a failing UErrorCode.
 *
 * <p><strong>Pattern BNF</strong>
 *
 * <pre>
 * pattern    := subpattern (';' subpattern)?
 * subpattern := prefix? number suffix?
 * number     := integer ('.' fraction)? exponent?
 * prefix     := '\u0000'..'\uFFFD' - specialCharacters
 * suffix     := '\u0000'..'\uFFFD' - specialCharacters
 * integer    := '#'* '0'* '0'
 * fraction   := '0'* '#'*
 * exponent   := 'E' '+'? '0'* '0'
 * padSpec    := '*' padChar
 * padChar    := '\u0000'..'\uFFFD' - quote
 * &#32;
 * Notation:
 *   X*       0 or more instances of X
 *   X?       0 or 1 instances of X
 *   X..Y     any character from X up to Y, inclusive
 *   S - T    characters in S, except those in T
 * </pre>
 * The first subpattern is for positive numbers. The second (optional)
 * subpattern is for negative numbers.
 * 
 * <p>Not indicated in the BNF syntax above:
 * <ul><li>The grouping separator ',' can occur inside the integer portion between the
 * most significant digit and the least significant digit.
 *
 * <li>Two grouping intervals are recognized: That between the
 *     decimal point and the first grouping symbol, and that
 *     between the first and second grouping symbols. These
 *     intervals are identical in most locales, but in some
 *     locales they differ. For example, the pattern
 *     &quot;#,##,###&quot; formats the number 123456789 as
 *     &quot;12,34,56,789&quot;.</li>
 * 
 * <li>The pad specifier <code>padSpec</code> may appear before the prefix,
 * after the prefix, before the suffix, after the suffix, or not at all.
 *
 * <li>In place of '0', the digits '1' through '9' may be used to
 * indicate a rounding increment.
 * </ul>
 *
 * <p><strong>Parsing</strong>
 *
 * <p>DecimalFormat parses all Unicode characters that represent
 * decimal digits, as defined by u_charDigitValue().  In addition,
 * DecimalFormat also recognizes as digits the ten consecutive
 * characters starting with the localized zero digit defined in the
 * DecimalFormatSymbols object.  During formatting, the
 * DecimalFormatSymbols-based digits are output.
 *
 * <p>During parsing, grouping separators are ignored.
 *
 * <p>If parse(UnicodeString&,Formattable&,ParsePosition&)
 * fails to parse a string, it leaves the parse position unchanged.
 * The convenience method parse(UnicodeString&,Formattable&,UErrorCode&)
 * indicates parse failure by setting a failing
 * UErrorCode.
 *
 * <p><strong>Formatting</strong>
 *
 * <p>Formatting is guided by several parameters, all of which can be
 * specified either using a pattern or using the API.
 *
 * <p>For <em>non-exponential</em> formatters:
 *
 * <ul><li>If the number of actual integer digits exceeds the
 * <em>maximum integer digits</em>, then only the least significant
 * digits are shown.  For example, 1997 is formatted as "97" if the
 * maximum integer digits is set to 2.
 *
 * <li>If the number of actual integer digits is less than the
 * <em>minimum integer digits</em>, then leading zeros are added.  For
 * example, 1997 is formatted as "01997" if the minimum integer digits
 * is set to 5.
 *
 * <li>If the number of actual fraction digits exceeds the <em>maximum
 * fraction digits</em>, then half-even rounding it performed to the
 * maximum fraction digits.  For example, 0.125 is formatted as "0.12"
 * if the maximum fraction digits is 2.  This behavior can be changed
 * by specifying a rounding increment and a rounding mode.
 *
 * <li>If the number of actual fraction digits is less than the
 * <em>minimum fraction digits</em>, then trailing zeros are added.
 * For example, 0.125 is formatted as "0.1250" if the mimimum fraction
 * digits is set to 4.
 *
 * <li>Trailing fractional zeros are not displayed if they occur
 * <em>j</em> positions after the decimal, where <em>j</em> is greater
 * than the minimum fraction digits. For example, 0.10005 is
 * formatted as "0.1" if the minimum fraction digits is 0 or 1.
 * </ul>
 *
 * <p><strong>Special Values</strong>
 *
 * <p><code>NaN</code> is represented as a single character, typically
 * <code>\uFFFD</code>.  This character is determined by the
 * DecimalFormatSymbols object.  This is the only value for which
 * the prefixes and suffixes are not used.
 *
 * <p>Infinity is represented as a single character, typically
 * <code>\u221E</code>, with the positive or negative prefixes and suffixes
 * applied.  The infinity character is determined by the
 * DecimalFormatSymbols object.
 *
 * <p><strong>Scientific Notation</strong>
 *
 * <p>Numbers in scientific notation are expressed as the product of a mantissa
 * and a power of ten, for example, 1234 can be expressed as 1.234 x 10<sup>3</sup>. The
 * mantissa is typically in the half-open interval [1.0, 10.0) or sometimes [0.0, 1.0),
 * but it need not be.  DecimalFormat supports arbitrary mantissas.
 * DecimalFormat can be instructed to use scientific
 * notation through the API or through the pattern.  In a pattern, the exponent
 * character immediately followed by one or more digit characters indicates
 * scientific notation.  Example: "0.###E0" formats the number 1234 as
 * "1.234E3".
 *
 * <ul>
 * <li>The number of digit characters after the exponent character gives the
 * minimum exponent digit count.  There is no maximum.  Negative exponents are
 * formatted using the localized minus sign, <em>not</em> the prefix and suffix
 * from the pattern.  This allows patterns such as "0.###E0 m/s".  To prefix
 * positive exponents with a localized plus sign, specify '+' between the
 * exponent and the digits: "0.###E+0" will produce formats "1E+1", "1E+0",
 * "1E-1", etc.  (In localized patterns, use the localized plus sign rather than
 * '+'.)
 *
 * <li>The minimum number of integer digits is achieved by adjusting the
 * exponent.  Example: 0.00123 formatted with "00.###E0" yields "12.3E-4".  This
 * only happens if there is no maximum number of integer digits.  If there is a
 * maximum, then the minimum number of integer digits is fixed at one.
 *
 * <li>The maximum number of integer digits, if present, specifies the exponent
 * grouping.  The most common use of this is to generate <em>engineering
 * notation</em>, in which the exponent is a multiple of three, e.g.,
 * "##0.###E0".  The number 12345 is formatted using "##0.####E0" as "12.345E3".
 *
 * <li>The number of significant digits is the sum of the <em>minimum
 * integer</em> and <em>maximum fraction</em> digits, and is unaffected by the
 * maximum integer digits.  If this sum is zero, then all significant digits are
 * shown.  The number of significant digits limits the total number of integer
 * and fraction digits that will be shown in the mantissa; it does not affect
 * parsing.  For example, 12345 formatted with "##0.##E0" is "12.3E3".
 *
 * <li>Exponential patterns may not contain grouping separators.
 * </ul>
 *
 * <p><strong>Padding</strong>
 *
 * <p>DecimalFormat supports padding the result of
 * format() to a specific width.  Padding may be specified either
 * through the API or through the pattern syntax.  In a pattern the pad escape
 * character, followed by a single pad character, causes padding to be parsed
 * and formatted.  The pad escape character is '*' in unlocalized patterns, and
 * can be localized using DecimalFormatSymbols::setSymbol() with a
 * DecimalFormatSymbols::kPadEscapeSymbol
 * selector.  For example, <code>"$*x#,##0.00"</code> formats 123 to
 * <code>"$xx123.00"</code>, and 1234 to <code>"$1,234.00"</code>.
 *
 * <ul>
 * <li>When padding is in effect, the width of the positive subpattern,
 * including prefix and suffix, determines the format width.  For example, in
 * the pattern <code>"* #0 o''clock"</code>, the format width is 10.
 *
 * <li>The width is counted in 16-bit code units (UChars).
 *
 * <li>Some parameters which usually do not matter have meaning when padding is
 * used, because the pattern width is significant with padding.  In the pattern
 * "* ##,##,#,##0.##", the format width is 14.  The initial characters "##,##,"
 * do not affect the grouping size or maximum integer digits, but they do affect
 * the format width.
 *
 * <li>Padding may be inserted at one of four locations: before the prefix,
 * after the prefix, before the suffix, or after the suffix.  If padding is
 * specified in any other location, applyPattern()
 * sets a failing UErrorCode.  If there is no prefix,
 * before the prefix and after the prefix are equivalent, likewise for the
 * suffix.
 *
 * <li>When specified in a pattern, the 32-bit code point immediately
 * following the pad escape is the pad character. This may be any character,
 * including a special pattern character. That is, the pad escape
 * <em>escapes</em> the following character. If there is no character after
 * the pad escape, then the pattern is illegal.
 *
 * </ul>
 *
 * <p><strong>Rounding</strong>
 *
 * <p>DecimalFormat supports rounding to a specific increment.  For
 * example, 1230 rounded to the nearest 50 is 1250.  1.234 rounded to the
 * nearest 0.65 is 1.3.  The rounding increment may be specified through the API
 * or in a pattern.  To specify a rounding increment in a pattern, include the
 * increment in the pattern itself.  "#,#50" specifies a rounding increment of
 * 50.  "#,##0.05" specifies a rounding increment of 0.05.
 *
 * <ul>
 * <li>Rounding only affects the string produced by formatting.  It does
 * not affect parsing or change any numerical values.
 *
 * <li>A <em>rounding mode</em> determines how values are rounded; see
 * DecimalFormat::ERoundingMode.  Rounding increments specified in
 * patterns use the default mode, DecimalFormat::kRoundHalfEven.
 *
 * <li>Some locales use rounding in their currency formats to reflect the
 * smallest currency denomination.
 *
 * <li>In a pattern, digits '1' through '9' specify rounding, but otherwise
 * behave identically to digit '0'.
 * </ul>
 *
 * <p><strong>Synchronization</strong>
 *
 * <p>DecimalFormat objects are not synchronized.  Multiple
 * threads should not access one formatter concurrently.
 */
class U_I18N_API DecimalFormat: public NumberFormat {
public:
    /**
     * Rounding mode.
     * @draft ICU 2.4
     */
    enum ERoundingMode {
        kRoundCeiling,  /**< Round towards positive infinity */
        kRoundFloor,    /**< Round towards negative infinity */
        kRoundDown,     /**< Round towards zero */
        kRoundUp,       /**< Round away from zero */
        kRoundHalfEven, /**< Round towards the nearest integer, or
                             towards the nearest even integer if equidistant */
        kRoundHalfDown, /**< Round towards the nearest integer, or
                             towards zero if equidistant */
        kRoundHalfUp    /**< Round towards the nearest integer, or
                             away from zero if equidistant */
        // We don't support ROUND_UNNECESSARY
    };

    /**
     * Pad position.
     * @draft ICU 2.4
     */
    enum EPadPosition {
        kPadBeforePrefix,
        kPadAfterPrefix,
        kPadBeforeSuffix,
        kPadAfterSuffix
    };

    /**
     * Create a DecimalFormat using the default pattern and symbols
     * for the default locale. This is a convenient way to obtain a
     * DecimalFormat when internationalization is not the main concern.
     * <P>
     * To obtain standard formats for a given locale, use the factory methods
     * on NumberFormat such as createInstance. These factories will
     * return the most appropriate sub-class of NumberFormat for a given
     * locale.
     * @param status    Output param set to success/failure code. If the
     *                  pattern is invalid this will be set to a failure code.
     * @stable ICU 2.0
     */
    DecimalFormat(UErrorCode& status);

    /**
     * Create a DecimalFormat from the given pattern and the symbols
     * for the default locale. This is a convenient way to obtain a
     * DecimalFormat when internationalization is not the main concern.
     * <P>
     * To obtain standard formats for a given locale, use the factory methods
     * on NumberFormat such as createInstance. These factories will
     * return the most appropriate sub-class of NumberFormat for a given
     * locale.
     * @param pattern   A non-localized pattern string.
     * @param status    Output param set to success/failure code. If the
     *                  pattern is invalid this will be set to a failure code.
     * @stable ICU 2.0
     */
    DecimalFormat(const UnicodeString& pattern,
                  UErrorCode& status);

    /**
     * Create a DecimalFormat from the given pattern and symbols.
     * Use this constructor when you need to completely customize the
     * behavior of the format.
     * <P>
     * To obtain standard formats for a given
     * locale, use the factory methods on NumberFormat such as
     * createInstance or createCurrencyInstance. If you need only minor adjustments
     * to a standard format, you can modify the format returned by
     * a NumberFormat factory method.
     *
     * @param pattern           a non-localized pattern string
     * @param symbolsToAdopt    the set of symbols to be used.  The caller should not
     *                          delete this object after making this call.
     * @param status            Output param set to success/failure code. If the
     *                          pattern is invalid this will be set to a failure code.
     * @stable ICU 2.0
     */
    DecimalFormat(  const UnicodeString& pattern,
                    DecimalFormatSymbols* symbolsToAdopt,
                    UErrorCode& status);

    /**
     * Create a DecimalFormat from the given pattern and symbols.
     * Use this constructor when you need to completely customize the
     * behavior of the format.
     * <P>
     * To obtain standard formats for a given
     * locale, use the factory methods on NumberFormat such as
     * createInstance or createCurrencyInstance. If you need only minor adjustments
     * to a standard format, you can modify the format returned by
     * a NumberFormat factory method.
     *
     * @param pattern           a non-localized pattern string
     * @param symbolsToAdopt    the set of symbols to be used.  The caller should not
     *                          delete this object after making this call.
     * @param parseError        Output param to receive errors occured during parsing 
     * @param status            Output param set to success/failure code. If the
     *                          pattern is invalid this will be set to a failure code.
     * @stable ICU 2.0
     */
    DecimalFormat(  const UnicodeString& pattern,
                    DecimalFormatSymbols* symbolsToAdopt,
                    UParseError& parseError,
                    UErrorCode& status);
    /**
     * Create a DecimalFormat from the given pattern and symbols.
     * Use this constructor when you need to completely customize the
     * behavior of the format.
     * <P>
     * To obtain standard formats for a given
     * locale, use the factory methods on NumberFormat such as
     * createInstance or createCurrencyInstance. If you need only minor adjustments
     * to a standard format, you can modify the format returned by
     * a NumberFormat factory method.
     *
     * @param pattern           a non-localized pattern string
     * @param symbols   the set of symbols to be used
     * @param status            Output param set to success/failure code. If the
     *                          pattern is invalid this will be set to a failure code.
     * @stable ICU 2.0
     */
    DecimalFormat(  const UnicodeString& pattern,
                    const DecimalFormatSymbols& symbols,
                    UErrorCode& status);

    /**
     * Copy constructor.
     * 
     * @param source    the DecimalFormat object to be copied from.
     * @stable ICU 2.0
     */
    DecimalFormat(const DecimalFormat& source);

    /**
     * Assignment operator.
     *
     * @param rhs    the DecimalFormat object to be copied.
     * @stable ICU 2.0
     */
    DecimalFormat& operator=(const DecimalFormat& rhs);

    /**
     * Destructor.
     * @stable ICU 2.0
     */
    virtual ~DecimalFormat();

    /**
     * Clone this Format object polymorphically. The caller owns the
     * result and should delete it when done.
     *
     * @return    a polymorphic copy of this DecimalFormat.
     * @stable ICU 2.0
     */
    virtual Format* clone(void) const;

    /**
     * Return true if the given Format objects are semantically equal.
     * Objects of different subclasses are considered unequal.
     *
     * @param other    the object to be compared with.
     * @return         true if the given Format objects are semantically equal.
     * @stable ICU 2.0
     */
    virtual UBool operator==(const Format& other) const;

    /**
     * Format a double or long number using base-10 representation.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @stable ICU 2.0
    */
    virtual UnicodeString& format(double number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;
    /**
     * Format a long number using base-10 representation.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @stable ICU 2.0
     */
    virtual UnicodeString& format(int32_t number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;
    /**
     * Format an int64 number using base-10 representation.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @draft ICU 2.8
     */
    virtual UnicodeString& format(int64_t number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;

	/**
     * Format a Formattable using base-10 representation.
     *
     * @param obj       The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @param status    Error code indicating success or failure.
     * @return          Reference to 'appendTo' parameter.
     * @stable ICU 2.0
     */
    virtual UnicodeString& format(const Formattable& obj,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos,
                                  UErrorCode& status) const;

    /**
     * Redeclared NumberFormat method.
     * Formats an object to produce a string.
     *
     * @param obj       The object to format.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param status    Output parameter filled in with success or failure status.
     * @return          Reference to 'appendTo' parameter.
     * @stable ICU 2.0
     */
    UnicodeString& format(const Formattable& obj,
                          UnicodeString& appendTo,
                          UErrorCode& status) const;

    /**
     * Redeclared NumberFormat method.
     * Format a double number.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @return          Reference to 'appendTo' parameter.
     * @stable ICU 2.0
     */
    UnicodeString& format(double number,
                          UnicodeString& appendTo) const;

    /**
     * Redeclared NumberFormat method.
     * Format a long number. These methods call the NumberFormat
     * pure virtual format() methods with the default FieldPosition.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @return          Reference to 'appendTo' parameter.
     * @stable ICU 2.0
     */
    UnicodeString& format(int32_t number,
                          UnicodeString& appendTo) const;

    /**
     * Redeclared NumberFormat method.
     * Format an int64 number. These methods call the NumberFormat
     * pure virtual format() methods with the default FieldPosition.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @return          Reference to 'appendTo' parameter.
     * @draft ICU 2.8
     */
    UnicodeString& format(int64_t number,
                          UnicodeString& appendTo) const;
   /**
    * Parse the given string using this object's choices. The method
    * does string comparisons to try to find an optimal match.
    * If no object can be parsed, index is unchanged, and NULL is
    * returned.  The result is returned as the most parsimonious
    * type of Formattable that will accomodate all of the
    * necessary precision.  For example, if the result is exactly 12,
    * it will be returned as a long.  However, if it is 1.5, it will
    * be returned as a double.
    *
    * @param text           The text to be parsed.
    * @param result         Formattable to be set to the parse result.
    *                       If parse fails, return contents are undefined.
    * @param parsePosition  The position to start parsing at on input.
    *                       On output, moved to after the last successfully
    *                       parse character. On parse failure, does not change.
    * @see Formattable
    * @stable ICU 2.0
    */
    virtual void parse(const UnicodeString& text,
                       Formattable& result,
                       ParsePosition& parsePosition) const;

    // Declare here again to get rid of function hiding problems.
    /** 
     * Parse the given string using this object's choices.
     *
     * @param text           The text to be parsed.
     * @param result         Formattable to be set to the parse result.
     * @param status    Output parameter filled in with success or failure status.
     * @stable ICU 2.0
     */
    virtual void parse(const UnicodeString& text, 
                       Formattable& result, 
                       UErrorCode& status) const;

    /**
     * Returns the decimal format symbols, which is generally not changed
     * by the programmer or user.
     * @return desired DecimalFormatSymbols
     * @see DecimalFormatSymbols
     * @stable ICU 2.0
     */
    virtual const DecimalFormatSymbols* getDecimalFormatSymbols(void) const;

    /**
     * Sets the decimal format symbols, which is generally not changed
     * by the programmer or user.
     * @param symbolsToAdopt DecimalFormatSymbols to be adopted.
     * @stable ICU 2.0
     */
    virtual void adoptDecimalFormatSymbols(DecimalFormatSymbols* symbolsToAdopt);

    /**
     * Sets the decimal format symbols, which is generally not changed
     * by the programmer or user.
     * @param symbols DecimalFormatSymbols.
     * @stable ICU 2.0
     */
    virtual void setDecimalFormatSymbols(const DecimalFormatSymbols& symbols);


    /**
     * Get the positive prefix.
     *
     * @param result    Output param which will receive the positive prefix.
     * @return          A reference to 'result'.
     * Examples: +123, $123, sFr123
     * @stable ICU 2.0
     */
    UnicodeString& getPositivePrefix(UnicodeString& result) const;

    /**
     * Set the positive prefix.
     *
     * @param newValue    the new value of the the positive prefix to be set.
     * Examples: +123, $123, sFr123
     * @stable ICU 2.0
     */
    virtual void setPositivePrefix(const UnicodeString& newValue);

    /**
     * Get the negative prefix.
     *
     * @param result    Output param which will receive the negative prefix.
     * @return          A reference to 'result'.
     * Examples: -123, ($123) (with negative suffix), sFr-123
     * @stable ICU 2.0
     */
    UnicodeString& getNegativePrefix(UnicodeString& result) const;

    /**
     * Set the negative prefix.
     *
     * @param newValue    the new value of the the negative prefix to be set.
     * Examples: -123, ($123) (with negative suffix), sFr-123
     * @stable ICU 2.0
     */
    virtual void setNegativePrefix(const UnicodeString& newValue);

    /**
     * Get the positive suffix.
     *
     * @param result    Output param which will receive the positive suffix.
     * @return          A reference to 'result'.
     * Example: 123%
     * @stable ICU 2.0
     */
    UnicodeString& getPositiveSuffix(UnicodeString& result) const;

    /**
     * Set the positive suffix.
     *
     * @param newValue    the new value of the positive suffix to be set.
     * Example: 123%
     * @stable ICU 2.0
     */
    virtual void setPositiveSuffix(const UnicodeString& newValue);

    /**
     * Get the negative suffix.
     *
     * @param result    Output param which will receive the negative suffix.
     * @return          A reference to 'result'.
     * Examples: -123%, ($123) (with positive suffixes)
     * @stable ICU 2.0
     */
    UnicodeString& getNegativeSuffix(UnicodeString& result) const;

    /**
     * Set the negative suffix.
     *
     * @param newValue    the new value of the negative suffix to be set.
     * Examples: 123%
     * @stable ICU 2.0
     */
    virtual void setNegativeSuffix(const UnicodeString& newValue);

    /**
     * Get the multiplier for use in percent, permill, etc.
     * For a percentage, set the suffixes to have "%" and the multiplier to be 100.
     * (For Arabic, use arabic percent symbol).
     * For a permill, set the suffixes to have "\u2031" and the multiplier to be 1000.
     *
     * @return    the multiplier for use in percent, permill, etc.
     * Examples: with 100, 1.23 -> "123", and "123" -> 1.23
     * @stable ICU 2.0
     */
    int32_t getMultiplier(void) const;

    /**
     * Set the multiplier for use in percent, permill, etc.
     * For a percentage, set the suffixes to have "%" and the multiplier to be 100.
     * (For Arabic, use arabic percent symbol).
     * For a permill, set the suffixes to have "\u2031" and the multiplier to be 1000.
     *
     * @param newValue    the new value of the multiplier for use in percent, permill, etc.
     * Examples: with 100, 1.23 -> "123", and "123" -> 1.23
     * @stable ICU 2.0
     */
    virtual void setMultiplier(int32_t newValue);

    /**
     * Get the rounding increment.
     * @return A positive rounding increment, or 0.0 if rounding
     * is not in effect.
     * @see #setRoundingIncrement
     * @see #getRoundingMode
     * @see #setRoundingMode
     * @stable ICU 2.0
     */
    virtual double getRoundingIncrement(void);

    /**
     * Set the rounding increment.  This method also controls whether
     * rounding is enabled.
     * @param newValue A positive rounding increment, or 0.0 to disable rounding.
     * Negative increments are equivalent to 0.0.
     * @see #getRoundingIncrement
     * @see #getRoundingMode
     * @see #setRoundingMode
     * @stable ICU 2.0
     */
    virtual void setRoundingIncrement(double newValue);

    /**
     * Get the rounding mode.
     * @return A rounding mode
     * @see #setRoundingIncrement
     * @see #getRoundingIncrement
     * @see #setRoundingMode
     * @stable ICU 2.0
     */
    virtual ERoundingMode getRoundingMode(void);

    /**
     * Set the rounding mode.  This has no effect unless the rounding
     * increment is greater than zero.
     * @param roundingMode A rounding mode
     * @see #setRoundingIncrement
     * @see #getRoundingIncrement
     * @see #getRoundingMode
     * @stable ICU 2.0
     */
    virtual void setRoundingMode(ERoundingMode roundingMode);

    /**
     * Get the width to which the output of format() is padded.
     * The width is counted in 16-bit code units.
     * @return the format width, or zero if no padding is in effect
     * @see #setFormatWidth
     * @see #getPadCharacter
     * @see #setPadCharacter
     * @see #getPadPosition
     * @see #setPadPosition
     * @stable ICU 2.0
     */
    virtual int32_t getFormatWidth(void);

    /**
     * Set the width to which the output of format() is padded.
     * The width is counted in 16-bit code units.
     * This method also controls whether padding is enabled.
     * @param width the width to which to pad the result of
     * format(), or zero to disable padding.  A negative
     * width is equivalent to 0.
     * @see #getFormatWidth
     * @see #getPadCharacter
     * @see #setPadCharacter
     * @see #getPadPosition
     * @see #setPadPosition
     * @stable ICU 2.0
     */
    virtual void setFormatWidth(int32_t width);

    /**
     * Get the pad character used to pad to the format width.  The
     * default is ' '.
     * @return a string containing the pad character. This will always
     * have a length of one 32-bit code point.
     * @see #setFormatWidth
     * @see #getFormatWidth
     * @see #setPadCharacter
     * @see #getPadPosition
     * @see #setPadPosition
     * @stable ICU 2.0
     */
    virtual UnicodeString getPadCharacterString();

    /**
     * Set the character used to pad to the format width.  If padding
     * is not enabled, then this will take effect if padding is later
     * enabled.
     * @param padChar a string containing the pad charcter. If the string
     * has length 0, then the pad characer is set to ' '.  Otherwise
     * padChar.char32At(0) will be used as the pad character.
     * @see #setFormatWidth
     * @see #getFormatWidth
     * @see #getPadCharacter
     * @see #getPadPosition
     * @see #setPadPosition
     * @stable ICU 2.0
     */
    virtual void setPadCharacter(const UnicodeString &padChar);

    /**
     * Get the position at which padding will take place.  This is the location
     * at which padding will be inserted if the result of format()
     * is shorter than the format width.
     * @return the pad position, one of kPadBeforePrefix,
     * kPadAfterPrefix, kPadBeforeSuffix, or
     * kPadAfterSuffix.
     * @see #setFormatWidth
     * @see #getFormatWidth
     * @see #setPadCharacter
     * @see #getPadCharacter
     * @see #setPadPosition
     * @see #kPadBeforePrefix
     * @see #kPadAfterPrefix
     * @see #kPadBeforeSuffix
     * @see #kPadAfterSuffix
     * @stable ICU 2.0
     */
    virtual EPadPosition getPadPosition(void);

    /**
     * Set the position at which padding will take place.  This is the location
     * at which padding will be inserted if the result of format()
     * is shorter than the format width.  This has no effect unless padding is
     * enabled.
     * @param padPos the pad position, one of kPadBeforePrefix,
     * kPadAfterPrefix, kPadBeforeSuffix, or
     * kPadAfterSuffix.
     * @see #setFormatWidth
     * @see #getFormatWidth
     * @see #setPadCharacter
     * @see #getPadCharacter
     * @see #getPadPosition
     * @see #kPadBeforePrefix
     * @see #kPadAfterPrefix
     * @see #kPadBeforeSuffix
     * @see #kPadAfterSuffix
     * @stable ICU 2.0
     */
    virtual void setPadPosition(EPadPosition padPos);

    /**
     * Return whether or not scientific notation is used.
     * @return TRUE if this object formats and parses scientific notation
     * @see #setScientificNotation
     * @see #getMinimumExponentDigits
     * @see #setMinimumExponentDigits
     * @see #isExponentSignAlwaysShown
     * @see #setExponentSignAlwaysShown
     * @stable ICU 2.0
     */
    virtual UBool isScientificNotation(void);

    /**
     * Set whether or not scientific notation is used. When scientific notation
     * is used, the effective maximum number of integer digits is <= 8.  If the
     * maximum number of integer digits is set to more than 8, the effective
     * maximum will be 1.  This allows this call to generate a 'default' scientific
     * number format without additional changes.
     * @param useScientific TRUE if this object formats and parses scientific
     * notation
     * @see #isScientificNotation
     * @see #getMinimumExponentDigits
     * @see #setMinimumExponentDigits
     * @see #isExponentSignAlwaysShown
     * @see #setExponentSignAlwaysShown
     * @stable ICU 2.0
     */
    virtual void setScientificNotation(UBool useScientific);

    /**
     * Return the minimum exponent digits that will be shown.
     * @return the minimum exponent digits that will be shown
     * @see #setScientificNotation
     * @see #isScientificNotation
     * @see #setMinimumExponentDigits
     * @see #isExponentSignAlwaysShown
     * @see #setExponentSignAlwaysShown
     * @stable ICU 2.0
     */
    virtual int8_t getMinimumExponentDigits(void);

    /**
     * Set the minimum exponent digits that will be shown.  This has no
     * effect unless scientific notation is in use.
     * @param minExpDig a value >= 1 indicating the fewest exponent digits
     * that will be shown.  Values less than 1 will be treated as 1.
     * @see #setScientificNotation
     * @see #isScientificNotation
     * @see #getMinimumExponentDigits
     * @see #isExponentSignAlwaysShown
     * @see #setExponentSignAlwaysShown
     * @stable ICU 2.0
     */
    virtual void setMinimumExponentDigits(int8_t minExpDig);

    /**
     * Return whether the exponent sign is always shown.
     * @return TRUE if the exponent is always prefixed with either the
     * localized minus sign or the localized plus sign, false if only negative
     * exponents are prefixed with the localized minus sign.
     * @see #setScientificNotation
     * @see #isScientificNotation
     * @see #setMinimumExponentDigits
     * @see #getMinimumExponentDigits
     * @see #setExponentSignAlwaysShown
     * @stable ICU 2.0
     */
    virtual UBool isExponentSignAlwaysShown(void);

    /**
     * Set whether the exponent sign is always shown.  This has no effect
     * unless scientific notation is in use.
     * @param expSignAlways TRUE if the exponent is always prefixed with either
     * the localized minus sign or the localized plus sign, false if only
     * negative exponents are prefixed with the localized minus sign.
     * @see #setScientificNotation
     * @see #isScientificNotation
     * @see #setMinimumExponentDigits
     * @see #getMinimumExponentDigits
     * @see #isExponentSignAlwaysShown
     * @stable ICU 2.0
     */
    virtual void setExponentSignAlwaysShown(UBool expSignAlways);

    /**
     * Return the grouping size. Grouping size is the number of digits between
     * grouping separators in the integer portion of a number.  For example,
     * in the number "123,456.78", the grouping size is 3.
     *
     * @return    the grouping size.
     * @see setGroupingSize
     * @see NumberFormat::isGroupingUsed
     * @see DecimalFormatSymbols::getGroupingSeparator
     * @stable ICU 2.0
     */
    int32_t getGroupingSize(void) const;

    /**
     * Set the grouping size. Grouping size is the number of digits between
     * grouping separators in the integer portion of a number.  For example,
     * in the number "123,456.78", the grouping size is 3.
     *
     * @param newValue    the new value of the grouping size.
     * @see getGroupingSize
     * @see NumberFormat::setGroupingUsed
     * @see DecimalFormatSymbols::setGroupingSeparator
     * @stable ICU 2.0
     */
    virtual void setGroupingSize(int32_t newValue);

    /**
     * Return the secondary grouping size. In some locales one
     * grouping interval is used for the least significant integer
     * digits (the primary grouping size), and another is used for all
     * others (the secondary grouping size).  A formatter supporting a
     * secondary grouping size will return a positive integer unequal
     * to the primary grouping size returned by
     * getGroupingSize().  For example, if the primary
     * grouping size is 4, and the secondary grouping size is 2, then
     * the number 123456789 formats as "1,23,45,6789", and the pattern
     * appears as "#,##,###0".
     * @return the secondary grouping size, or a value less than
     * one if there is none
     * @see setSecondaryGroupingSize
     * @see NumberFormat::isGroupingUsed
     * @see DecimalFormatSymbols::getGroupingSeparator
     * @draft ICU 2.4
     */
    int32_t getSecondaryGroupingSize(void) const;

    /**
     * Set the secondary grouping size. If set to a value less than 1,
     * then secondary grouping is turned off, and the primary grouping
     * size is used for all intervals, not just the least significant.
     *
     * @param newValue    the new value of the secondary grouping size.
     * @see getSecondaryGroupingSize
     * @see NumberFormat#setGroupingUsed
     * @see DecimalFormatSymbols::setGroupingSeparator
     * @draft ICU 2.4
     */
    virtual void setSecondaryGroupingSize(int32_t newValue);

    /**
     * Allows you to get the behavior of the decimal separator with integers.
     * (The decimal separator will always appear with decimals.)
     *
     * @return    TRUE if the decimal separator always appear with decimals.
     * Example: Decimal ON: 12345 -> 12345.; OFF: 12345 -> 12345
     * @stable ICU 2.0
     */
    UBool isDecimalSeparatorAlwaysShown(void) const;

    /**
     * Allows you to set the behavior of the decimal separator with integers.
     * (The decimal separator will always appear with decimals.)
     *
     * @param newValue    set TRUE if the decimal separator will always appear with decimals.
     * Example: Decimal ON: 12345 -> 12345.; OFF: 12345 -> 12345
     * @stable ICU 2.0
     */
    virtual void setDecimalSeparatorAlwaysShown(UBool newValue);

    /**
     * Synthesizes a pattern string that represents the current state
     * of this Format object.
     *
     * @param result    Output param which will receive the pattern.
     *                  Previous contents are deleted.
     * @return          A reference to 'result'.
     * @see applyPattern
     * @stable ICU 2.0
     */
    virtual UnicodeString& toPattern(UnicodeString& result) const;

    /**
     * Synthesizes a localized pattern string that represents the current
     * state of this Format object.
     *
     * @param result    Output param which will receive the localized pattern.
     *                  Previous contents are deleted.
     * @return          A reference to 'result'.
     * @see applyPattern
     * @stable ICU 2.0
     */
    virtual UnicodeString& toLocalizedPattern(UnicodeString& result) const;
 
    /**
     * Apply the given pattern to this Format object.  A pattern is a
     * short-hand specification for the various formatting properties.
     * These properties can also be changed individually through the
     * various setter methods.
     * <P>
     * There is no limit to integer digits are set
     * by this routine, since that is the typical end-user desire;
     * use setMaximumInteger if you want to set a real value.
     * For negative numbers, use a second pattern, separated by a semicolon
     * <pre>
     * .      Example "#,#00.0#" -> 1,234.56
     * </pre>
     * This means a minimum of 2 integer digits, 1 fraction digit, and
     * a maximum of 2 fraction digits.
     * <pre>
     * .      Example: "#,#00.0#;(#,#00.0#)" for negatives in parantheses.
     * </pre>
     * In negative patterns, the minimum and maximum counts are ignored;
     * these are presumed to be set in the positive pattern.
     *
     * @param pattern    The pattern to be applied.
     * @param parseError Struct to recieve information on position 
     *                   of error if an error is encountered
     * @param status     Output param set to success/failure code on
     *                   exit. If the pattern is invalid, this will be
     *                   set to a failure result.
     * @stable ICU 2.0
     */
    virtual void applyPattern(const UnicodeString& pattern,
                             UParseError& parseError,
                             UErrorCode& status);
    /**
     * Sets the pattern.
     * @param pattern   The pattern to be applied.
     * @param status    Output param set to success/failure code on
     *                  exit. If the pattern is invalid, this will be
     *                  set to a failure result.
     * @stable ICU 2.0
     */  
    virtual void applyPattern(const UnicodeString& pattern,
                             UErrorCode& status);

    /**
     * Apply the given pattern to this Format object.  The pattern
     * is assumed to be in a localized notation. A pattern is a
     * short-hand specification for the various formatting properties.
     * These properties can also be changed individually through the
     * various setter methods.
     * <P>
     * There is no limit to integer digits are set
     * by this routine, since that is the typical end-user desire;
     * use setMaximumInteger if you want to set a real value.
     * For negative numbers, use a second pattern, separated by a semicolon
     * <pre>
     * .      Example "#,#00.0#" -> 1,234.56
     * </pre>
     * This means a minimum of 2 integer digits, 1 fraction digit, and
     * a maximum of 2 fraction digits.
     *
     * Example: "#,#00.0#;(#,#00.0#)" for negatives in parantheses.
     *
     * In negative patterns, the minimum and maximum counts are ignored;
     * these are presumed to be set in the positive pattern.
     *
     * @param pattern   The localized pattern to be applied.
     * @param parseError Struct to recieve information on position 
     *                   of error if an error is encountered
     * @param status    Output param set to success/failure code on
     *                  exit. If the pattern is invalid, this will be
     *                  set to a failure result.
     * @stable ICU 2.0
     */
    virtual void applyLocalizedPattern(const UnicodeString& pattern,
                                       UParseError& parseError,
                                       UErrorCode& status);

    /**
     * Apply the given pattern to this Format object.
     *
     * @param pattern   The localized pattern to be applied.
     * @param status    Output param set to success/failure code on
     *                  exit. If the pattern is invalid, this will be
     *                  set to a failure result.
     * @stable ICU 2.0
     */
    virtual void applyLocalizedPattern(const UnicodeString& pattern,
                                       UErrorCode& status);


    /**
     * Sets the maximum number of digits allowed in the integer portion of a
     * number. This override limits the integer digit count to 309.
     *
     * @param newValue    the new value of the maximum number of digits 
     *                      allowed in the integer portion of a number.
     * @see NumberFormat#setMaximumIntegerDigits
     * @stable ICU 2.0
     */
    virtual void setMaximumIntegerDigits(int32_t newValue);

    /**
     * Sets the minimum number of digits allowed in the integer portion of a
     * number. This override limits the integer digit count to 309.
     * 
     * @param newValue    the new value of the minimum number of digits 
     *                      allowed in the integer portion of a number.
     * @see NumberFormat#setMinimumIntegerDigits
     * @stable ICU 2.0
     */
    virtual void setMinimumIntegerDigits(int32_t newValue);

    /**
     * Sets the maximum number of digits allowed in the fraction portion of a
     * number. This override limits the fraction digit count to 340.
     *
     * @param newValue    the new value of the maximum number of digits 
     *                    allowed in the fraction portion of a number.
     * @see NumberFormat#setMaximumFractionDigits
     * @stable ICU 2.0
     */
    virtual void setMaximumFractionDigits(int32_t newValue);

    /**
     * Sets the minimum number of digits allowed in the fraction portion of a
     * number. This override limits the fraction digit count to 340.
     *
     * @param newValue    the new value of the minimum number of digits 
     *                    allowed in the fraction portion of a number.
     * @see NumberFormat#setMinimumFractionDigits
     * @stable ICU 2.0
     */
    virtual void setMinimumFractionDigits(int32_t newValue);

    /**
     * Sets the currency used to display currency
     * amounts.  This takes effect immediately, if this format is a
     * currency format.  If this format is not a currency format, then
     * the currency is used if and when this object becomes a
     * currency format through the application of a new pattern.
     * @param theCurrency a 3-letter ISO code indicating new currency
     * to use.  It need not be null-terminated.  May be the empty
     * string or NULL to indicate no currency.
     * @param ec input-output error code
     * @draft ICU 3.0
     */
    virtual void setCurrency(const UChar* theCurrency, UErrorCode& ec);

    /**
     * The resource tags we use to retrieve decimal format data from
     * locale resource bundles.
     * @stable ICU 2.0
     */
    static const char fgNumberPatterns[];

public:

    /**
     * Return the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     * <pre>
     * .      Base* polymorphic_pointer = createPolymorphicObject();
     * .      if (polymorphic_pointer->getDynamicClassID() ==
     * .          Derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @stable ICU 2.0
     */
    static UClassID getStaticClassID(void);

    /**
     * Returns a unique class ID POLYMORPHICALLY.  Pure virtual override.
     * This method is to implement a simple version of RTTI, since not all
     * C++ compilers support genuine RTTI.  Polymorphic operator==() and
     * clone() methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @stable ICU 2.0
     */
    virtual UClassID getDynamicClassID(void) const;

private:
    DecimalFormat(); // default constructor not implemented

	int32_t precision(UBool isIntegral) const;

    /**
     * Do real work of constructing a new DecimalFormat.
     */
    void construct(UErrorCode&               status,
                   UParseError&             parseErr,
                   const UnicodeString*     pattern = 0,
                   DecimalFormatSymbols*    symbolsToAdopt = 0
                   );

    /**
     * Does the real work of generating a pattern.
     *
     * @param result     Output param which will receive the pattern.
     *                   Previous contents are deleted.
     * @param localized  TRUE return localized pattern.
     * @return           A reference to 'result'.
     */
    UnicodeString& toPattern(UnicodeString& result, UBool localized) const;

    /**
     * Does the real work of applying a pattern.
     * @param pattern    The pattern to be applied.
     * @param localized  If true, the pattern is localized; else false.
     * @param parseError Struct to recieve information on position 
     *                   of error if an error is encountered
     * @param status     Output param set to success/failure code on
     *                   exit. If the pattern is invalid, this will be
     *                   set to a failure result.
     */
    void applyPattern(const UnicodeString& pattern,
                            UBool localized,
                            UParseError& parseError,
                            UErrorCode& status);
    /**
     * Do the work of formatting a number, either a double or a long.
     *
     * @param appendTo       Output parameter to receive result.
     *                       Result is appended to existing contents.
     * @param fieldPosition  On input: an alignment field, if desired.
     *                       On output: the offsets of the alignment field.
     * @param digits         the digits to be formatted.
     * @param isInteger      if TRUE format the digits as Integer.
     * @return               Reference to 'appendTo' parameter.
     */
    UnicodeString& subformat(UnicodeString& appendTo,
                             FieldPosition& fieldPosition,
                             DigitList& digits,
                             UBool         isInteger) const;

    enum {
        fgStatusInfinite,
        fgStatusLength      // Leave last in list.
    } StatusFlags;

    /**
     * Parse the given text into a number.  The text is parsed beginning at
     * parsePosition, until an unparseable character is seen.
     * @param text The string to parse.
     * @param parsePosition The position at which to being parsing.  Upon
     *                      return, the first unparseable character.
     * @param digits        The DigitList to set to the parsed value.
     * @param isExponent    If true, parse an exponent.  This means no
     *                      infinite values and integer only.
     * @param status        Upon return contains boolean status flags indicating
     *                      whether the value was infinite and whether it was positive.
     */
    UBool subparse(const UnicodeString& text, ParsePosition& parsePosition,
                    DigitList& digits, UBool* status) const;

    int32_t skipPadding(const UnicodeString& text, int32_t position) const;

    int32_t compareAffix(const UnicodeString& input,
                         int32_t pos,
                         UBool isNegative,
                         UBool isPrefix) const;
    
    static int32_t compareSimpleAffix(const UnicodeString& affix,
                                      const UnicodeString& input,
                                      int32_t pos);
    
    static int32_t skipRuleWhiteSpace(const UnicodeString& text, int32_t pos);
    
    static int32_t skipUWhiteSpace(const UnicodeString& text, int32_t pos);
    
    int32_t compareComplexAffix(const UnicodeString& affixPat,
                                const UnicodeString& input,
                                int32_t pos) const;

    static int32_t match(const UnicodeString& text, int32_t pos, UChar32 ch);

    static int32_t match(const UnicodeString& text, int32_t pos, const UnicodeString& str);

    /**
     * Get a decimal format symbol.
     * Returns a const reference to the symbol string.
     * @internal
     */
    inline const UnicodeString &getConstSymbol(DecimalFormatSymbols::ENumberFormatSymbol symbol) const;

    int32_t appendAffix(UnicodeString& buf, double number,
                        UBool isNegative, UBool isPrefix) const;

    /**
     * Append an affix to the given UnicodeString, using quotes if
     * there are special characters.  Single quotes themselves must be
     * escaped in either case.
     */
    void appendAffixPattern(UnicodeString& appendTo, const UnicodeString& affix, 
                            UBool localized) const;

    void appendAffixPattern(UnicodeString& appendTo,
                            const UnicodeString* affixPattern,
                            const UnicodeString& expAffix, UBool localized) const;

    void expandAffix(const UnicodeString& pattern,
                     UnicodeString& affix,
                     double number,
                     UBool doFormat) const;

    void expandAffixes();
    
    static double round(double a, ERoundingMode mode, UBool isNegative);

    void addPadding(UnicodeString& appendTo,
                    FieldPosition& fieldPosition,
                    int32_t prefixLen, int32_t suffixLen) const;

    UBool isGroupingPosition(int32_t pos) const;

    void setCurrencyForSymbols();

    void setCurrencyForLocale(const char* locale, UErrorCode& ec);

    /**
     * Constants.
     */
    //static const int8_t fgMaxDigit; // The largest digit, in this case 9

    /*transient*/ //DigitList* fDigitList;

    UnicodeString           fPositivePrefix;
    UnicodeString           fPositiveSuffix;
    UnicodeString           fNegativePrefix;
    UnicodeString           fNegativeSuffix;
    UnicodeString*          fPosPrefixPattern;
    UnicodeString*          fPosSuffixPattern;
    UnicodeString*          fNegPrefixPattern;
    UnicodeString*          fNegSuffixPattern;

    /**
     * Formatter for ChoiceFormat-based currency names.  If this field
     * is not null, then delegate to it to format currency symbols.
     * @since ICU 2.6
     */
    ChoiceFormat*           fCurrencyChoice;

    int32_t                 fMultiplier;
    int32_t                 fGroupingSize;
    int32_t                 fGroupingSize2;
    UBool                  fDecimalSeparatorAlwaysShown;
    /*transient*/ UBool    fIsCurrencyFormat;
    DecimalFormatSymbols*   fSymbols;

    UBool                  fUseExponentialNotation;
    int8_t                  fMinExponentDigits;
    UBool                  fExponentSignAlwaysShown;

    /* If fRoundingIncrement is NULL, there is no rounding.  Otherwise, round to
     * fRoundingIncrement.getDouble().  Since this operation may be expensive,
     * we cache the result in fRoundingDouble.  All methods that update
     * fRoundingIncrement also update fRoundingDouble. */
    DigitList*              fRoundingIncrement;
    /*transient*/ double    fRoundingDouble;
    ERoundingMode           fRoundingMode;

    UChar32                 fPad;
    int32_t                 fFormatWidth;
    EPadPosition            fPadPosition;

protected:
  /** number of integer digits 
   * @draft ICU 2.4
   */  
    static const int32_t  kDoubleIntegerDigits;
  /** number of fraction digits 
   * @draft ICU 2.4
   */  
    static const int32_t  kDoubleFractionDigits;

    /**
     * When someone turns on scientific mode, we assume that more than this
     * number of digits is due to flipping from some other mode that didn't
     * restrict the maximum, and so we force 1 integer digit.  We don't bother
     * to track and see if someone is using exponential notation with more than
     * this number, it wouldn't make sense anyway, and this is just to make sure
     * that someone turning on scientific mode with default settings doesn't
     * end up with lots of zeroes.
     */
	static const int32_t  kMaxScientificIntegerDigits;
};

inline UnicodeString&
DecimalFormat::format(const Formattable& obj,
                      UnicodeString& appendTo,
                      UErrorCode& status) const {
    // Don't use Format:: - use immediate base class only,
    // in case immediate base modifies behavior later.
    return NumberFormat::format(obj, appendTo, status);
}

inline UnicodeString&
DecimalFormat::format(double number,
                      UnicodeString& appendTo) const {
    FieldPosition pos(0);
    return format(number, appendTo, pos);
}

inline UnicodeString&
DecimalFormat::format(int32_t number,
                      UnicodeString& appendTo) const {
    FieldPosition pos(0);
    return format((int64_t)number, appendTo, pos);
}

inline const UnicodeString &
DecimalFormat::getConstSymbol(DecimalFormatSymbols::ENumberFormatSymbol symbol) const {
    return fSymbols->getConstSymbol(symbol);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // _DECIMFMT
//eof
