/*
 **********************************************************************
 *   Copyright (C) 2002, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "layout/LETypes.h"
#include "layout/LELanguages.h"
#include "layout/LayoutEngine.h"
#include "layout/LEFontInstance.h"

#include "unicode/ubidi.h"
#include "unicode/uchriter.h"
#include "unicode/brkiter.h"

#include "Utilities.h"
#include "usc_impl.h" /* this is currently private! */
#include "cstring.h"  /* this too! */

#include "layout/ParagraphLayout.h"

U_NAMESPACE_BEGIN

#define ARRAY_SIZE(array) (sizeof array  / sizeof array[0])

struct ParagraphLayout::StyleRunInfo
{
      LayoutEngine   *engine;
const LEFontInstance *font;
const Locale         *locale;
      LEGlyphID      *glyphs;
      float          *positions;
      UScriptCode     script;
      UBiDiLevel      level;
      le_int32        runBase;
      le_int32        runLimit;
      le_int32        glyphBase;
      le_int32        glyphCount;
};

class StyleRuns
{
public:
    StyleRuns(const RunArray *styleRunArrays[], le_int32 styleCount);

    ~StyleRuns();

    le_int32 getRuns(le_int32 runLimits[], le_int32 styleIndices[]);

private:
    le_int32 fStyleCount;
    le_int32 fRunCount;

    le_int32 *fRunLimits;
    le_int32 *fStyleIndices;
};

StyleRuns::StyleRuns(const RunArray *styleRunArrays[], le_int32 styleCount)
    : fStyleCount(styleCount), fRunCount(0), fRunLimits(NULL), fStyleIndices(NULL)
{
    le_int32 maxRunCount = 0;
    le_int32 style, run, runStyle;
    le_int32 *currentRun = LE_NEW_ARRAY(le_int32, styleCount);

    for (int i = 0; i < styleCount; i += 1) {
        maxRunCount += styleRunArrays[i]->getCount();
    }

    maxRunCount -= styleCount - 1;

    fRunLimits    = LE_NEW_ARRAY(le_int32, maxRunCount);
    fStyleIndices = LE_NEW_ARRAY(le_int32, maxRunCount * styleCount);

    for (style = 0; style < styleCount; style += 1) {
        currentRun[style] = 0;
    }

    run = 0;
    runStyle = 0;

    /*
     * Since the last run limit for each style run must be
     * the same, all the styles will hit the last limit at
     * the same time, so we know when we're done when the first
     * style hits the last limit.
     */
    while (currentRun[0] < styleRunArrays[0]->getCount()) {
        fRunLimits[run] = 0x7FFFFFFF;

        // find the minimum run limit for all the styles
        for (style = 0; style < styleCount; style += 1) {
            if (styleRunArrays[style]->getLimit(currentRun[style]) < fRunLimits[run]) {
                fRunLimits[run] = styleRunArrays[style]->getLimit(currentRun[style]);
            }
        }

        // advance all styles whose current run is at this limit to the next run
        for (style = 0; style < styleCount; style += 1) {
            fStyleIndices[runStyle++] = currentRun[style];

            if (styleRunArrays[style]->getLimit(currentRun[style]) == fRunLimits[run]) {
                currentRun[style] += 1;
            }
        }

        run += 1;
    }

    fRunCount = run;
    LE_DELETE_ARRAY(currentRun);
}

StyleRuns::~StyleRuns()
{
    fRunCount = 0;

    LE_DELETE_ARRAY(fStyleIndices);
    fStyleIndices = NULL;

    LE_DELETE_ARRAY(fRunLimits);
    fRunLimits = NULL;
}

le_int32 StyleRuns::getRuns(le_int32 runLimits[], le_int32 styleIndices[])
{
    if (runLimits != NULL) {
        LE_ARRAY_COPY(runLimits, fRunLimits, fRunCount);
    }

    if (styleIndices != NULL) {
        LE_ARRAY_COPY(styleIndices, fStyleIndices, fRunCount * fStyleCount);
    }

    return fRunCount;
}

/*
 * NOTE: This table only has "true" values for
 * those scripts which the LayoutEngine can currently
 * process, rather for all scripts which require
 * complex processing for correct rendering.
 */
