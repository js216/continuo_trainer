// SPDX-License-Identifier: MIT
// run.c --- parallel process graph executor
// Copyright (c) 2026 Jakob Kastelic

/* DESCRIPTION
 * run.c is a C99/POSIX utility that executes a directed graph of
 * interconnected processes. It parses a DOT-subset file to wire up
 * standard streams (stdin/stdout) between commands using pipes.
 * It provides real-time monitoring: if any process writes to stderr
 * or exits with an error, the entire graph is terminated via SIGTERM.
 *
 * ARGUMENTS
 * arg[1]: Path to the graph definition file (e.g., "pipeline.dot").
 *
 * GRAPH SYNTAX
 * Statements follow the DOT format: "NodeA" -> "NodeB";
 * - Nodes can be shell commands or special identifiers.
 * - Arrows represent a pipe from the source's stdout to the sink's stdin.
 * - Multiple sinks (fan-out) or multiple sources (fan-in) are supported.
 * - Statements must terminate with a semicolon (;).
 *
 * SPECIAL NODES
 * STDIN       : The host terminal's standard input.
 * STDOUT      : The host terminal's standard output (line-buffered).
 * STDOUT_IMM  : Immediate terminal output (byte-by-byte, no buffering).
 *
 * BUILD
 * Linux:  gcc -std=c99 -O2 -o run run.c -lpthread -lutil
 * macOS:  clang -std=c99 -O2 -o run run.c -lpthread
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 600 /* usleep */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <pty.h>
#else
#include <util.h>
#endif

#define MAX_NODES 256
#define MAX_EDGES 512
#define MAX_ARGV 64
#define MAX_CHILDREN 256
#define LINE_BUF 65536

static void die(const char *msg)
{
	fprintf(stderr, "\x1b[31mError:\x1b[0m %s\n", msg);
	exit(1);
}

static volatile int g_terminating;
static pthread_mutex_t g_term_mu = PTHREAD_MUTEX_INITIALIZER;

static pid_t g_children[MAX_CHILDREN];
static int g_nchildren;
static pthread_mutex_t g_children_mu = PTHREAD_MUTEX_INITIALIZER;

static void register_child(pid_t pid)
{
	pthread_mutex_lock(&g_children_mu);
	if (g_nchildren < MAX_CHILDREN)
		g_children[g_nchildren++] = pid;
	pthread_mutex_unlock(&g_children_mu);
}

static void terminate_all(int exit_code)
{
	pthread_mutex_lock(&g_term_mu);
	if (g_terminating) {
		pthread_mutex_unlock(&g_term_mu);
		struct timespec ts = {0, 100000000L};
		nanosleep(&ts, NULL);
		exit(exit_code);
	}
	g_terminating = 1;
	pthread_mutex_unlock(&g_term_mu);

	pthread_mutex_lock(&g_children_mu);
	for (int i = 0; i < g_nchildren; i++)
		kill(g_children[i], SIGTERM);
	pthread_mutex_unlock(&g_children_mu);

	exit(exit_code);
}

static int fd_recv_line(int rd, char *buf, size_t bufsz)
{
	size_t pos = 0;
	char ch;
	while (pos + 1 < bufsz) {
		ssize_t n = read(rd, &ch, 1);
		if (n <= 0)
			return 0;
		if (ch == '\n') {
			buf[pos] = '\0';
			return 1;
		}
		buf[pos++] = ch;
	}
	buf[pos] = '\0';
	return 1;
}

typedef enum { NT_STDIN, NT_STDOUT, NT_STDOUT_IMM, NT_PROG } NodeKind;
typedef struct {
	NodeKind kind;
	char cmd[512];
} Node;
typedef struct {
	int from;
	int to;
} Edge;

static int node_eq(const Node *a, const Node *b)
{
	if (a->kind != b->kind)
		return 0;
	return (a->kind == NT_PROG) ? strcmp(a->cmd, b->cmd) == 0 : 1;
}

