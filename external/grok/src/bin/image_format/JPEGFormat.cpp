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
 */
#include <cstdio>
#include <cstdlib>
#include "grk_apps_config.h"
#include "grok.h"
#include "JPEGFormat.h"
#include "convert.h"
#include "color.h"
#include <cstring>

#ifndef GROK_HAVE_LIBJPEG
#error GROK_HAVE_LIBJPEG_NOT_DEFINED
#endif /* GROK_HAVE_LIBJPEG */

#include <setjmp.h>
#include <cassert>

#include "common.h"
#include "FileStreamIO.h"

struct my_error_mgr
{
	struct jpeg_error_mgr pub; /* "public" fields */

	jmp_buf setjmp_buffer; /* for return to caller */
};

typedef struct my_error_mgr* my_error_ptr;

METHODDEF(void) my_error_exit(j_common_ptr cinfo)
{
	/* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_ptr myerr = (my_error_ptr)cinfo->err;

	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	(*cinfo->err->output_message)(cinfo);

	/* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

/*
 * SOME FINE POINTS:
 *
 * In the code below, we ignored the return value of jpeg_read_scanlines,
 * which is the number of scanlines actually read.  We could get away with
 * this because we asked for only one line at a time and we weren't using
 * a suspending data source.  See libjpeg.txt for more info.
 *
 * We cheated a bit by calling alloc_sarray() after jpeg_start_decompress();
 * we should have done it beforehand to ensure that the space would be
 * counted against the JPEG max_memory setting.  In some systems the above
 * code would risk an out-of-memory error.  However, in general we don't
 * know the output image dimensions before jpeg_start_decompress(), unless we
 * call jpeg_calc_output_dimensions().  See libjpeg.txt for more about this.
 *
 * Scanlines are returned in the same order as they appear in the JPEG file,
 * which is standardly top-to-bottom.  If you must emit data bottom-to-top,
 * you can use one of the virtual arrays provided by the JPEG memory manager
 * to invert the data.  See wrbmp.c for an example.
 *
 * As with compression, some operating modes may require temporary files.
 * On some systems you may need to set up a signal handler to ensure that
 * temporary files are deleted if the program is interrupted.  See libjpeg.txt.
 */

grk_image* JPEGFormat::jpegtoimage(const char* filename, grk_cparameters* parameters)
{
	readFromStdin = grk::useStdio(filename);

	int32_t* planes[3];
	JDIMENSION w = 0, h = 0;
	uint32_t dest_stride;
	int bps = 0, numcomps = 0;
	cvtTo32 cvtJpegTo32s;
	GRK_COLOR_SPACE color_space = GRK_CLRSPC_UNKNOWN;
	grk_image_cmptparm cmptparm[3]; /* mono or RGB */
	cvtInterleavedToPlanar cvtToPlanar;
	JOCTET* icc_data_ptr = nullptr;
	unsigned int icc_data_len = 0;

	/* This struct contains the JPEG decompression parameters and pointers to
	 * working space (which is allocated as needed by the JPEG library).
	 */
	struct jpeg_decompress_struct cinfo;
	/* We use our private extension JPEG error handler.
	 * Note that this struct must live as long as the main JPEG parameter
	 * struct, to avoid dangling-pointer problems.
	 */
	struct my_error_mgr jerr;
	/* More stuff */
	JSAMPARRAY buffer; /* Output row buffer */
	int row_stride; /* physical row width in output buffer */

	/* In this example we want to open the input file before doing anything else,
	 * so that the setjmp() error recovery below can assume the file is open.
	 * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
	 * requires it in order to read binary files.
	 */

	if(readFromStdin)
	{
		if(!grk::grk_set_binary_mode(stdin))
			return nullptr;
		m_fileStream = stdin;
	}
	else
	{
		if((m_fileStream = fopen(filename, "rb")) == nullptr)
		{
			spdlog::error("can't open {}", filename);
			return 0;
		}
	}

	/* Step 1: allocate and initialize JPEG decompression object */

	/* We set up the normal JPEG error routines, then override error_exit. */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if(setjmp(jerr.setjmp_buffer))
	{
		success = false;
		goto cleanup;
	}
	/* Now we can initialize the JPEG decompression object. */
	jpeg_create_decompress(&cinfo);
	setup_read_icc_profile(&cinfo);

	/* Step 2: specify data source (eg, a file) */
	jpeg_stdio_src(&cinfo, m_fileStream);

	/* Step 3: read file parameters with jpeg_read_header() */

	(void)jpeg_read_header(&cinfo, TRUE);
	/* We can ignore the return value from jpeg_read_header since
	 *   (a) suspension is not possible with the stdio data source, and
	 *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
	 * See libjpeg.txt for more info.
	 */

	// read ICC profile
	if(!read_icc_profile(&cinfo, &icc_data_ptr, &icc_data_len))
	{
		spdlog::warn("jpegtoimage: Failed to read ICC profile");
	}

	/* Step 4: set parameters for decompression */

	/* In this example, we don't need to change any of the defaults set by
	 * jpeg_read_header(), so we do nothing here.
	 */

	/* Step 5: Start decompressor */

	(void)jpeg_start_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
	 * with the stdio data source.
	 */

	bps = cinfo.data_precision;
	if(bps != 8)
	{
		spdlog::error("jpegtoimage: Unsupported m_image precision {}", bps);
		success = false;
		goto cleanup;
	}

	numcomps = cinfo.output_components;
	w = cinfo.image_width;
	h = cinfo.image_height;
	cvtJpegTo32s = cvtTo32_LUT[bps];
	memset(&cmptparm[0], 0, 3 * sizeof(grk_image_cmptparm));
	cvtToPlanar = cvtInterleavedToPlanar_LUT[numcomps];

	if(cinfo.output_components == 3)
		color_space = GRK_CLRSPC_SRGB;
	else
		color_space = GRK_CLRSPC_GRAY;

	for(int j = 0; j < cinfo.output_components; j++)
	{
		cmptparm[j].prec = (uint8_t)bps;
		cmptparm[j].dx = 1;
		cmptparm[j].dy = 1;
		cmptparm[j].w = w;
		cmptparm[j].h = h;
	}

	m_image = grk_image_new((uint16_t)numcomps, &cmptparm[0], color_space, true);
	if(!m_image)
	{
		success = false;
		goto cleanup;
	}
	if(icc_data_ptr && icc_data_len)
	{
		if(grk::validate_icc(color_space, icc_data_ptr, icc_data_len))
		{
			grk::copy_icc(m_image, icc_data_ptr, icc_data_len);
			icc_data_len = 0;
		}
		else
		{
			spdlog::warn("ICC profile does not match underlying colour space. Ignoring");
		}
	}
	if(icc_data_ptr)
		free(icc_data_ptr);
	icc_data_ptr = nullptr;
	/* set m_image offset and reference grid */
	m_image->x0 = parameters->image_offset_x0;
	m_image->x1 = !m_image->x0 ? (w - 1) * 1 + 1 : m_image->x0 + (w - 1) * 1 + 1;
	if(m_image->x1 <= m_image->x0)
	{
		spdlog::error("jpegtoimage: Bad value for m_image->x1({}) vs. "
					  "m_image->x0({}).",
					  m_image->x1, m_image->x0);
		success = false;
		goto cleanup;
	}

	m_image->y0 = parameters->image_offset_y0;
	m_image->y1 = !m_image->y0 ? (h - 1) * 1 + 1 : m_image->y0 + (h - 1) * 1 + 1;

	if(m_image->y1 <= m_image->y0)
	{
		spdlog::error("jpegtoimage: Bad value for m_image->y1({}) vs. "
					  "m_image->y0({}).",
					  m_image->y1, m_image->y0);
		success = false;
		goto cleanup;
	}

	for(int j = 0; j < numcomps; j++)
	{
		planes[j] = m_image->comps[j].data;
	}

	buffer32s = new int32_t[w * (size_t)numcomps];

	/* We may need to do some setup of our own at this point before reading
	 * the data.  After jpeg_start_decompress() we have the correct scaled
	 * output m_image dimensions available, as well as the output colormap
	 * if we asked for color quantization.
	 * In this example, we need to make an output work buffer of the right size.
	 */
	/* JSAMPLEs per row in output buffer */
	row_stride = (int)cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with m_image */
	buffer =
		(*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, (JDIMENSION)row_stride, 1);
	if(!buffer)
	{
		success = false;
		goto cleanup;
	}

	/* Step 6: while (scan lines remain to be read) */
	/*           jpeg_read_scanlines(...); */

	/* Here we use the library's state variable cinfo.output_scanline as the
	 * loop counter, so that we don't have to keep track ourselves.
	 */
	dest_stride = m_image->comps[0].stride;
	while(cinfo.output_scanline < cinfo.output_height)
	{
		/* jpeg_read_scanlines expects an array of pointers to scanlines.
		 * Here the array is only one element long, but you could ask for
		 * more than one scanline at a time if that's more convenient.
		 */
		(void)jpeg_read_scanlines(&cinfo, buffer, 1);

		// convert 8 bit buffer to 32 bit buffer
		cvtJpegTo32s(buffer[0], buffer32s, (size_t)w * (size_t)numcomps, false);

		// convert to planar
		cvtToPlanar(buffer32s, planes, (size_t)w);

		planes[0] += dest_stride;
		planes[1] += dest_stride;
		planes[2] += dest_stride;
	}

	/* Step 7: Finish decompression */

	(void)jpeg_finish_decompress(&cinfo);
/* We can ignore the return value since suspension is not possible
 * with the stdio data source.
 */
cleanup:
	if(icc_data_ptr)
		free(icc_data_ptr);
	jpeg_destroy_decompress(&cinfo);

	delete[] buffer32s;

	/* At this point you may want to check to see whether any corrupt-data
	 * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
	 */
	if(jerr.pub.num_warnings != 0)
	{
		spdlog::warn("JPEG library reported {} of corrupt data warnings",
					 (int)jerr.pub.num_warnings);
	}

	if(!success)
	{
		grk_object_unref(&m_image->obj);
		m_image = nullptr;
	}
	/* After finish_decompress, we can close the input file.
	 * Here we postpone it until after no more JPEG errors are possible,
	 * so as to simplify the setjmp error logic above.  (Actually, I don't
	 * think that jpeg_destroy can do an error exit, but why assume anything...)
	 */
	if(m_fileStream && !readFromStdin)
	{
		if(!grk::safe_fclose(m_fileStream))
		{
			grk_object_unref(&m_image->obj);
			m_image = nullptr;
		}
	}
	return m_image;
} /* jpegtoimage() */

JPEGFormat::JPEGFormat(void)
	: success(true), buffer(nullptr), buffer32s(nullptr), color_space(JCS_UNKNOWN), adjust(0),
	  readFromStdin(false), planes{0, 0, 0}, cvtPxToCx(nullptr), cvtTo8bpp(nullptr)
{}

bool JPEGFormat::encodeHeader(grk_image* image, const std::string& filename,
							  uint32_t compressionParam)
{
	if(!image)
		return false;
	m_image = image;
	int32_t firstAlpha = -1;
	size_t numAlphaChannels = 0;
	uint32_t numcomps = m_image->numcomps;
	uint32_t sgnd = m_image->comps[0].sgnd;
	adjust = sgnd ? 1 << (m_image->comps[0].prec - 1) : 0;
	uint32_t width = m_image->comps[0].w;

	// actual bits per sample
	uint8_t prec = m_image->comps[0].prec;
	uint32_t i = 0;

	struct my_error_mgr jerr;

	/* Step 1: allocate and initialize JPEG compression object */

	/* We have to set up the error handler first, in case the initialization
	 * step fails.  (Unlikely, but it could happen if you are out of memory.)
	 * This routine fills in the contents of struct jerr, and returns jerr's
	 * address which we place into the link field in cinfo.
	 */

	/* This struct represents a JPEG error handler.  It is declared separately
	 * because applications often want to supply a specialized error handler
	 * (see the second half of this file for an example).  But here we just
	 * take the easy way out and use the standard error handler, which will
	 * print a message on stderr and return 1 if compression fails.
	 * Note that this struct must live as long as the main JPEG parameter
	 * struct, to avoid dangling-pointer problems.
	 */
	JDIMENSION image_width = m_image->x1 - m_image->x0; /* input m_image width */
	JDIMENSION image_height = m_image->y1 - m_image->y0; /* input m_image height */

	switch(m_image->color_space)
	{
		case GRK_CLRSPC_SRGB: /**< sRGB */
			color_space = JCS_RGB;
			break;
		case GRK_CLRSPC_GRAY: /**< grayscale */
			color_space = JCS_GRAYSCALE;
			break;
		case GRK_CLRSPC_SYCC: /**< YUV */
			color_space = JCS_YCbCr;
			break;
		case GRK_CLRSPC_EYCC: /**< e-YCC */
			color_space = JCS_YCCK;
			break;
		case GRK_CLRSPC_CMYK: /**< CMYK */
			color_space = JCS_CMYK;
			break;
		default:
			if(numcomps == 3)
				color_space = JCS_RGB;
			else if(numcomps == 1)
				color_space = JCS_GRAYSCALE;
			else
			{
				spdlog::error("imagetojpeg: colour space must be "
							  "either RGB or Grayscale");
				return false;
			}
			break;
	}

	if(m_image->numcomps > 4)
	{
		spdlog::error("imagetojpeg: number of components {} "
					  "is greater than 4.",
					  m_image->numcomps);
		return false;
	}

	planes[0] = m_image->comps[0].data;
	if(prec == 0)
	{
		spdlog::error("imagetojpeg: m_image precision is zero.");
		return false;
	}

	// check for null m_image components
	for(uint32_t i = 0; i < numcomps; ++i)
	{
		auto comp = m_image->comps[i];
		if(!comp.data)
		{
			spdlog::error("imagetojpeg: component {} is null.", i);
			return false;
		}
	}
	for(i = 1U; i < numcomps; ++i)
	{
		if(m_image->comps[0].dx != m_image->comps[i].dx)
			break;
		if(m_image->comps[0].dy != m_image->comps[i].dy)
			break;
		if(m_image->comps[0].prec != m_image->comps[i].prec)
			break;
		if(m_image->comps[0].sgnd != m_image->comps[i].sgnd)
			break;
		planes[i] = m_image->comps[i].data;
	}
	if(i != numcomps)
	{
		spdlog::error(
			"imagetojpeg: All components shall have the same subsampling, same bit depth.");
		return false;
	}

	if(prec < 8 && numcomps > 1)
	{ /* GRAY_ALPHA, RGB, RGB_ALPHA */
		for(i = 0; i < numcomps; ++i)
			scale_component(&(m_image->comps[i]), 8);
		prec = 8;
	}
	else if((prec > 1) && (prec < 8) && ((prec == 6) || ((prec & 1) == 1)))
	{ /* GRAY with non native precision */
		if((prec == 5) || (prec == 6))
			prec = 8;
		else
			prec++;
		for(i = 0; i < numcomps; ++i)
			scale_component(&(m_image->comps[i]), prec);
	}

	if(prec != 1 && prec != 2 && prec != 4 && prec != 8)
	{
		spdlog::error("imagetojpeg: can not create {}\n\twrong bit_depth {}", filename, prec);
		return false;
	}

	cvtPxToCx = cvtPlanarToInterleaved_LUT[numcomps];
	cvtTo8bpp = cvtFrom32_LUT[prec];

	// Alpha channels
	for(i = 0U; i < numcomps; ++i)
	{
		if(m_image->comps[i].type)
		{
			if(firstAlpha == -1)
				firstAlpha = 0;
			numAlphaChannels++;
		}
	}
	// We assume that alpha channels occur as last channels in m_image.
	if(numAlphaChannels && ((uint32_t)firstAlpha + numAlphaChannels >= numcomps))
	{
		spdlog::warn("PNG requires that alpha channels occur"
					 " as last channels in m_image.");
		numAlphaChannels = 0;
	}
	buffer = new uint8_t[width * numcomps];
	buffer32s = new int32_t[width * numcomps];

	/* We set up the normal JPEG error routines, then override error_exit. */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if(setjmp(jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		jpeg_destroy_compress(&cinfo);
		return false;
	}
	/* Now we can initialize the JPEG compression object. */
	jpeg_create_compress(&cinfo);

	/* Step 2: specify data destination (eg, a file) */
	/* Note: steps 2 and 3 can be done in either order. */

	/* Here we use the library-supplied code to send compressed data to a
	 * stdio stream.  You can also write your own code to do something else.
	 * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
	 * requires it in order to write binary files.
	 */
	if(!ImageFormat::encodeHeader(m_image, filename, compressionParam))
		return false;

	m_fileStream = ((FileStreamIO*)m_fileIO)->getFileStream();
	jpeg_stdio_dest(&cinfo, m_fileStream);

	/* Step 3: set parameters for compression */

	/* First we supply a description of the input m_image.
	 * Four fields of the cinfo struct must be filled in:
	 */
	cinfo.image_width = image_width; /* m_image width and height, in pixels */
	cinfo.image_height = image_height;
	cinfo.input_components = (int)numcomps; /* # of color components per pixel */
	cinfo.in_color_space = color_space; /* colorspace of input m_image */

	/* Now use the library's routine to set default compression parameters.
	 * (You must set at least cinfo.in_color_space before calling this,
	 * since the defaults depend on the source color space.)
	 */
	jpeg_set_defaults(&cinfo);

	/* Now you can set any non-default parameters you wish to.
	 * Here we just illustrate the use of quality (quantization table) scaling:
	 */
	jpeg_set_quality(&cinfo,
					 (int)((compressionParam == GRK_DECOMPRESS_COMPRESSION_LEVEL_DEFAULT)
							   ? 90
							   : compressionParam),
					 (boolean)TRUE /* limit to baseline-JPEG values */);

	// set resolution
	if(m_image->capture_resolution[0] > 0 && m_image->capture_resolution[1] > 0)
	{
		cinfo.density_unit = 2; // dots per cm
		cinfo.X_density = (uint16_t)(m_image->capture_resolution[0] / 100.0 + 0.5);
		cinfo.Y_density = (uint16_t)(m_image->capture_resolution[1] / 100.0 + 0.5);
	}

	/* Step 4: Start compressor */

	/* TRUE ensures that we will write a complete interchange-JPEG file.
	 * Pass TRUE unless you are very sure of what you're doing.
	 */
	jpeg_start_compress(&cinfo, (boolean)TRUE);
	if(m_image->meta && m_image->meta->color.icc_profile_buf)
	{
		write_icc_profile(&cinfo, m_image->meta->color.icc_profile_buf,
						  m_image->meta->color.icc_profile_len);
	}

	return true;
}
bool JPEGFormat::encodeStrip(uint32_t rows)
{
	(void)rows;

	/* Step 5: while (scan lines remain to be written) */
	/*           jpeg_write_scanlines(...); */

	/* Here we use the library's state variable cinfo.next_scanline as the
	 * loop counter, so that we don't have to keep track ourselves.
	 * To keep things simple, we pass one scanline per call; you can pass
	 * more if you wish, though.
	 */

	while(cinfo.next_scanline < cinfo.image_height)
	{
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
		 * Here the array is only one element long, but you could pass
		 * more than one scanline at a time if that's more convenient.
		 */
		uint32_t stride = m_image->comps[0].stride;
		cvtPxToCx(planes, buffer32s, (size_t)m_image->comps[0].w, adjust);
		cvtTo8bpp(buffer32s, (uint8_t*)buffer, (size_t)m_image->comps[0].w * m_image->numcomps);
		JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
		row_pointer[0] = buffer;
		planes[0] += stride;
		planes[1] += stride;
		planes[2] += stride;
		(void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	return true;
}
bool JPEGFormat::encodeFinish(void)
{
	/* Step 6: Finish compression */
	jpeg_finish_compress(&cinfo);

	/* Step 7: release JPEG compression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_compress(&cinfo);

	delete[] buffer;
	delete[] buffer32s;

	/* After finish_compress, we can close the output file. */
	return ImageFormat::encodeFinish() && success;
}

grk_image* JPEGFormat::decode(const std::string& filename, grk_cparameters* parameters)
{
	return jpegtoimage(filename.c_str(), parameters);
}