static const le_bool complexTable[] = {
    false , /* Zyyy */
    false,  /* Qaai */
    true,   /* Arab */
    false,  /* Armn */
    true,   /* Beng */
    false,  /* Bopo */
    false,  /* Cher */
    false,  /* Qaac */
    false,  /* Cyrl */
    false,  /* Dsrt */
    true,   /* Deva */
    false,  /* Ethi */
    false,  /* Geor */
    false,  /* Goth */
    false,  /* Grek */
    true,   /* Gujr */
    true,   /* Guru */
    false,  /* Hani */
    false,  /* Hang */
    true,   /* Hebr */
    false,  /* Hira */
    true,   /* Knda */
    false,  /* Kana */
    false,  /* Khmr */
    false,  /* Laoo */
    false,  /* Latn */
    true,   /* Mlym */
    false,  /* Mong */
    false,  /* Mymr */
    false,  /* Ogam */
    false,  /* Ital */
    true,   /* Orya */
    false,  /* Runr */
    false,  /* Sinh */
    false,  /* Syrc */
    true,   /* Taml */
    true,   /* Telu */
    false,  /* Thaa */
    true,   /* Thai */
    false,  /* Tibt */
    false,  /* Cans */
    false,  /* Yiii */
    false,  /* Tglg */
    false,  /* Hano */
    false,  /* Buhd */
    false,  /* Tagb */
    false,  /* Brai */
    false,  /* Cprt */
    false,  /* Limb */
    false,  /* Linb */
    false,  /* Osma */
    false,  /* Shaw */
    false,  /* Tale */
    false   /* Ugar */
};


const char ParagraphLayout::fgClassID = 0;

/*
 * How to deal with composite fonts:
 *
 * Don't store the client's FontRuns; we'll need to compute sub-font FontRuns using Doug's
 * LEFontInstance method. Do that by intersecting the client's FontRuns with fScriptRuns. Use
 * that to compute fFontRuns, and then intersect fFontRuns, fScriptRuns and fLevelRuns. Doing
 * it in this order means we do a two-way intersection and a three-way intersection.
 *
 * An optimization would be to only do this if there's at least one composite font...
 *
 * Other notes:
 *
 * * Return the sub-fonts as the run fonts... could keep the mapping back to the client's FontRuns
 *   but that probably makes it more complicated of everyone...
 *
 * * Take the LineInfo and LineRun types from Paragraph and use them here, incorporate them into the API.
 *
 * * Might want to change the name of the StyleRun type, and make a new one that holds fonts, scripts and levels?
 *
 */
