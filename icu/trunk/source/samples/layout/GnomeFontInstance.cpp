
/*
 *******************************************************************************
 *
 *   Copyright (C) 1999-2001, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 *******************************************************************************
 *   file name:  GnomeFontInstance.cpp
 *
 *   created on: 08/30/2001
 *   created by: Eric R. Mader
 */

#include <gnome.h>
#include "freetype/freetype.h"

#include "layout/LETypes.h"
#include "layout/LESwaps.h"

#include "GnomeFontInstance.h"
#include "sfnt.h"
#include "cmaps.h"

GnomeFontInstance::GnomeFontInstance(TT_Engine engine, const TT_Text *fontPathName, le_int16 pointSize, RFIErrorCode &status)
    : RenderingFontInstance(NULL, pointSize)
{
    TT_Error error;
    TT_Face_Properties faceProperties;

    fFace.z = NULL;

    error = TT_Open_Face(engine, fontPathName, &fFace);

    if (error != 0) {
        status = RFI_FONT_FILE_NOT_FOUND_ERROR;
        return;
    }

    error = TT_New_Instance(fFace, &fInstance);

    if (error != 0) {
        status = RFI_OUT_OF_MEMORY_ERROR;
        return;
    }

    // FIXME: what about the display resolution?
    // TT_Set_Instance_Resolutions(fInstance, 72, 72);
    fDeviceScaleX = ((float) 96) / 72;
    fDeviceScaleY = ((float) 96) / 72;

    TT_Set_Instance_CharSize(fInstance, pointSize << 6);

    TT_Get_Face_Properties(fFace, &faceProperties);

    fUnitsPerEM = faceProperties.header->Units_Per_EM;

    fAscent  = (le_int32) yUnitsToPoints(faceProperties.horizontal->Ascender);
    fDescent = (le_int32) -yUnitsToPoints(faceProperties.horizontal->Descender);
    fLeading = (le_int32) yUnitsToPoints(faceProperties.horizontal->Line_Gap);

    // printf("Face = %s, unitsPerEM = %d, ascent = %d, descent = %d\n", fontPathName, fUnitsPerEM, fAscent, fDescent);

    error = TT_New_Glyph(fFace, &fGlyph);

    if (error != 0) {
        status = RFI_OUT_OF_MEMORY_ERROR;
        return;
    }

    status = initMapper();
    if (LE_SUCCESS(status)) {
        status = initFontTableCache();
    }
}

GnomeFontInstance::~GnomeFontInstance()
{
    if (fFace.z != NULL) {
        TT_Close_Face(fFace);
    }
}

const void *GnomeFontInstance::readFontTable(LETag tableTag) const
{
    TT_Long len = 0;
    void *result = NULL;

    TT_Get_Font_Data(fFace, tableTag, 0, NULL, &len);

    if (len > 0) {
        result = new char[len];
        TT_Get_Font_Data(fFace, tableTag, 0, result, &len);
    }

    return result;
}

void GnomeFontInstance::getGlyphAdvance(LEGlyphID glyph, LEPoint &advance) const
{
    advance.fX = 0;
    advance.fY = 0;

    if (glyph == 0xFFFF) {
        return;
    }

    TT_Glyph_Metrics metrics;
    TT_Error error;

    error = TT_Load_Glyph(fInstance, fGlyph, glyph, TTLOAD_SCALE_GLYPH | TTLOAD_HINT_GLYPH);

    if (error != 0) {
        return;
    }

    TT_Get_Glyph_Metrics(fGlyph, &metrics);

    advance.fX = metrics.advance >> 6;
    return;
}

le_bool GnomeFontInstance::getGlyphPoint(LEGlyphID glyph, le_int32 pointNumber, LEPoint &point) const
{
#if 1
    TT_Outline outline;
    TT_Error error;

    error = TT_Load_Glyph(fInstance, fGlyph, glyph, TTLOAD_SCALE_GLYPH | TTLOAD_HINT_GLYPH);

    if (error != 0) {
        return false;
    }

    error = TT_Get_Glyph_Outline(fGlyph, &outline);

    if (error != 0 || pointNumber >= outline.n_points) {
        return false;
    }

    point.fX = outline.points[pointNumber].x >> 6;
    point.fY = outline.points[pointNumber].y >> 6;

    return true;
#else
    return false;
#endif
}