static int intern_node(Node *nodes, int *n, const char *name)
{
	Node tmp;
	if (strcmp(name, "STDIN") == 0)
		tmp.kind = NT_STDIN;
	else if (strcmp(name, "STDOUT") == 0)
		tmp.kind = NT_STDOUT;
	else if (strcmp(name, "STDOUT_IMM") == 0)
		tmp.kind = NT_STDOUT_IMM;
	else {
		tmp.kind = NT_PROG;
		snprintf(tmp.cmd, sizeof tmp.cmd, "%s", name);
	}

	for (int i = 0; i < *n; i++)
		if (node_eq(&nodes[i], &tmp))
			return i;
	if (*n >= MAX_NODES)
		die("too many nodes");
	nodes[(*n)++] = tmp;
	return *n - 1;
}

static void strip_comments(char *s)
{
	char *r = s, *w = s;
	while (*r) {
		if (r[0] == '/' && r[1] == '/') {
			while (*r && *r != '\n')
				r++;
		} else if (r[0] == '/' && r[1] == '*') {
			r += 2;
			while (*r && !(r[0] == '*' && r[1] == '/')) {
				if (*r == '\n')
					*w++ = '\n';
				r++;
			}
			if (*r)
				r += 2;
		} else
			*w++ = *r++;
	}
	*w = '\0';
}

static int split_arrow(char *s, char **parts, int max_parts)
{
	int n = 0;
	char *start = s;
	int in_q = 0;
	char *p = s;
	while (*p) {
		if (*p == '"') {
			in_q = !in_q;
			p++;
			continue;
		}
		if (!in_q && p[0] == '-' && p[1] == '>') {
			if (n >= max_parts - 1)
				die("too many nodes in statement");
			*p = '\0';
			parts[n++] = start;
			p += 2;
			start = p;
		} else
			p++;
	}
	parts[n++] = start;
	return n;
}

static char *trim(char *s)
{
	while (isspace((unsigned char)*s))
		s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		e--;
	*e = '\0';
	return s;
}

static void parse_graph(char *src, Node *nodes, int *n_nodes, Edge *edges,
			int *n_edges)
{
	strip_comments(src);
	for (char *p = src; *p; p++)
		if (*p == '\n' || *p == '\r')
			*p = ' ';
	char *stmt = src;
	for (;;) {
		char *semi = strchr(stmt, ';');
		if (!semi) {
			if (*trim(stmt))
				die("statement not terminated with ';'");
			break;
		}
		*semi = '\0';
		char *t = trim(stmt);
		if (*t) {
			char *parts[MAX_NODES];
			int np = split_arrow(t, parts, MAX_NODES);
			if (np < 2)
				die("expected '->'");
			for (int i = 0; i + 1 < np; i++) {
				char ta[512], tb[512];
				snprintf(ta, sizeof ta, "%s", trim(parts[i]));
				snprintf(tb, sizeof tb, "%s",
					 trim(parts[i + 1]));
				char *ca = ta, *cb = tb;
				if (ca[0] == '"') {
					ca++;
					ca[strlen(ca) - 1] = '\0';
				}
				if (cb[0] == '"') {
					cb++;
					cb[strlen(cb) - 1] = '\0';
				}
				int ai = intern_node(nodes, n_nodes, ca);
				int bi = intern_node(nodes, n_nodes, cb);
				if (*n_edges >= MAX_EDGES)
					die("too many edges");
				edges[*n_edges].from = ai;
				edges[*n_edges].to = bi;
				(*n_edges)++;
			}
		}
		stmt = semi + 1;
	}
}

static char *parse_argv(const char *cmd, char **out, int max_out)
{
	char *buf = malloc(strlen(cmd) + 1);
	char *wp = buf;
	int n = 0, inq = 0, in_token = 0;
	for (const char *p = cmd;; p++) {
		char c = *p;
		if (c == '"') {
			inq = !inq;
			continue;
		}
		if ((c == ' ' || c == '\0') && !inq) {
			if (in_token) {
				*wp++ = '\0';
				n++;
				in_token = 0;
			}
			if (c == '\0')
				break;
			continue;
		}
		if (!in_token) {
			if (n >= max_out - 1)
				die("too many tokens");
			out[n] = wp;
			in_token = 1;
		}
		*wp++ = c;
	}
	out[n] = NULL;
	return buf;
}