ParagraphLayout::ParagraphLayout(const LEUnicode chars[], le_int32 count,
                                 const FontRuns   *fontRuns,
                                 const ValueRuns  *levelRuns,
                                 const ValueRuns  *scriptRuns,
                                 const LocaleRuns *localeRuns,
                                 UBiDiLevel paragraphLevel, le_bool vertical)
                                 : fChars(chars), fCharCount(count),
                                   fFontRuns(NULL), fLevelRuns(levelRuns), fScriptRuns(scriptRuns), fLocaleRuns(localeRuns),
                                   fVertical(vertical), fClientLevels(true), fClientScripts(true), fClientLocales(true), fEmbeddingLevels(NULL),
                                   fAscent(0), fDescent(0), fLeading(0),
                                   fGlyphToCharMap(NULL), fCharToGlyphMap(NULL), fGlyphWidths(NULL), fGlyphCount(0),
                                   fParaBidi(NULL), fLineBidi(NULL),
                                   fStyleRunLimits(NULL), fStyleIndices(NULL), fStyleRunCount(0),
                                   fBreakIterator(NULL), fLineStart(-1), fLineEnd(0),
                                 /*fVisualRuns(NULL), fStyleRunInfo(NULL), fVisualRunCount(-1),
                                   fFirstVisualRun(-1), fLastVisualRun(-1),*/ fVisualRunLastX(0), fVisualRunLastY(0)
{
    // FIXME: should check the limit arrays for consistency...

    computeLevels(paragraphLevel);

    if (scriptRuns == NULL) {
        computeScripts();
    }

    if (localeRuns == NULL) {
        computeLocales();
    }

    computeSubFonts(fontRuns);

    // now intersect the font, direction and script runs...
    const RunArray *styleRunArrays[] = {fFontRuns, fLevelRuns, fScriptRuns, fLocaleRuns};
    le_int32  styleCount = sizeof styleRunArrays / sizeof styleRunArrays[0];
    StyleRuns styleRuns(styleRunArrays, styleCount);
    LEErrorCode layoutStatus = LE_NO_ERROR;
    
    fStyleRunCount = styleRuns.getRuns(NULL, NULL);

    fStyleRunLimits = LE_NEW_ARRAY(le_int32, fStyleRunCount);
    fStyleIndices   = LE_NEW_ARRAY(le_int32, fStyleRunCount * styleCount);
    
    styleRuns.getRuns(fStyleRunLimits, fStyleIndices);

    // now build a LayoutEngine for each style run...
    le_int32 *styleIndices = fStyleIndices;
    le_int32 run, runStart;

    fStyleRunInfo = LE_NEW_ARRAY(StyleRunInfo, fStyleRunCount);

    fGlyphCount = 0;
    for (runStart = 0, run = 0; run < fStyleRunCount; run += 1) {
        fStyleRunInfo[run].font      = fFontRuns->getFont(styleIndices[0]);
        fStyleRunInfo[run].runBase   = runStart;
        fStyleRunInfo[run].runLimit  = fStyleRunLimits[run];
        fStyleRunInfo[run].script    = (UScriptCode) fScriptRuns->getValue(styleIndices[2]);
        fStyleRunInfo[run].locale    = fLocaleRuns->getLocale(styleIndices[3]);
        fStyleRunInfo[run].level     = (UBiDiLevel) fLevelRuns->getValue(styleIndices[1]);
        fStyleRunInfo[run].glyphBase = fGlyphCount;

        fStyleRunInfo[run].engine = LayoutEngine::layoutEngineFactory(fStyleRunInfo[run].font,
            fStyleRunInfo[run].script, getLanguageCode(fStyleRunInfo[run].locale), layoutStatus);

        fStyleRunInfo[run].glyphCount = fStyleRunInfo[run].engine->layoutChars(fChars, runStart, fStyleRunLimits[run] - runStart, fCharCount,
            fStyleRunInfo[run].level & 1, 0, 0, layoutStatus);

        runStart = fStyleRunLimits[run];
        styleIndices += styleCount;
        fGlyphCount += fStyleRunInfo[run].glyphCount;
    }

    // Make big arrays for the glyph widths, glyph-to-char and char-to-glyph maps,
    // in logical order. (Both maps need an extra entry for the end of the text.) 
    //
    // For each layout get the positions and convert them into glyph widths, in
    // logical order. Get the glyph-to-char mapping, offset by starting index in the
    // width array, and swap it into logical order. Then fill in the char-to-glyph map
    // from this. (charToGlyph[glyphToChar[i]] = i)
    fGlyphWidths    = LE_NEW_ARRAY(float, fGlyphCount);
    fGlyphToCharMap = LE_NEW_ARRAY(le_int32, fGlyphCount);
    fCharToGlyphMap = LE_NEW_ARRAY(le_int32, fCharCount + 1);

    for (runStart = 0, run = 0; run < fStyleRunCount; run += 1) {
        LayoutEngine *engine = fStyleRunInfo[run].engine;
        le_int32 glyphCount  = fStyleRunInfo[run].glyphCount;
        le_int32 glyphBase   = fStyleRunInfo[run].glyphBase;
        le_int32 glyph;

        fStyleRunInfo[run].glyphs = LE_NEW_ARRAY(LEGlyphID, glyphCount);
        fStyleRunInfo[run].positions = LE_NEW_ARRAY(float, glyphCount * 2 + 2);

        engine->getGlyphs(fStyleRunInfo[run].glyphs, layoutStatus);
        engine->getGlyphPositions(fStyleRunInfo[run].positions, layoutStatus);
        engine->getCharIndices(&fGlyphToCharMap[glyphBase], runStart, layoutStatus);

        for (glyph = 0; glyph < glyphCount; glyph += 1) {
            fGlyphWidths[glyphBase + glyph] = fStyleRunInfo[run].positions[glyph * 2 + 2] - fStyleRunInfo[run].positions[glyph * 2];
            fCharToGlyphMap[fGlyphToCharMap[glyphBase + glyph]] = glyphBase + glyph;
        }

        if ((fStyleRunInfo[run].level & 1) != 0) {
            Utilities::reverse(&fGlyphWidths[glyphBase], glyphCount);
            Utilities::reverse(&fGlyphToCharMap[glyphBase], glyphCount);

            // Utilities::reverse(&fCharToGlyphMap[runStart], fStyleRunLimits[run] - runStart);
            // convert from visual to logical glyph indices
            for (glyph = glyphBase; glyph < glyphBase + glyphCount; glyph += 1) {
                le_int32 ch = fGlyphToCharMap[glyph];
                le_int32 lastGlyph = glyphBase + glyphCount - 1;

                // both lastGlyph and fCharToGlyphMap[ch] are biased by
                // glyphBase, so subtracting them will remove the bias.
                fCharToGlyphMap[ch] = lastGlyph - fCharToGlyphMap[ch] + glyphBase;
            }
        }

        runStart = fStyleRunLimits[run];

        delete engine;
        fStyleRunInfo[run].engine = NULL;
    }

    fCharToGlyphMap[fCharCount]  = fGlyphCount;
    fGlyphToCharMap[fGlyphCount] = fCharCount;
}

