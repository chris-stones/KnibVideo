
#pragma once

#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <deque>
#include <condition_variable>

#include "PlanarWorkSet.hpp"
#include "PackedWorkSet.hpp"

template<typename WorkType>
class ThreadPool
{
	typedef std::vector< std::thread> ThreadVector;
	typedef std::deque< std::unique_ptr<WorkType> >	  WorkDeque;

	ThreadVector 	threadVector;
	WorkDeque 		workDeque;
	const int		max_queue;
	bool			noMoreWork{false};
	int				final_set_index{0};

	std::mutex mutex;
	std::condition_variable readable;
	std::condition_variable writable;

	SetAssembler<WorkType> setAssembler;

	std::unique_ptr<WorkType> GetWork() {

		std::unique_lock<std::mutex> lock( mutex );

		while( !noMoreWork && ( workDeque.size() == 0) )
			readable.wait( lock );

		std::unique_ptr<WorkType> work;

		if(workDeque.size()) {

			work = std::move(workDeque.front());

			workDeque.pop_front();

			writable.notify_all();
		}

		return work;
	}

	void Main() {

		std::unique_ptr<WorkType> work;

		while( ( work = std::move(GetWork()) ) ) {

			printf("ThreadPool: Start Work %d\n", work->GetSetIndex());

			if(work->Work())
				setAssembler.Assemble( std::move(work) );
			else {
				printf("ThreadPool: WORK ERROR!\n");
				throw std::runtime_error("WorkType error.");
			}
		}
	}

public:

	ThreadPool(std::shared_ptr<KnibFile> knibFile, int threads)
		:	setAssembler(knibFile, threads * 2),
		 	max_queue(threads)
	{
		threadVector.reserve(threads);
		for(int i=0;i<threads; i++)
			threadVector.push_back( std::thread( &ThreadPool::Main, this ) );
	}

	~ThreadPool() {

		for(int i=0;i<threadVector.size(); i++)
			threadVector[i].join();
	}

	void AddWork( std::unique_ptr<WorkType> work ) {

		std::unique_lock<std::mutex> lock( mutex );

		final_set_index = work->GetSetIndex();

		workDeque.push_back( std::move(work) );
		readable.notify_all();

		while(workDeque.size() >= max_queue)
			writable.wait(lock);
	}

	void NoMoreWork() {

		std::unique_lock<std::mutex> lock( mutex );

		noMoreWork = true;

		setAssembler.FinalSetIndex(final_set_index);

		readable.notify_all();
	}
};

