/*
**********************************************************************
*   Copyright (C) 1999, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   11/24/99    aliu        Creation.
**********************************************************************
*/

/* This program reads a text file full of parsed time zone data and
 * outputs a binary file, tz.dat, which then goes on to become part of
 * the memory-mapped (or dll) ICU data file.
 *
 * The data file read by this program is generated by a perl script,
 * tz.pl.  The input to tz.pl is standard unix time zone data from
 * ftp://elsie.nci.nih.gov.
 *
 * As a matter of policy, the perl script tz.pl wants to do as much of
 * the parsing, data processing, and error checking as possible, and
 * this program wants to just do the binary translation step.
 * 
 * See tz.pl for the file format that is READ by this program.
 */

#include <stdio.h>
#include <stdlib.h>
#include "unicode/utypes.h"
#include "cmemory.h"
#include "cstring.h"
#include "filestrm.h"
#include "unicode/udata.h"
#include "unewdata.h"
#include "tzdat.h"

#define INPUT_FILE "tz.txt"
#define OUTPUT_FILE "tz.dat"

/* UDataInfo cf. udata.h */
static UDataInfo dataInfo = {
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    sizeof(UChar),
    0,

     0x7a, 0x6f, 0x6e, 0x65,  /* see TZ_SIG. Changed to literals, thanks to HP compiler */
    TZ_FORMAT_VERSION, 0, 0, 0,                 /* formatVersion */
    0, 0, 0, 0 /* dataVersion - will be filled in with year.suffix */
};


class gentz {
    // These must match SimpleTimeZone!!!
    enum { WALL_TIME = 0,
           STANDARD_TIME,
           UTC_TIME
    };
    
    // The largest number of zones we accept as sensible.  Anything
    // larger is considered an error.  Adjust as needed.
    enum { MAX_ZONES = 1000 };

    // The largest maxNameLength we accept as sensible.  Adjust as needed.
    enum { MAX_MAX_NAME_LENGTH = 100 };

    // The maximum sensible GMT offset, in seconds
    static const int32_t MAX_GMT_OFFSET;

    static const char COMMENT;
    static const char CR;
    static const char LF;
    static const char MINUS;
    static const char SPACE;
    static const char TAB;
    static const char ZERO;
    static const char SEP;
    static const char NUL;
    
    static const char* END_KEYWORD;

    enum { BUFLEN = 1024 };
    char buffer[BUFLEN];
    int32_t lineNumber;
    
    TZHeader header;
    StandardZone* stdZones;
    DSTZone* dstZones;
    char* nameTable;
    int32_t* indexByName;
    OffsetIndex* indexByOffset;
    
    int32_t maxPerOffset; // Maximum number of zones per offset
    int32_t stdZoneSize;
    int32_t dstZoneSize;
    int32_t offsetIndexSize; // Total bytes in offset index table
    int32_t nameTableSize; // Total bytes in name table

    bool_t useCopyright;

public:
    int     main(int argc, char *argv[]);
private:
    int32_t  writeTzDatFile(const char *destdir);
    void     parseTzTextFile(FileStream* in);

    // High level parsing
    void          parseHeader(FileStream* in);

    StandardZone* parseStandardZones(FileStream* in);
    void          parse1StandardZone(FileStream* in, StandardZone& zone);

    DSTZone*      parseDSTZones(FileStream* in);
    void          parse1DSTZone(FileStream* in, DSTZone& zone);
    void          parseDSTRule(char*& p, TZRule& rule);

    int32_t*      parseIndexTable(FileStream* in);
    OffsetIndex*  parseOffsetIndexTable(FileStream* in);

    char*         parseNameTable(FileStream* in);

    // Low level parsing and reading
    void     readEndMarker(FileStream* in);
    int32_t  readIntegerLine(FileStream* in, int32_t min, int32_t max);
    int32_t  _parseInteger(char*& p);
    int32_t  parseInteger(char*& p, char nextExpectedChar, int32_t, int32_t);
    int32_t  readLine(FileStream* in);

    // Error handling
    void    die(const char* msg);
    void    usage(const char* argv0);
};

int main(int argc, char *argv[]) {
    gentz x;
    return x.main(argc, argv);
}

