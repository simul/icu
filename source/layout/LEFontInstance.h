
/*
 *
 * (C) Copyright IBM Corp. 1998-2004 - All Rights Reserved
 *
 */

#ifndef __LEFONTINSTANCE_H
#define __LEFONTINSTANCE_H

#include "LETypes.h"

U_NAMESPACE_BEGIN

/**
 * Instances of this class are used by LEFontInstance::mapCharsToGlyphs and
 * LEFontInstance::mapCharToGlyph to adjust character codes before the character
 * to glyph mapping process. Examples of this are filtering out control characters
 * and character mirroring - replacing a character which has both a left and a right
 * hand form with the opposite form.
 *
 * @draft ICU 2.2
 */
class LECharMapper /* not : public UObject because this is an interface/mixin class */
{
public:
    /**
     * Destructor.
     * @draft ICU 2.4
     */
    virtual inline ~LECharMapper() {};

    /**
     * This method does the adjustments.
     *
     * @param ch - the input character
     *
     * @return the adjusted character
     *
     * @stable ICU 2.8
     */
    virtual LEUnicode32 mapChar(LEUnicode32 ch) const = 0;
};

class LEGlyphStorage;

/**
 * This is a virtual base class that serves as the interface between a LayoutEngine
 * and the platform font environment. It allows a LayoutEngine to access font tables, do
 * character to glyph mapping, and obtain metrics information without knowing any platform
 * specific details. There are also a few utility methods for converting between points,
 * pixels and funits. (font design units)
 *
 * An instance of an <code>LEFontInstance</code> represents a font at a particular point
 * size. Each instance can represent either a single physical font, or a composite font.
 * A composite font is a collection of physical fonts, each of which contains a subset of
 * the characters contained in the composite font.
 *
 * Note: with the exception of <code>getSubFont</code>, the methods in this class only
 * make sense for a physical font. If you have an <code>LEFontInstance</code> which
 * represents a composite font you should only call the methods below which have
 * an <code>LEGlyphID</code>, an <code>LEUnicode</code> or an <code>LEUnicode32</code>
 * as one of the arguments because these can be used to select a particular subfont.
 *
 * Subclasses which implement composite fonts should supply an implementation of these
 * methods with some default behavior such as returning constant values, or using the
 * values from the first subfont.
 *
 * @draft ICU 2.6
 */
class U_LAYOUT_API LEFontInstance : public UObject
{
public:

    /**
     * This virtual destructor is here so that the subclass
     * destructors can be invoked through the base class.
     *
     * @stable ICU 2.8
     */
    virtual inline ~LEFontInstance() {};

    /**
     * Get a physical font which can render the given text. For composite fonts,
     * if there is no single physical font which can render all of the text,
     * return a physical font which can render an initial substring of the text,
     * and set the <code>offset</code> parameter to the end of that substring.
     *
     * Internally, the LayoutEngine works with runs of text all in the same
     * font and script, so it is best to call this method with text which is
     * in a single script, passing the script code in as a hint. If you don't
     * know the script of the text, you can use zero, which is the script code
     * for characters used in more than one script.
     *
     * The default implementation of this method is intended for instances of
     * <code>LEFontInstance</code> which represent a physical font. It returns
     * <code>this</code> and indicates that the entire string can be rendered.
     *
     * This method will return a valid <code>LEFontInstance</code> unless you
     * have passed illegal parameters, or an internal error has been encountered. 
     * For composite fonts, it may return the warning <code>LE_NO_SUBFONT_WARNING</code>
     * to indicate that the returned font may not be able to render all of
     * the text. Whenever a valid font is returned, the <code>offset</code> parameter
     * will be advanced by at least one.
     *
     * Subclasses which implement composite fonts must override this method.
     * Where it makes sense, they should use the script code as a hint to render
     * characters from the COMMON script in the font which is used for the given
     * script. For example, if the input text is a series of Arabic words separated
     * by spaces, and the script code passed in is <code>arabScriptCode</code> you
     * should return the font used for Arabic characters for all of the input text,
     * including the spaces. If, on the other hand, the input text contains characters
     * which cannot be rendered by the font used for Arabic characters, but which can
     * be rendered by another font, you should return that font for those characters.
     *
     * @param chars   - the array of Unicode characters.
     * @param offset  - a pointer to the starting offset in the text. On exit this
     *                  will be set the the limit offset of the text which can be
     *                  rendered using the returned font.
     * @param limit   - the limit offset for the input text.
     * @param script  - the script hint.
     * @param success - set to an error code if the arguments are illegal, or no font
     *                  can be returned for some reason. May also be set to
     *                  <code>LE_NO_SUBFONT_WARNING</code> if the subfont which
     *                  was returned cannot render all of the text.
     *
     * @return an <code>LEFontInstance</code> for the sub font which can render the characters, or
     *         <code>NULL</code> if there is an error.
     *
     * @see LEScripts.h
     *
     * @draft ICU 2.6
     */
    virtual const LEFontInstance *getSubFont(const LEUnicode chars[], le_int32 *offset, le_int32 limit, le_int32 script, LEErrorCode &success) const;