ParagraphLayout::~ParagraphLayout()
{
    delete (FontRuns *) fFontRuns;

    if (! fClientLevels) {
        delete (ValueRuns *) fLevelRuns;
        fLevelRuns = NULL;

        fClientLevels = true;
    }

    if (! fClientScripts) {
        delete (ValueRuns *) fScriptRuns;
        fScriptRuns = NULL;

        fClientScripts = true;
    }

    if (! fClientLocales) {
        delete (LocaleRuns *) fLocaleRuns;
        fLocaleRuns = NULL;

        fClientLocales = true;
    }

    if (fEmbeddingLevels != NULL) {
        LE_DELETE_ARRAY(fEmbeddingLevels);
        fEmbeddingLevels = NULL;
    }

    if (fGlyphToCharMap != NULL) {
        LE_DELETE_ARRAY(fGlyphToCharMap);
        fGlyphToCharMap = NULL;
    }

    if (fCharToGlyphMap != NULL) {
        LE_DELETE_ARRAY(fCharToGlyphMap);
        fCharToGlyphMap = NULL;
    }

    if (fGlyphWidths != NULL) {
        LE_DELETE_ARRAY(fGlyphWidths);
        fGlyphWidths = NULL;
    }

    if (fParaBidi != NULL) {
        ubidi_close(fParaBidi);
        fParaBidi = NULL;
    }

    if (fLineBidi != NULL) {
        ubidi_close(fLineBidi);
        fLineBidi = NULL;
    }

    if (fStyleRunCount > 0) {
        le_int32 run;

        LE_DELETE_ARRAY(fStyleRunLimits);
        LE_DELETE_ARRAY(fStyleIndices);

        for (run = 0; run < fStyleRunCount; run += 1) {
            LE_DELETE_ARRAY(fStyleRunInfo[run].glyphs);
            LE_DELETE_ARRAY(fStyleRunInfo[run].positions);

            fStyleRunInfo[run].glyphs    = NULL;
            fStyleRunInfo[run].positions = NULL;
        }

        LE_DELETE_ARRAY(fStyleRunInfo);

        fStyleRunLimits = NULL;
        fStyleIndices   = NULL;
        fStyleRunInfo        = NULL;
        fStyleRunCount  = 0;
    }

    if (fBreakIterator != NULL) {
        delete fBreakIterator;
        fBreakIterator = NULL;
    }
}

    
le_bool ParagraphLayout::isComplex(const LEUnicode chars[], le_int32 count)
{
    UErrorCode scriptStatus = U_ZERO_ERROR;
    UScriptCode scriptCode  = USCRIPT_INVALID_CODE;
    UScriptRun *sr = uscript_openRun(chars, count, &scriptStatus);

    while (uscript_nextRun(sr, NULL, NULL, &scriptCode)) {
        if (isComplex(scriptCode)) {
            return true;
        }
    }

    return false;
}

le_int32 ParagraphLayout::getAscent() const
{
    if (fAscent <= 0) {
        ((ParagraphLayout *) this)->computeMetrics();
    }

    return fAscent;
}

le_int32 ParagraphLayout::getDescent() const
{
    if (fAscent <= 0) {
        ((ParagraphLayout *) this)->computeMetrics();
    }

    return fDescent;
}

le_int32 ParagraphLayout::getLeading() const
{
    if (fAscent <= 0) {
        ((ParagraphLayout *) this)->computeMetrics();
    }

    return fLeading;
}

const ParagraphLayout::Line *ParagraphLayout::nextLine(float width)
{
    if (fLineEnd >= fCharCount) {
        return NULL;
    }

    fLineStart = fLineEnd;

    if (width > 0) {
        le_int32 glyph    = fCharToGlyphMap[fLineStart];
        float widthSoFar  = 0;

        while (glyph < fGlyphCount && widthSoFar + fGlyphWidths[glyph] <= width) {
            widthSoFar += fGlyphWidths[glyph++];
        }

        // If no glyphs fit on the line, force one to fit.
        //
        // (There shouldn't be any zero width glyphs at the
        // start of a line unless the paragraph consists of
        // only zero width glyphs, because otherwise the zero
        // width glyphs will have been included on the end of
        // the previous line...)
        if (widthSoFar == 0 && glyph < fGlyphCount) {
            glyph += 1;
        }

        fLineEnd = previousBreak(fGlyphToCharMap[glyph]);

        // If there's no real break, break at the
        // glyph that didn't fit.
        if (fLineEnd <= fLineStart) {
            fLineEnd = fGlyphToCharMap[glyph];
        }
    } else {
        fLineEnd = fCharCount;
    }

    return computeVisualRuns();
}

