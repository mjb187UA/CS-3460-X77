
#pragma once

#include "cs477.h"


namespace cs477
{


#ifdef _WIN32
	using thread = HANDLE;
#else
	using thread = pthread_t;
#endif

	template <typename Fn> thread create_thread(Fn fn);
	void join(thread thread);

	class mutex;
	class condition_variable;

	template <typename T> class future;

	template <typename Fn> future<typename std::result_of<Fn()>::type> queue_work(Fn fn);





}