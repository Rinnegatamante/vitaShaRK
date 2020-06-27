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

#ifndef _VITASHARK_H_
#define _VITASHARK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <vitasdk.h>

typedef enum shark_opt {
	SHARK_OPT_SLOW,      //!< Equivalent to O0
	SHARK_OPT_SAFE,      //!< Equivalent to O1
	SHARK_OPT_DEFAULT,   //!< Equivalent to O2
	SHARK_OPT_FAST,      //!< Equivalent to O3
	SHARK_OPT_UNSAFE     //!< Equivalent to Ofast
} shark_opt;

typedef enum shark_type {
	SHARK_VERTEX_SHADER,
	SHARK_FRAGMENT_SHADER
} shark_type;

#define SHARK_DISABLE 0
#define SHARK_ENABLE  1

int shark_init(const char *path); //!< Initializes runtime shader compiler
SceGxmProgram *shark_compile_shader_extended(const char *src, uint32_t *size, shark_type type, shark_opt opt, int32_t use_fastmath, int32_t use_fastprecision, int32_t use_fastint); //!< Compiles a shader with extended settings
SceGxmProgram *shark_compile_shader(const char *src, uint32_t *size, shark_type type); //!< Compiles a shader
void shark_clear_output(); //!< Clears output of a successfull compilation
void shark_end(); //!< Terminates runtime shader compiler and frees used memory

#ifdef __cplusplus
}
#endif

#endif