    //
    // Font file access
    //

    /**
     * This method reads a table from the font. Note that in general,
     * it only makes sense to call this method on an <code>LEFontInstance</code>
     * which represents a physical font - i.e. one which has been returned by
     * <code>getSubFont()</code>. This is because each subfont in a composite font
     * will have different tables, and there's no way to know which subfont to access.
     *
     * Subclasses which represent composite fonts should always return <code>NULL</code>.
     *
     * @param tableTag - the four byte table tag. (e.g. 'cmap') 
     *
     * @return the address of the table in memory, or <code>NULL</code>
     *         if the table doesn't exist.
     *
     * @stable ICU 2.8
     */
    virtual const void *getFontTable(LETag tableTag) const = 0;

    /**
     * This method is used to determine if the font can
     * render the given character. This can usually be done
     * by looking the character up in the font's character
     * to glyph mapping.
     *
     * The default implementation of this method will return
     * <code>TRUE</code> if <code>mapCharToGlyph(ch)</code>
     * returns a non-zero value.
     *
     * @param ch - the character to be tested
     *
     * @return TRUE if the font can render ch.
     *
     * @draft ICU 2.6
     */
    virtual le_bool canDisplay(LEUnicode32 ch) const;

    /**
     * This method returns the number of design units in
     * the font's EM square.
     *
     * @return the number of design units pre EM.
     *
     * @stable ICU 2.8
     */
    virtual le_int32 getUnitsPerEM() const = 0;

    /**
     * This method maps an array of character codes to an array of glyph
     * indices, using the font's character to glyph map.
     *
     * @param chars - the character array
     * @param offset - the index of the first character
     * @param count - the number of characters
     * @param reverse - if TRUE, store the glyph indices in reverse order.
     * @param mapper - the character mapper.
     * @param glyphStorage - the object which contains the output glyph array
     *
     * @see LECharMapper
     *
     * @draft ICU 2.6
     */
    virtual void mapCharsToGlyphs(const LEUnicode chars[], le_int32 offset, le_int32 count, le_bool reverse, const LECharMapper *mapper, LEGlyphStorage &glyphStorage) const;

    /**
     * This method maps a single character to a glyph index, using the
     * font's character to glyph map. The default implementation of this
     * method calls the mapper, and then calls <code>mapCharToGlyph(mappedCh)</code>.
     *
     * @param ch - the character
     * @param mapper - the character mapper
     *
     * @return the glyph index
     *
     * @see LECharMapper
     *
     * @draft ICU 2.6
     */
    virtual LEGlyphID mapCharToGlyph(LEUnicode32 ch, const LECharMapper *mapper) const;

    /**
     * This method maps a single character to a glyph index, using the
     * font's character to glyph map. There is no default implementation
     * of this method because it requires information about the platform
     * font implementation.
     *
     * @param ch - the character
     *
     * @return the glyph index
     *
     * @draft ICU 2.6
     */
    virtual LEGlyphID mapCharToGlyph(LEUnicode32 ch) const = 0;

