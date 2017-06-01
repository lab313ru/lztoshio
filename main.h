#pragma once

typedef unsigned char byte;
typedef unsigned short ushort;
typedef unsigned int uint;

/* You should define ADD_EXPORTS *only* when building the DLL. */
#ifdef ADD_EXPORTS
#define ADDAPI __declspec(dllexport)
#else
#define ADDAPI
#endif

/* Define calling convention in one place, for convenience. */
#define ADDCALL __cdecl

ADDAPI int ADDCALL decompress(byte *input, byte *output);
ADDAPI int ADDCALL compress(byte *input, byte *output, int size);
ADDAPI int ADDCALL compressed_size(byte *input);
