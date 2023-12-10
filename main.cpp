#include "main.hpp"

static const size_t init_conn_size = 1024;
static const uint16_t bind_port = 8080;

volatile bool main_running = true;

static int main_sock_init(struct main_state *st)
{
	struct sockaddr_in6 addr;
	int r;

	r = socket(AF_INET, SOCK_STREAM, 0);
	if (r < 0) {
		perror("socket");
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(bind_port);
	addr.sin6_addr = in6addr_any;
	st->accept_fd = r;
	r = bind(st->accept_fd, (struct sockaddr *)&addr, sizeof(addr));
	if (r < 0) {
		perror("bind");
		close(st->accept_fd);
		return 1;
	}

	r = listen(st->accept_fd, 1024);
	if (r < 0) {
		perror("listen");
		close(st->accept_fd);
		return 1;
	}

	return 0;
}

static void main_sock_free(struct main_state *st)
{
	close(st->accept_fd);
	st->accept_fd = -1;
}

static int main_init_conns(struct main_state *st)
{
	st->conns_size = init_conn_size;
	st->conns = (struct conn **)calloc(st->conns_size, sizeof(*st->conns));
	if (!st->conns) {
		perror("calloc");
		return 1;
	}

	return 0;
}

static void main_free_conns(struct main_state *st)
{
	size_t i;

	for (i = 0; i < st->conns_size; i++) {
		if (st->conns[i]) {
			if (st->conns[i]->fd >= 0)
				close(st->conns[i]->fd);
			free(st->conns[i]->buffer);
			free(st->conns[i]);
		}
	}

	free(st->conns);
	st->conns = nullptr;
}

static int main_run(struct main_state *st)
{
	const char *we = getenv("WAIT_EVENT");

	if (!we || !strcmp(we, "epoll"))
		return run_epoll(st);

	if (!strcmp(we, "io_uring"))
		return run_io_uring(st);

	fprintf(stderr, "unknown WAIT_EVENT: %s\n", we);
	return 1;
}

int
main(int argc, char *argv[])
{
	struct main_state st;
	int r;

	if (main_sock_init(&st))
		return 1;

	if (main_init_conns(&st)) {
		main_sock_free(&st);
		return 1;
	}

	r = 1;
	if (signal(SIGINT, [](int) { main_running = false; }) == SIG_ERR) {
		perror("signal");
		goto out;
	}

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("signal");
		goto out;
	}

	r = main_run(&st);
out:
	main_free_conns(&st);
	main_sock_free(&st);
	return r;
}
