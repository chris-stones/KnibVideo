#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum knib_header_flags {

        // Set IF video has an Alpha channel.
        KNIB_ALPHA      = (1<<0),

        // Frame Format.
        KNIB_CHANNELS_PLANAR = (1<<1), // ETC1 or DXT1 compressed YCbCr(A)
        KNIB_CHANNELS_PACKED = (2<<1), // ETC1 or DXT1 compressed RGB(A)
        KNIB_CHANNELS_MASK   = (3<<1), // frames format mask.


        // File compression flags. Must have exactly ONE of the following set.
        KNIB_DATA_PLAIN = (1<<22), // texture data is NOT compressed.
        KNIB_DATA_LZ4   = (2<<22), // texture data is LZ4 compressed.
        KNIB_DATA_MASK  = (3<<22), // data mask

        // Texture format flags. Must have exactly ONE of the following set.
        KNIB_TEX_GREY   = (1<<27), // texture data is in GreyScale format.
        KNIB_TEX_ETC1   = (2<<27), // texture data is in ETC1 format
        KNIB_TEX_DXT1   = (3<<27), // texture data is in DXT1 format
        KNIB_TEX_MASK   = (3<<27), // texture data mask.
};

typedef size_t (*knib_read)(void *ptr, size_t size, size_t nmemb, void *stream);
typedef int (*knib_seek)(void *stream, long offset, int whence);

typedef struct knib_context * knib_handle;

int knib_open_custom( knib_read read_func, knib_seek seek_func, void * stream, knib_handle * h );

int knib_open_file( const char * fn, knib_handle * h );

int knib_flags(knib_handle ctx);

int knib_get_dimensions(knib_handle ctx, int *w, int *h);

int knib_get_frame_data(knib_handle ctx,
		void ** YData,  int * YSize,
		void ** CbData, int * CbSize,
		void ** CrData, int * CrSize,
		void ** AData,  int * ASize);

int knib_next_frame(knib_handle ctx);

int knib_current_frame(knib_handle ctx);

int knib_close(knib_handle ctx);

#ifdef __cplusplus
} /* extern "C" { */
#endif

