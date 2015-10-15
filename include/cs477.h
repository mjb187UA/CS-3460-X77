
#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <WinSock2.h>
#include <MSWSock.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else

#include <pthread.h>

#endif



#include <exception>
#include <system_error>
#include <vector>
#include <list>
#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <cassert>


#include "thread.h"
#include "mutex.h"
#include "future.h"
#include "vector.h"
#include "queue.h"
#include "timer.h"

#include "details.h"

#include "thread.inl"
#include "mutex.inl"
#include "future.inl"
#include "vector.inl"
#include "queue.inl"