const int32_t gentz::MAX_GMT_OFFSET = (int32_t)24*60*60; // seconds
const char    gentz::COMMENT        = '#';
const char    gentz::CR             = ((char)13);
// OS390 uses x'15' NL new line for LF               
#ifdef OS390                                         
const char    gentz::LF             = ((char)21);    
#else
const char    gentz::LF             = ((char)10);
#endif
const char    gentz::MINUS          = '-';
const char    gentz::SPACE          = ' ';
const char    gentz::TAB            = ((char)9);
const char    gentz::ZERO           = '0';
const char    gentz::SEP            = ',';
const char    gentz::NUL            = ((char)0);
const char*   gentz::END_KEYWORD    = "end";

void gentz::usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [-c[+|-]] infile\n"
            " -c[+|-] [do|do not] include copyright (default=+)\n"
            " infile  text file produced by tz.pl\n",
            argv0);
    exit(1);
}

int gentz::main(int argc, char *argv[]) {
    ////////////////////////////////////////////////////////////
    // Parse arguments
    ////////////////////////////////////////////////////////////
    useCopyright = TRUE;
    const char* infile = 0;
    const char* destdir = 0;
    for (int i=1; i<argc; ++i) {
        const char* arg = argv[i];
        if (arg[0] == '-') {
            if (arg[1] != 'c') {
                usage(argv[0]);
            }
            switch (arg[2]) {
            case '+':
                useCopyright = TRUE;
                break;
            case '-':
                useCopyright = FALSE;
                break;
            default:
                usage(argv[0]);
            }
        } else if (infile == 0) {
            infile = arg;
        } else {
            usage(argv[0]);
        }
    }
    if (infile == 0) {
        usage(argv[0]);
    }
    if (destdir == 0) {
        destdir = u_getDataDirectory();
    }
    ////////////////////////////////////////////////////////////
    // Read the input file
    ////////////////////////////////////////////////////////////
    *buffer = NUL;
    lineNumber = 0;
    fprintf(stdout, "Input file: %s\n", infile);
    FileStream* in = T_FileStream_open(infile, "r");
    if (in == 0) {
        die("Cannot open input file");
    }
    parseTzTextFile(in);
    T_FileStream_close(in);
    *buffer = NUL;

    ////////////////////////////////////////////////////////////
    // Write the output file
    ////////////////////////////////////////////////////////////
    int32_t wlen = writeTzDatFile(destdir);
    fprintf(stdout, "Output file: %s.%s, %ld bytes\n",
            TZ_DATA_NAME, TZ_DATA_TYPE, wlen);

    return 0; // success
}

int32_t gentz::writeTzDatFile(const char *destdir) {
    UNewDataMemory *pdata;
    UErrorCode status = U_ZERO_ERROR;

    // Fill in dataInfo with year.suffix
    *(uint16_t*)&(dataInfo.dataVersion[0]) = header.versionYear;
    *(uint16_t*)&(dataInfo.dataVersion[2]) = header.versionSuffix;

    pdata = udata_create(destdir, TZ_DATA_TYPE, TZ_DATA_NAME, &dataInfo,
                         useCopyright ? U_COPYRIGHT_STRING : 0, &status);
    if (U_FAILURE(status)) {
        die("Unable to create data memory");
    }

    // Careful: This order cannot be changed (without changing
    // the offset fixup code).
    udata_writeBlock(pdata, &header, sizeof(header));
    udata_writeBlock(pdata, stdZones, stdZoneSize);
    udata_writeBlock(pdata, dstZones, dstZoneSize);
    udata_writeBlock(pdata, indexByName, header.count * sizeof(indexByName[0]));
    udata_writeBlock(pdata, indexByOffset, offsetIndexSize);
    udata_writeBlock(pdata, nameTable, nameTableSize);

    uint32_t dataLength = udata_finish(pdata, &status);
    if (U_FAILURE(status)) {
        die("Error writing output file");
    }

    if (dataLength != (sizeof(header) + stdZoneSize +
                       dstZoneSize + nameTableSize +
                       header.count * sizeof(indexByName[0]) +
                       offsetIndexSize
                       )) {
        die("Written file doesn't match expected size");
    }
    return dataLength;
}