void ParagraphLayout::computeLevels(UBiDiLevel paragraphLevel)
{
    UErrorCode bidiStatus = U_ZERO_ERROR;

    if (fLevelRuns != NULL) {
        le_int32 ch;
        le_int32 run;

        fEmbeddingLevels = LE_NEW_ARRAY(UBiDiLevel, fCharCount);

        for (ch = 0, run = 0; run < fLevelRuns->getCount(); run += 1) {
            UBiDiLevel runLevel = (UBiDiLevel) fLevelRuns->getValue(run) | UBIDI_LEVEL_OVERRIDE;
            le_int32   runLimit = fLevelRuns->getLimit(run);

            while (ch < runLimit) {
                fEmbeddingLevels[ch++] = runLevel;
            }
        }
    }

    fParaBidi = ubidi_openSized(fCharCount, 0, &bidiStatus);
    ubidi_setPara(fParaBidi, fChars, fCharCount, paragraphLevel, fEmbeddingLevels, &bidiStatus);

    if (fLevelRuns == NULL) {
        le_int32 levelRunCount = ubidi_countRuns(fParaBidi, &bidiStatus);
        ValueRuns *levelRuns = new ValueRuns(levelRunCount);

        le_int32 logicalStart = 0;
        le_int32 run;
        le_int32 limit;
        UBiDiLevel level;

        for (run = 0; run < levelRunCount; run += 1) {
            ubidi_getLogicalRun(fParaBidi, logicalStart, &limit, &level);
            levelRuns->add(level, limit);
            logicalStart = limit;
        }

        fLevelRuns    = levelRuns;
        fClientLevels = false;
    }
}

void ParagraphLayout::computeScripts()
{
    UErrorCode scriptStatus = U_ZERO_ERROR;
    UScriptRun *sr = uscript_openRun(fChars, fCharCount, &scriptStatus);
    ValueRuns  *scriptRuns = new ValueRuns(0);
    le_int32 limit;
    UScriptCode script;

    while (uscript_nextRun(sr, NULL, &limit, &script)) {
        scriptRuns->add(script, limit);
    }

    uscript_closeRun(sr);

    fScriptRuns = scriptRuns;
}

void ParagraphLayout::computeLocales()
{
    LocaleRuns *localeRuns = new LocaleRuns(0);
    const Locale *defaultLocale = &Locale::getDefault();

    localeRuns->add(defaultLocale, fCharCount);

    fLocaleRuns    = localeRuns;
    fClientLocales = false;
}

void ParagraphLayout::computeSubFonts(const FontRuns *fontRuns)
{
    const RunArray *styleRunArrays[] = {fontRuns, fScriptRuns};
    le_int32 styleCount = sizeof styleRunArrays / sizeof styleRunArrays[0];
    StyleRuns styleRuns(styleRunArrays, styleCount);
    le_int32 styleRunCount = styleRuns.getRuns(NULL, NULL);
    le_int32 *styleRunLimits = LE_NEW_ARRAY(le_int32, styleRunCount);
    le_int32 *styleIndices = LE_NEW_ARRAY(le_int32, styleRunCount * styleCount);
    FontRuns *subFontRuns  = new FontRuns(0);
    le_int32  run, offset, *si;

    styleRuns.getRuns(styleRunLimits, styleIndices);

    si = styleIndices;
    offset = 0;

    for (run = 0; run < styleRunCount; run += 1) {
        const LEFontInstance *runFont = fontRuns->getFont(si[0]);
        le_int32 script = fScriptRuns->getValue(si[1]);
        le_int32 count;

        while ((count = styleRunLimits[run] - offset) > 0) {
            const LEFontInstance *subFont = runFont->getSubFont(fChars, &offset, count, script);

            subFontRuns->add(subFont, offset);
        }

        si += styleCount;
    }

    fFontRuns = subFontRuns;

    LE_DELETE_ARRAY(styleIndices);
    LE_DELETE_ARRAY(styleRunLimits);
}

void ParagraphLayout::computeMetrics()
{
    le_int32 i, count = fFontRuns->getCount();
    le_int32 maxDL = 0;

    for (i = 0; i < count; i += 1) {
        const LEFontInstance *font = fFontRuns->getFont(i);
        le_int32 ascent  = font->getAscent();
        le_int32 descent = font->getDescent();
        le_int32 leading = font->getLeading();
        le_int32 dl      = descent + leading;

        if (ascent > fAscent) {
            fAscent = ascent;
        }

        if (descent > fDescent) {
            fDescent = descent;
        }

        if (leading > fLeading) {
            fLeading = leading;
        }

        if (dl > maxDL) {
            maxDL = dl;
        }
    }

    fLeading = maxDL - fDescent;
}

