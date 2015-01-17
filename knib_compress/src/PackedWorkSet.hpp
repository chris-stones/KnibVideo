
#pragma once

#include <memory>

class PackedWorkSet {

	int w;
	int h;
	bool alpha;
	bool do_lz4;
	imgFormat textureFmt;

	const int set_index;

	copy_quality_t quality;

	std::unique_ptr<Image> RGBa0;
	std::unique_ptr<Image> RGBa1;
	std::unique_ptr<Image> RGBa2;
	std::unique_ptr<Image> A012;

	std::unique_ptr<Image> compressedRGB0;
	std::unique_ptr<Image> compressedRGB1;
	std::unique_ptr<Image> compressedRGB2;
	std::unique_ptr<Image> compressedA012;

	std::unique_ptr<Image> work_input_img0;
	std::unique_ptr<Image> work_input_img1;
	std::unique_ptr<Image> work_input_img2;

	void Dispose() {

	}

	void MoveAlphaToChannel( Image & dstImage, Image & srcImage, int channel ) {

		unsigned char * src = static_cast<unsigned char *>( srcImage.Data(0) ) + 3;
		unsigned char * dst = static_cast<unsigned char *>( srcImage.Data(0) ) + channel;
		unsigned int len = dstImage.LinearSize(0);
		for(unsigned int i=0;i<len;i++) {
			*dst = *src;
			*src = 0xff;
			dst += 4;
			src += 4;
		}
	}

	bool DoTextureCompression() {

		if(RGBa0 && !compressedRGB0->CopyFrom( *RGBa0,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing RGB0\n");
			goto err;
		}
		if(RGBa1 && !compressedRGB1->CopyFrom( *RGBa1,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing RGB1\n");
			goto err;
		}
		if(RGBa2 && !compressedRGB2->CopyFrom( *RGBa2,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing RGB2\n");
			goto err;
		}
		if(A012 && !compressedA012->CopyFrom( *A012,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing A012\n");
			goto err;
		}

		return true;

	err:
		throw std::runtime_error("Output error");
		return false;
	}

public:

	PackedWorkSet(std::vector<std::unique_ptr<Image> > &images, int w, int h, bool alpha, bool do_lz4, imgFormat textureFmt, copy_quality_t quality, int set_index)
		:	w(w), h(h),
		 	alpha(alpha),
		 	do_lz4(do_lz4),
		 	textureFmt(textureFmt),
			quality(quality),
		 	set_index(set_index)
	{
		if(images[0])
			work_input_img0 = std::move(images[0]);
		if(images[1])
			work_input_img1 = std::move(images[1]);
		if(images[2])
			work_input_img2 = std::move(images[2]);
	}

	~PackedWorkSet() {
		Dispose();
	}

	int GetSetIndex() const {

		return set_index;
	}

	bool Work() {

		std::vector<std::unique_ptr<Image> > images;
		images.push_back( std::move(work_input_img0) );
		images.push_back( std::move(work_input_img1) );
		images.push_back( std::move(work_input_img2) );

		if(images[0])
			RGBa0 = std::unique_ptr<Image>( new Image(w, h, IMG_FMT_RGBA32) );
		if(images[1])
			RGBa1 = std::unique_ptr<Image>( new Image(w, h, IMG_FMT_RGBA32) );
		if(images[2])
			RGBa2 = std::unique_ptr<Image>( new Image(w, h, IMG_FMT_RGBA32) );
		if(alpha)
			A012 = std::unique_ptr<Image>( new Image(w, h, IMG_FMT_RGBA32) );

		if(images[0])
			compressedRGB0 = std::unique_ptr<Image>( new Image(w, h, textureFmt) );
		if(images[1])
			compressedRGB1 = std::unique_ptr<Image>( new Image(w, h, textureFmt) );
		if(images[2])
			compressedRGB2 = std::unique_ptr<Image>( new Image(w, h, textureFmt) );
		if(alpha)
			compressedA012 = std::unique_ptr<Image>( new Image(w, h, textureFmt) );

		if(images[0])
			RGBa0->CopyFrom( *images[0] );
		if(images[1])
			RGBa1->CopyFrom( *images[1] );
		if(images[2])
			RGBa2->CopyFrom( *images[2] );

		if(alpha) {
			memset( A012->Data(0), 0xff, A012->LinearSize(0) );
			if(RGBa0) MoveAlphaToChannel(*A012, *RGBa0, 0);
			if(RGBa1) MoveAlphaToChannel(*A012, *RGBa1, 1);
			if(RGBa2) MoveAlphaToChannel(*A012, *RGBa2, 2);
		}

		bool ret = DoTextureCompression();

		RGBa0.reset();
		RGBa1.reset();
		RGBa2.reset();
		A012.reset();

		return ret;
	}

	void * RGB0Data() const { return compressedRGB0 ? compressedRGB0->Data(0) : NULL; }
	void * RGB1Data() const { return compressedRGB1 ? compressedRGB1->Data(0) : NULL; }
	void * RGB2Data() const { return compressedRGB2 ? compressedRGB2->Data(0) : NULL; }
	void * A012Data() const { return compressedA012 ? compressedA012->Data(0) : NULL; }

	int    RGB0Size() const { return compressedRGB0 ? compressedRGB0->LinearSize(0) : 0; }
	int    RGB1Size() const { return compressedRGB1 ? compressedRGB1->LinearSize(0) : 0; }
	int    RGB2Size() const { return compressedRGB2 ? compressedRGB2->LinearSize(0) : 0; }
	int    A012Size() const { return compressedA012 ? compressedA012->LinearSize(0) : 0; }
};

