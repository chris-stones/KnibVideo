/*
 Knib ( pronounced with a silent 'k' ) is a video format for OpenGL/DirectX applications that
 need to display videos in textures, but don't have the CPU horsepower to decode Bink.
 E.g... Raspberry Pi!

 Your hardware will need to support fragment shaders, and Ideally DXT1 or ETC1 texture compression.

 How it works...

 Frames are grouped in sets of 3, and converted to YUV420P or YUVA420P ( YCbCr<A> ) colour space.
 The 'Y' channels from the 3 frames are pakced (Y0,Y1,Y2,Y0,Y1,Y2...) and further compressed as DTX1 or ETC1.
 The same happens with the 'Cb', 'Cr' and optionally 'Alpha' channels.
 All channels are then LZ4 compressed together.
*/


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

#include <knib_read.h>
#include <libimg.h>
#include <libimgutil.h>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lz4.h"
#include "lz4hc.h"
#include "args.h"

class KnibFile {

	FILE * file;

	void * compressedbuffer;
	int compressedbuffer_size;

	void * uncompressedbuffer;
	int uncompressedbuffer_size;

	knib_header file_header;

	void AllocateCompressionBuffer(int uncompressed) {

		int required = LZ4_compressBound(uncompressed);

		if(required > compressedbuffer_size) {
			free(compressedbuffer);
			if((compressedbuffer = malloc(required)))
				compressedbuffer_size = required;
			else
				compressedbuffer_size = 0;
		}
		if(uncompressed > uncompressedbuffer_size) {
			free(uncompressedbuffer);
			if((uncompressedbuffer = malloc(uncompressed)))
				uncompressedbuffer_size = uncompressed;
			else
				uncompressedbuffer_size = 0;
		}
		if(compressedbuffer_size < required || uncompressedbuffer_size < uncompressed)
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
		:	file(NULL),
			compressedbuffer(NULL),
		 	compressedbuffer_size(0),
		 	uncompressedbuffer(NULL),
		 	uncompressedbuffer_size(0)
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

	bool Output( const void * YTex,  const int YSize,
				 const void * CbTex, const int CbSize,
				 const void * CrTex, const int CrSize,
				 const void * ATex,  const int ASize ) {

		if(ASize && ATex)
			file_header.flags |= KNIB_ALPHA;

		const int uncompressedTextureSize = YSize+CbSize+CrSize+ASize;

		AllocateCompressionBuffer(uncompressedTextureSize);

		{
//			printf("OUTPUT SIZES %d %d %d %d\n",YSize,CbSize,CrSize,ASize);
			char * unc_buff = static_cast<char *>(uncompressedbuffer);
			memcpy(unc_buff,  YTex, YSize ); unc_buff += YSize;
			memcpy(unc_buff, CbTex, CbSize); unc_buff += CbSize;
			memcpy(unc_buff, CrTex, CrSize); unc_buff += CrSize;
			memcpy(unc_buff,  ATex, ASize ); unc_buff += ASize;
		}

		const int compressedSize =
			LZ4_compressHC((const char*)uncompressedbuffer, (char*)compressedbuffer,uncompressedTextureSize);

		char * unc_buff = static_cast<char *>(uncompressedbuffer);

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
		Write(compressedbuffer, set.data_size);

		if(set.data_size > file_header.compressed_buffer_size)
			file_header.compressed_buffer_size = set.data_size;
		if(set.data_uncompressed_size > file_header.uncompressed_buffer_size)
			file_header.uncompressed_buffer_size = set.data_uncompressed_size;

		return true;
	}

};

class KnibSet {

	int index;
	int w;
	int h;

	imgImage * Y;
	imgImage * Cb;
	imgImage * Cr;
	imgImage * A;

	imgImage * compressedY;
	imgImage * compressedCb;
	imgImage * compressedCr;
	imgImage * compressedA;

	void Dispose() {
		imgFreeAll(Y);
		imgFreeAll(Cb);
		imgFreeAll(Cr);
		imgFreeAll(A);
		imgFreeAll(compressedY);
		imgFreeAll(compressedCb);
		imgFreeAll(compressedCr);
		imgFreeAll(compressedA);
	}

