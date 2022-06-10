/*
 * This file is part of vitaShaRK
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
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
#include <psp2/kernel/modulemgr.h>
#include <psp2/shacccg.h>
#include <shacccg_ext.h>

// Default path for SceShaccCg module location
#define DEFAULT_SHACCCG_PATH "ur0:/data/libshacccg.suprx"

static void (*shark_log_cb)(const char *msg, shark_log_level msg_level, int line) = NULL;
static shark_warn_level shark_warnings_level = SHARK_WARN_SILENT;

static SceUID shark_module_id = 0;
static uint8_t shark_initialized = 0;
static const SceShaccCgCompileOutput *shark_output = NULL;
static SceShaccCgSourceFile shark_input;
static SceShaccCgCallbackList shark_callbacks;
static SceShaccCgCompileOptions shark_options;

static void *(*shark_malloc)(size_t size) = malloc;
static void (*shark_free)(void *ptr) = free;

// Dummy Open File callback
static SceShaccCgSourceFile *shark_open_file_cb(const char *fileName,
	const SceShaccCgSourceLocation *includedFrom,
	const SceShaccCgCompileOptions *compileOptions,
	const char **errorString)
{
	return &shark_input;
}

void shark_set_allocators(void *(*malloc_func)(size_t size), void (*free_func)(void *ptr)) {
	shark_malloc = malloc_func;
	shark_free = free_func;
}

int shark_init(const char *path) {
	// Initializing sceShaccCg module
	if (!shark_initialized) {
		shark_module_id = sceKernelLoadStartModule(path ? path : DEFAULT_SHACCCG_PATH, 0, NULL, 0, NULL, NULL);
		if (shark_module_id < 0) return shark_module_id;
		sceShaccCgExtEnableExtensions();
		sceShaccCgSetDefaultAllocator(shark_malloc, shark_free);
		sceShaccCgInitializeCallbackList(&shark_callbacks, SCE_SHACCCG_TRIVIAL);
		shark_callbacks.openFile = shark_open_file_cb;
		shark_initialized = 1;
	}
	return 0;
}

void shark_end() {
	if (!shark_initialized) return;
	
	// Terminating sceShaccCg module
	sceShaccCgReleaseCompiler();
	sceShaccCgExtDisableExtensions();
	sceKernelStopUnloadModule(shark_module_id, 0, NULL, 0, NULL, NULL);
	shark_initialized = 0;
}

void shark_install_log_cb(void (*cb)(const char *msg, shark_log_level msg_level, int line)) {
	shark_log_cb = cb;
}

void shark_set_warnings_level(shark_warn_level level) {
	// Changing current warnings level
	shark_warnings_level = level;
}

void shark_clear_output() {
	// Clearing sceShaccCg output
	if (shark_output) {
		sceShaccCgDestroyCompileOutput(shark_output);
		shark_output = NULL;
	}
}

SceGxmProgram *shark_compile_shader_extended(const char *src, uint32_t *size, shark_type type, shark_opt opt, int32_t use_fastmath, int32_t use_fastprecision, int32_t use_fastint) {
	if (!shark_initialized) return NULL;
	
	// Forcing usage for memory source for the shader to compile
	shark_input.fileName = "<built-in>";
	shark_input.text = src;
	shark_input.size = *size;
	
	// Properly configuring SceShaccCg with requested settings
	sceShaccCgInitializeCompileOptions(&shark_options);
	shark_options.mainSourceFile = shark_input.fileName;
	shark_options.targetProfile = type;
	shark_options.entryFunctionName = "main";
	shark_options.macroDefinitions = NULL;
	shark_options.useFx = 1;
	shark_options.warningLevel = shark_warnings_level;
	shark_options.optimizationLevel = opt;
	shark_options.useFastmath = use_fastmath;
	shark_options.useFastint = use_fastint;
	shark_options.useFastprecision = use_fastprecision;
	shark_options.pedantic = shark_warnings_level == SHARK_WARN_MAX ? SHARK_ENABLE : SHARK_DISABLE;
	shark_options.performanceWarnings = shark_warnings_level > SHARK_WARN_SILENT ? SHARK_ENABLE : SHARK_DISABLE;
	
	shark_output = sceShaccCgCompileProgram(&shark_options, &shark_callbacks, 0);
	// Executing logging
	if (shark_log_cb) {
		for (int i = 0; i < shark_output->diagnosticCount; ++i) {
			const SceShaccCgDiagnosticMessage *log = &shark_output->diagnostics[i];
			shark_log_cb(log->message, log->level, log->location->lineNumber);
		}
	}
	
	// Returning output
	if (shark_output->programData) *size = shark_output->programSize;
	return (SceGxmProgram *)shark_output->programData;
}

SceGxmProgram *shark_compile_shader(const char *src, uint32_t *size, shark_type type) {
	return shark_compile_shader_extended(src, size, type, SHARK_OPT_DEFAULT, SHARK_DISABLE, SHARK_DISABLE, SHARK_DISABLE);
}
