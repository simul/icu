/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-1999, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*
* File CALTZTST.H
*
* Modification History:
*
*   Date        Name        Description
*   08/06/97    aliu        Creation.
********************************************************************************
*/

#include "caltztst.h"
#include "unicode/smpdtfmt.h"
#include "mutex.h"

DateFormat*         CalendarTimeZoneTest::fgDateFormat = 0;
Calendar*           CalendarTimeZoneTest::fgCalendar   = 0;

UBool CalendarTimeZoneTest::failure(UErrorCode status, const char* msg)
{
    if (U_FAILURE(status))
    {
        errln(UnicodeString("FAIL: ") + msg + " failed, error " + u_errorName(status));
        return TRUE;
    }
    return FALSE;
}

DateFormat*   CalendarTimeZoneTest::getDateFormat()
{
    DateFormat *theFormat = 0;

    if (fgDateFormat != 0) // if there's something in the cache
    {
        Mutex lock;

        if (fgDateFormat != 0) // Someone might have grabbed it.
        {
            theFormat = fgDateFormat;
            fgDateFormat = 0; // We have exclusive right to this formatter.
        }
    }

    if(theFormat == 0) // If we weren't able to pull it out of the cache, then we have to create it.
    {
        UErrorCode status = U_ZERO_ERROR;
        theFormat = new SimpleDateFormat(UnicodeString("EEE MMM dd HH:mm:ss zzz yyyy"), status);
        if (U_FAILURE(status))
        {
            delete theFormat;
            theFormat = 0;
            errln("FAIL: Could not create SimpleDateFormat");
        }
    }

    return theFormat;
}

void CalendarTimeZoneTest::releaseDateFormat(DateFormat *adopt)
{
    if(fgDateFormat == 0) // If the cache is empty we must add it back.
    {
        Mutex lock;

        if(fgDateFormat == 0)
        {
            fgDateFormat = adopt;
            adopt = 0;
        }
    }
    else {
        delete adopt;
    }
}

Calendar*  CalendarTimeZoneTest::getCalendar()
{
    Calendar *theCalendar = 0;

    if (fgCalendar != 0) // if there's something in the cache
    {
        Mutex lock;

        if (fgCalendar != 0) // Someone might have grabbed it.
        {
            theCalendar = fgCalendar;
            fgCalendar = 0; // We have exclusive right to this calendar.
        }
    }

    if(theCalendar == 0) // If we weren't able to pull it out of the cache, then we have to create it.
    {
        UErrorCode status = U_ZERO_ERROR;
        theCalendar = Calendar::createInstance(status);
        if (U_FAILURE(status))
        {
            delete theCalendar;
            theCalendar = 0;
            errln("FAIL: Calendar::createInstance failed");
        }
    }
    return theCalendar;
}

void CalendarTimeZoneTest::releaseCalendar(Calendar* adopt)
{
    if(fgCalendar == 0) // If the cache is empty we must add it back.
    {
        Mutex lock;

        if(fgCalendar == 0)
        {
            fgCalendar = adopt;
            adopt = 0;
        }
    }
    else
    {
        delete adopt;
    }
}

// Utility method for formatting dates for printing; useful for Java->C++ conversion.
// Tries to mimic the Java Date.toString() format.
UnicodeString
CalendarTimeZoneTest::dateToString(UDate d)
{
    UnicodeString str;
    return dateToString(d, str);
}

UnicodeString&
CalendarTimeZoneTest::dateToString(UDate d, UnicodeString& str)
{
    str.remove();
    DateFormat* format = getDateFormat();
    if (format == 0)
    {
        str += "DATE_FORMAT_FAILURE";
        return str;
    }
    format->format(d, str);
    releaseDateFormat(format);
    return str;
}

// Utility methods to create a date.  This is useful for converting Java constructs
// which create a Date object.
UDate
CalendarTimeZoneTest::date(int32_t y, int32_t m, int32_t d, int32_t hr, int32_t min, int32_t sec)
{
    Calendar* cal = getCalendar();
    if (cal == 0) return 0.0;
    cal->clear();
    cal->set(1900 + y, m, d, hr, min, sec); // Add 1900 to follow java.util.Date protocol
    UErrorCode status = U_ZERO_ERROR;
    UDate dt = cal->getTime(status);
    releaseCalendar(cal);
    if (U_FAILURE(status))
    {
        errln("FAIL: Calendar::getTime failed");
        return 0.0;
    }
    return dt;
}

// Utility methods to create a date.  The returned Date is UTC rather than local.
/*Date
CalendarTimeZoneTest::utcDate(int32_t y, int32_t m, int32_t d, int32_t hr, int32_t min, int32_t sec)
{
    Calendar* cal = getCalendar();
    if (cal == 0) return 0.0;
    UErrorCode status = U_ZERO_ERROR;
    Date dt = date(y, m, d, hr, min, sec) +
        cal->get(Calendar::ZONE_OFFSET, status) -
        cal->get(Calendar::DST_OFFSET, status);
    releaseCalendar(cal);
    if (U_FAILURE(status))
    {
        errln("FAIL: Calendar::get failed");
        return 0.0;
    }
    return dt;
}*/

// Mimics Date.getYear() etc.
void
CalendarTimeZoneTest::dateToFields(UDate date, int32_t& y, int32_t& m, int32_t& d, int32_t& hr, int32_t& min, int32_t& sec)
{
    Calendar* cal = getCalendar();
    if (cal == 0) return;
    UErrorCode status = U_ZERO_ERROR;
    cal->setTime(date, status);
    y = cal->get(Calendar::YEAR, status) - 1900;
    m = cal->get(Calendar::MONTH, status);
    d = cal->get(Calendar::DATE, status);
    hr = cal->get(Calendar::HOUR_OF_DAY, status);
    min = cal->get(Calendar::MINUTE, status);
    sec = cal->get(Calendar::SECOND, status);
    releaseCalendar(cal);
}

//eof
