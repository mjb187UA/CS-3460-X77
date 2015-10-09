
#pragma once

#include "cs477.h"



namespace cs477
{

	template <typename T> class future;
	template <typename T> class promise;

	template <typename T, typename Iterator> future<std::vector<T>> when_all(Iterator first, Iterator last);


	namespace details
	{


		class basic_shared_state
		{
		public:
			std::atomic<int> ref;

			enum
			{
				not_ready = 0,
				has_value,
				has_error,
			} state;

			std::exception_ptr ex;

			mutex mtx;
			condition_variable cv;

			basic_shared_state()
				: ref(1), state(not_ready)
			{
			}

			virtual ~basic_shared_state()
			{
				if (ex)
				{
					try
					{
						std::rethrow_exception(ex);
					}
					catch (std::exception &ex)
					{
						printf("Warning: uncaught exception! %s\n", ex.what());
					}
					catch (...)
					{
						printf("Warning: uncaught exception!\n");
					}
				}
			}

			void addref()
			{
				++ref;
			}

			void release()
			{
				if (ref == 1)
				{
					delete this;
					return;
				}
				--ref;
			}

			void set_exception(std::exception_ptr ptr)
			{
				lock_guard<> guard(mtx);
				if (state != not_ready)
				{
					// TODO: Throw
				}

				state = has_error;
				ex = std::move(ptr);
				cv.notify_all();
			}

			void set()
			{
				lock_guard<> guard(mtx);
				if (state != not_ready)
				{
					// TODO: Throw
				}
				state = has_value;
				cv.notify_all();
			}

			void wait()
			{
				lock_guard<> guard(mtx);
				while (state == not_ready)
				{
					cv.wait(mtx);
				}

				if (ex)
				{
					auto ptr = std::move(ex);
					std::rethrow_exception(ptr);
				}
			}

			virtual void execute()
			{
				// Non invokable!
			}
		};

		template <typename T>
		class basic_shared_state_with_value : public basic_shared_state
		{
		public:
			T value;

		public:
			basic_shared_state_with_value()
			{
			}

			virtual ~basic_shared_state_with_value()
			{
			}

			void set(T &&val)
			{
				lock_guard<> guard(mtx);
				if (state != not_ready)
				{
					// TODO: Throw
				}
				state = has_value;
				value = std::move(val);
				cv.notify_all();
			}

			T get()
			{
				lock_guard<> guard(mtx);
				while (state == not_ready)
				{
					cv.wait(mtx);
				}

				if (ex)
				{
					auto ptr = std::move(ex);
					std::rethrow_exception(ptr);
				}

				return std::move(value);
			}


		};



		template <typename T, typename Fn>
		class shared_state : public basic_shared_state_with_value<T>
		{
		public:
			shared_state(Fn fn)
				: fn(std::move(fn))
			{
			}

			virtual ~shared_state()
			{
			}

			virtual void execute()
			{
				try
				{
					auto val = fn();
					set(std::move(val));
				}
				catch (...)
				{
					set_exception(std::current_exception());
				}
			}

			Fn fn;
		};

		template <typename Fn>
		class shared_state<void, Fn> : public basic_shared_state
		{
		public:
			shared_state(Fn fn)
				: fn(std::move(fn))
			{
			}

			virtual ~shared_state()
			{
			}

			virtual void execute()
			{
				try
				{
					fn();
					set();
				}
				catch (...)
				{
					set_exception(std::current_exception());
				}
			}

			Fn fn;
		};




		template <typename T> future<T> make_future(basic_shared_state_with_value<T> *state);
		future<void> make_future(basic_shared_state *state);


		inline void queue_work(basic_shared_state *state)
		{
			state->addref();

#ifdef _WIN32
			auto work = CreateThreadpoolWork([](PTP_CALLBACK_INSTANCE, PVOID context, PTP_WORK work)
			{
				auto state = static_cast<basic_shared_state *>(context);
				state->execute();
				state->release();
			}, state, nullptr);
			SubmitThreadpoolWork(work);

#else
			static_assert(false, "Get a threadpool");
#endif
		}


	}


	template <typename Fn> future<typename std::result_of<Fn()>::type> queue_work(Fn fn);


	template <typename T>
	class future
	{
	public:
		future()
			: state(nullptr)
		{
		}

		virtual ~future()
		{
			if (state)
			{
				state->release();
			}
		}

		future(future &&f)
			: state(f.state)
		{
			f.state = nullptr;
		}

		future &operator =(future &&f)
		{
			std::swap(state, f.state);
			return *this;
		}

		future(const future &&f) = delete;
		future &operator =(const future &f) = delete;

	public:
		void wait()
		{
			if (!state)
			{
				// TODO: Throw
			}
			state->wait();
		}

		T get()
		{
			if (!state)
			{
				// TODO: Throw
			}
			return state->get();
		}

