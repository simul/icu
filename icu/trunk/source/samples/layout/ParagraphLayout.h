/*
 **********************************************************************
 *   Copyright (C) 2002, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __PARAGRAPHLAYOUT_H

#define __PARAGRAPHLAYOUT_H

#include "unicode/uscript.h"

#include "layout/LETypes.h"
#include "layout/LEFontInstance.h"
#include "unicode/ubidi.h"
#include "unicode/brkiter.h"

#include "RunArrays.h"

/**
 * ParagraphLayout.
 *
 * The <code>ParagraphLayout</code> object will analyze the text into runs of text in the
 * same font, script and direction, and will create a <code>LayoutEngine</code> object for each run.
 * The <code>LayoutEngine</code> will transform the characters into glyph codes in visual order.
 *
 * Clients can use this to break a paragraph into lines, and to display the glyphs in each line.
 *
 */
class ParagraphLayout
{
public:
    /**
     * A class which represents one line in the paragraph.
     *
     * @draft ICU 2.6
     */
    class Line;

    /**
     * A class which represents one visual run in a line.
     *
     * @draft ICU 2.6
     */
    class VisualRun;

    /**
     * Construct a <code>ParagraphLayout</code> object for a styled paragraph. The paragraph is specified
     * as runs of text all in the same font. An <code>LEFontInstance</code> object and a limit offset
     * are specified for each font run. The limit offset is the offset of the character immediately
     * after the font run.
     *
     * Clients can optionally specify directional runs and / or script runs. If these aren't specified
     * they will be computed.
     *
     * @param chars is an array of the characters in the paragraph
     *
     * @param count is the number of characters in the paragraph.
     *
     * @param fontRuns a pointer to a <code>FontRuns</code> object representing the font runs.
     *
     * @param levelRuns is a pointer to a <code>ValueRuns</code> object representing the directional levels.
     *        If this pointer in <code>NULL</code> the levels will be determined by running the Unicde
     *        Bidi algorithm.
     *
     * @param scriptRuns is a pointer to a <code>ValueRuns</code> object representing script runs.
     *        If this pointer in <code>NULL</code> the script runs will be determined using the
     *        Unicode code points.
     *
     * @param localeRuns is a pointer to a <code>LocaleRuns</code> object representing locale runs.
     *        The <code>Locale</code> objects are used to determind the language of the text. If this
     *        pointer is <code>NULL</code> the default locale will be used for all of the text. 
     *
     * @param paragraphLevel is the directionality of the paragraph, as in the UBiDi object.
     *
     * @param vertical is <code>true</code> if the paragraph should be set vertically.
     *
     * @see ubidi.h
     * @see LEFontInstance.h
     * @see LayoutEngine.h
     * @see RunArrays.h
     *
     * @draft ICU 2.6
     */
    ParagraphLayout(const LEUnicode chars[], le_int32 count,
                    const FontRuns *fontRuns,
                    const ValueRuns *levelRuns,
                    const ValueRuns *scriptRuns,
                    const LocaleRuns *localeRuns,
                    UBiDiLevel paragraphLevel, le_bool vertical);

    ~ParagraphLayout();

#if 0
    /**
     * Examine the given styled paragraph and determine if it contains any text which
     * requires complex processing. (i.e. that cannot be correctly rendered by
     * just mapping the characters to glyphs and rendering them in order)
     *
     * @param chars is an array of the characters in the paragraph
     *
     * @param count is the number of characters in the paragraph.
     *
     * @param fonts is an array of the <code>LEFontInstance</code> objects associated
     *        with each font run.
     *
     * @param fontRunLimits is an array of the run limits of each font run.
     *
     * @param fontRunCount is the number of font runs.
     *
     * @return <code>true</code> if the paragraph contains complex text.
     */
    static le_bool isComplex(const LEUnicode chars[], le_int32 count,
                      const LEFontInstance *fonts[], const le_int32 fontRunLimits[], le_int32 fontRunCount);
#else
    /**
     * Examine the given text and determine if it contains characters in any
     * script which requires complex processing to be rendered correctly.
     *
     * @param chars is an array of the characters in the paragraph
     *
     * @param count is the number of characters in the paragraph.
     *
     * @return <code>true</code> if any of the text requires complex processing.
     */
    static le_bool isComplex(const LEUnicode chars[], le_int32 count);

#endif

