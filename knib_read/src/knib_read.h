#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*knib_read)(void *ptr, size_t size, size_t nmemb, void *stream);
typedef int (*knib_seek)(void *stream, long offset, int whence);

typedef struct knib_context * knib_handle;

int knib_open_file( const char * fn, knib_handle * h );

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

