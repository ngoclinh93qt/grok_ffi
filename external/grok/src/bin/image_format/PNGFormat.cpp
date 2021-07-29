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
 *    This source code incorporates work covered by the BSD 2-clause license.
 *    Please see the LICENSE m_fileHandle in the root directory for details.
 *
 */

#include <cstdio>
#include <cstdlib>
#include "grk_apps_config.h"
#include "grok.h"
#include "PNGFormat.h"
#include "convert.h"
#include "color.h"
#include <cstring>

#ifdef GROK_HAVE_LIBLCMS
#include <lcms2.h>
#endif
#include <memory>
#include <iostream>
#include <string>
#include <cassert>
#include <locale>
#include "common.h"
#include "FileStreamIO.h"

#define PNG_MAGIC "\x89PNG\x0d\x0a\x1a\x0a"
#define MAGIC_SIZE 8
/* PNG allows bits per sample: 1, 2, 4, 8, 16 */

static bool pngWarningHandlerVerbose = true;

static void png_warning_fn(png_structp png_ptr, png_const_charp warning_message)
{
	(void)png_ptr;
	if(pngWarningHandlerVerbose)
		spdlog::warn("libpng: {}", warning_message);
}

static void png_error_fn(png_structp png_ptr, png_const_charp error_message)
{
	(void)png_ptr;
	spdlog::error("libpng: {}", error_message);
}

void pngSetVerboseFlag(bool verbose)
{
	pngWarningHandlerVerbose = verbose;
}