		template<class Fn>
		auto then(Fn fn)
		{
			if (!state)
			{
				// TODO: Throw
			}

			lock_guard<> lock(state->mtx);
			if (state->state == details::basic_shared_state::not_ready) 
			{
				// Just queue a thread to wait on the cv.  
				return queue_work([f = std::move(*this), fn = std::move(fn)] () mutable
				{
					f.wait();
					return fn(std::move(f));
				});
			}
			else
			{
				return make_ready_future(fn(std::move(*this)));
			}
		}

	public:
		details::basic_shared_state_with_value<T> *state;

		template <typename T> friend future<T> details::make_future(details::basic_shared_state_with_value<T> *);
		friend class promise<T>;
	};



	template <>
	class future<void>
	{
	public:
		future()
			: state(nullptr)
		{
		}

		~future()
		{
			if (state)
			{
				state->release();
			}
		}

		future(future &&f)
			: state(f.state)
		{
			f.state = nullptr;
		}

		future &operator =(future &&f)
		{
			std::swap(state, f.state);
			return *this;
		}

		future(const future &&f) = delete;
		future &operator =(const future &f) = delete;

	public:
		void wait() 
		{
			if (!state)
			{
				// TODO: Throw
			}
			state->wait();
		}

		void get()
		{
			if (!state)
			{
				// TODO: Throw
			}
			state->wait();
		}

		template<class Fn>
		auto then(Fn fn)
		{
			if (!state)
			{
				// TODO: Throw
			}

			lock_guard<> lock(state->mtx);

			if (state->state == details::basic_shared_state::not_ready)
			{
				// Just queue a thread to wait on the cv.  
				// not the bes
				return queue_work([f = std::move(*this), fn = std::move(fn)]() mutable
				{
					f.wait();
					return fn(std::move(f));
				});
			}
			else
			{
				return make_ready_future(fn(std::move(*this)));
			}
		}

	private:
		details::basic_shared_state *state;

		friend future<void> details::make_future(basic_shared_state *);
		friend class promise<void>;
	};


	template <typename T> future<T> make_ready_future(T value)
	{
		auto state = new details::basic_shared_state_with_value<T>();
		auto f = details::make_future(state);
		state->set(std::move(value));
		state->release();
		return f;
	}
	
	inline future<void> make_ready_future()
	{
		auto state = new details::basic_shared_state();
		auto f = details::make_future(state);
		state->set();
		state->release();
		return f;
	}




	template <typename T>
	class promise
	{
	public:
		promise()
			: state(new details::basic_shared_state_with_value<T>())
		{
		}

		virtual ~promise()
		{
			if (state)
			{
				state->release();
			}
		}

		promise(promise &&p)
			: state(p.state)
		{
			p.state = nullptr;
		}

		promise &operator =(promise &&p)
		{
			std::swap(state, p.state);
			return *this;
		}

		promise(const promise &&) = delete;
		promise &operator =(const promise &) = delete;

	public:
		void set(T value)
		{
			if (!state)
			{
				// TODO: Throw
			}
			state->set(std::move(value));
		}

		void set_exception(std::exception_ptr err)
		{
			if (!state)
			{
				// TODO: Throw
			}
			state->set_exception(err);
		}

		future<T> get_future()
		{
			return details::make_future(state);
		}

	private:
		details::basic_shared_state_with_value<T> *state;
	};

	template <>
	class promise<void>
	{
	public:
		promise()
			: state(new details::basic_shared_state())
		{
		}

		virtual ~promise()
		{
			if (state)
			{
				state->release();
			}
		}

		promise(promise &&p)
			: state(p.state)
		{
			p.state = nullptr;
		}

		promise &operator =(promise &&p)
		{
			std::swap(state, p.state);
			return *this;
		}

		promise(const promise &&) = delete;
		promise &operator =(const promise &) = delete;

	public:
		void set()
		{
			if (!state)
			{
				// TODO: Throw
			}
			state->set();
		}

		void set_exception(std::exception_ptr err)
		{
			if (!state)
			{
				// TODO: Throw
			}
			state->set_exception(err);
		}

		future<void> get_future()
		{
			return details::make_future(state);
		}

	private:
		details::basic_shared_state *state;
	};





	template <typename T, typename Iterator> future<std::vector<T>> when_all(Iterator first, Iterator last)
	{
		promise<std::vector<T>> p;
		auto f = p.get_future();

		std::vector<T> list;
		std::exception_ptr ex;
		while (first != last)
		{
			try
			{
				list.push_back(first->get());
			}
			catch (...)
			{
				ex = std::current_exception();
			}

			first++;
		}

		if (ex)
		{
			p.set_exception(ex);
		}
		else
		{
			p.set(std::move(list));
		}

		return f;
	}




	namespace details
	{

		template <typename T> future<T> make_future(basic_shared_state_with_value<T> *state)
		{
			future<T> f;
			f.state = state;
			f.state->addref();
			return f;
		}

		inline future<void> make_future(basic_shared_state *state)
		{
			future<void> f;
			f.state = state;
			f.state->addref();
			return f;
		}

	}

	inline future<void> do_while(std::function<future<bool>()> body) 
	{
		return queue_work([=]
		{
			while (body().get())
			{
			}
		});
	}

}