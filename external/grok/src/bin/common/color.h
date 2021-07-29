/*
 *    Copyright (C) 2016-2021 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the BSD 2-clause license.
 *    Please see the LICENSE file in the root directory for details.
 */

#pragma once

namespace grk
{
bool color_sycc_to_rgb(grk_image* img, bool oddFirstX, bool oddFirstY);
#if defined(GROK_HAVE_LIBLCMS)
bool color_cielab_to_rgb(grk_image* image);
void color_apply_icc_profile(grk_image* image, bool forceRGB);
#endif
bool color_cmyk_to_rgb(grk_image* image);
bool color_esycc_to_rgb(grk_image* image);
void alloc_palette(grk_color* color, uint8_t num_channels, uint16_t num_entries);
bool validate_icc(GRK_COLOR_SPACE color_space, uint8_t* iccbuf, uint32_t icclen);
void copy_icc(grk_image* dest, uint8_t* iccbuf, uint32_t icclen);
void create_meta(grk_image* img);

} // namespace grk