    /**
     * Return the resolved paragraph level. This is useful for those cases
     * where the bidi analysis has determined the level based on the first
     * strong character in the paragraph.
     *
     * @return the resolved paragraph level.
     */
    UBiDiLevel getParagraphLevel();

    /**
     * Return the directionality of the text in the paragraph.
     *
     * @return <code>UBIDI_LTR</code> if the text is all left to right,
     *         <code>UBIDI_RTL</code> if the text is all right to left,
     *         or <code>UBIDI_MIXED</code> if the text has mixed direction.
     */
    UBiDiDirection getTextDirection();

    /**
     * Return the max ascent value for all the fonts
     * in the paragraph.
     *
     * @return the ascent value.
     */
    virtual le_int32 getAscent() const;

    /**
     * Return the max descent value for all the fonts
     * in the paragraph.
     *
     * @return the decent value.
     */
    virtual le_int32 getDescent() const;

    /**
     * Return the max leading value for all the fonts
     * in the paragraph.
     *
     * @return the leading value.
     */
    virtual le_int32 getLeading() const;

    /**
     * Reset line breaking to start from the beginning of the paragraph.
     *
     */
    void reflow();

    /**
     * Return a <code>ParagraphLayout::Line</code> object which represents next line
     * in the paragraph. The width of the line is specified each time so that it can
     * be varied to support arbitrary paragraph shapes.
     *
     * @param width is the width of the line. If <code>width</code> is less than or equal
     *              to zero, a <code>ParagraphLayout::Line</code> object representing the
     *              rest of the paragraph will be returned.
     *
     * @return a <code>ParagraphLayout::Line</code> object which represents the line. The caller
     *         is responsible for deleting the object.
     *
     * @see ParagraphLayout::Line
     *
     * @draft ICU 2.6
     */
    const Line *nextLine(float width);

private:

    struct VisualRunInfo;
    struct StyleRunInfo;

    ParagraphLayout() {};

    void computeLevels(UBiDiLevel paragraphLevel);

    const Line *computeVisualRuns();
    void appendRun(Line *line, le_int32 run, le_int32 firstChar, le_int32 lastChar);

    void computeScripts();

    void computeLocales();

    void computeSubFonts(const FontRuns *fontRuns);

    void computeMetrics();

    le_int32 getLanguageCode(const Locale *locale);

    le_int32 getCharRun(le_int32 charIndex);

    static le_bool isComplex(UScriptCode script);

    le_int32 previousBreak(le_int32 charIndex);


    const LEUnicode *fChars;
          le_int32   fCharCount;

    const FontRuns   *fFontRuns;
    const ValueRuns  *fLevelRuns;
    const ValueRuns  *fScriptRuns;
    const LocaleRuns *fLocaleRuns;

          le_bool fVertical;
          le_bool fClientLevels;
          le_bool fClientScripts;
          le_bool fClientLocales;

          UBiDiLevel *fEmbeddingLevels;

          le_int32 fAscent;
          le_int32 fDescent;
          le_int32 fLeading;

          le_int32 *fGlyphToCharMap;
          le_int32 *fCharToGlyphMap;
          float    *fGlyphWidths;
          le_int32  fGlyphCount;

          UBiDi *fParaBidi;
          UBiDi *fLineBidi;

          le_int32     *fStyleRunLimits;
          le_int32     *fStyleIndices;
          StyleRunInfo *fStyleRunInfo;
          le_int32      fStyleRunCount;

          BreakIterator *fBreakIterator;
          le_int32       fLineStart;
          le_int32       fLineEnd;

          le_int32       fFirstVisualRun;
          le_int32       fLastVisualRun;
          float          fVisualRunLastX;
          float          fVisualRunLastY;
};

inline UBiDiLevel ParagraphLayout::getParagraphLevel()
{
    return ubidi_getParaLevel(fParaBidi);
}

inline UBiDiDirection ParagraphLayout::getTextDirection()
{
    return ubidi_getDirection(fParaBidi);
}

inline void ParagraphLayout::reflow()
{
    fLineEnd = 0;
}

/**
 * This class represents a single line of text in a <code>ParagraphLayout</code>. They
 * can only be created by calling <code>ParagraphLayout::nextLine()</code>. Each line
 * consists of multiple visual runs, represented by <code>ParagraphLayout::VisualRun</code>
 * objects.
 *
 * @see ParagraphLayout
 * @see ParagraphLayout::VisualRun
 *
 * @draft ICU 2.6
 */
