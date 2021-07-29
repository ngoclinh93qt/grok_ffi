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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "spdlog/spdlog.h"
#define TCLAP_NAMESTARTSTRING "-"
#include "tclap/CmdLine.h"
#include <string>
#include "test_common.h"

typedef struct test_cmp_parameters {
	/**  */
	char *base_filename;
	/**  */
	char *test_filename;
} test_cmp_parameters;

/*******************************************************************************
 * Command line help function
 *******************************************************************************/
static void compare_raw_files_help_display(void) {
	fprintf(stdout,
			"\nList of parameters for the compare_raw_files function  \n");
	fprintf(stdout, "\n");
	fprintf(stdout,
			"  -b \t REQUIRED \t filename to the reference/baseline RAW image \n");
	fprintf(stdout, "  -t \t REQUIRED \t filename to the test RAW image\n");
	fprintf(stdout, "\n");
}

/*******************************************************************************
 * Parse command line
 *******************************************************************************/
static int parse_cmdline_cmp(int argc, char **argv,
		test_cmp_parameters *param) {
	size_t sizemembasefile, sizememtestfile;
	int index = 0;

	/* Init parameters*/
	param->base_filename = nullptr;
	param->test_filename = nullptr;
	try {
		TCLAP::CmdLine cmd("compare_raw_files command line", ' ', "");

		TCLAP::ValueArg<std::string> baseArg("b", "base", "base file", false, "", "string",
				cmd);

		TCLAP::ValueArg<std::string> testArg("t", "test", "test file", false, "", "string",
				cmd);

		cmd.parse(argc, argv);

		if (baseArg.isSet()) {
			sizemembasefile = baseArg.getValue().length() + 1;
			param->base_filename = (char*) malloc(sizemembasefile);
			if (!param->base_filename) {
				spdlog::error("Out of memory");
				return 1;
			}
			strcpy(param->base_filename, baseArg.getValue().c_str());
			/*printf("param->base_filename = %s [%u / %u]\n", param->base_filename, strlen(param->base_filename), sizemembasefile );*/
			index++;
		}

		if (testArg.isSet()) {
			sizememtestfile = testArg.getValue().length() + 1;
			param->test_filename = (char*) malloc(sizememtestfile);
			if (!param->test_filename) {
				spdlog::error("Out of memory");
				return 1;
			}
			strcpy(param->test_filename, testArg.getValue().c_str());
			/*printf("param->test_filename = %s [%u / %u]\n", param->test_filename, strlen(param->test_filename), sizememtestfile);*/
			index++;
		}


	} catch (TCLAP::ArgException &e)  // catch any exceptions
	{
		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
		return 0;
	}
	return index;
}

/*******************************************************************************
 * MAIN
 *******************************************************************************/
int main(int argc, char **argv) {
#ifndef NDEBUG
	std::string out;
	for (int i = 0; i < argc; ++i) {
		out += std::string(" ") + argv[i];
	}
	out += "\n";
	printf("%s", out.c_str());
#endif

	int pos = 0;
	test_cmp_parameters inParam;
	FILE *file_test = nullptr, *file_base = nullptr;
	unsigned char equal = 0U; /* returns error by default */

	/* Get parameters from command line*/
	if (parse_cmdline_cmp(argc, argv, &inParam) == 1) {
		compare_raw_files_help_display();
		goto cleanup;
	}

#ifdef COPY_TEST_FILES_TO_REPO
	rename(inParam.test_filename, inParam.base_filename);
#endif

	file_test = fopen(inParam.test_filename, "rb");
	if (!file_test) {
		fprintf(stderr, "Failed to open %s for reading !!\n",
				inParam.test_filename);
		goto cleanup;
	}

	file_base = fopen(inParam.base_filename, "rb");
	if (!file_base) {
		fprintf(stderr, "Failed to open %s for reading !!\n",
				inParam.base_filename);
		goto cleanup;
	}

	/* Read simultaneously the two files*/
	equal = 1U;
	while (equal) {
		unsigned char value_test = 0;
		unsigned char eof_test = 0;
		unsigned char value_base = 0;
		unsigned char eof_base = 0;

		/* Read one byte*/
		if (!fread(&value_test, 1, 1, file_test)) {
			eof_test = 1;
		}

		/* Read one byte*/
		if (!fread(&value_base, 1, 1, file_base)) {
			eof_base = 1;
		}

		/* End of file reached by the two files?*/
		if (eof_test && eof_base)
			break;

		/* End of file reached only by one file?*/
		if (eof_test || eof_base) {
			fprintf(stdout, "Files have different sizes.\n");
			equal = 0;
		}

		/* Binary values are equal?*/
		if (value_test != value_base) {
			fprintf(stdout,
					"Binary values read in the file are different %x vs %x at position %u.\n",
					value_test, value_base, pos);
			equal = 0;
		}
		pos++;
	}

	if (equal)
		fprintf(stdout, "---- TEST SUCCEED: Files are equal ----\n");
	cleanup: if (file_test)
		fclose(file_test);
	if (file_base)
		fclose(file_base);

	/* Free Memory */
	free(inParam.base_filename);
	free(inParam.test_filename);

	return equal ? EXIT_SUCCESS : EXIT_FAILURE;
}