    //
    // Metrics
    //

    /**
     * This method gets the X and Y advance of a particular glyph, in pixels.
     *
     * @param glyph - the glyph index
     * @param advance - the X and Y pixel values will be stored here
     *
     * @draft ICU 2.2
     */
    virtual void getGlyphAdvance(LEGlyphID glyph, LEPoint &advance) const = 0;

    /**
     * This method gets the hinted X and Y pixel coordinates of a particular
     * point in the outline of the given glyph.
     *
     * @param glyph - the glyph index
     * @param pointNumber - the number of the point
     * @param point - the point's X and Y pixel values will be stored here
     *
     * @return TRUE if the point coordinates could be stored.
     *
     * @stable ICU 2.8
     */
    virtual le_bool getGlyphPoint(LEGlyphID glyph, le_int32 pointNumber, LEPoint &point) const = 0;

    /**
     * This method returns the width of the font's EM square
     * in pixels.
     *
     * @return the pixel width of the EM square
     *
     * @stable ICU 2.8
     */
    virtual float getXPixelsPerEm() const = 0;

    /**
     * This method returns the height of the font's EM square
     * in pixels.
     *
     * @return the pixel height of the EM square
     *
     * @stable ICU 2.8
     */
    virtual float getYPixelsPerEm() const = 0;

    /**
     * This method converts font design units in the
     * X direction to points.
     *
     * @param xUnits - design units in the X direction
     *
     * @return points in the X direction
     *
     * @draft ICU 2.6
     */
    virtual float xUnitsToPoints(float xUnits) const;

    /**
     * This method converts font design units in the
     * Y direction to points.
     *
     * @param yUnits - design units in the Y direction
     *
     * @return points in the Y direction
     *
     * @draft ICU 2.6
     */
    virtual float yUnitsToPoints(float yUnits) const;

    /**
     * This method converts font design units to points.
     *
     * @param units - X and Y design units
     * @param points - set to X and Y points
     *
     * @draft ICU 2.6
     */
    virtual void unitsToPoints(LEPoint &units, LEPoint &points) const;

    /**
     * This method converts pixels in the
     * X direction to font design units.
     *
     * @param xPixels - pixels in the X direction
     *
     * @return font design units in the X direction
     *
     * @draft ICU 2.6
     */
    virtual float xPixelsToUnits(float xPixels) const;

    /**
     * This method converts pixels in the
     * Y direction to font design units.
     *
     * @param yPixels - pixels in the Y direction
     *
     * @return font design units in the Y direction
     *
     * @draft ICU 2.6
     */
    virtual float yPixelsToUnits(float yPixels) const;

    /**
     * This method converts pixels to font design units.
     *
     * @param pixels - X and Y pixel
     * @param units - set to X and Y font design units
     *
     * @draft ICU 2.6
     */
    virtual void pixelsToUnits(LEPoint &pixels, LEPoint &units) const;

    /**
     * Get the X scale factor from the font's transform. The default
     * implementation of <code>transformFunits()</code> will call this method.
     *
     * @return the X scale factor.
     *
     *
     * @see transformFunits
     *
     * @draft ICU 2.6
     */
    virtual float getScaleFactorX() const = 0;

    /**
     * Get the Y scale factor from the font's transform. The default
     * implementation of <code>transformFunits()</code> will call this method.
     *
     * @return the Yscale factor.
     *
     * @see transformFunits
     *
     * @draft ICU 2.6
     */
    virtual float getScaleFactorY() const = 0;

    /**
     * This method transforms an X, Y point in font design units to a
     * pixel coordinate, applying the font's transform. The default
     * implementation of this method calls <code>getScaleFactorX()</code>
     * and <code>getScaleFactorY()</code>.
     *
     * @param xFunits - the X coordinate in font design units
     * @param yFunits - the Y coordinate in font design units
     * @param pixels - the tranformed co-ordinate in pixels
     *
     * @see getScaleFactorX
     * @see getScaleFactorY
     *
     * @draft ICU 2.6
     */
    virtual void transformFunits(float xFunits, float yFunits, LEPoint &pixels) const;

