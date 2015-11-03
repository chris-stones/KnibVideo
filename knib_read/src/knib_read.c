
struct knib_header {

	char magick[4]; // must be "knib"
	int version; // must be 0
	int flags; // see 'knib_header_flags'
	int orig_width; // width of the input media.
	int orig_height; // height of the input media.
	int frame_width; // frame data width ( different if video was sampled at a lower resolution )
	int frame_height; // frame data height ( different if video was sampled at a lower resolution )
	int frames; // total number of frames.
	int framerate; // TODO: frames per second, or seconds per frame?
	int compressed_buffer_size; // size of the buffer required to hold the largest compressed set.
	int uncompressed_buffer_size; // size of the buffer required to hold the largest uncompressed set.
	int first_set_offset; // offset of the first 'knib_set_header'
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lz4.h"

#include "knib_read.h"

struct knib_set_header {

	int data_offset; // file offset of this sets data.
	int data_size; // file size of this sets data.
	int data_uncompressed_size; // size of this sets data once uncompressed.

	int y_data_buffer_offset; // 'Y' data offset in the uncompressed buffer.
	int y_data_buffer_size; // 'Y' data size in the uncompressed buffer.
	int cb_data_buffer_offset; // 'Cb' data offset in the uncompressed buffer.
	int cb_data_buffer_size;// 'Cb' data size in the uncompressed buffer.
	int cr_data_buffer_offset;// 'Cr' data offset in the uncompressed buffer.
	int cr_data_buffer_size;// 'Cr' data size in the uncompressed buffer.
	int a_data_buffer_offset;// 'A' data offset in the uncompressed buffer.
	int a_data_buffer_size;// 'A' data size in the uncompressed buffer.

	int next_set_offset; // file offset of the next
};

struct knib_context {

	int is_custom_io;

	knib_read read_func;
	knib_seek seek_func;
	void * stream;

	int    flags;
	int    first_set;
	int    tex_width;
	int    tex_height;
	void * read_buffer;
	int    read_buffer_size;
	void * decode_buffer;
	int    decode_buffer_size;
	int    cur_frame;
	int    frames;