grk_image* PNGFormat::do_decode(const char* read_idf, grk_cparameters* params)
{
	int bit_depth, interlace_type, compression_type, filter_type;
	png_uint_32 width = 0U, height = 0U;
	uint32_t stride;
	int color_type;
	m_useStdIO = grk::useStdio(read_idf);
	grk_image_cmptparm cmptparm[4];
	uint16_t nr_comp;
	uint8_t sigbuf[8];
	cvtTo32 cvtXXTo32s = nullptr;
	cvtInterleavedToPlanar cvtToPlanar = nullptr;
	int32_t* m_planes[4];
	int srgbIntent = -1;
	png_textp text_ptr;
	int num_comments = 0;
	int unit;
	png_uint_32 resx, resy;

	if(params->subsampling_dx != 1 || params->subsampling_dy != 1)
	{
		spdlog::error("pngtoimage: unsupported sub-sampling ({},{})", params->subsampling_dx,
					  params->subsampling_dy);
		return nullptr;
	}

	if(m_useStdIO)
	{
		if(!grk::grk_set_binary_mode(stdin))
			return nullptr;
		m_fileStream = stdin;
	}
	else
	{
		if((m_fileStream = fopen(read_idf, "rb")) == nullptr)
		{
			spdlog::error("pngtoimage: can not open {}", read_idf);
			return nullptr;
		}
	}

	if(fread(sigbuf, 1, MAGIC_SIZE, m_fileStream) != MAGIC_SIZE ||
	   memcmp(sigbuf, PNG_MAGIC, MAGIC_SIZE) != 0)
	{
		spdlog::error("pngtoimage: {} is no valid PNG m_fileHandle", read_idf);
		goto beach;
	}

	if((png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, png_error_fn,
									 png_warning_fn)) == nullptr)
		goto beach;

	// allow Microsoft/HP 3144-byte sRGB profile, normally skipped by library
	// because it deems it broken. (a controversial decision)
	png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);

	// treat some errors as warnings
	png_set_benign_errors(png, 1);

	if((m_info = png_create_info_struct(png)) == nullptr)
		goto beach;

	if(setjmp(png_jmpbuf(png)))
		goto beach;

	png_init_io(png, m_fileStream);
	png_set_sig_bytes(png, MAGIC_SIZE);

	png_read_info(png, m_info);

	if(png_get_IHDR(png, m_info, &width, &height, &bit_depth, &color_type, &interlace_type,
					&compression_type, &filter_type) == 0)
		goto beach;

	if(!width || !height)
		goto beach;

	if(interlace_type == PNG_INTERLACE_ADAM7)
	{
		auto number_of_passes = png_set_interlace_handling(png);
		assert(number_of_passes == 7);
		(void)number_of_passes;
	}

	/* png_set_expand():
	 * expand paletted images to RGB, expand grayscale images of
	 * less than 8-bit depth to 8-bit depth, and expand tRNS chunks
	 * to alpha channels.
	 */
	if(color_type == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_expand(png);
	}

	if(png_get_valid(png, m_info, PNG_INFO_tRNS))
	{
		png_set_expand(png);
	}
	/* We might want to expand background */
	/*
	 if(png_get_valid(png, info, PNG_INFO_bKGD)) {
	 png_color_16p bgnd;
	 png_get_bKGD(png, info, &bgnd);
	 png_set_background(png, bgnd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
	 }
	 */

	if(png_get_sRGB(png, m_info, &srgbIntent))
	{
		if(srgbIntent >= 0 && srgbIntent <= 3)
			m_colorSpace = GRK_CLRSPC_SRGB;
	}

	png_read_update_info(png, m_info);
	color_type = png_get_color_type(png, m_info);

	switch(color_type)
	{
		case PNG_COLOR_TYPE_GRAY:
			nr_comp = 1;
			break;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			nr_comp = 2;
			break;
		case PNG_COLOR_TYPE_RGB:
			nr_comp = 3;
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			nr_comp = 4;
			break;
		default:
			spdlog::error("pngtoimage: colortype {} is not supported", color_type);
			goto beach;
	}

	if(m_colorSpace == GRK_CLRSPC_UNKNOWN)
		m_colorSpace = (nr_comp > 2U) ? GRK_CLRSPC_SRGB : GRK_CLRSPC_GRAY;

	cvtToPlanar = cvtInterleavedToPlanar_LUT[nr_comp];
	bit_depth = png_get_bit_depth(png, m_info);

	switch(bit_depth)
	{
		case 1:
		case 2:
		case 4:
		case 8:
			cvtXXTo32s = cvtTo32_LUT[bit_depth];
			break;
		case 16: /* 16 bpp is specific to PNG */
			cvtXXTo32s = convert_16u32s_C1R;
			break;
		default:
			spdlog::error("pngtoimage: bit depth {} is not supported", bit_depth);
			goto beach;
	}

	row_buf_array = (uint8_t**)calloc(height, sizeof(uint8_t*));
	if(row_buf_array == nullptr)
	{
		spdlog::error("pngtoimage: out of memory");
		goto beach;
	}
	for(uint32_t i = 0; i < height; ++i)
	{
		row_buf_array[i] = (uint8_t*)malloc(png_get_rowbytes(png, m_info));
		if(!row_buf_array[i])
		{
			spdlog::error("pngtoimage: out of memory");
			goto beach;
		}
	}

	png_read_image(png, row_buf_array);

	/* Create image */
	memset(cmptparm, 0, sizeof(cmptparm));
	for(uint32_t i = 0; i < nr_comp; ++i)
	{
		auto img_comp = cmptparm + i;
		img_comp->prec = (uint8_t)bit_depth;
		img_comp->sgnd = false;
		img_comp->dx = params->subsampling_dx;
		img_comp->dy = params->subsampling_dy;
		img_comp->w = grk::ceildiv<uint32_t>(width, img_comp->dx);
		img_comp->h = grk::ceildiv<uint32_t>(height, img_comp->dy);
	}

	m_image = grk_image_new(nr_comp, &cmptparm[0], m_colorSpace, true);
	if(m_image == nullptr)
		goto beach;
	m_image->x0 = params->image_offset_x0;
	m_image->y0 = params->image_offset_y0;
	m_image->x1 = m_image->x0 + (width - 1) * params->subsampling_dx + 1;
	m_image->y1 = m_image->y0 + (height - 1) * params->subsampling_dy + 1;

	/* Set alpha channel. Only non-premultiplied alpha is supported */
	if((nr_comp & 1U) == 0)
	{
		m_image->comps[nr_comp - 1U].type = GRK_COMPONENT_TYPE_OPACITY;
		m_image->comps[nr_comp - 1U].association = GRK_COMPONENT_ASSOC_WHOLE_IMAGE;
	}
	for(uint32_t i = 0; i < nr_comp; i++)
		m_planes[i] = m_image->comps[i].data;

	// See if iCCP chunk is present
	if(png_get_valid(png, m_info, PNG_INFO_iCCP))
	{
		uint32_t ProfileLen = 0;
		png_bytep ProfileData = nullptr;
		int Compression = 0;
		png_charp ProfileName = nullptr;
		if(png_get_iCCP(png, m_info, &ProfileName, &Compression, &ProfileData, &ProfileLen) ==
		   PNG_INFO_iCCP)
		{
			if(grk::validate_icc(m_colorSpace, ProfileData, ProfileLen))
			{
				grk::copy_icc(m_image, ProfileData, ProfileLen);
			}
			else
			{
				spdlog::warn("ICC profile does not match underlying colour space. Ignoring");
			}
		}
	}

	if(png_get_valid(png, m_info, PNG_INFO_gAMA))
	{
		spdlog::warn(
			"input PNG contains gamma value; this will not be stored in compressed image.");
	}

	if(png_get_valid(png, m_info, PNG_INFO_cHRM))
	{
		spdlog::warn(
			"input PNG contains chroma information which will not be stored in compressed image.");
	}

	/*
	 double fileGamma = 0.0;
	 if (png_get_gAMA(png, info, &fileGamma)) {
	 spdlog::warn("input PNG contains gamma value of %f; this will not be stored in compressed
	 image.", fileGamma);
	 }
	 double wpx, wpy, rx, ry, gx, gy, bx, by; // white point and primaries
	 if (png_get_cHRM(png, info, &wpx, &wpy, &rx, &ry, &gx, &gy, &bx, &by)) 	{
	 spdlog::warn("input PNG contains chroma information which will not be stored in compressed
	 image.");
	 }
	 */

	num_comments = png_get_text(png, m_info, &text_ptr, nullptr);
	if(num_comments)
	{
		for(int i = 0; i < num_comments; ++i)
		{
			const char* key = text_ptr[i].key;
			if(!strcmp(key, "Description"))
			{
			}
			else if(!strcmp(key, "Author"))
			{
			}
			else if(!strcmp(key, "Title"))
			{
			}
			else if(!strcmp(key, "XML:com.adobe.xmp"))
			{
				if(text_ptr[i].text_length)
				{
					grk::create_meta(m_image);
					m_image->meta->xmp_len = text_ptr[i].text_length;
					m_image->meta->xmp_buf = new uint8_t[m_image->meta->xmp_len];
					memcpy(m_image->meta->xmp_buf, text_ptr[i].text, m_image->meta->xmp_len);
				}
			}
			// other comments
			else
			{
			}
		}
	}

	if(png_get_pHYs(png, m_info, &resx, &resy, &unit))
	{
		if(unit == PNG_RESOLUTION_METER)
		{
			m_image->capture_resolution[0] = resx;
			m_image->capture_resolution[1] = resy;
		}
		else
		{
			spdlog::warn("input PNG contains resolution information"
						 " in unknown units. Ignoring");
		}
	}

	stride = m_image->comps[0].stride;
	row32s = (int32_t*)malloc((size_t)width * nr_comp * sizeof(int32_t));
	if(row32s == nullptr)
		goto beach;

	for(uint32_t i = 0; i < height; ++i)
	{
		cvtXXTo32s(row_buf_array[i], row32s, (size_t)width * nr_comp, false);
		cvtToPlanar(row32s, m_planes, width);
		m_planes[0] += stride;
		m_planes[1] += stride;
		m_planes[2] += stride;
		m_planes[3] += stride;
	}