class ParagraphLayout::Line
{
public:
    /**
     * The constructor is private since these objects can only be
     * created by <code>ParagraphLayout</code>. However, it is the
     * clients responsibility to destroy the objects, so the destructor
     * is public.
     */
    ~Line();

    /**
     * Count the number of visual runs in the line.
     *
     * @return the number of visual runs.
     *
     * @draft ICU 2.6
     */
    le_int32 countRuns() const;

    /**
     * Get the ascent of the line. This is the maximum ascent
     * of all the fonts on the line.
     *
     * @return the ascent of the line.
     *
     * @draft ICU 2.6
     */
    le_int32 getAscent() const;

    /**
     * Get the descent of the line. This is the maximum descent
     * of all the fonts on the line.
     *
     * @return the descent of the line.
     *
     * @draft ICU 2.6
     */
    le_int32 getDescent() const;

    /**
     * Get the leading of the line. This is the maximum leading
     * of all the fonts on the line.
     *
     * @return the leading of the line.
     *
     * @draft ICU 2.6
     */
    le_int32 getLeading() const;
    
    /**
     * Get a <code>ParagraphLayout::VisualRun</code> object for a given
     * visual run in the line.
     *
     * @param runIndex is the index of the run, in visual order.
     *
     * @return the <code>ParagraphLayout::VisualRun</code> object representing the
     *         visual run. This object is owned by the <code>Line</code> object which
     *         created it, and will remain valid for as long as the <code>Line</code>
     *         object is valid.
     *
     * @see ParagraphLayout::VisualRun
     *
     * @draft ICU 2.6
     */
    const ParagraphLayout::VisualRun *getVisualRun(le_int32 runIndex) const;

private:
    friend class ParagraphLayout;

    le_int32 fAscent;
    le_int32 fDescent;
    le_int32 fLeading;

    le_int32 fRunCount;
    le_int32 fRunCapacity;

    ParagraphLayout::VisualRun **fRuns;

    Line();

    void computeMetrics();

    void append(const LEFontInstance *font, UBiDiDirection direction, le_int32 glyphCount,
                const LEGlyphID glyphs[], const float positions[], const le_int32 glyphToCharMap[]);
};

inline ParagraphLayout::Line::Line()
    : fAscent(0), fDescent(0), fLeading(0), fRunCount(0), fRunCapacity(0), fRuns(NULL)
{
    // nothing else to do
}

inline le_int32 ParagraphLayout::Line::countRuns() const
{
    return fRunCount;
}

/**
 * This object represents a single visual run in a line of text in
 * a paragraph. A visual run is text which is in the same font, 
 * script, and direction. The text is represented by an array of
 * <code>LEGlyphIDs</code>, an array of (x, y) glyph positions and
 * a table which maps indices into the glyph array to indices into
 * the original character array which was used to create the paragraph.
 *
 * These objects are only created by <code>ParagraphLayout::Line<code> objects,
 * so their constructors and destructors are private.
 *
 * @see ParagraphLayout::Line
 *
 * @draft ICU 2.6
 */
class ParagraphLayout::VisualRun
{
public:
    /**
     * Get the <code>LEFontInstance</code> object which
     * represents the font of the visual run. This will always
     * be a non-composite font.
     *
     * @return the <code>LEFontInstance</code> object which represents the
     *         font of the visual run.
     *
     * @see LEFontInstance
     *
     * @draft ICU 2.6
     */
    const LEFontInstance *getFont() const;

    /**
     * Get the direction of the visual run.
     *
     * @return the direction of the run. This will be UBIDI_LTR if the
     *         run is left-to-right and UBIDI_RTL if the line is right-to-left.
     *
     * @draft ICU 2.6
     */
    UBiDiDirection getDirection() const;

    /**
     * Get the number of glyphs in the visual run.
     *
     * @return the number of glyphs.
     *
     * @draft ICU 2.6
     */
    le_int32 getGlyphCount() const;

    /**
     * Get the glyphs in the visual run. Glyphs with the values <code>0xFFFE</code> and
     * <code>0xFFFF</code> should be ignored.
     *
     * @return the address of the array of glyphs for this visual run. The storage
     *         is owned by the <code>VisualRun</code> object and must not be deleted.
     *         It will remain valid as long as the <code>VisualRun</code> object is valid.
     *
     * @draft ICU 2.6
     */
    const LEGlyphID *getGlyphs() const;