typedef struct {
	int *data;
	int n, cap;
} IntList;
static void il_push(IntList *l, int v)
{
	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 4;
		l->data = realloc(l->data, (size_t)l->cap * sizeof(int));
	}
	l->data[l->n++] = v;
}

typedef struct {
	int src_rd;
	int *dst_wrs;
	int n_dst;
} FanoutArg;
static void *thr_fanout(void *arg)
{
	FanoutArg *a = arg;
	char buf[LINE_BUF];
	ssize_t n;
	while (!g_terminating && (n = read(a->src_rd, buf, sizeof buf)) > 0) {
		int alive = 0;
		for (int i = 0; i < a->n_dst; i++) {
			if (a->dst_wrs[i] < 0)
				continue;
			if (write(a->dst_wrs[i], buf, (size_t)n) != n) {
				close(a->dst_wrs[i]);
				a->dst_wrs[i] = -1;
			} else
				alive = 1;
		}
		if (!alive)
			break;
	}
	close(a->src_rd);
	for (int i = 0; i < a->n_dst; i++)
		if (a->dst_wrs[i] >= 0)
			close(a->dst_wrs[i]);
	free(a->dst_wrs);
	free(a);
	return NULL;
}

typedef struct {
	int src_rd;
	int dst_wr;
	pthread_mutex_t *mu;
} FaninSubArg;
static void *thr_fanin_sub(void *arg)
{
	FaninSubArg *a = arg;
	char buf[LINE_BUF];
	ssize_t n;
	while (!g_terminating && (n = read(a->src_rd, buf, sizeof buf)) > 0) {
		pthread_mutex_lock(a->mu);
		ssize_t nw = write(a->dst_wr, buf, (size_t)n);
		pthread_mutex_unlock(a->mu);
		if (nw != n)
			break;
	}
	close(a->src_rd);
	free(a);
	return NULL;
}

typedef struct {
	int *src_rds;
	int n_src;
	int dst_wr;
} FaninArg;
static void *thr_fanin(void *arg)
{
	FaninArg *a = arg;
	pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
	pthread_t *subs = malloc((size_t)a->n_src * sizeof(pthread_t));
	for (int i = 0; i < a->n_src; i++) {
		FaninSubArg *sa = malloc(sizeof *sa);
		sa->src_rd = a->src_rds[i];
		sa->dst_wr = a->dst_wr;
		sa->mu = &mu;
		pthread_create(&subs[i], NULL, thr_fanin_sub, sa);
	}
	for (int i = 0; i < a->n_src; i++)
		pthread_join(subs[i], NULL);
	close(a->dst_wr);
	free(subs);
	free(a->src_rds);
	free(a);
	return NULL;
}

void *thr_stdin_pump(void *arg)
{
	int dst = (int)(intptr_t)arg;
	char buf[LINE_BUF];
	ssize_t n;
	while (!g_terminating &&
	       (n = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
		if (write(dst, buf, (size_t)n) != n)
			break;
	}
	close(dst);
	return NULL;
}

typedef struct {
	int src_rd;
} StdoutArg;
static void *thr_stdout(void *arg)
{
	StdoutArg *a = arg;
	char line[LINE_BUF];
	while (!g_terminating && fd_recv_line(a->src_rd, line, sizeof line)) {
		puts(line);
		fflush(stdout);
	}
	close(a->src_rd);
	free(a);
	return NULL;
}

static void *thr_stdout_imm(void *arg)
{
	StdoutArg *a = arg;
	char buf[4096];
	ssize_t n;
	while (!g_terminating && (n = read(a->src_rd, buf, sizeof buf)) > 0) {
		if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n)
			break;
		fflush(stdout);
	}
	close(a->src_rd);
	free(a);
	return NULL;
}

