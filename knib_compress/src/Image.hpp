
#include <libimg.h>
#include <libimgutil.h>
#include <cstdarg>
#include <memory>

#pragma once

class Image {

	imgImage * img {};

public:
	enum open_enum {
		Read,
		Stat,
	};

private:
	static imgImage * _Load(open_enum open, const char * file) {

		imgImage * img = NULL;

		switch(open) {
		case Read:
			if(imgAllocAndRead(&img, file) != 0)
				throw std::runtime_error("Image: can't read Image.");
			break;
		case Stat:
			if(imgAllocAndStat(&img, file) != 0)
				throw std::runtime_error("Image: can't stat Image.");
			break;
		default:
			throw std::runtime_error("Image: bad open enum.");
			break;
		}

		return img;
	}

public:

	Image() {

		if( imgAllocImage(&img) != 0)
			throw std::runtime_error("Image: can't allocate Image.");
	}

	Image(const Image &image) = delete;

	Image(Image &&image) {

		this->img = image.img;
		image.img = NULL;
	}

	Image(open_enum open, const char * file) {

		this->img = _Load(open, file);
	}

	Image(open_enum open, const char * format, ...) {

		va_list va;
		va_start(va,format);

		const int max_path = 1024;
		char *path = (char*)malloc(sizeof (char) * max_path);


		if(path && vsnprintf(path,max_path,format,va)<max_path) {
			try {
				this->img = _Load(open, path);
			}
			catch(...) {}
		}
		va_end(va);
		free(path);
		if(!this->img)
			throw std::runtime_error("Image: out of memory.");
	}

	Image(int w, int h, imgFormat fmt) {

		if( imgAllocImage(&img) == 0) {

			img->width = w;
			img->height = h;
			img->format = fmt;

			if(imgAllocPixelBuffers(img) == 0)
				return;

			imgFreeAll(img);
		}
		throw std::runtime_error("Image: out of memory.");
	}

	bool CopyFrom( const Image & image ) {

		return imguCopyImage(img, image.img) == 0;
	}
	bool CopyFrom( std::unique_ptr<Image> image ) {

		return CopyFrom(*image);
	}

	bool CopyFrom( const Image & image, err_diffuse_kernel_t diffuse_kernel, copy_quality_t quality ) {

		return imguCopyImage3(img, image.img, diffuse_kernel, quality) == 0;
	}
	bool CopyFrom( std::unique_ptr<Image> image, err_diffuse_kernel_t diffuse_kernel, copy_quality_t quality ) {

		return CopyFrom(*image, diffuse_kernel, quality);
	}

	int Width() const { return img->width; }
	int Height() const { return img->height; }

	~Image() {
		imgFreeAll(img);
	}

	int LinearSize(int channel) const {

		return img->linearsize[channel];
	}

	const void * Data(int channel) const {

		return img->data.channel[channel];
	}

	void * Data(int channel) {

		return img->data.channel[channel];
	}
};