beach:
	if(row_buf_array)
	{
		for(uint32_t i = 0; i < height; ++i)
			free(row_buf_array[i]);
		free(row_buf_array);
	}
	free(row32s);
	if(png)
		png_destroy_read_struct(&png, &m_info, nullptr);
	if(!m_useStdIO && m_fileStream)
	{
		if(!grk::safe_fclose(m_fileStream))
		{
			grk_object_unref(&m_image->obj);
			m_image = nullptr;
		}
	}
	return m_image;
} /* pngtoimage() */

static void user_warning_fn(png_structp png_ptr, png_const_charp message)
{
	(void)png_ptr;
	spdlog::warn("libpng warning: {}", message);
}

static void user_error_fn(png_structp png_ptr, png_const_charp message)
{
	(void)png_ptr;
	spdlog::error("libpng error: {}", message);
}

static void convert_32s16u_C1R(const int32_t* pSrc, uint8_t* pDst, size_t length)
{
	size_t i;
	for(i = 0; i < length; i++)
	{
		uint32_t val = (uint32_t)pSrc[i];
		*pDst++ = (uint8_t)(val >> 8);
		*pDst++ = (uint8_t)val;
	}
}

PNGFormat::PNGFormat()
	: m_info(nullptr), png(nullptr), row_buf(nullptr), row_buf_array(nullptr), row32s(nullptr),
	  m_colorSpace(GRK_CLRSPC_UNKNOWN), prec(0), nr_comp(0), m_planes{nullptr}
{}

