
#pragma once

#include <memory>

class PlanarWorkSet {

	int w;
	int h;
	bool alpha;
	bool do_lz4;
	imgFormat textureFmt;

	const int set_index;

	copy_quality_t quality;

	std::unique_ptr<Image> Y;
	std::unique_ptr<Image> Cb;
	std::unique_ptr<Image> Cr;
	std::unique_ptr<Image> A;

	std::unique_ptr<Image> compressedY;
	std::unique_ptr<Image> compressedCb;
	std::unique_ptr<Image> compressedCr;
	std::unique_ptr<Image> compressedA;

	std::unique_ptr<Image> work_input_img0;
	std::unique_ptr<Image> work_input_img1;
	std::unique_ptr<Image> work_input_img2;

	void Dispose() {

	}

	bool DoTextureCompression() {

		if(!compressedY->CopyFrom( *Y,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing Y\n");
			goto err;
		}
		if(!compressedCb->CopyFrom( *Cb,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing Cb\n");
			goto err;
		}
		if(!compressedCr->CopyFrom( *Cr,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing Cr\n");
			goto err;
		}
		if(A && !compressedA->CopyFrom( *A,ERR_DIFFUSE_KERNEL_DEFAULT,quality)) {
			printf("Error compressing A\n");
			goto err;
		}

		return true;

	err:
		throw std::runtime_error("Output error");
		return false;
	}

	bool ConvertToYCbCrA(std::unique_ptr<Image> src, imgFormat fmt, int index) {

		std::unique_ptr<Image> planar = std::unique_ptr<Image>(
			new Image(src->Width(), src->Height(), fmt));

		if( !planar->CopyFrom(*src) )
			return false;

		src.reset();

		// Copy Y data.
		{
			unsigned char * d = static_cast<unsigned char *>(Y->Data(0));
			unsigned char * s = static_cast<unsigned char *>(planar->Data(0));
			d += index;
			for(int i=0;i<planar->LinearSize(0);i++) {
				*d = *(s++);
				 d+=4;
			}
		}

		// Copy Cb data.
		{
			unsigned char * d = static_cast<unsigned char *>(Cb->Data(0));
			unsigned char * s = static_cast<unsigned char *>(planar->Data(1));
			d += index;
			for(int i=0;i<planar->LinearSize(1);i++) {
				*d = *(s++);
				 d+=4;
			}
		}

		// Copy Cr data.
		{
			unsigned char * d = static_cast<unsigned char *>(Cr->Data(0));
			unsigned char * s = static_cast<unsigned char *>(planar->Data(2));
			d += index;
			for(int i=0;i<planar->LinearSize(2);i++) {
				*d = *(s++);
				 d+=4;
			}
		}

		// Copy 'Alpha' data.
		if(A)
		{
			unsigned char * d = static_cast<unsigned char *>(A->Data(0));
			unsigned char * s = static_cast<unsigned char *>(planar->Data(3));
			d += index;
			for(int i=0;i<planar->LinearSize(3);i++) {
				*d = *(s++);
				 d+=4;
			}
		}
		return true;
	}

	static int CrCbAdjustResolution(int res,int channel) {

	  switch(channel) {
	  case 1:
	  case 2:
		  return (res+1)>>1;
	  default:
		  return res;
	  }
	}

	static int TexSize(int rawSize, int channel) {
		return CrCbAdjustResolution(((rawSize+7)/8)*8,channel);
	}

public:

	PlanarWorkSet(std::vector<std::unique_ptr<Image> > &images, int w, int h, bool alpha, bool do_lz4, imgFormat textureFmt, copy_quality_t quality, int set_index)
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

	~PlanarWorkSet() {
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

		Y = std::unique_ptr<Image>( new Image(TexSize(w,0), TexSize(h,0), IMG_FMT_RGBA32) );
		Cb = std::unique_ptr<Image>( new Image(TexSize(w,1), TexSize(h,1), IMG_FMT_RGBA32) );
		Cr = std::unique_ptr<Image>( new Image(TexSize(w,2), TexSize(h,2), IMG_FMT_RGBA32) );
		if(alpha)
			A = std::unique_ptr<Image>( new Image(TexSize(w,3), TexSize(h,3), IMG_FMT_RGBA32) );

		compressedY = std::unique_ptr<Image>( new Image(TexSize(w,0), TexSize(h,0), textureFmt) );
		compressedCb = std::unique_ptr<Image>( new Image(TexSize(w,1), TexSize(h,1), textureFmt) );
		compressedCr = std::unique_ptr<Image>( new Image(TexSize(w,2), TexSize(h,2), textureFmt) );
		if(alpha)
			compressedA = std::unique_ptr<Image>( new Image(TexSize(w,3), TexSize(h,3), textureFmt) );

		memset(Y->Data(0),  0xff, Y->LinearSize(0));
		memset(Cb->Data(0), 0xff, Cb->LinearSize(0));
		memset(Cr->Data(0), 0xff, Cr->LinearSize(0));
		if(A)
			memset(A->Data(0),  0xff, A->LinearSize(0));

		std::unique_ptr<Image> resized;

		if(images[0] && (images[0]->Width() != this->w || images[0]->Height() != this->h))
			resized = std::unique_ptr<Image>( new Image(this->w, this->h, IMG_FMT_RGBA32) );

		for(int img_index=0; img_index<3; img_index++) {

			std::unique_ptr<Image> img =
				std::move(images[img_index]);

			if(img) {

				if(img->Width() != this->w || img->Height() != this->h) {

					if(!resized)
						return false;

					if( !resized->CopyFrom(std::move(img)) )
						return false;

					if(!ConvertToYCbCrA(std::move(resized), IMG_FMT_YUVA420P, img_index))
						return false;

				}
				else {
					if(!ConvertToYCbCrA(std::move(img), IMG_FMT_YUVA420P, img_index))
						return false;
				}
			}
		}
		if(!DoTextureCompression())
			return false;

		return true;
	}

	void * YData()  const { return compressedY ->Data(0); }
	void * CBData() const { return compressedCb->Data(0); }
	void * CRData() const { return compressedCr->Data(0); }
	void * AData()  const { return compressedA ? compressedA ->Data(0) : NULL; }

	int    YSize()  const { return compressedY ->LinearSize(0); }
	int    CBSize() const { return compressedCb->LinearSize(0); }
	int    CRSize() const { return compressedCr->LinearSize(0); }
	int    ASize()  const { return compressedA ? compressedA ->LinearSize(0) : 0; }
};