#if 1
struct LanguageMap
{
    const char *localeCode;
    le_int32 languageCode;
};

static const LanguageMap languageMap[] =
{
    {"ara", araLanguageCode}, // Arabic
    {"asm", asmLanguageCode}, // Assamese
    {"ben", benLanguageCode}, // Bengali
    {"fas", farLanguageCode}, // Farsi
    {"guj", gujLanguageCode}, // Gujarati
    {"heb", iwrLanguageCode}, // Hebrew
    {"hin", hinLanguageCode}, // Hindi
    {"jpn", janLanguageCode}, // Japanese
    {"kan", kanLanguageCode}, // Kannada
    {"kas", kshLanguageCode}, // Kashmiri
    {"kok", kokLanguageCode}, // Konkani
    {"kor", korLanguageCode}, // Korean
//  {"mal_XXX", malLanguageCode}, // Malayalam - Traditional
    {"mal", mlrLanguageCode}, // Malayalam - Reformed
    {"mar", marLanguageCode}, // Marathi
    {"mni", mniLanguageCode}, // Manipuri
    {"ori", oriLanguageCode}, // Oriya
    {"san", sanLanguageCode}, // Sanskrit
    {"snd", sndLanguageCode}, // Sindhi
    {"sin", snhLanguageCode}, // Sinhalese
    {"syr", syrLanguageCode}, // Syriac
    {"tam", tamLanguageCode}, // Tamil
    {"tel", telLanguageCode}, // Telugu
    {"tha", thaLanguageCode}, // Thai
    {"urd", urdLanguageCode}, // Urdu
    {"yid", jiiLanguageCode}, // Yiddish
//  {"zhp", zhpLanguageCode}, // Chinese - Phonetic
    {"zho", zhsLanguageCode}, // Chinese
    {"zho_CHN", zhsLanguageCode}, // Chinese - China
    {"zho_HKG", zhsLanguageCode}, // Chinese - Hong Kong
    {"zho_MAC", zhtLanguageCode}, // Chinese - Macao
    {"zho_SGP", zhsLanguageCode}, // Chinese - Singapore
    {"zho_TWN", zhtLanguageCode}  // Chinese - Taiwan
};