    /**
     * This is a convenience method used to convert
     * values in a 16.16 fixed point format to floating point.
     *
     * @param fixed - the fixed point value
     *
     * @return the floating point value
     *
     * @stable ICU 2.8
     */
    static float fixedToFloat(le_int32 fixed);

    /**
     * This is a convenience method used to convert
     * floating point values to 16.16 fixed point format.
     *
     * @param theFloat - the floating point value
     *
     * @return the fixed point value
     *
     * @stable ICU 2.8
     */
    static le_int32 floatToFixed(float theFloat);

    //
    // These methods won't ever be called by the LayoutEngine,
    // but are useful for clients of <code>LEFontInstance</code> who
    // need to render text.
    //

    /**
     * Get the font's ascent.
     *
     * @return the font's ascent, in points. This value
     * will always be positive.
     *
     * @draft ICU 2.6
     */
    virtual le_int32 getAscent() const = 0;

    /**
     * Get the font's descent.
     *
     * @return the font's descent, in points. This value
     * will always be positive.
     *
     * @draft ICU 2.6
     */
    virtual le_int32 getDescent() const = 0;

    /**
     * Get the font's leading.
     *
     * @return the font's leading, in points. This value
     * will always be positive.
     *
     * @draft ICU 2.6
     */
    virtual le_int32 getLeading() const = 0;

    /**
     * Get the line height required to display text in
     * this font. The default implementation of this method
     * returns the sum of the ascent, descent, and leading.
     *
     * @return the line height, in points. This vaule will
     * always be positive.
     *
     * @draft ICU 2.6
     */
    virtual le_int32 getLineHeight() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 2.6
     */
    virtual inline UClassID getDynamicClassID() const { return getStaticClassID(); }

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 2.6
     */
    static inline UClassID getStaticClassID() { return (UClassID)&fgClassID; }

private:

    /**
     * The address of this static class variable serves as this class's ID
     * for ICU "poor man's RTTI".
     */
    static const char fgClassID;
};

inline le_bool LEFontInstance::canDisplay(LEUnicode32 ch) const
{
    return LE_GET_GLYPH(mapCharToGlyph(ch)) != 0;
}

inline float LEFontInstance::xUnitsToPoints(float xUnits) const
{
    return (xUnits * getXPixelsPerEm()) / (float) getUnitsPerEM();
}

inline float LEFontInstance::yUnitsToPoints(float yUnits) const
{
    return (yUnits * getYPixelsPerEm()) / (float) getUnitsPerEM();
}

inline void LEFontInstance::unitsToPoints(LEPoint &units, LEPoint &points) const
{
    points.fX = xUnitsToPoints(units.fX);
    points.fY = yUnitsToPoints(units.fY);
}

inline float LEFontInstance::xPixelsToUnits(float xPixels) const
{
    return (xPixels * getUnitsPerEM()) / (float) getXPixelsPerEm();
}

inline float LEFontInstance::yPixelsToUnits(float yPixels) const
{
    return (yPixels * getUnitsPerEM()) / (float) getYPixelsPerEm();
}

inline void LEFontInstance::pixelsToUnits(LEPoint &pixels, LEPoint &units) const
{
    units.fX = xPixelsToUnits(pixels.fX);
    units.fY = yPixelsToUnits(pixels.fY);
}

inline void LEFontInstance::transformFunits(float xFunits, float yFunits, LEPoint &pixels) const
{
    pixels.fX = xUnitsToPoints(xFunits) * getScaleFactorX();
    pixels.fY = yUnitsToPoints(yFunits) * getScaleFactorY();
}

inline float LEFontInstance::fixedToFloat(le_int32 fixed)
{
    return (float) (fixed / 65536.0);
}

inline le_int32 LEFontInstance::floatToFixed(float theFloat)
{
    return (le_int32) (theFloat * 65536.0);
}

inline le_int32 LEFontInstance::getLineHeight() const
{
    return getAscent() + getDescent() + getLeading();
}

U_NAMESPACE_END
#endif


