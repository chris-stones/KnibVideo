
#pragma once

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

class KnibFile {

	FILE * file {NULL};

	void * compressedbuffer {NULL};
	int compressedbuffer_size {0};

	void * uncompressedbuffer {NULL};
	int uncompressedbuffer_size {0};

	knib_header file_header;

	void AllocateBuffers(int uncompressed, bool lz4Compressed) {

		int required = 0;
		if(lz4Compressed) {
			required = LZ4_compressBound(uncompressed);
			if(required > compressedbuffer_size) {
				free(compressedbuffer);
				if((compressedbuffer = malloc(required)))
					compressedbuffer_size = required;
				else
					compressedbuffer_size = 0;
			}
		}
		if(uncompressed > uncompressedbuffer_size) {
			free(uncompressedbuffer);
			if((uncompressedbuffer = malloc(uncompressed)))
				uncompressedbuffer_size = uncompressed;
			else
				uncompressedbuffer_size = 0;
		}
		if((compressedbuffer_size < required) || (uncompressedbuffer_size < uncompressed))
			throw std::runtime_error("out of memory!");
	}

	static const char * WhenceStr(int whence) {

		switch(whence) {
			default: return "???";
			case SEEK_SET: return "SEEK_SET";
			case SEEK_CUR: return "SEEK_CUR";
			case SEEK_END: return "SEEK_END";
		}
	}

	void Seek( long offset, int whence ) {

	    if( fseek( file, offset, whence ) != 0 ) {

	      printf("oops - bad seek %d %s\n", (int)offset, WhenceStr((int)whence));
	      throw std::runtime_error("Seek error.");
	    }
	  }

	void Write( const void * data, unsigned int size ) {

		if(fwrite(data,size,1,file) != 1)
			throw std::runtime_error("Write error.");
	}

	template<typename _T> void Write( const _T & data ) {

		Write(&data,sizeof data);
	}

public:

	KnibFile(const char * fn)
	{
		if(!(file = fopen(fn, "wb")))
			throw std::runtime_error("can't open output file!");

		memset(&file_header, 0, sizeof file_header);
		memcpy((void*)file_header.magick, (const void *)"knib", 4);
		file_header.first_set_offset = sizeof file_header;

		Write(file_header);
	}

	~KnibFile() {

		free(compressedbuffer);
		free(uncompressedbuffer);
		Seek(0, SEEK_SET);
		Write(file_header);
		fclose(file);
	}

	void SetFrames(int f) {

		file_header.frames = f;
	}

	void SetFlags(int f) {

		file_header.flags = f;
	}

	void SetSize(int w, int h) {

		file_header.orig_width = w;
		file_header.frame_width = w;
		file_header.orig_height = h;
		file_header.frame_height = h;
	}

	bool OutputPacked(
				const void * RGBa0Tex, const int RGBa0Size,
				const void * RGBa1Tex, const int RGBa1Size,
				const void * RGBa2Tex, const int RGBa2Size,
				const void * A012Tex,  const int A012Size) {

		if(A012Size && A012Tex)
			file_header.flags |= KNIB_ALPHA;

		if(RGBa0Tex) OutputPackedPart( RGBa0Tex, RGBa0Size, A012Tex, A012Size );
		if(RGBa1Tex) OutputPackedPart( RGBa1Tex, RGBa1Size, nullptr, 0 );
		if(RGBa2Tex) OutputPackedPart( RGBa2Tex, RGBa2Size, nullptr, 0 );

		return true;
	}
private:

	bool OutputPackedPart( const void * RGBTex, const int RGBSize, const void * ATex, const int ASize) {

		const int uncompressedTextureSize = RGBSize + ASize;
		int compressedSize = uncompressedTextureSize;

		AllocateBuffers(uncompressedTextureSize,((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4));

		const void * useUncompressedBuffer = NULL;

		if(ASize) {

			char * unc_buff = static_cast<char *>(uncompressedbuffer);
			memcpy(unc_buff, RGBTex, RGBSize ); unc_buff += RGBSize;
			memcpy(unc_buff, ATex, ASize);

			useUncompressedBuffer = uncompressedbuffer;

			if((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4) {

				compressedSize =
						LZ4_compressHC((const char*)uncompressedbuffer,
							(char*)compressedbuffer,
							uncompressedTextureSize);
			}
		}
		else {

			useUncompressedBuffer = RGBTex;

			if((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4) {
				compressedSize =
						LZ4_compressHC((const char*)RGBTex,
							(char*)compressedbuffer,
							uncompressedTextureSize);
			}
		}

		knib_set_header set;
		memset(&set, 0, sizeof set);

		set.data_offset = ftell(file) + sizeof(set);
		set.data_size = compressedSize;
		set.data_uncompressed_size = uncompressedTextureSize;
		set.y_data_buffer_offset = 0;
		set.y_data_buffer_size = RGBSize;
		set.cb_data_buffer_offset = 0;
		set.cb_data_buffer_size = 0;
		set.cr_data_buffer_offset = 0;
		set.cr_data_buffer_size = 0;
		set.a_data_buffer_offset = RGBSize;
		set.a_data_buffer_size = ASize;
		set.next_set_offset = set.data_offset + set.data_size;

		printf("writing set @ %ld, next set @ %d\n",ftell(file), set.next_set_offset);
		Write(set);
		if((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4)
			Write(compressedbuffer, set.data_size);
		else
			Write(useUncompressedBuffer, set.data_size);

		if(set.data_size > file_header.compressed_buffer_size)
			file_header.compressed_buffer_size = set.data_size;

		if((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4)
			if(set.data_uncompressed_size > file_header.uncompressed_buffer_size)
				file_header.uncompressed_buffer_size = set.data_uncompressed_size;

		return true;
	}

public:

	bool OutputPlanar(
				 const void * YTex,  const int YSize,
				 const void * CbTex, const int CbSize,
				 const void * CrTex, const int CrSize,
				 const void * ATex,  const int ASize ) {

		if(ASize && ATex)
			file_header.flags |= KNIB_ALPHA;

		const int uncompressedTextureSize = YSize+CbSize+CrSize+ASize;

		AllocateBuffers(uncompressedTextureSize,((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4));

		{
			char * unc_buff = static_cast<char *>(uncompressedbuffer);
			memcpy(unc_buff,  YTex, YSize ); unc_buff += YSize;
			memcpy(unc_buff, CbTex, CbSize); unc_buff += CbSize;
			memcpy(unc_buff, CrTex, CrSize); unc_buff += CrSize;
			memcpy(unc_buff,  ATex, ASize );
		}

		int compressedSize = uncompressedTextureSize;

		if((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4) {
			compressedSize =
					LZ4_compressHC((const char*)uncompressedbuffer,
						(char*)compressedbuffer,
						uncompressedTextureSize);
		}

		knib_set_header set;
		memset(&set, 0, sizeof set);

		set.data_offset = ftell(file) + sizeof(set);
		set.data_size = compressedSize;
		set.data_uncompressed_size = uncompressedTextureSize;
		set.y_data_buffer_offset = 0;
		set.y_data_buffer_size = YSize;
		set.cb_data_buffer_offset = YSize;
		set.cb_data_buffer_size = CbSize;
		set.cr_data_buffer_offset = YSize+CbSize;
		set.cr_data_buffer_size = CrSize;
		set.a_data_buffer_offset = YSize+CbSize+CrSize;
		set.a_data_buffer_size = ASize;
		set.next_set_offset = set.data_offset + set.data_size;

		printf("writing set @ %ld, next set @ %d\n",ftell(file), set.next_set_offset);
		Write(set);
		if((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4)
			Write(compressedbuffer, set.data_size);
		else
			Write(uncompressedbuffer, set.data_size);

		if(set.data_size > file_header.compressed_buffer_size)
			file_header.compressed_buffer_size = set.data_size;

		if((file_header.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4)
			if(set.data_uncompressed_size > file_header.uncompressed_buffer_size)
				file_header.uncompressed_buffer_size = set.data_uncompressed_size;

		return true;
	}

};

