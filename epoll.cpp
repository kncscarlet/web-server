#include "main.hpp"

static int add_fd(int epoll_fd, int fd, uint32_t events, void *data)
{
	struct epoll_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.ptr = data;
	return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

static int del_fd(int epoll_fd, int fd)
{
	return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

static void run_epoll_thread(struct main_state *st)
{

}

static int accept_conn(struct main_epoll *ep, uint32_t events)
{
	struct sockaddr_in6 addr;
	socklen_t addr_len = sizeof(addr);
	struct conn *c;
	int fd, r;

	if ((events & (EPOLLERR | EPOLLHUP)) != 0)
		return 1;

	fd = accept(ep->st->accept_fd, (struct sockaddr *)&addr, &addr_len);
	if (fd < 0) {
		r = errno;
		if (r == EAGAIN || r == EINTR)
			return 0;
		if (r == EMFILE || r == ENFILE || r == ENOMEM) {
			del_fd(ep->epoll_fd, ep->st->accept_fd);
			ep->need_turn_on_accept = true;
			return 0;
		}
		perror("accept");
		return 1;
	}

	std::unique_lock<std::mutex> lock(ep->st->conns_mutex);

	if (ep->st->used_conns == ep->st->conns_size) {
		size_t new_size = ep->st->conns_size ? ep->st->conns_size * 2 : 16;
		struct conn **new_conns = (struct conn **)realloc(ep->st->conns, new_size * sizeof(*new_conns));
		if (!new_conns) {
			perror("realloc");
			close(fd);
			return 1;
		}
		ep->st->conns = new_conns;
		ep->st->conns_size = new_size;
	}

	c = (struct conn *)calloc(1, sizeof(*c));
	if (!c) {
		perror("calloc");
		close(fd);
		return 1;
	}

	c->fd = fd;
	c->buffer = (char *)malloc(4096);
	if (!c->buffer) {
		perror("malloc");
		free(c);
		close(fd);
		return 1;
	}

	ep->st->conns[ep->st->used_conns++] = c;
	return 0;
}

static int regular_conn(struct main_epoll *ep, void *data, uint32_t events)
{
	return 0;
}

int run_epoll(struct main_state *st)
{
	struct main_epoll ep;

	ep.st = st;
	ep.timeout = -1;
	ep.events_size = 200;
	ep.events = (struct epoll_event *)calloc(ep.events_size, sizeof(*ep.events));
	if (!ep.events) {
		perror("calloc");
		return 1;
	}

	ep.epoll_fd = epoll_create(ep.events_size);
	if (ep.epoll_fd < 0) {
		perror("epoll_create");
		free(ep.events);
		return 1;
	}

	if (add_fd(ep.epoll_fd, st->accept_fd, EPOLLIN, NULL)) {
		perror("epoll_ctl -> add_fd");
		free(ep.events);
		close(ep.epoll_fd);
		return 1;
	}

	while (main_running) {
		int r, i;

		r = epoll_wait(ep.epoll_fd, ep.events, ep.events_size, ep.timeout);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			perror("epoll_wait");
			break;
		}

		for (i = 0; i < r; i++) {
			if (!ep.events[i].data.ptr)
				r = accept_conn(&ep, ep.events[i].events);
			else
				r = regular_conn(&ep, ep.events[i].data.ptr, ep.events[i].events);

			if (r)
				break;
		}
	}

	del_fd(ep.epoll_fd, st->accept_fd);
	free(ep.events);
	close(ep.epoll_fd);
	return 0;
}
