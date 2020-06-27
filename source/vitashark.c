/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 * Copyright 2020 Asakura Reiko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "vitashark.h"
#include <stdlib.h>
#include <psp2/shacccg.h>

#define DEFAULT_SHACCCG_PATH "ur0:/data/libshacccg.suprx"

static SceUID shark_module_id = 0;
static uint8_t shark_initialized = 0;
static SceShaccCgCompileOutput *shark_output = NULL;
static SceShaccCgSourceFile shark_input;

static SceShaccCgSourceFile *shark_open_file_cb(const char *fileName,
	const SceShaccCgSourceLocation *includedFrom,
	const SceShaccCgCompileOptions *compileOptions,
	const char **errorString)
{
	return &shark_input;
}

int shark_init(const char *path) {
	if (!shark_initialized) {
		shark_module_id = sceKernelLoadStartModule(path ? path : DEFAULT_SHACCCG_PATH, 0, NULL, 0, NULL, NULL);
		if (shark_module_id < 0) return -1;
		sceShaccCgSetDefaultAllocator(malloc, free);
		shark_initialized = 1;
	}
	return 0;
}

void shark_end() {
	if (!shark_initialized) return;
	
	sceKernelStopUnloadModule(shark_module_id, 0, NULL, 0, NULL, NULL);
	shark_initialized = 0;
}

void shark_clear_output() {
	if (shark_output) {
		sceShaccCgDestroyCompileOutput(shark_output);
		shark_output = NULL;
	}
}

SceGxmProgram *shark_compile_shader_extended(const char *src, uint32_t *size, shark_type type, shark_opt opt, int32_t use_fastmath, int32_t use_fastprecision, int32_t use_fastint) {
	if (!shark_initialized) return NULL;
	
	shark_input.fileName = "<built-in>";
	shark_input.text = src;
	shark_input.size = *size;
	
	SceShaccCgCompileOptions options = {0};
	options.mainSourceFile = shark_input.fileName;
	options.targetProfile = type;
	options.entryFunctionName = "main";
	options.macroDefinitions = NULL;
	options.useFx = 1;
	options.optimizationLevel = opt;
	options.useFastmath = use_fastmath;
	options.useFastint = use_fastint;
	options.useFastprecision = use_fastprecision;
	
	SceShaccCgCallbackList callbacks = {0};
	sceShaccCgInitializeCallbackList(&callbacks, SCE_SHACCCG_TRIVIAL);
	callbacks.openFile = shark_open_file_cb;
	const SceShaccCgCompileOutput *shark_output = sceShaccCgCompileProgram(&options, &callbacks, 0);
	
	if (shark_output->programData) *size = shark_output->programSize;
	
	return (SceGxmProgram *)shark_output->programData;
}

SceGxmProgram *shark_compile_shader(const char *src, uint32_t *size, shark_type type) {
	return shark_compile_shader_extended(src, size, type, SHARK_OPT_DEFAULT, SHARK_DISABLE, SHARK_DISABLE, SHARK_DISABLE);
}
