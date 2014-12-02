
#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <atomic>
#include <map>

#include "KnibFile.hpp"

class SetAssembler {

	std::thread thread;

	typedef std::map<int, std::unique_ptr<WorkSet> > Map;
	Map 						map;
	std::mutex			  		mutex;
	std::condition_variable 	writable;
	std::condition_variable 	readable;
	int							final_set_index{-1};

	std::shared_ptr<KnibFile> knibFile;

	const int max_sets;
	int next_set;

	void Output(std::unique_ptr<WorkSet> ws) {

		printf("SetAssembler: Output %d\n", ws->GetSetIndex());

		bool result = knibFile->Output(
			ws-> YData(), ws-> YSize(),
			ws->CBData(), ws->CBSize(),
			ws->CRData(), ws->CRSize(),
			ws-> AData(), ws-> ASize());

		if(!result)
			throw std::runtime_error("output error!");
	}

	bool NeedMoreSets() const {

		return (final_set_index < 0) || (final_set_index >= next_set);
	}

	std::unique_ptr<WorkSet> GetNextSet() {

		std::unique_lock<std::mutex> lock( mutex );

		Map::iterator itor = map.end();

		while( NeedMoreSets() && (itor = map.find(next_set)) == map.end())
			readable.wait( lock );

		std::unique_ptr<WorkSet> ws;
		if( itor != map.end()) {
			next_set++;
			ws = std::move(itor->second);
			map.erase(itor);
		}
		writable.notify_all();

		return ws;
	}

	void WriteThread() {

		for(;;) {

			std::unique_ptr<WorkSet> ws = GetNextSet();

			if(ws)
				Output(std::move(ws));
			else
				break;
		}
	}

public:

	SetAssembler(std::shared_ptr<KnibFile> knibFile, int max_sets)
		:	max_sets(max_sets),
		 	next_set(0),
		 	knibFile(knibFile)
	{
		thread = std::thread( &SetAssembler::WriteThread, this );
	}

	~SetAssembler() {

		thread.join();
	}

	void Assemble( std::unique_ptr<WorkSet> set ) {

		std::unique_lock<std::mutex> lock( mutex );

		int setIndex = set->GetSetIndex();

		printf("SetAssembler: Assemble Set Index %d\n", setIndex );

		map[ setIndex ] = std::move(set);

		readable.notify_one();

		// worker threads are delivering sets faster than we can output them to disk.
		// here we stall to prevent too much memory being consumed.
		while( map.size() > max_sets )
			writable.wait( lock );
	}

	void FinalSetIndex(int final_set_index) {

		std::unique_lock<std::mutex> lock( mutex );

		this->final_set_index = final_set_index;

		readable.notify_one();
	}
};