void gentz::parseTzTextFile(FileStream* in) {
    parseHeader(in);
    stdZones = parseStandardZones(in);
    dstZones = parseDSTZones(in);
    if (header.count != (header.standardCount + header.dstCount)) {
        die("Zone counts don't add up");
    }
    nameTable = parseNameTable(in);

    // Fixup the header offsets
    header.standardDelta = sizeof(header);
    header.dstDelta = header.standardDelta + stdZoneSize;
    header.nameIndexDelta = header.dstDelta + dstZoneSize;

    // Read in index tables after header is mostly fixed up
    indexByName = parseIndexTable(in);
    indexByOffset = parseOffsetIndexTable(in);

    header.offsetIndexDelta = header.nameIndexDelta + header.count *
        sizeof(indexByName[0]);
    header.nameTableDelta = header.offsetIndexDelta + offsetIndexSize;

    if (header.standardDelta < 0 ||
        header.dstDelta < 0 ||
        header.nameTableDelta < 0) {
        die("Negative offset in header after fixup");
    }
}

/**
 * Index tables are lists of specifiers of the form /[sd]\d+/, where
 * the first character determines if it is a standard or DST zone,
 * and the following number is in the range 0..n-1, where n is the
 * count of that type of zone.
 *
 * Header must already be read in and the offsets must be fixed up.
 * Standard and DST zones must be read in.
 */
int32_t* gentz::parseIndexTable(FileStream* in) {
    uint32_t n = readIntegerLine(in, 1, MAX_ZONES);
    if (n != header.count) {
        die("Count mismatch in index table");
    }
    int32_t* result = new int32_t[n];
    for (uint32_t i=0; i<n; ++i) {
        readLine(in);
        char* p = buffer+1;
        uint32_t index = parseInteger(p, NUL, 0, header.count);
        switch (buffer[0]) {
        case 's':
            if (index >= header.standardCount) {
                die("Standard index entry out of range");
            }
            result[i] = header.standardDelta +
                ((char*)&stdZones[index] - (char*)&stdZones[0]); 
            break;
        case 'd':
            if (index >= header.dstCount) {
                die("DST index entry out of range");
            } 
            result[i] = header.dstDelta +
                ((char*)&dstZones[index] - (char*)&dstZones[0]);
            break;
        default:
            die("Malformed index entry");
            break;
        }
    }
    readEndMarker(in);
    fprintf(stdout, " Read %lu name index table entries, in-memory size %ld bytes\n",
            n, n * sizeof(int32_t));
    return result;
}

OffsetIndex* gentz::parseOffsetIndexTable(FileStream* in) {
    uint32_t n = readIntegerLine(in, 1, MAX_ZONES);

    // We don't know how big the whole thing will be yet, but we can use
    // the maxPerOffset number to compute an upper limit.
    //
    // The gmtOffset field within each OffsetIndex struct must be
    // 4-aligned for some architectures.  To ensure this, we do two
    // things: 1. The entire struct is 4-aligned.  2. The gmtOffset is
    // placed at a 4-aligned position within the struct.  3. The size
    // of the whole structure is padded out to 4n bytes.  We achieve
    // this last condition by adding two bytes of padding after the
    // last zoneNumber, if count is _even_.  That is, the struct size
    // is 10+2count+padding, where padding is (count%2==0 ? 2:0).
    //
    // Note that we don't change the count itself, but rather adjust
    // the nextEntryDelta and add 2 bytes of padding if necessary.
    //
    // Don't try to compute the exact size in advance
    // (unless we want to avoid the use of sizeof(), which may
    // introduce padding that we won't actually employ).
    int32_t maxPossibleSize = n * (sizeof(OffsetIndex) +
        (maxPerOffset-1) * sizeof(uint16_t));

    int8_t *result = new int8_t[maxPossibleSize];
    if (result == 0) {
        die("Out of memory");
    }

    // Read each line and construct the corresponding entry
    OffsetIndex* index = (OffsetIndex*)result;
    for (uint32_t i=0; i<n; ++i) {
        uint16_t alignedCount;
        readLine(in);
        char* p = buffer;
        index->gmtOffset = 1000 * // Convert s -> ms
            parseInteger(p, SEP, -MAX_GMT_OFFSET, MAX_GMT_OFFSET);
        index->defaultZone = (uint16_t)parseInteger(p, SEP, 0, header.count-1);
        index->count = (uint16_t)parseInteger(p, SEP, 1, maxPerOffset);
        uint16_t* zoneNumberArray = &(index->zoneNumber);
        bool_t sawOffset = FALSE; // Sanity check - make sure offset is in zone list
        for (uint16_t j=0; j<index->count; ++j) {
            zoneNumberArray[j] = (uint16_t)
                parseInteger(p, (j==(index->count-1))?NUL:SEP,
                             0, header.count-1);
            if (zoneNumberArray[j] == index->defaultZone) {
                sawOffset = TRUE;
            }
        }
        if (!sawOffset) {
            die("Error: bad offset index entry; default not in zone list");
        }
        alignedCount = index->count;
        if((alignedCount%2)==0) /* force count to be ODD - see above */
        {
            // Use invalid zoneNumber for 2 bytes of padding
            zoneNumberArray[alignedCount++] = (uint16_t)0xFFFF;
        }
        int8_t* nextIndex = (int8_t*)&(zoneNumberArray[alignedCount]);
	
        index->nextEntryDelta = (i==(n-1)) ? 0 : (nextIndex - (int8_t*)index);
        index = (OffsetIndex*)nextIndex;
    }
    offsetIndexSize = (int8_t*)index - (int8_t*)result;
    if (offsetIndexSize > maxPossibleSize) {
        die("Yikes! Interal error while constructing offset index table");
    }
    readEndMarker(in);
    fprintf(stdout, " Read %lu offset index table entries, in-memory size %ld bytes\n",
            n, offsetIndexSize);
    return (OffsetIndex*)result;
}