    /**
     * Get the (x, y) positions of the glyphs in the visual run. To simplify storage
     * management, the x and y positions are stored in a single array with the x positions
     * at even offsets in the array and the corresponding y position in the following even offset.
     * There is an extra (x, y) pair at the end of the array which represents the advance of
     * the final glyph in the run.
     *
     * @return the address of the array of glyph positions for this visual run. The storage
     *         is owned by the <code>VisualRun</code> object and must not be deleted.
     *         It will remain valid as long as the <code>VisualRun</code> object is valid.
     *
     * @draft ICU 2.6
     */
    const float *getPositions() const;

    /**
     * Get the glyph-to-character map for this visual run. This maps the indices into
     * the glyph array to indices into the character array used to create the paragraph.
     *
     * @return the address of the character-to-glyph map for this visual run. The storage
     *         is owned by the <code>VisualRun</code> object and must not be deleted.
     *         It will remain valid as long as the <code>VisualRun</code> object is valid.
     *
     * @draft ICU 2.6
     */
    const le_int32 *getGlyphToCharMap() const;

    /**
     * A convenience method which returns the ascent value for the font
     * associated with this run.
     *
     * @return the ascent value of this run's font.
     *
     * @draft ICU 2.6
     */
    le_int32 getAscent() const;

    /**
     * A convenience method which returns the descent value for the font
     * associated with this run.
     *
     * @return the descent value of this run's font.
     *
     * @draft ICU 2.6
     */
    le_int32 getDescent() const;

    /**
     * A convenience method which returns the leading value for the font
     * associated with this run.
     *
     * @return the leading value of this run's font.
     *
     * @draft ICU 2.6
     */
    le_int32 getLeading() const;

private:
    const LEFontInstance *fFont;
    const UBiDiDirection  fDirection;

    const le_int32 fGlyphCount;

    const LEGlyphID *fGlyphs;
    const float     *fPositions;
    const le_int32  *fGlyphToCharMap;

    friend class ParagraphLayout::Line;

    VisualRun();

    VisualRun(const LEFontInstance *font, UBiDiDirection direction, le_int32 glyphCount,
              const LEGlyphID glyphs[], const float positions[], const le_int32 glyphToCharMap[]);

    ~VisualRun();
};

inline const LEFontInstance *ParagraphLayout::VisualRun::getFont() const
{
    return fFont;
}

inline UBiDiDirection ParagraphLayout::VisualRun::getDirection() const
{
    return fDirection;
}

inline le_int32 ParagraphLayout::VisualRun::getGlyphCount() const
{
    return fGlyphCount;
}

inline const LEGlyphID *ParagraphLayout::VisualRun::getGlyphs() const
{
    return fGlyphs;
}

inline const float *ParagraphLayout::VisualRun::getPositions() const
{
    return fPositions;
}

inline const le_int32 *ParagraphLayout::VisualRun::getGlyphToCharMap() const
{
    return fGlyphToCharMap;
}

inline le_int32 ParagraphLayout::VisualRun::getAscent() const
{
    return fFont->getAscent();
}

inline le_int32 ParagraphLayout::VisualRun::getDescent() const
{
    return fFont->getDescent();
}

inline le_int32 ParagraphLayout::VisualRun::getLeading() const
{
    return fFont->getLeading();
}

inline ParagraphLayout::VisualRun::VisualRun()
    : fFont(NULL), fDirection(UBIDI_LTR), fGlyphCount(0), fGlyphs(NULL), fPositions(NULL), fGlyphToCharMap(NULL)
{
    // nothing
}

inline ParagraphLayout::VisualRun::VisualRun(const LEFontInstance *font, UBiDiDirection direction, le_int32 glyphCount,
                                             const LEGlyphID glyphs[], const float positions[], const le_int32 glyphToCharMap[])
    : fFont(font), fDirection(direction), fGlyphCount(glyphCount),
      fGlyphs(glyphs), fPositions(positions), fGlyphToCharMap(glyphToCharMap)
{
    // nothing else needs to be done!
}

inline ParagraphLayout::VisualRun::~VisualRun()
{
    LE_DELETE_ARRAY(fGlyphToCharMap);
    LE_DELETE_ARRAY(fPositions);
    LE_DELETE_ARRAY(fGlyphs);
}

#endif