bool PNGFormat::encodeHeader(grk_image* img, const std::string& filename, uint32_t compressionLevel)
{
	m_image = img;
	m_fileName = filename;
	uint32_t color_type;
	png_color_8 sig_bit;
	uint32_t i;
	bool fails = true;

	if(grk::isSubsampled(m_image))
	{
		spdlog::error("Sub-sampled images not supported");
		return false;
	}

	memset(&sig_bit, 0, sizeof(sig_bit));

	prec = m_image->comps[0].prec;
	nr_comp = m_image->numcomps;

	if(nr_comp > 4)
	{
		spdlog::warn("imagetopng: number of components {} is "
					 "greater than 4. Truncating to 4",
					 nr_comp);
		nr_comp = 4;
	}
	for(i = 1; i < nr_comp; ++i)
	{
		if(m_image->comps[0].dx != m_image->comps[i].dx)
			break;
		if(m_image->comps[0].dy != m_image->comps[i].dy)
			break;
		if(m_image->comps[0].prec != m_image->comps[i].prec)
			break;
		if(m_image->comps[0].sgnd != m_image->comps[i].sgnd)
			break;
		if(!m_image->comps[i].data)
		{
			spdlog::error("imagetopng: component {} is null.", i);
			return false;
		}
	}
	if(i != nr_comp)
	{
		spdlog::error("imagetopng: All components shall have the same sub-sampling,"
					  " bit depth and sign.");
		return false;
	}
	if(prec > 8 && prec < 16)
	{
		prec = 16;
	}
	else if(prec < 8 && nr_comp > 1)
	{ /* GRAY_ALPHA, RGB, RGB_ALPHA */
		prec = 8;
	}
	else if((prec > 1) && (prec < 8) && ((prec == 6) || ((prec & 1) == 1)))
	{ /* GRAY with non native precision */
		if((prec == 5) || (prec == 6))
			prec = 8;
		else
			prec++;
	}

	if(prec != 1 && prec != 2 && prec != 4 && prec != 8 && prec != 16)
	{
		spdlog::error("imagetopng: can not create {}\n\twrong bit_depth {}", m_fileName.c_str(),
					  prec);
		return false;
	}
	if(!ImageFormat::encodeHeader(m_image, m_fileName, compressionLevel))
		return false;

	m_fileStream = ((FileStreamIO*)m_fileIO)->getFileStream();

	/* Create and initialize the png_struct with the desired error handler
	 * functions.  If you want to use the default stderr and longjump method,
	 * you can supply nullptr for the last three parameters.  We also check that
	 * the library version is compatible with the one used at compile time,
	 * in case we are using dynamically linked libraries.  REQUIRED.
	 */
	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, this, user_error_fn, user_warning_fn);
	if(png == nullptr)
		goto beach;

	// allow Microsoft/HP 3144-byte sRGB profile, normally skipped by library
	// because it deems it broken. (a controversial decision)
	png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);

	// treat some errors as warnings
	png_set_benign_errors(png, 1);

	/* Allocate/initialize the image information data.  REQUIRED
	 */
	m_info = png_create_info_struct(png);

	if(m_info == nullptr)
		goto beach;

	/* Set error handling.  REQUIRED if you are not supplying your own
	 * error handling functions in the png_create_write_struct() call.
	 */
	if(setjmp(png_jmpbuf(png)))
		goto beach;

	/* I/O initialization functions is REQUIRED
	 */
	png_init_io(png, m_fileStream);

	/* Set the image information here.  Width and height are up to 2^31,
	 * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
	 * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
	 * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
	 * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
	 * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
	 * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE.
	 * REQUIRED
	 *
	 * ERRORS:
	 *
	 * color_type == PNG_COLOR_TYPE_PALETTE && bit_depth > 8
	 * color_type == PNG_COLOR_TYPE_RGB && bit_depth < 8
	 * color_type == PNG_COLOR_TYPE_GRAY_ALPHA && bit_depth < 8
	 * color_type == PNG_COLOR_TYPE_RGB_ALPHA) && bit_depth < 8
	 *
	 */
	png_set_compression_level(png,
							  (int)((compressionLevel == GRK_DECOMPRESS_COMPRESSION_LEVEL_DEFAULT)
										? 0
										: compressionLevel));

	if(nr_comp >= 3)
	{ /* RGB(A) */
		color_type = PNG_COLOR_TYPE_RGB;
		sig_bit.red = sig_bit.green = sig_bit.blue = (png_byte)prec;
	}
	else
	{ /* GRAY(A) */
		color_type = PNG_COLOR_TYPE_GRAY;
		sig_bit.gray = (png_byte)prec;
	}
	if((nr_comp & 1) == 0)
	{ /* ALPHA */
		color_type |= PNG_COLOR_MASK_ALPHA;
		sig_bit.alpha = (png_byte)prec;
	}

	png_set_IHDR(png, m_info, m_image->comps[0].w, m_image->comps[0].h, (int32_t)prec,
				 (int32_t)color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
				 PNG_FILTER_TYPE_BASE);

	png_set_sBIT(png, m_info, &sig_bit);
	/* png_set_gamma(png, 2.2, 1./2.2); */
	/* png_set_sRGB(png, info, PNG_sRGB_INTENT_PERCEPTUAL); */

	// Set iCCP chunk
	if(m_image->meta && m_image->meta->color.icc_profile_buf &&
	   m_image->meta->color.icc_profile_len)
	{
		std::string profileName = "Unknown";
		// if lcms is present, try to extract the description tag from the ICC header,
		// and use this tag as the profile name
#if defined(GROK_HAVE_LIBLCMS)
		auto in_prof = cmsOpenProfileFromMem(m_image->meta->color.icc_profile_buf,
											 m_image->meta->color.icc_profile_len);
		if(in_prof)
		{
			cmsUInt32Number bufferSize = cmsGetProfileInfoASCII(
				in_prof, cmsInfoDescription, cmsNoLanguage, cmsNoCountry, nullptr, 0);
			if(bufferSize)
			{
				std::unique_ptr<char[]> description(new char[bufferSize]);
				cmsUInt32Number result =
					cmsGetProfileInfoASCII(in_prof, cmsInfoDescription, cmsNoLanguage, cmsNoCountry,
										   description.get(), bufferSize);
				if(result)
				{
					profileName = description.get();
				}
			}
			cmsCloseProfile(in_prof);
		}
#endif
		png_set_iCCP(png, m_info, profileName.c_str(), PNG_COMPRESSION_TYPE_BASE,
					 m_image->meta->color.icc_profile_buf, m_image->meta->color.icc_profile_len);
	}

	if(m_image->meta && m_image->meta->xmp_buf && m_image->meta->xmp_len)
	{
		png_text txt;
		txt.key = (png_charp) "XML:com.adobe.xmp";
		txt.compression = PNG_ITXT_COMPRESSION_NONE;
		txt.text_length = m_image->meta->xmp_len;
		txt.text = (png_charp)m_image->meta->xmp_buf;
		txt.lang = nullptr;
		txt.lang_key = nullptr;
		png_set_text(png, m_info, &txt, 1);
	}

	if(m_image->capture_resolution[0] > 0 && m_image->capture_resolution[1] > 0)
	{
		png_set_pHYs(png, m_info, (png_uint_32)(m_image->capture_resolution[0]),
					 (png_uint_32)(m_image->capture_resolution[1]), PNG_RESOLUTION_METER);
	}

	// handle libpng errors
	if(setjmp(png_jmpbuf(png)))
	{
		goto beach;
	}

	for(i = 0; i < nr_comp; ++i)
		scale_component(&(m_image->comps[i]), prec);

	png_write_info(png, m_info);

	/* set up conversion */
	{
		size_t rowStride;
		png_size_t png_row_size;

		png_row_size = png_get_rowbytes(png, m_info);
		rowStride = ((size_t)m_image->comps[0].w * (size_t)nr_comp * (size_t)prec + 7U) / 8U;
		if(rowStride != (size_t)png_row_size)
		{
			spdlog::error("Invalid PNG row size");
			goto beach;
		}
		row_buf = (png_bytep)malloc(png_row_size);
		if(row_buf == nullptr)
		{
			spdlog::error("Can't allocate memory for PNG row");
			goto beach;
		}
		row32s = (int32_t*)malloc((size_t)m_image->comps[0].w * (size_t)nr_comp * sizeof(int32_t));
		if(row32s == nullptr)
		{
			spdlog::error("Can't allocate memory for interleaved 32s row");
			goto beach;
		}
	}
	for(uint32_t compno = 0; compno < nr_comp; ++compno)
		m_planes[compno] = m_image->comps[compno].data;

	fails = false;

