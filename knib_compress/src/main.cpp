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

#include "Image.hpp"
#include "ImageReader.hpp"
#include "WorkSet.hpp"
#include "SetAssembler.hpp"
#include "KnibFile.hpp"
#include "ThreadPool.hpp"


int main(int argc, char * argv[]) {

	arguments args = read_args(argc,argv);

	// fix expected common mistake... from 10, to 1, increment 1.
	//	change increment to -1.
	if((args.ff_from > args.ff_to) && (args.ff_inc > 0))
		args.ff_inc *= -1;

	imgImage * img = NULL;
	if( imgAllocAndStatF(&img, args.ff_string , args.ff_from) == 0) {

		std::shared_ptr<KnibFile> knibFile( new KnibFile(args.output_fn) );

		knibFile->SetSize( img->width, img->height );

		if(img->width  % 8) img->width  += 8 - (img->width  % 8);
		if(img->height % 8) img->height += 8 - (img->height % 8);

		bool alpha = !!(img->format & IMG_FMT_COMPONENT_ALPHA);
		if( alpha) {
			printf("Source has alpha channel.\n");
			args.flags |= KNIB_ALPHA;
		}
		else
			printf("No alpha channel.\n");

		const bool do_lz4 = (args.flags & KNIB_DATA_MASK) == KNIB_DATA_LZ4;

		knibFile->SetFlags( args.flags );

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

		{
			// TODO: assuming 8 threads is a good balance.
			int threads = 8;

			ImageReader imageReader(args.ff_string, args.ff_from, args.ff_to, args.ff_inc, 3);

			ThreadPool threadPool(knibFile, threads);

			std::vector<std::unique_ptr<Image> > images(3);
			int frames = 0;
			int set_index = 0;

			while((images[frames%3] = std::move(imageReader.NextImage()))) {

				++frames;

				if((frames%3)==0) {

					threadPool.AddWork( std::unique_ptr<WorkSet>( new WorkSet(
							images,
							img->width,
							img->height,
							alpha,
							do_lz4,
							textureFmt,
							args.quality,
							set_index++)));
				}
			}

			if(frames%3) {
				threadPool.AddWork( std::unique_ptr<WorkSet>( new WorkSet(
					images,
					img->width,
					img->height,
					alpha,
					do_lz4,
					textureFmt,
					args.quality,
					set_index++)));
			}

			imgFreeAll(img);

			threadPool.NoMoreWork();

			knibFile->SetFrames(frames);
		}

		return 0;
	}

	printf("Can't open ");
	printf(args.ff_string, args.ff_from);
	printf("\n");

	return -1;
}

