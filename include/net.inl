#include "net.h"


namespace cs477
{

	namespace details
	{
		inline void async_accept::call_handler()
		{
			net::socket s;
			s.sock = sock;
			sock = nullptr;
			fn(s);
		}

	}


	namespace net
	{

		inline void initialize()
		{
			WSADATA data;
			WSAStartup(MAKEWORD(2, 2), &data);
		}

		inline void finalize()
		{
			WSACleanup();
		}

		inline sockaddr_in resolve_address(const std::string &hostname, int port)
		{
			char service[12];
			sprintf_s(service, "%d", port);

			addrinfo hint = { 0 };
			hint.ai_family = AF_INET;

			addrinfo *info = nullptr;
			int err = getaddrinfo(hostname.c_str(), service, &hint, &info);
			if (err == SOCKET_ERROR)
			{
				throw std::exception();
			}

			auto addr = *(sockaddr_in *)info->ai_addr;
			freeaddrinfo(info);

			return addr;
		}





		inline socket::socket()
			: sock(nullptr)
		{
		}

		inline socket::~socket()
		{
			if (sock)
			{
				sock->release();
			}
		}

		inline socket::socket(socket &&sock)
			: sock(sock.sock)
		{
			sock.sock = nullptr;
		}

		inline socket &socket::operator=(socket &&sock)
		{
			std::swap(this->sock, sock.sock);
			return *this;
		}

		inline socket::socket(const socket &sock)
			: sock(sock.sock)
		{
			this->sock->addref();
		}

		inline socket &socket::operator=(const socket &sock)
		{
			if (this->sock != sock.sock)
			{
				if (this->sock) 
				{
					this->sock->release();
				}
				this->sock = sock.sock;
				if (this->sock)
				{
					this->sock->addref();
				}
			}
			return *this;
		}

		inline void socket::connect(const sockaddr_in &addr)
		{
			sock = new details::socket;
			sock->create(INVALID_SOCKET);

			auto err = ::connect(sock->handle, (sockaddr *)&addr, sizeof(sockaddr_in));
			if (err == SOCKET_ERROR)
			{
				throw std::exception();
			}
		}

		inline void socket::send(const char *buf, size_t len)
		{
			if (!sock) 
			{
				throw std::exception();
			}

			auto ptr = buf;
			auto end = buf + len;

			while (ptr < end)
			{
				auto bytes = static_cast<int>(min(end - ptr, 1048576));
				auto sent = ::send(sock->handle, ptr, bytes, 0);
				if (sent == SOCKET_ERROR || sent == 0)
				{
					throw std::exception();
				}
				ptr += sent;
			}
		}

		inline size_t socket::recv(char *buf, size_t len)
		{
			if (!sock)
			{
				throw std::exception();
			}

			auto recvd = ::recv(sock->handle, buf, static_cast<int>(len), 0);
			if (recvd == SOCKET_ERROR || recvd == 0)
			{
				throw std::exception();
			}

			return static_cast<size_t>(recvd);
		}

		inline future<void> socket::send_async(const char *buf, size_t len)
		{
			if (!sock)
			{
				throw std::exception();
			}

			return sock->send(buf, len);
		}

		inline future<std::string> socket::recv_async()
		{
			if (!sock)
			{
				throw std::exception();
			}

			return sock->recv(65536);
		}

		inline acceptor::acceptor()
			: sock(nullptr)
		{
		}

		inline acceptor::~acceptor()
		{
			if (sock)
			{
				sock->release();
			}
		}

		inline void acceptor::listen(const sockaddr_in &addr)
		{
			sock = new details::socket();
			sock->create(INVALID_SOCKET);

			auto err = ::bind(sock->handle, (sockaddr *)&addr, sizeof(sockaddr_in));
			if (err == SOCKET_ERROR)
			{
				throw std::exception();
			}

			err = ::listen(sock->handle, SOMAXCONN);
			if (err == SOCKET_ERROR)
			{
				throw std::exception();
			}
		}

		inline socket acceptor::accept()
		{
			auto s = ::accept(sock->handle, nullptr, 0);
			if (s == INVALID_SOCKET)
			{
				throw std::exception();
			}

			socket sock;
			sock.sock = new details::socket();
			sock.sock->create(s);

			return sock;
		}

		template <typename Fn> void acceptor::accept_async(Fn fn)
		{
			sock->accept(fn);
		}
	}


}

