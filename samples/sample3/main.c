// Compiling all shaders in a given directory
#include <vitasdk.h>
#include <vitashark.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SHADERS_PATH "ux0:data/shaders"
char out_dir[256];

void saveGXP(SceGxmProgram *p, uint32_t size, const char *fname) {
	FILE *f = fopen(fname, "wb");
	fwrite(p, 1, size, f);
	fclose(f);
}

void compileShader(const char *fname, int type) {
	// Reading the shader from file
	char full_name[256], out_name[256];
	sprintf(full_name, "%s/%s", SHADERS_PATH, fname);
	FILE *f = fopen(full_name, "rb");
	fseek(f, 0, SEEK_END);
	uint32_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = (char*)malloc(size);
	fread(buf, 1, size, f);
	fclose(f);
	
	// Compiling and saving the resulting GXP file
	SceGxmProgram *p = shark_compile_shader(buf, &size, type);
	if (p) {
		char extless_name[256];
		strncpy(extless_name, fname, strstr(fname, ".cg") - fname);
		extless_name[strstr(fname, ".cg") - fname] = 0;
		sprintf(out_name, "%s/%s.gxp", out_dir, extless_name);
		saveGXP(p, size, out_name);
	}
	
	shark_clear_output();
}

int main() {
	// Initializing vitaShaRK
	if (shark_init(NULL) < 0) // NOTE: libshacccg.suprx will need to be placed in ur0:data
		return -1;
	
	// Creating dir for gxp output files
	sprintf(out_dir, "%s/gxp", SHADERS_PATH);
	sceIoMkdir(out_dir, 0777);
	
	// Scanning input folder
	SceIoDirent g_dir;
	int fd = sceIoDopen(SHADERS_PATH);
	while (sceIoDread(fd, &g_dir) > 0) {
		if (!SCE_S_ISDIR(g_dir.d_stat.st_mode)) {
			if (strstr(g_dir.d_name, "_v.cg")) {
				compileShader(g_dir.d_name, SHARK_VERTEX_SHADER);
			} else if (strstr(g_dir.d_name, "_f.cg")) {
				compileShader(g_dir.d_name, SHARK_FRAGMENT_SHADER);
			}
		}
	}
	
	shark_end();
	
	return 0;
}
