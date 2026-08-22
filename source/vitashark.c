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
#include <stdio.h>
#include <vitasdk.h>

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#define XXH_NAMESPACE SHARK_
#include "xxhash.h"

// Settings
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
static SceShaccCgLocale shark_locale_mode = SCE_SHACCCG_ENGLISH;
static uint8_t shark_is_simple = 0;
static char shark_association_path[256] = {};
static char shark_sdb_body[256 * 1024] = {};

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
		if (shark_module_id < 0)
			return shark_module_id;
		sceShaccCgExtEnableExtensions();
		sceShaccCgSetDefaultAllocator(shark_malloc, shark_free);
		sceShaccCgInitializeCallbackList(&shark_callbacks, SCE_SHACCCG_TRIVIAL);
		shark_callbacks.openFile = shark_open_file_cb;
		shark_initialized = 1;
		shark_is_simple = 0;
	}
	return 0;
}

int shark_init_simple(const char *path) {
	// Initializing sceShaccCg module
	if (!shark_initialized) {
		shark_module_id = sceKernelLoadStartModule(path ? path : DEFAULT_SHACCCG_PATH, 0, NULL, 0, NULL, NULL);
		if (shark_module_id < 0)
			return shark_module_id;
		sceShaccCgSetDefaultAllocator(shark_malloc, shark_free);
		sceShaccCgInitializeCallbackList(&shark_callbacks, SCE_SHACCCG_TRIVIAL);
		shark_callbacks.openFile = shark_open_file_cb;
		shark_initialized = 1;
		shark_is_simple = 1;
	}
	return 0;	
}

void shark_end() {
	if (!shark_initialized)
		return;
	
	// Terminating sceShaccCg module
	sceShaccCgReleaseCompiler();
	if (!shark_is_simple)
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

void shark_set_locale(shark_locale locale) {
	// Changing current locale
	shark_locale_mode = (SceShaccCgLocale)locale;
}

void shark_set_shader_association_path(const char *path) {
	strcpy(shark_association_path, path);
}

SceGxmProgram *shark_compile_shader_extended(const char *src, uint32_t *size, shark_type type, shark_opt opt, int32_t use_fastmath, int32_t use_fastprecision, int32_t use_fastint) {
	if (!shark_initialized)
		return NULL;
	
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
	shark_options.locale = shark_locale_mode;
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
			shark_log_cb(log->message, log->level, log->location ? log->location->lineNumber : -1);
		}
	}
	
	// Returning output
	if (shark_output->programData) {
		if (shark_association_path[0]) {
			// Patching GXP file to have valid fake binary/source GUIDs
			uint32_t bin_hash = XXH32(shark_output->programData, shark_output->programSize, 0);
			uint32_t cg_hash = XXH32(src, *size, 0);
			uint32_t *prg_data = (uint32_t *)shark_output->programData;
			prg_data[3] = bin_hash;
			prg_data[4] = cg_hash;
			
			// Generating shader association file
			char fname[256];
			sprintf(fname, "%s/%08x_%08x%08x.sdb", shark_association_path, bin_hash, bin_hash, cg_hash);
			SceUID fd = sceIoOpen(fname, SCE_O_CREAT | SCE_O_TRUNC | SCE_O_WRONLY, 0777);
			sprintf(shark_sdb_body,
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				"<sdb xmlns=\"http://sce/sdb\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" binFileName=\"%08x.gxp\" compiler=\"psp2cgc\" workingDirectory=\"%s\">\n"
				"\t<guid bin=\"0x%08x\" src=\"0x%08x\"/>"
				"\t<compilerVersion name=\"PSP2 Cg Compiler\" major=\"3\" minor=\"5\" build=\"13225\" version=\"3.5 build 13225\"/>\n"
				"\t<commandLineArgs commandLine=\"shacccg\"/>\n"
				"\t<compileOptions>\n"
				"\t\t<option name=\"mainfilename\" value=\"%08x.cg\"/>\n"
				"\t\t<option name=\"entryfunction\" value=\"main\"/>\n"
				"\t\t<option name=\"profile\" value=\"%s\"/>\n"
				"\t\t<option name=\"cgfx\" value=\"true\"/>\n"
				"\t\t<option name=\"nostdlib\" value=\"false\"/>\n"
				"\t\t<option name=\"optimizationlevel\" value=\"%d\"/>\n"
				"\t\t<option name=\"warninglevel\" value=\"%d\"/>\n"
				"\t\t<option name=\"fastmath\" value=\"%s\"/>\n"
				"\t\t<option name=\"fastprecision\" value=\"%s\"/>\n"
				"\t\t<option name=\"fastint\" value=\"%s\"/>\n"
				"\t\t<option name=\"outputfilename\" value=\"%08x.gxp\"/>\n"
				"\t\t<option name=\"locale\" value=\"%s\"/>\n"
				"\t</compileOptions>\n"
				"\t<allFiles>\n"
				"\t\t<file userSpecifiedFileName=\"%08x.cg\" absoluteFileName=\"%s/%08x.cg\" fileID=\"1\" primary=\"1\">\n"
				"\t\t\t<fileSource>\n"
				"\t\t\t\t<![CDATA[%s]]>\n"
				"\t\t\t</fileSource>\n"
				"\t\t</file>\n"
				"\t</allFiles>\n"
				"</sdb>",
				bin_hash, shark_association_path, bin_hash, cg_hash, bin_hash, type == SHARK_FRAGMENT_SHADER ? "sce_fp_psp2" : "sce_vp_psp2",
				opt, shark_warnings_level, use_fastmath ? "true" : "false", use_fastprecision ? "true" : "false", use_fastint ? "true" : "false",
				bin_hash, shark_locale_mode == SHARK_LOCALE_ENGLISH ? "English" : "Japanese", bin_hash, shark_association_path, bin_hash, src);
			sceIoWrite(fd, shark_sdb_body, strlen(shark_sdb_body));
			sceIoClose(fd);
		}
		
		*size = shark_output->programSize;
	}
	
	return (SceGxmProgram *)shark_output->programData;
}

SceGxmProgram *shark_compile_shader(const char *src, uint32_t *size, shark_type type) {
	return shark_compile_shader_extended(src, size, type, SHARK_OPT_DEFAULT, SHARK_DISABLE, SHARK_DISABLE, SHARK_DISABLE);
}

const SceShaccCgCompileOutput *shark_get_internal_compile_output() {
	return shark_output;
}