	struct knib_set_header cur_set;
};

static int _is_a_knib_stream(struct knib_context * ctx) {

	struct knib_header file_header;
	if((*ctx->read_func)(&file_header, sizeof file_header, 1, ctx->stream) ==1) {

		if(strcmp(file_header.magick,"knib")==0)
			return 0;
		else
			printf("bad magick\n\n");
	}
	printf("not a knib stream\n");
	return -1;
}

static int _decode_set(struct knib_context * ctx) {

	if((*ctx->seek_func)(ctx->stream, ctx->cur_set.data_offset, SEEK_SET) != 0) {
		printf("cant seek to cur set data\n");
		return -1;
	}

	if(ctx->cur_set.data_size > ctx->read_buffer_size) {
		printf("buffer not big enough!\n");
		return -1; // BAD KNIB FILE!
	}

	if((*ctx->read_func)( ctx->read_buffer, ctx->cur_set.data_size, 1, ctx->stream ) != 1) {
		printf("set data truncated\n");
		printf("read %d bytes from offset %d\n", ctx->cur_set.data_size, ctx->cur_set.data_offset);
		return -1; // TRUNCATED KNIB FILE!?
	}

	if((ctx->flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4) {

//		printf("read size = %d\n", ctx->cur_set.data_size);
//		printf("LZ4_uncompress( %p, %p, %d)\n",ctx->read_buffer, ctx->decode_buffer, ctx->cur_set.data_uncompressed_size);

		if(LZ4_uncompress( ctx->read_buffer, ctx->decode_buffer, ctx->cur_set.data_uncompressed_size ) != ctx->cur_set.data_size) {
			printf("LZ4 failed\n");
			return -1; // BAD OR TRUNCATED KNIB FILE!
		}
	}

	return 0;
}

static int _init(struct knib_context * ctx) {

	struct knib_header file_header;

	// READ HEADER
	if(((*ctx->seek_func)(ctx->stream, 0, SEEK_SET) != 0) ||
		((*ctx->read_func)(&file_header, sizeof file_header, 1, ctx->stream) != 1)) {
			printf("cant read header\n");
			return -1;
	}

	// Load header info.
	ctx->frames = file_header.frames;
	ctx->flags = file_header.flags;
	ctx->first_set = file_header.first_set_offset;
	ctx->tex_width = file_header.frame_width;
	ctx->tex_height = file_header.frame_height;
	ctx->read_buffer_size = file_header.compressed_buffer_size;
	ctx->decode_buffer_size = file_header.uncompressed_buffer_size;

	// READ FIRST SET
	if(((*ctx->seek_func)(ctx->stream, ctx->first_set, SEEK_SET) != 0) ||
		((*ctx->read_func)(&ctx->cur_set, sizeof ctx->cur_set, 1, ctx->stream) != 1)) {
			printf("cant read first set\n");
			return -1;
	}

	printf("Allocating %d bytes read buffer\n",ctx->read_buffer_size);
	printf("Allocating %d bytes decode buffer\n",ctx->decode_buffer_size);

	// Allocate buffers for reading and decoding.
	if((ctx->read_buffer = malloc(ctx->read_buffer_size))) {
		if(ctx->decode_buffer_size)
			if((ctx->decode_buffer = malloc(ctx->decode_buffer_size))==NULL) {
				printf("cant allocate buffers\n");
				free(ctx->read_buffer);
				return -1;
			}
	}

	if(_decode_set(ctx)!=0) {
		free(ctx->decode_buffer);
		free(ctx->read_buffer);
		return -1;
	}

	return 0;
}

int knib_open_file( const char * fn, knib_handle * h ) {

	if((*h = calloc(1, sizeof(struct knib_context) ))) {

		if(((*h)->stream = fopen(fn, "rb"))) {
			(*h)->read_func = (knib_read)&fread;
			(*h)->seek_func = (knib_seek)&fseek;

			if(_is_a_knib_stream(*h)==0 && _init(*h)==0)
				return 0;
		}

		fclose((FILE*)((*h)->stream));
		free(*h);
		*h = NULL;
	}
	return -1;
}

int knib_open_custom( knib_read read_func, knib_seek seek_func, void * stream, knib_handle * h ) {

	if((*h = calloc(1, sizeof(struct knib_context) ))) {

		(*h)->read_func = read_func;
		(*h)->seek_func = seek_func;
		(*h)->stream    = stream;
		(*h)->is_custom_io = 1;

		if(_is_a_knib_stream(*h)==0 && _init(*h)==0)
			return 0;

		free(*h);
		*h = NULL;
	}
	return -1;
}

int knib_close(struct knib_context * ctx) {

	free( ctx->decode_buffer );
	free( ctx->read_buffer );
	if(ctx->is_custom_io==0)
		fclose((FILE*)(ctx->stream));
	free(ctx);
	return 0;
}

int knib_next_frame(struct knib_context * ctx) {

	int next_set_offset = ctx->cur_set.next_set_offset;
	ctx->cur_frame++;

	if(ctx->cur_frame == ctx->frames) {
		ctx->cur_frame = 0;
		next_set_offset = ctx->first_set;
	}

	// Packed formats are updated every frame, Planar formats are updated every 3rd.
	if(((ctx->flags & KNIB_CHANNELS_MASK) == KNIB_CHANNELS_PACKED) || ((ctx->cur_frame % 3) == 0)) {

		int e;

		if((*ctx->seek_func)(ctx->stream, next_set_offset, SEEK_SET) != 0) {
			printf("knib_next_frame: couldn't seek\n");
			return -1;
		}

		if(((*ctx->read_func)(&ctx->cur_set, sizeof ctx->cur_set, 1, ctx->stream) != 1)) {
			printf("knib_next_frame: couldn't read frame %d set @ %d\n", ctx->cur_frame, next_set_offset);
			return -1;
		}
		if((e = _decode_set(ctx)) == 0)
			return ctx->cur_frame;
		else
			return e;
	}

	return ctx->cur_frame;
}

int knib_current_frame(struct knib_context * ctx) {

	return ctx->cur_frame;
}

int knib_total_frames(struct knib_context * ctx) {

	return ctx->frames;
}

int knib_flags(struct knib_context * ctx) {

	return ctx->flags;
}

int knib_get_dimensions(struct knib_context * ctx, int *w, int *h) {

	*w = ctx->tex_width;
	*h = ctx->tex_height;
	return 0;
}

int knib_get_cur_channel(struct knib_context * ctx) {

	return ctx->cur_frame % 3;
}

int knib_get_frame_data(struct knib_context * ctx,
		void ** YData,  int * YSize,
		void ** CbData, int * CbSize,
		void ** CrData, int * CrSize,
		void ** AData,  int * ASize)
{
	void * buff;
	if((ctx->flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4)
		buff = ctx->decode_buffer;
	else
		buff = ctx->read_buffer;

	*YData  = (((char *)buff) + ctx->cur_set.y_data_buffer_offset);
	*CbData = (((char *)buff) + ctx->cur_set.cb_data_buffer_offset);
	*CrData = (((char *)buff) + ctx->cur_set.cr_data_buffer_offset);
	*AData  = (((char *)buff) + ctx->cur_set.a_data_buffer_offset);

	*YSize  = ctx->cur_set.y_data_buffer_size;
	*CbSize = ctx->cur_set.cb_data_buffer_size;
	*CrSize = ctx->cur_set.cr_data_buffer_size;
	*ASize  = ctx->cur_set.a_data_buffer_size;
	return 0;
}

