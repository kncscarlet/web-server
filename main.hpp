#ifndef MAIN_HPP
#define MAIN_HPP
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <liburing.h>
#include <sys/epoll.h>
#include <cstring>
#include <mutex>
#include <cstdint>
#include <signal.h>
struct conn {
	int fd;
	char *buffer;
	time_t last_active;
};
struct main_state {
	int accept_fd;
	struct conn **conns;
	size_t conns_size;
	size_t used_conns;
	std::mutex conns_mutex;

	inline main_state(void):
		accept_fd(-1),
		conns(nullptr),
		conns_size(0),
		used_conns(0)
	{
	}
};
struct main_epoll {
	int epoll_fd;
	int timeout;
	size_t events_size;
	struct epoll_event *events;
	struct main_state *st;
	bool need_turn_on_accept;
};
int run_io_uring(struct main_state *st);
int run_epoll(struct main_state *st);
extern volatile bool main_running;
#endif
