/*
*******************************************************************************
* Copyright (C) 2007, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef RELDTFMT_H
#define RELDTFMT_H

#include "unicode/utypes.h"

/**
 * \file 
 * \brief C++ API: Format and parse relative dates and times.
 */
 
#if !UCONFIG_NO_FORMATTING

#include "unicode/datefmt.h"

U_NAMESPACE_BEGIN

// forward declarations
class CalendarData;
class MessageFormat;

// internal structure used for caching strings
struct URelativeString;

/**
 * This class is normally accessed using the kRelative or k...Relative values of EStyle as parameters to DateFormat::createDateInstance.
 * 
 * Example: 
 *     DateFormat *fullrelative = DateFormat::createDateInstance(DateFormat::kFullRelative, loc);
 * 
 * @draft ICU 3.8
 */

class U_I18N_API RelativeDateFormat : public DateFormat {
public:
    RelativeDateFormat( UDateFormatStyle timeStyle, UDateFormatStyle dateStyle, const Locale& locale, UErrorCode& status);

    // overrides
    /**
     * Copy constructor.
     * @draft ICU 3.8
     */
    RelativeDateFormat(const RelativeDateFormat&);

    /**
     * Assignment operator.
     * @draft ICU 3.8
     */
    RelativeDateFormat& operator=(const RelativeDateFormat&);

    /**
     * Destructor.
     * @draft ICU 3.8
     */
    virtual ~RelativeDateFormat();

    /**
     * Clone this Format object polymorphically. The caller owns the result and
     * should delete it when done.
     * @return    A copy of the object.
     * @draft ICU 3.8
     */
    virtual Format* clone(void) const;

    /**
     * Return true if the given Format objects are semantically equal. Objects
     * of different subclasses are considered unequal.
     * @param other    the object to be compared with.
     * @return         true if the given Format objects are semantically equal.
     * @draft ICU 3.8
     */
    virtual UBool operator==(const Format& other) const;

    /**
     * Format a date or time, which is the standard millis since 24:00 GMT, Jan
     * 1, 1970. Overrides DateFormat pure virtual method.
     * <P>
     * Example: using the US locale: "yyyy.MM.dd e 'at' HH:mm:ss zzz" ->>
     * 1996.07.10 AD at 15:08:56 PDT
     *
     * @param cal       Calendar set to the date and time to be formatted
     *                  into a date/time string.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       The formatting position. On input: an alignment field,
     *                  if desired. On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @draft ICU 3.8
     */
    virtual UnicodeString& format(  Calendar& cal,
                                    UnicodeString& appendTo,
                                    FieldPosition& pos) const;


    /**
     * Parse a date/time string beginning at the given parse position. For
     * example, a time text "07/10/96 4:5 PM, PDT" will be parsed into a Date
     * that is equivalent to Date(837039928046).
     * <P>
     * By default, parsing is lenient: If the input is not in the form used by
     * this object's format method but can still be parsed as a date, then the
     * parse succeeds. Clients may insist on strict adherence to the format by
     * calling setLenient(false).
     *
     * @param text  The date/time string to be parsed
     * @param cal   a Calendar set to the date and time to be formatted
     *              into a date/time string.
     * @param pos   On input, the position at which to start parsing; on
     *              output, the position at which parsing terminated, or the
     *              start position if the parse failed.
     * @return      A valid UDate if the input could be parsed.
     * @draft ICU 3.8
     */
    virtual void parse( const UnicodeString& text,
                        Calendar& cal,
                        ParsePosition& pos) const;                        
private:
    DateFormat *fDateFormat; // the held date format 
    DateFormat *fTimeFormat; // the held time format
    MessageFormat *fCombinedFormat; //  the {0} {1} format. 
    
    UDateFormatStyle dateStyle;
    UDateFormatStyle timeStyle;
    Locale  locale;
    
    CalendarData *fCalData; // holds a reference to the resource data
    UResourceBundle *fStrings; // the array of strings
    
    int32_t dayMin;    // day id of lowest #
    int32_t dayMax;    // day id of highest #
    int32_t datesLen;    // Length of array
    URelativeString *dates; // array of strings
    
    
    /**
     * Get the string at a specific offset.
     * @param day day offset ( -1, 0, 1, etc.. )
     * @param len on output, length of string. 
     * @return the string, or NULL if none at that location.
     */
    const UChar *getStringForDay(int32_t day, int32_t &len, UErrorCode &status) const;
    
    /** 
     * Load the Date string array
     */
    void loadDates(UErrorCode &status) const;
    
    /**
     * Load the Strings resource needed for relative dates. Returns an error and null if it could not be loaded.
     * Semantic const.
     */
    UResourceBundle *getStrings(UErrorCode &status) const;

    /**
     * @return the number of days in "until-now"
     */
    static int32_t dayDifference(Calendar &until, UErrorCode &status);
    
    /**
     * initializes fCalendar from parameters.  Returns fCalendar as a convenience.
     * @param adoptZone  Zone to be adopted, or NULL for TimeZone::createDefault().
     * @param locale Locale of the calendar
     * @param status Error code
     * @return the newly constructed fCalendar
     * @draft ICU 3.8
     */
    Calendar* initializeCalendar(TimeZone* adoptZone, const Locale& locale, UErrorCode& status);
    
public:
    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       erived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @draft ICU 3.8
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID(void) const;
};


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // RELDTFMT_H
//eof