void gentz::parseHeader(FileStream* in) {
    int32_t ignored;

    // Version string, e.g., "1999j" -> (1999<<16) | 10
    header.versionYear = (uint16_t) readIntegerLine(in, 1990, 0xFFFF);
    header.versionSuffix = (uint16_t) readIntegerLine(in, 0, 0xFFFF);

    header.count = readIntegerLine(in, 1, MAX_ZONES);
    maxPerOffset = readIntegerLine(in, 1, MAX_ZONES);
    /*header.maxNameLength*/ ignored = readIntegerLine(in, 1, MAX_MAX_NAME_LENGTH);

    // Size of name table in bytes
    // (0x00FFFFFF is an arbitrary upper limit; adjust as needed.)
    nameTableSize = readIntegerLine(in, 1, 0x00FFFFFF);

    fprintf(stdout, " Read header, data version %u(%u), in-memory size %ld bytes\n",
            header.versionYear, header.versionSuffix, sizeof(header));
}

StandardZone* gentz::parseStandardZones(FileStream* in) {
    header.standardCount = readIntegerLine(in, 1, MAX_ZONES);
    StandardZone* zones = new StandardZone[header.standardCount];
    if (zones == 0) {
        die("Out of memory");
    }
    for (uint32_t i=0; i<header.standardCount; i++) {
        parse1StandardZone(in, zones[i]);
    }
    readEndMarker(in);
    stdZoneSize = (char*)&stdZones[header.standardCount] - (char*)&stdZones[0];
    fprintf(stdout, " Read %lu standard zones, in-memory size %ld bytes\n",
            header.standardCount, stdZoneSize);
    return zones;
}

void gentz::parse1StandardZone(FileStream* in, StandardZone& zone) {
    readLine(in);
    char* p = buffer;
    /*zone.nameDelta =*/ parseInteger(p, SEP, 0, nameTableSize);
    zone.gmtOffset = 1000 * // Convert s -> ms
        parseInteger(p, NUL, -MAX_GMT_OFFSET, MAX_GMT_OFFSET);
}

DSTZone* gentz::parseDSTZones(FileStream* in) {
    header.dstCount = readIntegerLine(in, 1, MAX_ZONES);
    DSTZone* zones = new DSTZone[header.dstCount];
    if (zones == 0) {
        die("Out of memory");
    }
    for (uint32_t i=0; i<header.dstCount; i++) {
        parse1DSTZone(in, zones[i]);
    }
    readEndMarker(in);
    dstZoneSize = (char*)&dstZones[header.dstCount] - (char*)&dstZones[0];
    fprintf(stdout, " Read %lu DST zones, in-memory size %ld bytes\n",
            header.dstCount, dstZoneSize);
    return zones;
}

void gentz::parse1DSTZone(FileStream* in, DSTZone& zone) {
    readLine(in);
    char* p = buffer;
    /*zone.nameDelta =*/ parseInteger(p, SEP, 0, nameTableSize);
    zone.gmtOffset = 1000 * // Convert s -> ms
        parseInteger(p, SEP, -MAX_GMT_OFFSET, MAX_GMT_OFFSET);
    parseDSTRule(p, zone.onsetRule);
    parseDSTRule(p, zone.ceaseRule);
    zone.dstSavings = (uint16_t) parseInteger(p, NUL, 0, 12*60);
}