le_int32 ParagraphLayout::getLanguageCode(const Locale *locale)
{
    char code[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const char *language = locale->getISO3Language();
    const char *country  = locale->getISO3Country();

    uprv_strcat(code, language);

    if ((uprv_strcmp(language, "zho") == 0) && country != NULL) {
        uprv_strcat(code, "_");
        uprv_strcat(code, country);
    }

    for (le_int32 i = 0; i < ARRAY_SIZE(languageMap); i += 1) {
        if (uprv_strcmp(code, languageMap[i].localeCode) == 0) {
            return languageMap[i].languageCode;
        }
    }

    return nullLanguageCode;
}
#elif

// TODO - dummy implementation for right now...
le_int32 ParagraphLayout::getLanguageCode(const Locale *locale)
{
    return nullLanguageCode;
}
#endif

le_bool ParagraphLayout::isComplex(UScriptCode script)
{
    if (script < 0 || script >= USCRIPT_CODE_LIMIT) {
        return false;
    }

    return complexTable[script];
}

le_int32 ParagraphLayout::previousBreak(le_int32 charIndex)
{
    // skip over any whitespace or control characters,
    // because they can hang in the margin.
    while (charIndex < fCharCount &&
           (u_isWhitespace(fChars[charIndex]) ||
            u_iscntrl(fChars[charIndex]))) {
        charIndex += 1;
    }

    // Create the BreakIterator if we don't already have one
    if (fBreakIterator == NULL) {
        /*Locale thai("en_us"); //*/Locale thai("th");
        UCharCharacterIterator *iter = new UCharCharacterIterator(fChars, fCharCount);
        UErrorCode status = U_ZERO_ERROR;

        fBreakIterator = BreakIterator::createLineInstance(thai, status);
        fBreakIterator->adoptText(iter);
    }

    // return the break location that's at or before
    // the character we stopped on. Note: if we're
    // on a break, the "+ 1" will cause preceding to
    // back up to it.
    return fBreakIterator->preceding(charIndex + 1);
}

const ParagraphLayout::Line *ParagraphLayout::computeVisualRuns()
{
    UErrorCode bidiStatus = U_ZERO_ERROR;
    le_int32 dirRunCount, visualRun;

    fVisualRunLastX = 0;
    fVisualRunLastY = 0;
    fFirstVisualRun = getCharRun(fLineStart);
    fLastVisualRun  = getCharRun(fLineEnd - 1);

    if (fLineBidi == NULL) {
        fLineBidi = ubidi_openSized(fCharCount, 0, &bidiStatus);
    }

    ubidi_setLine(fParaBidi, fLineStart, fLineEnd, fLineBidi, &bidiStatus);
    dirRunCount = ubidi_countRuns(fLineBidi, &bidiStatus);

    Line *line = new Line();

    for (visualRun = 0; visualRun < dirRunCount; visualRun += 1) {
        le_int32 relStart, run, runLength;
        UBiDiDirection runDirection = ubidi_getVisualRun(fLineBidi, visualRun, &relStart, &runLength);
        le_int32 runStart = fLineStart + relStart;
        le_int32 runEnd   = runStart + runLength - 1;
        le_int32 firstRun = getCharRun(runStart);
        le_int32 lastRun  = getCharRun(runEnd);
        le_int32 startRun = (runDirection == UBIDI_LTR)? firstRun : lastRun;
        le_int32 stopRun  = (runDirection == UBIDI_LTR)? lastRun + 1 : firstRun - 1;
        le_int32 dir      = (runDirection == UBIDI_LTR)?  1 : -1;

        for (run = startRun; run != stopRun; run += dir) {
            le_int32 firstChar = (run == firstRun)? runStart : fStyleRunInfo[run].runBase;
            le_int32 lastChar  = (run == lastRun)?  runEnd   : fStyleRunInfo[run].runLimit - 1;

            appendRun(line, run, firstChar, lastChar);
        }
    }

    return line;
}

void ParagraphLayout::appendRun(ParagraphLayout::Line *line, le_int32 run, le_int32 firstChar, le_int32 lastChar)
{
    le_int32 glyphBase = fStyleRunInfo[run].glyphBase;
    le_int32 inGlyph, outGlyph;

    // Get the glyph indices for all the characters between firstChar and lastChar,
    // make the minimum one be leftGlyph and the maximum one be rightGlyph.
    // (need to do this to handle local reorderings like Indic left matras)
    le_int32 leftGlyph  = fGlyphCount;
    le_int32 rightGlyph = -1;
    le_int32 ch;

    for (ch = firstChar; ch <= lastChar; ch += 1) {
        le_int32 glyph = fCharToGlyphMap[ch];

        if (glyph < leftGlyph) {
            leftGlyph = glyph;
        }

        if (glyph > rightGlyph) {
            rightGlyph = glyph;
        }
    }

    if ((fStyleRunInfo[run].level & 1) != 0) {
        le_int32 swap = rightGlyph;
        le_int32 last = glyphBase + fStyleRunInfo[run].glyphCount - 1;

        // Here, we want to remove the glyphBase bias...
        rightGlyph = last - leftGlyph;
        leftGlyph  = last - swap;
    } else {
        rightGlyph -= glyphBase;
        leftGlyph  -= glyphBase;
    }

    // Set the position bias for the glyphs. If we're at the start of
    // a line, we want the first glyph to be at x = 0, even if it comes
    // from the middle of a layout. If we've got a right-to-left run, we
    // want the left-most glyph to start at the final x position of the
    // previous run, even though this glyph may be in the middle of the
    // layout.
    if (run == fFirstVisualRun) {
        fVisualRunLastX = - fStyleRunInfo[run].positions[leftGlyph * 2];
    } else if ((fStyleRunInfo[run].level & 1) != 0) {
        fVisualRunLastX -= fStyleRunInfo[run].positions[leftGlyph * 2];
    }
 
    // Make rightGlyph be the glyph just to the right of
    // the run's glyphs
    rightGlyph += 1;

    UBiDiDirection direction  = ((fStyleRunInfo[run].level & 1) == 0)? UBIDI_LTR : UBIDI_RTL;
    le_int32   glyphCount     = rightGlyph - leftGlyph;
    LEGlyphID *glyphs         = LE_NEW_ARRAY(LEGlyphID, glyphCount);
    float     *positions      = LE_NEW_ARRAY(float, glyphCount * 2 + 2);
    le_int32  *glyphToCharMap = LE_NEW_ARRAY(le_int32, glyphCount);

    LE_ARRAY_COPY(glyphs, &fStyleRunInfo[run].glyphs[leftGlyph], glyphCount);

    for (outGlyph = 0, inGlyph = leftGlyph * 2; inGlyph <= rightGlyph * 2; inGlyph += 2, outGlyph += 2) {
        positions[outGlyph]     = fStyleRunInfo[run].positions[inGlyph] + fVisualRunLastX;
        positions[outGlyph + 1] = fStyleRunInfo[run].positions[inGlyph + 1] /* + fVisualRunLastY */;
    }

    // Save the ending position of this run
    // to use for the start of the next run
    fVisualRunLastX = positions[outGlyph - 2];
 // fVisualRunLastY = positions[rightGlyph * 2 + 2];

    if ((fStyleRunInfo[run].level & 1) == 0) {
        for (outGlyph = 0, inGlyph = leftGlyph; inGlyph < rightGlyph; inGlyph += 1, outGlyph += 1) {
            glyphToCharMap[outGlyph] = fGlyphToCharMap[glyphBase + inGlyph];
        }
    } else {
        for (outGlyph = 0, inGlyph = rightGlyph - 1; inGlyph >= leftGlyph; inGlyph -= 1, outGlyph += 1) {
            glyphToCharMap[outGlyph] = fGlyphToCharMap[glyphBase + inGlyph];
        }
    }

    line->append(fStyleRunInfo[run].font, direction, glyphCount, glyphs, positions, glyphToCharMap);
}

le_int32 ParagraphLayout::getCharRun(le_int32 charIndex)
{
    if (charIndex < 0 || charIndex > fCharCount) {
        return -1;
    }

    le_int32 run;

    // NOTE: as long as fStyleRunLimits is well-formed
    // the above range check guarantees that we'll never
    // fall off the end of the array.
    run = 0;
    while (charIndex >= fStyleRunLimits[run]) {
        run += 1;
    }

    return run;
}


const char ParagraphLayout::Line::fgClassID = 0;

#define INITIAL_RUN_CAPACITY 4
#define RUN_CAPACITY_GROW_LIMIT 16

ParagraphLayout::Line::~Line()
{
    le_int32 i;

    for (i = 0; i < fRunCount; i += 1) {
        delete fRuns[i];
    }

    LE_DELETE_ARRAY(fRuns);
}

le_int32 ParagraphLayout::Line::getAscent() const
{
    if (fAscent <= 0) {
        ((ParagraphLayout::Line *)this)->computeMetrics();
    }

    return fAscent;
}

le_int32 ParagraphLayout::Line::getDescent() const
{
    if (fAscent <= 0) {
        ((ParagraphLayout::Line *)this)->computeMetrics();
    }

    return fDescent;
}

le_int32 ParagraphLayout::Line::getLeading() const
{
    if (fAscent <= 0) {
        ((ParagraphLayout::Line *)this)->computeMetrics();
    }

    return fLeading;
}

const ParagraphLayout::VisualRun *ParagraphLayout::Line::getVisualRun(le_int32 runIndex) const
{
    if (runIndex < 0 || runIndex >= fRunCount) {
        return NULL;
    }

    return fRuns[runIndex];
}

void ParagraphLayout::Line::append(const LEFontInstance *font, UBiDiDirection direction, le_int32 glyphCount,
                                   const LEGlyphID glyphs[], const float positions[], const le_int32 glyphToCharMap[])
{
    if (fRunCount >= fRunCapacity) {
        if (fRunCapacity == 0) {
            fRunCapacity = INITIAL_RUN_CAPACITY;
            fRuns = LE_NEW_ARRAY(ParagraphLayout::VisualRun *, fRunCapacity);
        } else {
            fRunCapacity += (fRunCapacity < RUN_CAPACITY_GROW_LIMIT? fRunCapacity : RUN_CAPACITY_GROW_LIMIT);
            fRuns = (ParagraphLayout::VisualRun **) LE_GROW_ARRAY(fRuns, fRunCapacity);
        }
    }

    fRuns[fRunCount++] = new ParagraphLayout::VisualRun(font, direction, glyphCount, glyphs, positions, glyphToCharMap);
}

void ParagraphLayout::Line::computeMetrics()
{
    le_int32 maxDL = 0;

    for (le_int32 i = 0; i < fRunCount; i += 1) {
        le_int32 ascent  = fRuns[i]->getAscent();
        le_int32 descent = fRuns[i]->getDescent();
        le_int32 leading = fRuns[i]->getLeading();
        le_int32 dl      = descent + leading;

        if (ascent > fAscent) {
            fAscent = ascent;
        }

        if (descent > fDescent) {
            fDescent = descent;
        }

        if (leading > fLeading) {
            fLeading = leading;
        }

        if (dl > maxDL) {
            maxDL = dl;
        }
    }

    fLeading = maxDL - fDescent;
}

const char ParagraphLayout::VisualRun::fgClassID = 0;

U_NAMESPACE_END
