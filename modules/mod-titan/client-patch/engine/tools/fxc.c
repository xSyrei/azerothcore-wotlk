#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
typedef HRESULT (WINAPI *PD3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void *, void *, LPCSTR, LPCSTR, UINT, UINT,
                                      void **, void **);
typedef struct { void **vt; } Blob;
typedef void* (WINAPI *GP)(void*); typedef SIZE_T (WINAPI *GS)(void*);
/* A minimal fxc: fxc in.hlsl entry profile out.bin (tools/compile_shaders.sh runs it under Wine). */
int main(int argc, char **argv) {
  HMODULE m = LoadLibraryA(".\\d3dcompiler_47.dll"); if (!m) { printf("no dll\n"); return 1; }
  PD3DCompile f = (PD3DCompile)GetProcAddress(m, "D3DCompile");
  FILE *fp = fopen(argv[1], "rb"); fseek(fp, 0, SEEK_END); long n = ftell(fp); rewind(fp);
  char *src = malloc(n); fread(src, 1, n, fp); fclose(fp);
  void *code = 0, *err = 0;
  HRESULT hr = f(src, n, argv[1], 0, 0, argv[2], argv[3], 1 << 15, 0, &code, &err);
  if (err) { Blob *b = err; fwrite(((GP)b->vt[3])(err), 1, ((GS)b->vt[4])(err), stdout); }
  if (code && argc > 4) { Blob *b = code; FILE *o = fopen(argv[4], "wb");
    fwrite(((GP)b->vt[3])(code), 1, ((GS)b->vt[4])(code), o); fclose(o); }
  printf("\nhr=%08lx\n", hr); return hr < 0;
}