	bool Output(KnibFile & knibFile) {

		if(imguCopyImage(compressedY,Y)!=0)
			goto err;
		if(imguCopyImage(compressedCb,Cb)!=0)
			goto err;
		if(imguCopyImage(compressedCr,Cr)!=0)
			goto err;
		if(A && imguCopyImage(compressedA,A)!=0)
			goto err;

//		printf("COMPRESSED FORMATS %s %s %s %s\n",
//				imguGetFormatName(compressedY->format),
//				imguGetFormatName(compressedCb->format),
//				imguGetFormatName(compressedCr->format),
//				compressedA ? imguGetFormatName(compressedA->format) : "" );

//		printf("TEX COMPRESS  Y %d -> %d (%f%%)\n", Y->linearsize[0], compressedY->linearsize[0], (float)compressedY->linearsize[0] / (float)Y->linearsize[0]);
//		printf("TEX COMPRESS Cb %d -> %d (%f%%)\n",Cb->linearsize[0],compressedCb->linearsize[0], (float)compressedCb->linearsize[0] / (float)Cb->linearsize[0]);
//		printf("TEX COMPRESS Cr %d -> %d (%f%%)\n",Cr->linearsize[0],compressedCr->linearsize[0], (float)compressedCr->linearsize[0] / (float)Cr->linearsize[0]);
//		if(A)
//			printf("TEX COMPRESS  A %d -> %d (%f%%)\n", A->linearsize[0], compressedA->linearsize[0], (float)compressedA->linearsize[0] / (float)A->linearsize[0]);

		knibFile.Output(
			compressedY->data.channel[0],
			compressedY->linearsize[0],
			compressedCb->data.channel[0],
			compressedCb->linearsize[0],
			compressedCr->data.channel[0],
			compressedCr->linearsize[0],
			compressedA ? compressedA->data.channel[0] : NULL,
			compressedA ? compressedA->linearsize[0] : 0);

		return true;
	err:
		throw std::runtime_error("Output error");
		return false;
	}

	bool Add(imgImage * src, imgFormat fmt, int index) {

		imgImage * planar = NULL;
		if(imgAllocImage(&planar)!=0)
			return false;

		planar->format = fmt;
		planar->width = src->width;
		planar->height = src->height;
		if(imgAllocPixelBuffers(planar)!=0) {
			imgFreeAll(planar);
			return false;
		}
		if(imguCopyImage(planar, src)!=0) {
			imgFreeAll(planar);
			return false;
		}

		// Copy Y data.
		{
			unsigned char * d = static_cast<unsigned char *>(Y->data.channel[0]);
			unsigned char * s = static_cast<unsigned char *>(planar->data.channel[0]);
			d += index;
			for(int i=0;i<planar->linearsize[0];i++) {
				*d = *(s++);
				 d+=4;
			}
		}

		// Copy Cb data.
		{
			unsigned char * d = static_cast<unsigned char *>(Cb->data.channel[0]);
			unsigned char * s = static_cast<unsigned char *>(planar->data.channel[1]);
			d += index;
			for(int i=0;i<planar->linearsize[1];i++) {
				*d = *(s++);
				 d+=4;
			}
		}

		// Copy Cr data.
		{
			unsigned char * d = static_cast<unsigned char *>(Cr->data.channel[0]);
			unsigned char * s = static_cast<unsigned char *>(planar->data.channel[2]);
			d += index;
			for(int i=0;i<planar->linearsize[2];i++) {
				*d = *(s++);
				 d+=4;
			}
		}

		// Copy 'Alpha' data.
		if(A)
		{
			unsigned char * d = static_cast<unsigned char *>(A->data.channel[0]);
			unsigned char * s = static_cast<unsigned char *>(planar->data.channel[3]);
			d += index;
			for(int i=0;i<planar->linearsize[3];i++) {
				*d = *(s++);
				 d+=4;
			}
		}

		imgFreeAll(planar);
		return true;
	}

public:

	KnibSet(int w, int h, bool alpha, imgFormat textureFmt)
		:	index(0),
			w(w), h(h),
		 	Y(NULL),
		 	Cb(NULL),
		 	Cr(NULL),
		 	A(NULL),
		 	compressedY(NULL),
		 	compressedCb(NULL),
		 	compressedCr(NULL),
		 	compressedA(NULL)
	{
		// Allocate images!
		if(imgAllocImage(&Y)!=0)
			goto err;
		if(imgAllocImage(&Cb)!=0)
			goto err;
		if(imgAllocImage(&Cr)!=0)
			goto err;
		if(alpha && imgAllocImage(&A)!=0)
			goto err;
		if(imgAllocImage(&compressedY)!=0)
			goto err;
		if(imgAllocImage(&compressedCb)!=0)
			goto err;
		if(imgAllocImage(&compressedCr)!=0)
			goto err;
		if(alpha && imgAllocImage(&compressedA)!=0)
			goto err;

		// Set formats.
		Y->format =
		Cb->format =
		Cr->format = IMG_FMT_RGBA32;
		if(alpha)
			A->format = Y->format;
		compressedY->format =
		compressedCb->format =
		compressedCr->format = textureFmt;
		if(alpha)
			compressedA->format = compressedY->format;

		// Set dimensions.
		Y->width = compressedY->width = w;
		Y->height = compressedY->height = h;
		Cr->width = compressedCr->width = Cb->width = compressedCb->width = (w+1)>>1;
		Cr->height = compressedCr->height = Cb->height = compressedCb->height = (h+1)>>1;
		if(alpha) {
			A->width = compressedA->width = Y->width;
			A->height = compressedA->height = Y->height;
		}

		// Allocate allocate buffers.
		if(imgAllocPixelBuffers(Y)!=0)
			goto err;
		if(imgAllocPixelBuffers(Cb)!=0)
			goto err;
		if(imgAllocPixelBuffers(Cr)!=0)
			goto err;
		if(alpha && imgAllocPixelBuffers(A)!=0)
			goto err;
		if(imgAllocPixelBuffers(compressedY)!=0)
			goto err;
		if(imgAllocPixelBuffers(compressedCb)!=0)
			goto err;
		if(imgAllocPixelBuffers(compressedCr)!=0)
			goto err;
		if(alpha && imgAllocPixelBuffers(compressedA)!=0)
			goto err;

		memset(Y->data.channel[0],  0xff, Y->linearsize[0]);
		memset(Cb->data.channel[0], 0xff, Cb->linearsize[0]);
		memset(Cr->data.channel[0], 0xff, Cr->linearsize[0]);
		if(A)
			memset(A->data.channel[0],  0xff, A->linearsize[0]);

		return;
	err:
		Dispose();
		throw std::runtime_error("out of memory");
	}

	~KnibSet() {
		Dispose();
	}

	bool Flush(KnibFile & knibFile) {

		if(index%3)
			Output(knibFile);

		return true;
	}

	bool Add(const char * format, int frame, KnibFile & knibFile) {

		imgImage * img = NULL;

		if(imgAllocAndReadF(&img, format, frame)!=0)
			return false;

		if(!Add(img, IMG_FMT_YUVA420P, index)){
			imgFreeAll(img);
			return false;
		}

		index++;
		if(index==3) {
			Output(knibFile);
			index = 0;
		}

		imgFreeAll(img);
		return true;
	}

};


int main(int argc, char * argv[]) {

	arguments args = read_args(argc,argv);

	// fix expected common mistake... from 10, to 1, increment 1.
	//	change increment to -1.
	if((args.ff_from > args.ff_to) && (args.ff_inc > 0))
		args.ff_inc *= -1;

	imgImage * img = NULL;
	if( imgAllocAndStatF(&img, args.ff_string , args.ff_from) == 0) {

		KnibFile knibFile(argv[5]);
		knibFile.SetSize( img->width, img->height );
		bool alpha = !!(img->format & IMG_FMT_COMPONENT_ALPHA);
		if( alpha) {
			printf("Source has alpha channel.");
			args.flags |= KNIB_ALPHA;
		}
		else
			printf("No alpha channel.");

		knibFile.SetFlags( args.flags );

		imgFormat textureFmt;
		switch(args.flags & KNIB_TEX_MASK)
		{
		default:
			printf("Unknown texture format.");
			printf(" use --DXT1 for desktop targets,\n");
			printf(" and --ETC1 for embedded targets.");
			return -1;
		case KNIB_TEX_DXT1:
			textureFmt = IMG_FMT_DXT1;
			break;
		case KNIB_TEX_ETC1:
			textureFmt = IMG_FMT_ETC1;
			break;
		}

		KnibSet knibSet(img->width, img->height, alpha, textureFmt);

		int frames = 0;

		// support reading the frames backwards.
		for(int i=args.ff_from;
				((args.ff_from <= args.ff_to) && (i<=args.ff_to)) ||
				((args.ff_from  > args.ff_to) && (i<=args.ff_to))  ;
				i+=args.ff_inc) {

			printf("processing frame %d\n", i);
			if(knibSet.Add( args.ff_string, i, knibFile )) {
				frames++;
				knibFile.SetFrames(frames);
			}
		}

		knibSet.Flush(knibFile);

		return 0;
	}

	printf("Can't open ");
	printf(args.ff_string, args.ff_from);
	printf("\n");

	return -1;
}