beach:
	return !fails;
}

bool PNGFormat::encodeStrip(uint32_t rows)
{
	(void)rows;

	cvtPlanarToInterleaved cvtPxToCx = cvtPlanarToInterleaved_LUT[nr_comp];
	cvtFrom32 cvt32sToPack = nullptr;
	png_bytep row_buf_cpy = row_buf;
	int32_t* buffer32s_cpy = row32s;

	switch(prec)
	{
		case 1:
		case 2:
		case 4:
		case 8:
			cvt32sToPack = cvtFrom32_LUT[prec];
			break;
		case 16:
			cvt32sToPack = convert_32s16u_C1R;
			break;
		default:
			/* never here */
			return false;
			break;
	}

	int32_t adjust = m_image->comps[0].sgnd ? 1 << (prec - 1) : 0;
	size_t width = m_image->comps[0].w;
	uint32_t stride = m_image->comps[0].stride;
	uint32_t max = maxY(rows);
	for(uint32_t y = m_rowCount; y < max; ++y)
	{
		cvtPxToCx(m_planes, buffer32s_cpy, width, adjust);
		cvt32sToPack(buffer32s_cpy, row_buf_cpy, width * (size_t)nr_comp);
		png_write_row(png, row_buf_cpy);
		m_planes[0] += stride;
		m_planes[1] += stride;
		m_planes[2] += stride;
		m_planes[3] += stride;
	}
	m_rowCount += rows;

	return true;
}
bool PNGFormat::encodeFinish(void)
{
	if(setjmp(png_jmpbuf(png)))
		return false;

	if(m_rowCount < m_image->comps[0].h)
		spdlog::warn("Full image was not written");

	if(png)
	{
		png_write_end(png, m_info);
		png_destroy_write_struct(&png, &m_info);
	}
	free(row_buf);
	free(row32s);
	bool rc = ImageFormat::encodeFinish();
	m_fileStream = nullptr;
	return rc;
}
grk_image* PNGFormat::decode(const std::string& filename, grk_cparameters* parameters)
{
	return do_decode(filename.c_str(), parameters);
}
