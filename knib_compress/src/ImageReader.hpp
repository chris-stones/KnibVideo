
#pragma once

#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>

class ImageReader {

	std::thread thread;

	std::deque<std::unique_ptr<Image> > 	deque;
	std::mutex			  					mutex;
	std::condition_variable 				writable;
	std::condition_variable 				readable;

	const std::string format;
	const int from;
	const int to;
	const int inc;
	const int max_frames;
	bool finished;

	void ReadThread() {

		int i = 0;
		try {

			for(i=from;
				((from <= to) && (i<=to)) ||
				((from  > to) && (i>=to))  ;
				i+=inc) {

				std::unique_ptr<Image> image = std::unique_ptr<Image>( new Image( Image::Read, format.c_str(), i ) );

				std::unique_lock<std::mutex> lock( mutex );

				deque.push_back( std::move(image) );

				readable.notify_one();

				while( deque.size() >= max_frames )
					writable.wait( lock );
			}
		}
		catch(...) {
			printf("ERROR: Opening %s frame %d\n", format.c_str(), i);
		}
		{
			std::unique_lock<std::mutex> lock( mutex );
			finished = true;
			readable.notify_all();
		}
	}

public:

	ImageReader(
		const std::string & format,
		int from, int to, int inc,
		int max_frames)
	:	format(format),
	 	from(from),
	 	to(to),
	 	inc(inc),
	 	max_frames(max_frames),
	 	finished(false)
	{
		thread = std::thread( &ImageReader::ReadThread, this );
	}

	~ImageReader() {

		thread.join();
	}

	std::unique_ptr<Image> NextImage() {

		std::unique_ptr<Image> img;

		std::unique_lock<std::mutex> lock( mutex );

		while( !finished && deque.size()==0 )
			readable.wait(lock);

		if(deque.size()) {
			img = std::move(deque.front());
			deque.pop_front();
			writable.notify_one();
		}

		return img;
	}
};

