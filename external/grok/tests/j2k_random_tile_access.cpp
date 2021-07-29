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

#include "grk_config.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif /* _WIN32 */

static int32_t test_tile( uint16_t tile_index, grk_image *image, grk_stream *stream,
								grk_codec *codec) {
	spdlog::info("Decompressing tile {} ...", tile_index);
	if(!grk_decompress_tile(codec, tile_index )){
		spdlog::error("random tile processor: failed to decompress tile {}", tile_index);
		return EXIT_FAILURE;
	}
  for(uint32_t index = 0; index < image->numcomps; ++index) {
    if( image->comps[index].data == nullptr ){
    	spdlog::error("random tile processor: failed to decompress tile {}", tile_index);
        return EXIT_FAILURE;
        }
  }
  spdlog::info("Tile {} decoded successfully", tile_index);
  return EXIT_SUCCESS;
}

int32_t main(int argc, char **argv) {
	grk_dparameters parameters; /* decompression parameters */
	int32_t ret = EXIT_FAILURE, rc;

	if (argc != 2) {
		spdlog::error("Usage: {} <input_file>", argv[0]);
		return EXIT_FAILURE;
	}

	grk_initialize(nullptr, 0);
	grk_set_info_handler(grk::infoCallback, nullptr);
	grk_set_warning_handler(grk::warningCallback, nullptr);
	grk_set_error_handler(grk::errorCallback, nullptr);
	grk_decompress_set_default_params(&parameters);
	strncpy(parameters.infile, argv[1], GRK_PATH_LEN - 1);

	if (!grk::jpeg2000_file_format(parameters.infile,
			&parameters.decod_format)) {
		spdlog::error("Failed to detect JPEG 2000 file format for file {}",
				parameters.infile);
		return EXIT_FAILURE;
	}

	for (uint32_t i = 0; i < 4; ++i){
		grk_codec *codec = nullptr; /* Handle to a decompressor */
		grk_image *image = nullptr;

		/* Index of corner tiles */
		uint16_t tile[4];

		auto stream = grk_stream_create_file_stream(parameters.infile, 1024 * 1024, 1);
		if (!stream) {
			spdlog::error("failed to create a stream from file {}",
					parameters.infile);
			return EXIT_FAILURE;
		}
		switch (parameters.decod_format) {
		case GRK_J2K_FMT: { /* JPEG-2000 codestream */
			codec = grk_decompress_create(GRK_CODEC_J2K, stream);
			break;
		}
		case GRK_JP2_FMT: { /* JPEG 2000 compressed image data */
			codec = grk_decompress_create(GRK_CODEC_JP2, stream);
			break;
		}
		default:
			spdlog::error(
					"Unrecognized format for input {} [accept only *.j2k, *.jp2 or *.jpc]",
					parameters.infile);
			return EXIT_FAILURE;
		}

		/* Set up the decompress parameters using user parameters */
		if (!grk_decompress_init(codec, &parameters)) {
			spdlog::error("random tile processor: failed to set up decompressor");
			goto cleanup;
		}

		/* Read the main header of the codestream and if necessary the JP2 boxes*/
		grk_header_info headerInfo;
		if (!grk_decompress_read_header(codec, &headerInfo)) {
			spdlog::error("randome tile processor : failed to read header");
			goto cleanup;
		}

		spdlog::info("The file contains {}x{} tiles", headerInfo.t_grid_width,
				headerInfo.t_grid_height);

		tile[0] = 0;
		tile[1] = (uint16_t) (headerInfo.t_grid_width - 1);
		tile[2] = (uint16_t) (headerInfo.t_grid_width * headerInfo.t_grid_height - 1);
		tile[3] = (uint16_t) (tile[2] - headerInfo.t_grid_width);

		image = grk_decompress_get_composited_image(codec);
		rc = test_tile(tile[i], image, stream, codec);

		grk_object_unref(codec);
		grk_object_unref(stream);
		if (rc)
			goto cleanup;
	}
	ret = EXIT_SUCCESS;
cleanup:
	grk_deinitialize();

	return ret;
}
