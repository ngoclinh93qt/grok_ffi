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
 */

#include "Plugin.h"
#include <plugin/plugin_interface.h>

static const char* PluginId = "SamplePlugin";

extern "C" PLUGIN_API int32_t exit_func()
{
	return 0;
}

extern "C" PLUGIN_API void* create(grk::minpf_object_params* params)
{
	return 0;
}

extern "C" PLUGIN_API int32_t destroy(void* object)
{
	return 0;
}

extern "C" PLUGIN_API grk::minpf_exit_func
	minpf_post_load_plugin(const char* pluginPath, const grk::minpf_platform_services* params)
{
	int res = 0;

	grk::minpf_register_params rp;
	rp.version.major = 1;
	rp.version.minor = 0;

	rp.createFunc = create;
	rp.destroyFunc = destroy;

	res = params->registerObject(PluginId, &rp);
	if(res < 0)
		return nullptr;

	// custom plugin initialization can happen here
	// printf("Loaded plugin has been registered\n");
	return exit_func;
}

///////////////////////////////////
// Initialization
//////////////////////////////////

extern "C" PLUGIN_API bool plugin_init(grk_plugin_init_info initInfo)
{
	return false;
}

////////////////////////////////////
// Encoder Interface Implementation
////////////////////////////////////

extern "C" PLUGIN_API int32_t plugin_encode(grk_cparameters* compress_parameters,
											grk::PLUGIN_ENCODE_USER_CALLBACK userCallback)
{
	return -1;
}

extern "C" PLUGIN_API int32_t plugin_batch_encode(const char* input_dir, const char* output_dir,
												  grk_cparameters* compress_parameters,
												  grk::PLUGIN_ENCODE_USER_CALLBACK userCallback)
{
	return -1;
}

extern "C" PLUGIN_API bool plugin_is_batch_complete(void)
{
	return true;
}

extern "C" PLUGIN_API void plugin_stop_batch_encode(void) {}

////////////////////////////////////
// Decoder Interface Implementation
////////////////////////////////////

extern "C" PLUGIN_API int32_t plugin_decompress(grk_decompress_parameters* decompress_parameters,
												grk::PLUGIN_DECODE_USER_CALLBACK userCallback)
{
	return -1;
}

extern "C" PLUGIN_API int32_t plugin_init_batch_decompress(
	const char* input_dir, const char* output_dir, grk_decompress_parameters* decompress_parameters,
	grk::PLUGIN_DECODE_USER_CALLBACK userCallback)
{
	return -1;
}

extern "C" PLUGIN_API int32_t plugin_batch_decompress(void)
{
	return -1;
}
extern "C" PLUGIN_API void plugin_stop_batch_decompress(void) {}

//////////////////////////////////
// Debug Interface
/////////////////////////////////

extern "C" PLUGIN_API uint32_t plugin_get_debug_state(void)
{
	return GRK_PLUGIN_STATE_NO_DEBUG;
}

extern "C" PLUGIN_API void plugin_debug_next_cxd(grk::grk_plugin_debug_mqc* mqc, uint32_t d) {}

extern "C" PLUGIN_API void plugin_debug_mqc_next_plane(grk::grk_plugin_debug_mqc* mqc) {}
