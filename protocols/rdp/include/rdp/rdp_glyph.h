/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#ifndef _GUAC_RDP_RDP_GLYPH_H
#define _GUAC_RDP_RDP_GLYPH_H

#include <guacamole/config.h>

#include <cairo/cairo.h>
#include <freerdp/freerdp.h>

#ifdef ENABLE_WINPR
#include <winpr/wtypes.h>
#else
#include <rdp/compat/winpr-wtypes.h>
#endif

/**
 * Guacamole-specific rdpGlyph data.
 */
typedef struct guac_rdp_glyph {

    /**
     * FreeRDP glyph data - MUST GO FIRST.
     */
    rdpGlyph glyph;

    /**
     * Cairo surface layer containing cached image data.
     */
    cairo_surface_t* surface;

} guac_rdp_glyph;

/**
 * Caches the given glyph. Note that this caching currently only occurs server-
 * side, as it is more efficient to transmit the text as PNG.
 *
 * @param context
 *     The rdpContext associated with the current RDP session.
 *
 * @param glyph
 *     The glyph to cache.
 */
void guac_rdp_glyph_new(rdpContext* context, rdpGlyph* glyph);

/**
 * Draws a previously-cached glyph at the given coordinates within the current
 * drawing surface.
 *
 * @param context
 *     The rdpContext associated with the current RDP session.
 *
 * @param glyph
 *     The cached glyph to draw.
 *
 * @param x
 *     The destination X coordinate of the upper-left corner of the glyph.
 *
 * @param y
 *     The destination Y coordinate of the upper-left corner of the glyph.
 */
void guac_rdp_glyph_draw(rdpContext* context, rdpGlyph* glyph, int x, int y);

/**
 * Frees any Guacamole-specific data associated with the given glyph, such that
 * it can be safely freed by FreeRDP.
 *
 * @param context
 *     The rdpContext associated with the current RDP session.
 *
 * @param glyph
 *     The cached glyph to free.
 */
void guac_rdp_glyph_free(rdpContext* context, rdpGlyph* glyph);

/**
 * Called just prior to rendering a series of glyphs. After this function is
 * called, the glyphs will be individually rendered by calls to
 * guac_rdp_glyph_draw().
 *
 * @param context
 *     The rdpContext associated with the current RDP session.
 *
 * @param x
 *     The X coordinate of the upper-left corner of the background rectangle of
 *     the drawing operation, or 0 if the background is transparent.
 *
 * @param y
 *     The Y coordinate of the upper-left corner of the background rectangle of
 *     the drawing operation, or 0 if the background is transparent.
 *
 * @param width
 *     The width of the background rectangle of the drawing operation, or 0 if
 *     the background is transparent.
 *
 * @param height 
 *     The height of the background rectangle of the drawing operation, or 0 if
 *     the background is transparent.
 *
 * @param fgcolor
 *     The foreground color of each glyph. This color will be in the colorspace
 *     of the RDP session, and may even be a palette index, and must be
 *     translated via guac_rdp_convert_color().
 *
 * @param bgcolor
 *     The background color of the drawing area. This color will be in the
 *     colorspace of the RDP session, and may even be a palette index, and must
 *     be translated via guac_rdp_convert_color(). If the background is
 *     transparent, this value is undefined.
 */
void guac_rdp_glyph_begindraw(rdpContext* context,
        int x, int y, int width, int height, UINT32 fgcolor, UINT32 bgcolor);

/**
 * Called immediately after rendering a series of glyphs. Unlike
 * guac_rdp_glyph_begindraw(), there is no way to detect through any invocation
 * of this function whether the background color is opaque or transparent. We
 * currently do NOT implement this function.
 *
 * @param context
 *     The rdpContext associated with the current RDP session.
 *
 * @param x
 *     The X coordinate of the upper-left corner of the background rectangle of
 *     the drawing operation.
 *
 * @param y
 *     The Y coordinate of the upper-left corner of the background rectangle of
 *     the drawing operation.
 *
 * @param width
 *     The width of the background rectangle of the drawing operation.
 *
 * @param height 
 *     The height of the background rectangle of the drawing operation.
 *
 * @param fgcolor
 *     The foreground color of each glyph. This color will be in the colorspace
 *     of the RDP session, and may even be a palette index, and must be
 *     translated via guac_rdp_convert_color().
 *
 * @param bgcolor
 *     The background color of the drawing area. This color will be in the
 *     colorspace of the RDP session, and may even be a palette index, and must
 *     be translated via guac_rdp_convert_color(). If the background is
 *     transparent, this value is undefined.
 */
void guac_rdp_glyph_enddraw(rdpContext* context,
        int x, int y, int width, int height, UINT32 fgcolor, UINT32 bgcolor);

#endif