#define _R(b7,b6,b5,b4,b3,b2,b1,b0) ((b0<<7)|(b1<<6)|(b2<<5)|(b3<<4)|(b4<<3)|(b5<<2)|(b6<<1)|b7)
#define _B(b,n) ((b>>n)&1)
#define _H(b) _R(_B(b,7),_B(b,6),_B(b,5),_B(b,4),_B(b,3),_B(b,2),_B(b,1),_B(b,0))

const char bitReverse[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};


// FIXME: this would be much faster if we cached the TT_Glyph objects based on the glyph ID...
void GnomeFontInstance::drawGlyphs(void *surface, LEGlyphID *glyphs, le_uint32 count, le_int32 *dx,
                                   le_int32 x, le_int32 y, le_int32 width, le_int32 height) const
{
    GtkWidget *widget = (GtkWidget *) surface;
    le_int32 i, xx = 0, yy = 0, zz;
    le_int32 minx = 0, maxx = 0, miny = 0, maxy = 0;
    TT_Glyph_Metrics metrics;
    TT_Error error;

    //printf("drawGlyphs() - x = %d, y = %d, ", x, y);

    for (i = 0; i < count; i += 1) {
        error = TT_Load_Glyph(fInstance, fGlyph, glyphs[i], TTLOAD_SCALE_GLYPH | TTLOAD_HINT_GLYPH);
        if (error == 0) {
            TT_Get_Glyph_Metrics(fGlyph, &metrics);

            zz = xx + metrics.bbox.xMin;
            if (minx > zz) {
                minx = zz;
            }

            zz = xx + metrics.bbox.xMax;
            if (maxx < zz) {
                maxx = zz;
            }

            zz = yy + metrics.bbox.yMin;
            if (miny > zz) {
                miny = zz;
            }

            zz = yy + metrics.bbox.yMax;
            if (maxy < zz) {
                maxy = zz;
            }
        }

        xx += (dx[i] * 64);
    }


    minx = (minx & -64) >> 6;
    miny = (miny & -64) >> 6;

    maxx = ((maxx + 63) & -64) >> 6;
    maxy = ((maxy + 63) & -64) >> 6;

    //printf("minx = %d, maxx = %d, miny = %d, maxy = %d\n", minx, maxx, miny, maxy);

    TT_Raster_Map raster;
    unsigned char *bits;

    raster.flow  = TT_Flow_Down;
    raster.width = maxx - minx;
    raster.rows  = maxy - miny;
    raster.cols  = (raster.width + 7) / 8;
    raster.size  = raster.cols * raster.rows;

    raster.bitmap = bits = new unsigned char[raster.size];

    for (i = 0; i < raster.size; i += 1) {
        bits[i] = 0;
    }

    xx = (-minx) * 64; yy = (-miny) * 64;

    for (i = 0; i < count; i += 1) {
        if (glyphs[i] < 0xFFFE) {
            error = TT_Load_Glyph(fInstance, fGlyph, glyphs[i], TTLOAD_SCALE_GLYPH | TTLOAD_HINT_GLYPH);
        
            if (error == 0) {
                TT_Get_Glyph_Bitmap(fGlyph, &raster, xx, yy);
            }
        }

        xx += (dx[i] * 64);
    }

    for (i = 0; i < raster.size; i += 1) {
        bits[i] = bitReverse[bits[i]];
    }

    if (raster.width > 0 && raster.rows > 0) {
        GdkBitmap *bitmap = gdk_bitmap_create_from_data(NULL, (const gchar *) raster.bitmap, raster.width, raster.rows);

        gint bitsx = x + minx;
        gint bitsy = y - maxy;

        gdk_gc_set_clip_origin(widget->style->black_gc, bitsx, bitsy);
        gdk_gc_set_clip_mask(widget->style->black_gc, bitmap);

        gdk_draw_rectangle(widget->window,
                           widget->style->black_gc,
                           TRUE,
                           bitsx, bitsy,
                           raster.width, raster.rows);

        gdk_gc_set_clip_origin(widget->style->black_gc, 0, 0);
        gdk_gc_set_clip_mask(widget->style->black_gc, NULL);

        gdk_bitmap_unref(bitmap);
    }
    
    delete[] bits;
}