typedef struct {
	pid_t pid;
	char cmd[64];
} WaitArg;
void *thr_wait(void *arg)
{
	WaitArg *a = arg;
	int status;
	if (waitpid(a->pid, &status, 0) < 0) {
		if (!g_terminating) {
			fprintf(stderr,
				"\x1b[31mError:\x1b[0m waitpid('%s'): %s\n",
				a->cmd, strerror(errno));
			terminate_all(1);
		}
	} else if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) == 0)
			terminate_all(0); // Quiet success
		else if (!g_terminating) {
			fprintf(stderr,
				"\x1b[31mError:\x1b[0m `%s` exited status %d\n",
				a->cmd, WEXITSTATUS(status));
			terminate_all(1);
		}
	} else if (WIFSIGNALED(status) && !g_terminating) {
		fprintf(stderr,
			"\x1b[31mError:\x1b[0m `%s` killed by signal %d\n",
			a->cmd, WTERMSIG(status));
		terminate_all(1);
	}
	free(a);
	return NULL;
}

static void run(Node *nodes, int n_nodes, Edge *edges, int n_edges)
{
	IntList *node_out = calloc((size_t)n_nodes, sizeof(IntList));
	IntList *node_in = calloc((size_t)n_nodes, sizeof(IntList));
	for (int i = 0; i < n_edges; i++) {
		il_push(&node_out[edges[i].from], i);
		il_push(&node_in[edges[i].to], i);
	}

	int *pipe_rd = malloc((size_t)n_edges * sizeof(int));
	int *pipe_wr = malloc((size_t)n_edges * sizeof(int));
	for (int i = 0; i < n_edges; i++) {
		int fds[2];
		if (pipe(fds))
			die("pipe()");
		pipe_rd[i] = fds[0];
		pipe_wr[i] = fds[1];
	}

	pthread_t *threads =
	    malloc((size_t)(n_nodes * 6 + 2) * sizeof(pthread_t));
	int n_threads = 0;
#define SPAWN(fn, arg) pthread_create(&threads[n_threads++], NULL, fn, arg)

	for (int ni = 0; ni < n_nodes; ni++) {
		if (nodes[ni].kind != NT_STDIN)
			continue;
		IntList *outs = &node_out[ni];
		if (outs->n == 0)
			continue;
		int relay[2];
		if (pipe(relay))
			die("pipe(stdin relay)");
		FanoutArg *fa = malloc(sizeof *fa);
		fa->src_rd = relay[0];
		fa->n_dst = outs->n;
		fa->dst_wrs = malloc((size_t)outs->n * sizeof(int));
		for (int j = 0; j < outs->n; j++)
			fa->dst_wrs[j] = pipe_wr[outs->data[j]];
		SPAWN(thr_fanout, fa);
		SPAWN(thr_stdin_pump, (void *)(intptr_t)relay[1]);
	}

	for (int ni = 0; ni < n_nodes; ni++) {
		if (nodes[ni].kind != NT_STDOUT &&
		    nodes[ni].kind != NT_STDOUT_IMM)
			continue;
		IntList *ins = &node_in[ni];
		for (int j = 0; j < ins->n; j++) {
			StdoutArg *a = malloc(sizeof *a);
			a->src_rd = pipe_rd[ins->data[j]];
			if (nodes[ni].kind == NT_STDOUT_IMM)
				SPAWN(thr_stdout_imm, a);
			else
				SPAWN(thr_stdout, a);
		}
	}

	for (int ni = 0; ni < n_nodes; ni++) {
		if (nodes[ni].kind != NT_PROG)
			continue;
		IntList *outs = &node_out[ni], *ins = &node_in[ni];
		char cmd_copy[512];
		snprintf(cmd_copy, sizeof cmd_copy, "%s", nodes[ni].cmd);
		char *argv_arr[MAX_ARGV];
		char *argv_buf = parse_argv(cmd_copy, argv_arr, MAX_ARGV);
		if (argv_arr[0] == NULL) {
			free(argv_buf);
			die("empty command");
		}

		int cin_rd = -1, cin_wr = -1, cout_rd = -1, cout_wr = -1;
		if (ins->n > 0) {
			int fds[2];
			if (pipe(fds))
				die("pipe(stdin)");
			cin_rd = fds[0];
			cin_wr = fds[1];
		}
		if (outs->n > 0) {
			int master, slave;
			if (openpty(&master, &slave, NULL, NULL, NULL) == 0) {
				cout_rd = master;
				cout_wr = slave;
			} else {
				int fds[2];
				if (pipe(fds))
					die("pipe(stdout)");
				cout_rd = fds[0];
				cout_wr = fds[1];
			}
		}

		pid_t pid = fork();
		if (pid == 0) {
			if (cin_rd >= 0)
				dup2(cin_rd, STDIN_FILENO);
			else {
				int n = open("/dev/null", O_RDONLY);
				dup2(n, STDIN_FILENO);
				close(n);
			}
			if (cout_wr >= 0)
				dup2(cout_wr, STDOUT_FILENO);
			else {
				int n = open("/dev/null", O_WRONLY);
				dup2(n, STDOUT_FILENO);
				close(n);
			}
			// STDERR is inherited directly from parent for
			// unbuffered forwarding
			if (cin_rd >= 0)
				close(cin_rd);
			if (cin_wr >= 0)
				close(cin_wr);
			if (cout_rd >= 0)
				close(cout_rd);
			if (cout_wr >= 0)
				close(cout_wr);
			for (int i = 0; i < n_edges; i++) {
				close(pipe_rd[i]);
				close(pipe_wr[i]);
			}
			execvp(argv_arr[0], argv_arr);
			_exit(127);
		}
		if (cin_rd >= 0)
			close(cin_rd);
		if (cout_wr >= 0)
			close(cout_wr);
		register_child(pid);

		if (ins->n > 0) {
			FaninArg *fa = malloc(sizeof *fa);
			fa->n_src = ins->n;
			fa->dst_wr = cin_wr;
			fa->src_rds = malloc((size_t)ins->n * sizeof(int));
			for (int j = 0; j < ins->n; j++)
				fa->src_rds[j] = pipe_rd[ins->data[j]];
			SPAWN(thr_fanin, fa);
		}
		if (outs->n > 0) {
			FanoutArg *fa = malloc(sizeof *fa);
			fa->src_rd = cout_rd;
			fa->n_dst = outs->n;
			fa->dst_wrs = malloc((size_t)outs->n * sizeof(int));
			for (int j = 0; j < outs->n; j++)
				fa->dst_wrs[j] = pipe_wr[outs->data[j]];
			SPAWN(thr_fanout, fa);
		} else if (cout_rd >= 0)
			close(cout_rd);

		WaitArg *wa = malloc(sizeof *wa);
		wa->pid = pid;
		snprintf(wa->cmd, sizeof wa->cmd, "%s", argv_arr[0]);
		SPAWN(thr_wait, wa);
		free(argv_buf);
	}
#undef SPAWN
	for (int i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);
	free(threads);
	for (int i = 0; i < n_nodes; i++) {
		free(node_out[i].data);
		free(node_in[i].data);
	}
	free(node_out);
	free(node_in);
	free(pipe_rd);
	free(pipe_wr);
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <graph.dot>\n", argv[0]);
		return 1;
	}
	FILE *f = fopen(argv[1], "r");
	if (!f)
		die("fopen");
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	rewind(f);
	char *src = malloc((size_t)sz + 1);
	if (fread(src, 1, (size_t)sz, f) != (size_t)sz)
		die("fread");
	src[sz] = '\0';
	fclose(f);
	static Node nodes[MAX_NODES];
	static Edge edges[MAX_EDGES];
	int n_nodes = 0, n_edges = 0;
	parse_graph(src, nodes, &n_nodes, edges, &n_edges);
	free(src);
	if (n_edges > 0)
		run(nodes, n_nodes, edges, n_edges);
	return 0;
}
