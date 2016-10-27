/*
 * Copyright © 2011  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#include "view-cairo.hh"

#include <assert.h>


void
view_cairo_t::render (const font_options_t *font_opts)
{
  bool vertical = HB_DIRECTION_IS_VERTICAL (direction);
  int vert  = vertical ? 1 : 0;
  int horiz = vertical ? 0 : 1;

  int x_sign = font_opts->font_size_x < 0 ? -1 : +1;
  int y_sign = font_opts->font_size_y < 0 ? -1 : +1;

  cairo_scaled_font_t *scaled_font = helper_cairo_create_scaled_font (font_opts);
  cairo_font_extents_t font_extents;
  cairo_scaled_font_extents (scaled_font, &font_extents);
  /* Looks like cairo doesn't negate the sign of font extents even if
   * y_scale is negative.  This is probably a bug, but that's the way
   * it is, and we code for it.  Assert, just in case this accidentally
   * changes in the future (or is different on non-FreeType cairo font
   * backends. */
  assert (font_extents.height >= 0);
  double leading = font_extents.height + view_options.line_space;

  /* Calculate surface size. */
  double w, h;
  (vertical ? w : h) = (int) lines->len * leading - view_options.line_space;
  (vertical ? h : w) = 0;
  for (unsigned int i = 0; i < lines->len; i++) {
    helper_cairo_line_t &line = g_array_index (lines, helper_cairo_line_t, i);
    double x_advance, y_advance;
    line.get_advance (&x_advance, &y_advance);
    if (vertical)
      h =  MAX (h, y_sign * y_advance);
    else
      w =  MAX (w, x_sign * x_advance);
  }

  /* Create surface. */
  cairo_t *cr = helper_cairo_create_context (w + view_options.margin.l + view_options.margin.r,
					     h + view_options.margin.t + view_options.margin.b,
					     &view_options, &output_options);
  cairo_set_scaled_font (cr, scaled_font);

  /* Setup coordinate system. */
  cairo_translate (cr, view_options.margin.l, view_options.margin.t);
  if (vertical)
    cairo_translate (cr,
		     w /* We stack lines right to left */
		     -font_extents.height * .5 /* "ascent" for vertical */,
		     y_sign < 0 ? h : 0);
  else
   {
    cairo_translate (cr,
		     x_sign < 0 ? w : 0,
		     y_sign < 0 ? font_extents.descent : font_extents.ascent);
   }

  /* Draw. */
  cairo_translate (cr, +vert * leading, -horiz * leading);
  for (unsigned int i = 0; i < lines->len; i++)
  {
    helper_cairo_line_t &l = g_array_index (lines, helper_cairo_line_t, i);

    cairo_translate (cr, -vert * leading, +horiz * leading);

    if (view_options.annotate) {
      cairo_save (cr);

      /* Draw actual glyph origins */
      cairo_set_source_rgba (cr, 1., 0., 0., .5);
      cairo_set_line_width (cr, 5);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
      for (unsigned i = 0; i < l.num_glyphs; i++) {
	cairo_move_to (cr, l.glyphs[i].x, l.glyphs[i].y);
	cairo_rel_line_to (cr, 0, 0);
      }
      cairo_stroke (cr);

      cairo_restore (cr);
    }

    if (0 && cairo_surface_get_type (cairo_get_target (cr)) == CAIRO_SURFACE_TYPE_IMAGE) {
      /* cairo_show_glyphs() doesn't support subpixel positioning */
      cairo_glyph_path (cr, l.glyphs, l.num_glyphs);
      cairo_fill (cr);
    } else if (l.num_clusters)
      cairo_show_text_glyphs (cr,
			      l.utf8, l.utf8_len,
			      l.glyphs, l.num_glyphs,
			      l.clusters, l.num_clusters,
			      l.cluster_flags);
    else
      cairo_show_glyphs (cr, l.glyphs, l.num_glyphs);
  }

  /* Clean up. */
  helper_cairo_destroy_context (cr);
  cairo_scaled_font_destroy (scaled_font);
}