void gentz::parseDSTRule(char*& p, TZRule& rule) {
    rule.month = (uint8_t) parseInteger(p, SEP, 0, 11);
    rule.dowim = (int8_t) parseInteger(p, SEP, -31, 31);
    rule.dow = (int8_t) parseInteger(p, SEP, -7, 7);
    rule.time = (uint16_t) parseInteger(p, SEP, 0, 24*60);
    rule.mode = *p++;
    if (*p++ != SEP) {
        die("Separator missing");
    }
    switch ((char)rule.mode) {
    case 'w':
        rule.mode = WALL_TIME;
        break;
    case 's':
        rule.mode = STANDARD_TIME;
        break;
    case 'u':
        rule.mode = UTC_TIME;
        break;
    default:
        die("Invalid rule time mode");
        break;
    }
}

char* gentz::parseNameTable(FileStream* in) {
    int32_t n = readIntegerLine(in, 1, MAX_ZONES);
    if (n != (int32_t)header.count) {
        die("Zone count doesn't match name table count");
    }
    char* names = new char[nameTableSize];
    if (names == 0) {
        die("Out of memory");
    }
    char* p = names;
    char* limit = names + nameTableSize;
    for (int32_t i=0; i<n; ++i) {
        int32_t len = readLine(in);
        if ((p + len) <= limit) {
            uprv_memcpy(p, buffer, len);
            p += len;
            *p++ = NUL;
        } else {
            die("Name table longer than declared size");
        }
    }
    if (p != limit) {
        die("Name table shorter than declared size");
    }
    readEndMarker(in);
    fprintf(stdout, " Read %ld names, in-memory size %ld bytes\n", n, nameTableSize);
    return names;
}

/**
 * Read the end marker (terminates each list).
 */
void gentz::readEndMarker(FileStream* in) {
    readLine(in);
    if (uprv_strcmp(buffer, END_KEYWORD) != 0) {
        die("Keyword 'end' missing");
    }
}

/**
 * Read a line from the FileStream and parse it as an
 * integer.  There should be nothing else on the line.
 */
int32_t gentz::readIntegerLine(FileStream* in, int32_t min, int32_t max) {
    readLine(in);
    char* p = buffer;
    return parseInteger(p, NUL, min, max);
}

/**
 * Parse an integer from the given character buffer.
 * Advance p past the last parsed character.  Return
 * the result.  The integer must be of the form
 * /-?\d+/.
 */
int32_t gentz::_parseInteger(char*& p) {
    int32_t n = 0;
    int32_t digitCount = 0;
    int32_t digit;
    bool_t negative = FALSE;
    if (*p == MINUS) {
        ++p;
        negative = TRUE;
    }
    for (;;) {
        digit = *p - ZERO;
        if (digit < 0 || digit > 9) {
            break;
        }
        n = 10*n + digit;
        p++;
        digitCount++;
    }
    if (digitCount < 1) {
        die("Unable to parse integer");
    }
    if (negative) {
        n = -n;
    }
    return n;
}

int32_t gentz::parseInteger(char*& p, char nextExpectedChar,
                            int32_t min, int32_t max) {
    int32_t n = _parseInteger(p);
    if (*p++ != nextExpectedChar) {
        die("Character following integer unexpected");
    }
    if (n < min || n > max) {
        die("Integer field out of range");
    }
    return n;
}

void gentz::die(const char* msg) {
    fprintf(stderr, "ERROR, %s\n", msg);
    if (*buffer) {
        fprintf(stderr, "Input file line %ld: \"%s\"\n", lineNumber, buffer);
    }
    exit(1);
}

int32_t gentz::readLine(FileStream* in) {
    ++lineNumber;
    T_FileStream_readLine(in, buffer, BUFLEN);
    // Trim off trailing comment
    char* p = uprv_strchr(buffer, COMMENT);
    if (p != 0) {
        // Back up past any space or tab characters before
        // the comment character.
        while (p > buffer && (p[-1] == SPACE || p[-1] == TAB)) {
            p--;
        }
        *p = NUL;
    }
    // Delete any trailing ^J and/or ^M characters
    p = buffer + uprv_strlen(buffer);
    while (p > buffer && (p[-1] == CR || p[-1] == LF)) {
        p--;
    }
    *p = NUL;
    return uprv_strlen(buffer);
}
