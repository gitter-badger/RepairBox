#include <sys/stat.h>

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "server.h"
#include "log.h"
#include "stream.h"
#include "plugin.h"
#include "configparser.h"
#include "configfile.h"
#include "proc_open.h"

#include "sys-files.h"
#include "sys-process.h"

#ifndef PATH_MAX
/* win32 */
#define PATH_MAX 64
#endif

static int config_insert(server *srv) {
	size_t i;
	int ret = 0;
	buffer *stat_cache_string;

	config_values_t cv[] = {
		{ "server.bind",                 NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 0 */
		{ "server.errorlog",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 1 */
		{ "server.errorfile-prefix",     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 2 */
		{ "server.chroot",               NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 3 */
		{ "server.username",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 4 */
		{ "server.groupname",            NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 5 */
		{ "server.port",                 NULL, T_CONFIG_SHORT,  T_CONFIG_SCOPE_SERVER },      /* 6 */
		{ "server.tag",                  NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 7 */
		{ "server.use-ipv6",             NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 8 */
		{ "server.modules",              NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_SERVER },       /* 9 */

		{ "server.event-handler",        NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 10 */
		{ "server.pid-file",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 11 */
		{ "server.max-request-size",     NULL, T_CONFIG_INT, T_CONFIG_SCOPE_CONNECTION },     /* 12 */
		{ "server.max-worker",           NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },       /* 13 */
		{ "server.document-root",        NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 14 */
		{ "server.force-lowercase-filenames", NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },   /* 15 */
		{ "debug.log-condition-handling", NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },    /* 16 */
		{ "server.max-keep-alive-requests", NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION }, /* 17 */
		{ "server.name",                 NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 18 */
		{ "server.max-keep-alive-idle",  NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 19 */

		{ "server.max-read-idle",        NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 20 */
		{ "server.max-write-idle",       NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 21 */
		{ "server.error-handler-404",    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 22 */
		{ "server.max-fds",              NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },       /* 23 */
#ifdef HAVE_LSTAT
		{ "server.follow-symlink",       NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 24 */
#else
		{ "server.follow-symlink",
		  "Your system lacks lstat(). We cant differ symlinks from files."
		  "Please remove server.follow-symlinks from your config.",
		  T_CONFIG_UNSUPPORTED, T_CONFIG_SCOPE_UNSET }, /* 24 */
#endif
		{ "server.kbytes-per-second",    NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 25 */
		{ "connection.kbytes-per-second", NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },  /* 26 */
		{ "mimetype.use-xattr",          NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 27 */
		{ "mimetype.assign",             NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },   /* 28 */
		{ "ssl.pemfile",                 NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 29 */

		{ "ssl.engine",                  NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 30 */

		{ "debug.log-file-not-found",    NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 31 */
		{ "debug.log-request-handling",  NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 32 */
		{ "debug.log-response-header",   NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 33 */
		{ "debug.log-request-header",    NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 34 */

		{ "server.protocol-http11",      NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 35 */
		{ "debug.log-request-header-on-error", NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER }, /* 36 */
		{ "debug.log-state-handling",    NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 37 */
		{ "ssl.ca-file",                 NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 38 */

		{ "server.errorlog-use-syslog",  NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 39 */
		{ "server.range-requests",       NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 40 */
		{ "server.stat-cache-engine",    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 41 */
		{ "server.max-connections",      NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },       /* 42 */
		{ "server.network-backend",      NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 43 */
		{ "server.upload-dirs",          NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },   /* 44 */
		{ "server.core-files",           NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 45 */
		{ "debug.log-condition-cache-handling", NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },    /* 46 */
		{ "server.use-noatime",          NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 49 */
		{ "server.max-stat-threads",     NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 48 */
		{ "server.max-read-threads",    NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 49 */
		{ "server.max-connection-idle",  NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 50 */
		{ "debug.log-timing",            NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 51 */

		{ "server.host",                 "use server.bind instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.docroot",              "use server.document-root instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.virtual-root",         "load mod_simple_vhost and use simple-vhost.server-root instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.virtual-default-host", "load mod_simple_vhost and use simple-vhost.default-host instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.virtual-docroot",      "load mod_simple_vhost and use simple-vhost.document-root instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.userid",               "use server.username instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.groupid",              "use server.groupname instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.use-keep-alive",       "use server.max-keep-alive-requests = 0 instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },
		{ "server.force-lower-case-files",       "use server.force-lowercase-filenames instead", T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_UNSET },

		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};


	/* 0 */
	cv[0].destination = srv->srvconf.bindhost;
	cv[1].destination = srv->srvconf.errorlog_file;
	cv[3].destination = srv->srvconf.changeroot;
	cv[4].destination = srv->srvconf.username;
	cv[5].destination = srv->srvconf.groupname;
	cv[6].destination = &(srv->srvconf.port);

	cv[9].destination = srv->srvconf.modules;
	cv[10].destination = srv->srvconf.event_handler;
	cv[11].destination = srv->srvconf.pid_file;

	cv[13].destination = &(srv->srvconf.max_worker);
	cv[23].destination = &(srv->srvconf.max_fds);
	cv[36].destination = &(srv->srvconf.log_request_header_on_error);
	cv[37].destination = &(srv->srvconf.log_state_handling);

	cv[39].destination = &(srv->srvconf.errorlog_use_syslog);

	stat_cache_string = buffer_init();
	cv[41].destination = stat_cache_string;
	cv[43].destination = srv->srvconf.network_backend;
	cv[44].destination = srv->srvconf.upload_tempdirs;
	cv[45].destination = &(srv->srvconf.enable_cores);

	cv[42].destination = &(srv->srvconf.max_conns);
	cv[12].destination = &(srv->srvconf.max_request_size);
	cv[47].destination = &(srv->srvconf.use_noatime);
	cv[48].destination = &(srv->srvconf.max_stat_threads);
	cv[49].destination = &(srv->srvconf.max_read_threads);
	
	cv[51].destination = &(srv->srvconf.log_timing);

	srv->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	assert(srv->config_storage);

	for (i = 0; i < srv->config_context->used; i++) {
		specific_config *s;

		s = calloc(1, sizeof(specific_config));
		assert(s);
		s->document_root = buffer_init();
		s->mimetypes     = array_init();
		s->server_name   = buffer_init();
		s->ssl_pemfile   = buffer_init();
		s->ssl_ca_file   = buffer_init();
		s->error_handler = buffer_init();
		s->server_tag    = buffer_init();
		s->errorfile_prefix = buffer_init();
		s->max_keep_alive_requests = 16;
		s->max_keep_alive_idle = 5;
		s->max_read_idle = 60;
		s->max_write_idle = 360;
		s->max_connection_idle = 360;
		s->use_xattr     = 0;
		s->is_ssl        = 0;
		s->use_ipv6      = 0;
#ifdef HAVE_LSTAT
		s->follow_symlink = 1;
#endif
		s->kbytes_per_second = 0;
		s->allow_http11  = 1;
		s->range_requests = 1;
		s->force_lowercase_filenames = 0;
		s->global_kbytes_per_second = 0;
		s->global_bytes_per_second_cnt = 0;
		s->global_bytes_per_second_cnt_ptr = &s->global_bytes_per_second_cnt;

		cv[2].destination = s->errorfile_prefix;

		cv[7].destination = s->server_tag;
		cv[8].destination = &(s->use_ipv6);


		/* 13 max-worker */
		cv[14].destination = s->document_root;
		cv[15].destination = &(s->force_lowercase_filenames);
		cv[16].destination = &(s->log_condition_handling);
		cv[46].destination = &(s->log_condition_cache_handling);
		cv[17].destination = &(s->max_keep_alive_requests);
		cv[18].destination = s->server_name;
		cv[19].destination = &(s->max_keep_alive_idle);
		cv[20].destination = &(s->max_read_idle);
		cv[21].destination = &(s->max_write_idle);
		cv[22].destination = s->error_handler;
#ifdef HAVE_LSTAT
		cv[24].destination = &(s->follow_symlink);
#endif
		/* 23 -> max-fds */
		cv[25].destination = &(s->global_kbytes_per_second);
		cv[26].destination = &(s->kbytes_per_second);
		cv[27].destination = &(s->use_xattr);
		cv[28].destination = s->mimetypes;
		cv[29].destination = s->ssl_pemfile;
		cv[30].destination = &(s->is_ssl);

		cv[31].destination = &(s->log_file_not_found);
		cv[32].destination = &(s->log_request_handling);
		cv[33].destination = &(s->log_response_header);
		cv[34].destination = &(s->log_request_header);

		cv[35].destination = &(s->allow_http11);
		cv[38].destination = s->ssl_ca_file;
		cv[40].destination = &(s->range_requests);

		cv[50].destination = &(s->max_connection_idle);

		srv->config_storage[i] = s;

		if (0 != (ret = config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv))) {
			break;
		}
	}

	if (buffer_is_empty(stat_cache_string)) {
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_SIMPLE;
	} else if (buffer_is_equal_string(stat_cache_string, CONST_STR_LEN("simple"))) {
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_SIMPLE;
	} else if (buffer_is_equal_string(stat_cache_string, CONST_STR_LEN("fam"))) {
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_FAM;
	} else if (buffer_is_equal_string(stat_cache_string, CONST_STR_LEN("inotify"))) {
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_INOTIFY;
	} else if (buffer_is_equal_string(stat_cache_string, CONST_STR_LEN("disable"))) {
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_NONE;
	} else {
		log_error_write(srv, __FILE__, __LINE__, "sb",
				"server.stat-cache-engine can be one of \"disable\", \"simple\", \"fam\", \"inotify\" but not:", stat_cache_string);
		ret = HANDLER_ERROR;
	}

	buffer_free(stat_cache_string);

	return ret;

}

#define PATCH(x) \
	con->conf.x = s->x
int config_setup_connection(server *srv, connection *con) {
	specific_config *s = srv->config_storage[0];

	PATCH(allow_http11);
	PATCH(mimetypes);
	PATCH(document_root);
	PATCH(max_keep_alive_requests);
	PATCH(max_keep_alive_idle);
	PATCH(max_read_idle);
	PATCH(max_write_idle);
	PATCH(max_connection_idle);
	PATCH(use_xattr);
	PATCH(error_handler);
	PATCH(errorfile_prefix);
#ifdef HAVE_LSTAT
	PATCH(follow_symlink);
#endif
	PATCH(server_tag);
	PATCH(kbytes_per_second);
	PATCH(global_kbytes_per_second);
	PATCH(global_bytes_per_second_cnt);

	con->conf.global_bytes_per_second_cnt_ptr = &s->global_bytes_per_second_cnt;
	buffer_copy_string_buffer(con->server_name, s->server_name);

	PATCH(log_request_header);
	PATCH(log_response_header);
	PATCH(log_request_handling);
	PATCH(log_condition_handling);
	PATCH(log_condition_cache_handling);
	PATCH(log_file_not_found);

	PATCH(range_requests);
	PATCH(force_lowercase_filenames);
	PATCH(is_ssl);

	PATCH(ssl_pemfile);
	PATCH(ssl_ca_file);
	return 0;
}

int config_patch_connection(server *srv, connection *con, comp_key_t comp) {
	size_t i, j;

	con->conditional_is_valid[comp] = 1;

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		specific_config *s = srv->config_storage[i];

		/* not our stage */
		if (comp != dc->comp) continue;

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.document-root"))) {
				PATCH(document_root);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.range-requests"))) {
				PATCH(range_requests);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.error-handler-404"))) {
				PATCH(error_handler);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.errorfile-prefix"))) {
				PATCH(errorfile_prefix);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("mimetype.assign"))) {
				PATCH(mimetypes);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.max-keep-alive-requests"))) {
				PATCH(max_keep_alive_requests);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.max-keep-alive-idle"))) {
				PATCH(max_keep_alive_idle);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.max-write-idle"))) {
				PATCH(max_write_idle);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.max-read-idle"))) {
				PATCH(max_read_idle);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.max-connection-idle"))) {
				PATCH(max_connection_idle);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("mimetype.use-xattr"))) {
				PATCH(use_xattr);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssl.pemfile"))) {
				PATCH(ssl_pemfile);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssl.ca-file"))) {
				PATCH(ssl_ca_file);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssl.engine"))) {
				PATCH(is_ssl);
#ifdef HAVE_LSTAT
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.follow-symlink"))) {
				PATCH(follow_symlink);
#endif
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.name"))) {
				buffer_copy_string_buffer(con->server_name, s->server_name);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.tag"))) {
				PATCH(server_tag);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("connection.kbytes-per-second"))) {
				PATCH(kbytes_per_second);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("debug.log-request-handling"))) {
				PATCH(log_request_handling);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("debug.log-request-header"))) {
				PATCH(log_request_header);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("debug.log-response-header"))) {
				PATCH(log_response_header);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("debug.log-condition-handling"))) {
				PATCH(log_condition_handling);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("debug.log-condition-cache-handling"))) {
				PATCH(log_condition_cache_handling);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("debug.log-file-not-found"))) {
				PATCH(log_file_not_found);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.protocol-http11"))) {
				PATCH(allow_http11);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.force-lowercase-filenames"))) {
				PATCH(force_lowercase_filenames);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.kbytes-per-second"))) {
				PATCH(global_kbytes_per_second);
				PATCH(global_bytes_per_second_cnt);
				con->conf.global_bytes_per_second_cnt_ptr = &s->global_bytes_per_second_cnt;
			}
		}
	}

	return 0;
}
#undef PATCH

typedef struct {
	int foo;
	int bar;

	const buffer *source;
	const char *input;
	size_t offset;
	size_t size;

	int line_pos;
	int line;

	int in_key;
	int in_brace;
	int in_cond;
} tokenizer_t;

#if 0
static int tokenizer_open(server *srv, tokenizer_t *t, buffer *basedir, const char *fn) {
	if (buffer_is_empty(basedir) &&
			(fn[0] == '/' || fn[0] == '\\') &&
			(fn[0] == '.' && (fn[1] == '/' || fn[1] == '\\'))) {
		t->file = buffer_init_string(fn);
	} else {
		t->file = buffer_init_buffer(basedir);
		buffer_append_string(t->file, fn);
	}

	if (0 != stream_open(&(t->s), t->file)) {
		log_error_write(srv, __FILE__, __LINE__, "sbss",
				"opening configfile ", t->file, "failed:", strerror(errno));
		buffer_free(t->file);
		return -1;
	}

	t->input = t->s.start;
	t->offset = 0;
	t->size = t->s.size;
	t->line = 1;
	t->line_pos = 1;

	t->in_key = 1;
	t->in_brace = 0;
	t->in_cond = 0;
	return 0;
}

static int tokenizer_close(server *srv, tokenizer_t *t) {
	UNUSED(srv);

	buffer_free(t->file);
	return stream_close(&(t->s));
}
#endif
static int config_skip_newline(tokenizer_t *t) {
	int skipped = 1;
	assert(t->input[t->offset] == '\r' || t->input[t->offset] == '\n');
	if (t->input[t->offset] == '\r' && t->input[t->offset + 1] == '\n') {
		skipped ++;
		t->offset ++;
	}
	t->offset ++;
	return skipped;
}

static int config_skip_comment(tokenizer_t *t) {
	int i;
	assert(t->input[t->offset] == '#');
	for (i = 1; t->input[t->offset + i] &&
	     (t->input[t->offset + i] != '\n' && t->input[t->offset + i] != '\r');
	     i++);
	t->offset += i;
	return i;
}

static int config_tokenizer(server *srv, tokenizer_t *t, int *token_id, buffer *token) {
	int tid = 0;
	size_t i;

	for (tid = 0; tid == 0 && t->offset < t->size && t->input[t->offset] ; ) {
		char c = t->input[t->offset];
		const char *start = NULL;

		switch (c) {
		case '=':
			if (t->in_brace) {
				if (t->input[t->offset + 1] == '>') {
					t->offset += 2;

					buffer_copy_string(token, "=>");

					tid = TK_ARRAY_ASSIGN;
				} else {
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
							"source:", t->source,
							"line:", t->line, "pos:", t->line_pos,
							"use => for assignments in arrays");
					return -1;
				}
			} else if (t->in_cond) {
				if (t->input[t->offset + 1] == '=') {
					t->offset += 2;

					buffer_copy_string(token, "==");

					tid = TK_EQ;
				} else if (t->input[t->offset + 1] == '~') {
					t->offset += 2;

					buffer_copy_string(token, "=~");

					tid = TK_MATCH;
				} else {
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
							"source:", t->source,
							"line:", t->line, "pos:", t->line_pos,
							"only =~ and == are allowed in the condition");
					return -1;
				}
				t->in_key = 1;
				t->in_cond = 0;
			} else if (t->in_key) {
				tid = TK_ASSIGN;

				buffer_copy_string_len(token, t->input + t->offset, 1);

				t->offset++;
				t->line_pos++;
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
						"source:", t->source,
						"line:", t->line, "pos:", t->line_pos,
						"unexpected equal-sign: =");
				return -1;
			}

			break;
		case '!':
			if (t->in_cond) {
				if (t->input[t->offset + 1] == '=') {
					t->offset += 2;

					buffer_copy_string(token, "!=");

					tid = TK_NE;
				} else if (t->input[t->offset + 1] == '~') {
					t->offset += 2;

					buffer_copy_string(token, "!~");

					tid = TK_NOMATCH;
				} else {
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
							"source:", t->source,
							"line:", t->line, "pos:", t->line_pos,
							"only !~ and != are allowed in the condition");
					return -1;
				}
				t->in_key = 1;
				t->in_cond = 0;
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
						"source:", t->source,
						"line:", t->line, "pos:", t->line_pos,
						"unexpected exclamation-marks: !");
				return -1;
			}

			break;
		case '\t':
		case ' ':
			t->offset++;
			t->line_pos++;
			break;
		case '\n':
		case '\r':
			if (t->in_brace == 0) {
				int done = 0;
				while (!done && t->offset < t->size) {
					switch (t->input[t->offset]) {
					case '\r':
					case '\n':
						config_skip_newline(t);
						t->line_pos = 1;
						t->line++;
						break;

					case '#':
						t->line_pos += config_skip_comment(t);
						break;

					case '\t':
					case ' ':
						t->offset++;
						t->line_pos++;
						break;

					default:
						done = 1;
					}
				}
				t->in_key = 1;
				tid = TK_EOL;
				buffer_copy_string(token, "(EOL)");
			} else {
				config_skip_newline(t);
				t->line_pos = 1;
                t->line++;
			}
			break;
		case ',':
			if (t->in_brace > 0) {
				tid = TK_COMMA;

				buffer_copy_string(token, "(COMMA)");
			}

			t->offset++;
			t->line_pos++;
			break;
		case '"':
			/* search for the terminating " */
			start = t->input + t->offset + 1;
			buffer_copy_string(token, "");

			for (i = 1; t->input[t->offset + i]; i++) {
				if (t->input[t->offset + i] == '\\' &&
				    t->input[t->offset + i + 1] == '"') {

					buffer_append_string_len(token, start, t->input + t->offset + i - start);

					start = t->input + t->offset + i + 1;

					/* skip the " */
					i++;
					continue;
				}


				if (t->input[t->offset + i] == '"') {
					tid = TK_STRING;

					buffer_append_string_len(token, start, t->input + t->offset + i - start);

					break;
				}
			}

			if (t->input[t->offset + i] == '\0') {
				/* ERROR */

				log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
						"source:", t->source,
						"line:", t->line, "pos:", t->line_pos,
						"missing closing quote");

				return -1;
			}

			t->offset += i + 1;
			t->line_pos += i + 1;

			break;
		case '(':
			t->offset++;
			t->in_brace++;

			tid = TK_LPARAN;

			buffer_copy_string(token, "(");
			break;
		case ')':
			t->offset++;
			t->in_brace--;

			tid = TK_RPARAN;

			buffer_copy_string(token, ")");
			break;
		case '$':
			t->offset++;

			tid = TK_DOLLAR;
			t->in_cond = 1;
			t->in_key = 0;

			buffer_copy_string(token, "$");

			break;

		case '+':
			if (t->input[t->offset + 1] == '=') {
				t->offset += 2;
				buffer_copy_string(token, "+=");
				tid = TK_APPEND;
			} else {
				t->offset++;
				tid = TK_PLUS;
				buffer_copy_string(token, "+");
			}
			break;

		case '{':
			t->offset++;

			tid = TK_LCURLY;

			buffer_copy_string(token, "{");

			break;

		case '}':
			t->offset++;

			tid = TK_RCURLY;

			buffer_copy_string(token, "}");

			break;

		case '[':
			t->offset++;

			tid = TK_LBRACKET;

			buffer_copy_string(token, "[");

			break;

		case ']':
			t->offset++;

			tid = TK_RBRACKET;

			buffer_copy_string(token, "]");

			break;
		case '#':
			t->line_pos += config_skip_comment(t);

			break;
		default:
			if (t->in_cond) {
				for (i = 0; t->input[t->offset + i] &&
				     (isalpha((unsigned char)t->input[t->offset + i])
				      ); i++);

				if (i && t->input[t->offset + i]) {
					tid = TK_SRVVARNAME;
					buffer_copy_string_len(token, t->input + t->offset, i);

					t->offset += i;
					t->line_pos += i;
				} else {
					/* ERROR */
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
							"source:", t->source,
							"line:", t->line, "pos:", t->line_pos,
							"invalid character in condition");
					return -1;
				}
			} else if (isdigit((unsigned char)c)) {
				/* take all digits */
				for (i = 0; t->input[t->offset + i] && isdigit((unsigned char)t->input[t->offset + i]);  i++);

				/* was there it least a digit ? */
				if (i) {
					tid = TK_INTEGER;

					buffer_copy_string_len(token, t->input + t->offset, i);

					t->offset += i;
					t->line_pos += i;
				}
			} else {
				/* the key might consist of [-.0-9a-z] */
				for (i = 0; t->input[t->offset + i] &&
				     (isalnum((unsigned char)t->input[t->offset + i]) ||
				      t->input[t->offset + i] == '.' ||
				      t->input[t->offset + i] == '_' || /* for env.* */
				      t->input[t->offset + i] == '-'
				      ); i++);

				if (i && t->input[t->offset + i]) {
					buffer_copy_string_len(token, t->input + t->offset, i);

					if (buffer_is_equal_string(token, CONST_STR_LEN("include"))) {
						tid = TK_INCLUDE;
					} else if (buffer_is_equal_string(token, CONST_STR_LEN("include_shell"))) {
						tid = TK_INCLUDE_SHELL;
					} else if (buffer_is_equal_string(token, CONST_STR_LEN("global"))) {
						tid = TK_GLOBAL;
					} else if (buffer_is_equal_string(token, CONST_STR_LEN("else"))) {
						tid = TK_ELSE;
					} else {
						tid = TK_LKEY;
					}

					t->offset += i;
					t->line_pos += i;
				} else {
					/* ERROR */
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
							"source:", t->source,
							"line:", t->line, "pos:", t->line_pos,
							"invalid character in variable name");
					return -1;
				}
			}
			break;
		}
	}

	if (tid) {
		*token_id = tid;
#if 0
		log_error_write(srv, __FILE__, __LINE__, "sbsdsdbdd",
				"source:", t->source,
				"line:", t->line, "pos:", t->line_pos,
				token, token->used - 1, tid);
#endif

		return 1;
	} else if (t->offset < t->size) {
		fprintf(stderr, "%s.%d: %d, %s\n",
			__FILE__, __LINE__,
			tid, token->ptr);
	}
	return 0;
}

static int config_parse(server *srv, config_t *context, tokenizer_t *t) {
	void *pParser;
	int token_id;
	buffer *token, *lasttoken;
	int ret;

	pParser = configparserAlloc( malloc );
	lasttoken = buffer_init();
	token = buffer_init();

	while((1 == (ret = config_tokenizer(srv, t, &token_id, token))) && context->ok) {
		buffer_copy_string_buffer(lasttoken, token);
		configparser(pParser, token_id, token, context);

		token = buffer_init();
	}
	buffer_free(token);

	if (ret != -1 && context->ok) {
		/* add an EOL at EOF, better than say sorry */
		configparser(pParser, TK_EOL, buffer_init_string("(EOL)"), context);
		if (context->ok) {
			configparser(pParser, 0, NULL, context);
		}
	}
	configparserFree(pParser, free);

	if (ret == -1) {
		log_error_write(srv, __FILE__, __LINE__, "sb",
				"configfile parser failed:", lasttoken);
	} else if (context->ok == 0) {
		log_error_write(srv, __FILE__, __LINE__, "sbsdsdsb",
				"source:", t->source,
				"line:", t->line, "pos:", t->line_pos,
				"parser failed somehow near here:", lasttoken);
		ret = -1;
	}
	buffer_free(lasttoken);

	return ret == -1 ? -1 : 0;
}

static int tokenizer_init(tokenizer_t *t, const buffer *source, const char *input, size_t size) {

	t->source = source;
	t->input = input;
	t->size = size;
	t->offset = 0;
	t->line = 1;
	t->line_pos = 1;

	t->in_key = 1;
	t->in_brace = 0;
	t->in_cond = 0;
	return 0;
}

int config_parse_file(server *srv, config_t *context, const char *fn) {
	tokenizer_t t;
	stream s;
	int ret;
	buffer *filename;

	if (buffer_is_empty(context->basedir) &&
			(fn[0] == '/' || fn[0] == '\\') &&
			(fn[0] == '.' && (fn[1] == '/' || fn[1] == '\\'))) {
		filename = buffer_init_string(fn);
	} else {
		filename = buffer_init_buffer(context->basedir);
		buffer_append_string(filename, fn);
	}

	if (0 != stream_open(&s, filename)) {
		log_error_write(srv, __FILE__, __LINE__, "sbss",
				"opening configfile ", filename, "failed:", strerror(errno));
		ret = -1;
	} else {
		tokenizer_init(&t, filename, s.start, s.size);
		ret = config_parse(srv, context, &t);
	}

	stream_close(&s);
	buffer_free(filename);
	return ret;
}

int config_parse_cmd(server *srv, config_t *context, const char *cmd) {
	proc_handler_t proc;
	tokenizer_t t;
	int ret;
	buffer *source;
	buffer *out;
	char oldpwd[PATH_MAX];

	if (NULL == getcwd(oldpwd, sizeof(oldpwd))) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"cannot get cwd", strerror(errno));
		return -1;
	}

	source = buffer_init_string(cmd);
	out = buffer_init();

	if (!buffer_is_empty(context->basedir)) {
		chdir(context->basedir->ptr);
	}

	if (0 != proc_open_buffer(&proc, cmd, NULL, out, NULL)) {
		log_error_write(srv, __FILE__, __LINE__, "sbss",
				"opening", source, "failed:", strerror(errno));
		ret = -1;
	} else {
		tokenizer_init(&t, source, out->ptr, out->used);
		ret = config_parse(srv, context, &t);
	}

	buffer_free(source);
	buffer_free(out);
	chdir(oldpwd);
	return ret;
}

static void context_init(server *srv, config_t *context) {
	context->srv = srv;
	context->ok = 1;
	context->configs_stack = buffer_ptr_init(NULL);
	context->basedir = buffer_init();
}

static void context_free(config_t *context) {
	buffer_ptr_free(context->configs_stack);
	buffer_free(context->basedir);
}

int config_read(server *srv, const char *fn) {
	config_t context;
	data_config *dc;
	data_integer *dpid;
	data_string *dcwd;
	int ret;
	char *pos;
	data_array *modules;

	context_init(srv, &context);
	context.all_configs = srv->config_context;

    /* use the current dir as basedir for all other includes
    */
	pos = strrchr(fn, DIR_SEPERATOR);

	if (pos) {
		buffer_copy_string_len(context.basedir, fn, pos - fn + 1);
		fn = pos + 1;
	}

	dc = data_config_init();
	buffer_copy_string(dc->key, "global");

	assert(context.all_configs->used == 0);
	dc->context_ndx = context.all_configs->used;
	array_insert_unique(context.all_configs, (data_unset *)dc);
	context.current = dc;

	/* default context */
	srv->config = dc->value;
	dpid = data_integer_init();
	dpid->value = getpid();
	buffer_copy_string(dpid->key, "var.PID");
	array_insert_unique(srv->config, (data_unset *)dpid);

	dcwd = data_string_init();
	buffer_prepare_copy(dcwd->value, 1024);
	if (NULL != getcwd(dcwd->value->ptr, dcwd->value->size - 1)) {
		dcwd->value->used = strlen(dcwd->value->ptr) + 1;
		buffer_copy_string(dcwd->key, "var.CWD");
		array_insert_unique(srv->config, (data_unset *)dcwd);
	}

	ret = config_parse_file(srv, &context, fn);

	/* remains nothing if parser is ok */
	assert(!(0 == ret && context.ok && 0 != context.configs_stack->used));
	context_free(&context);

	if (0 != ret) {
		return ret;
	}

	if (NULL != (dc = (data_config *)array_get_element(srv->config_context, CONST_STR_LEN("global")))) {
		srv->config = dc->value;
	} else {
		return -1;
	}

	if (NULL != (modules = (data_array *)array_get_element(srv->config, CONST_STR_LEN("server.modules")))) {
		data_string *ds;
		data_array *prepends;

		if (modules->type != TYPE_ARRAY) {
			fprintf(stderr, "server.modules must be an array");
			return -1;
		}

		prepends = data_array_init();

		/* prepend default modules */
		if (NULL == array_get_element(modules->value, CONST_STR_LEN("mod_indexfile"))) {
			ds = data_string_init();
			buffer_copy_string(ds->value, "mod_indexfile");
			array_insert_unique(prepends->value, (data_unset *)ds);
		}

		prepends = (data_array *)configparser_merge_data((data_unset *)prepends, (data_unset *)modules);
		buffer_copy_string_buffer(prepends->key, modules->key);
		array_replace(srv->config, (data_unset *)prepends);
		modules->free((data_unset *)modules);
		modules = prepends;

		/* append default modules */
		if (NULL == array_get_element(modules->value, CONST_STR_LEN("mod_dirlisting"))) {
			ds = data_string_init();
			buffer_copy_string(ds->value, "mod_dirlisting");
			array_insert_unique(modules->value, (data_unset *)ds);
		}

		if (NULL == array_get_element(modules->value, CONST_STR_LEN("mod_staticfile"))) {
			ds = data_string_init();
			buffer_copy_string(ds->value, "mod_staticfile");
			array_insert_unique(modules->value, (data_unset *)ds);
		}

		if (NULL == array_get_element(modules->value, CONST_STR_LEN("mod_chunked"))) {
			ds = data_string_init();
			buffer_copy_string(ds->value, "mod_chunked");
			array_insert_unique(modules->value, (data_unset *)ds);
		}
	} else {
		data_string *ds;

		modules = data_array_init();

		/* server.modules is not set */
		ds = data_string_init();
		buffer_copy_string(ds->value, "mod_indexfile");
		array_insert_unique(modules->value, (data_unset *)ds);

		ds = data_string_init();
		buffer_copy_string(ds->value, "mod_dirlisting");
		array_insert_unique(modules->value, (data_unset *)ds);

		ds = data_string_init();
		buffer_copy_string(ds->value, "mod_staticfile");
		array_insert_unique(modules->value, (data_unset *)ds);

		ds = data_string_init();
		buffer_copy_string(ds->value, "mod_chunked");
		array_insert_unique(modules->value, (data_unset *)ds);

		buffer_copy_string(modules->key, "server.modules");
		array_insert_unique(srv->config, (data_unset *)modules);
	}


	if (0 != config_insert(srv)) {
		return -1;
	}

	return 0;
}


int config_set_defaults(server *srv) {
	size_t i;
	specific_config *s = srv->config_storage[0];
	struct stat st1, st2;

	struct ev_map { fdevent_handler_t et; const char *name; } event_handlers[] =
	{
		/* - poll is most reliable
		 * - select works everywhere
		 * - linux-* are experimental
		 */
#ifdef USE_POLL
		{ FDEVENT_HANDLER_POLL,           "poll" },
#endif
#ifdef USE_SELECT
		{ FDEVENT_HANDLER_SELECT,         "select" },
#endif
#ifdef USE_LINUX_EPOLL
		{ FDEVENT_HANDLER_LINUX_SYSEPOLL, "linux-sysepoll" },
#endif
#ifdef USE_LINUX_SIGIO
		{ FDEVENT_HANDLER_LINUX_RTSIG,    "linux-rtsig" },
#endif
#ifdef USE_SOLARIS_DEVPOLL
		{ FDEVENT_HANDLER_SOLARIS_DEVPOLL,"solaris-devpoll" },
#endif
#ifdef USE_FREEBSD_KQUEUE
		{ FDEVENT_HANDLER_FREEBSD_KQUEUE, "freebsd-kqueue" },
		{ FDEVENT_HANDLER_FREEBSD_KQUEUE, "kqueue" },
#endif
		{ FDEVENT_HANDLER_UNSET,          NULL }
	};


	if (buffer_is_empty(s->document_root)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"a default document-root has to be set");

		return -1;
	}

	if (buffer_is_empty(srv->srvconf.changeroot)) {
		pathname_unix2local(s->document_root);
		if (-1 == stat(s->document_root->ptr, &st1)) {
			log_error_write(srv, __FILE__, __LINE__, "sbs",
					"base-docroot doesn't exist:",
					s->document_root, strerror(errno));
			return -1;
		}

	} else {
		buffer_copy_string_buffer(srv->tmp_buf, srv->srvconf.changeroot);
		buffer_append_string_buffer(srv->tmp_buf, s->document_root);

		if (-1 == stat(srv->tmp_buf->ptr, &st1)) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
					"base-docroot doesn't exist:",
					srv->tmp_buf);
			return -1;
		}

	}

	buffer_copy_string_buffer(srv->tmp_buf, s->document_root);

	buffer_to_lower(srv->tmp_buf);

	if (0 == stat(srv->tmp_buf->ptr, &st1)) {
		int is_lower = 0;

		is_lower = buffer_is_equal(srv->tmp_buf, s->document_root);

		/* lower-case existed, check upper-case */
		buffer_copy_string_buffer(srv->tmp_buf, s->document_root);

		buffer_to_upper(srv->tmp_buf);

		/* we have to handle the special case that upper and lower-casing results in the same filename
		 * as in server.document-root = "/" or "/12345/" */

		if (is_lower && buffer_is_equal(srv->tmp_buf, s->document_root)) {
			/* lower-casing and upper-casing didn't result in
			 * an other filename, no need to stat(),
			 * just assume it is case-sensitive. */

			s->force_lowercase_filenames = 0;
		} else if (0 == stat(srv->tmp_buf->ptr, &st2)) {

			/* upper case exists too, doesn't the FS handle this ? */

			/* upper and lower have the same inode -> case-insensitve FS */

			if (st1.st_ino == st2.st_ino) {
				/* upper and lower have the same inode -> case-insensitve FS */

				s->force_lowercase_filenames = 1;
			}
		}
	}

	if (srv->srvconf.port == 0) {
		srv->srvconf.port = s->is_ssl ? 443 : 80;
	}

	if (srv->srvconf.event_handler->used == 0) {
		/* choose a good default
		 *
		 * the event_handler list is sorted by 'goodness'
		 * taking the first available should be the best solution
		 */
		srv->event_handler = event_handlers[0].et;

		if (FDEVENT_HANDLER_UNSET == srv->event_handler) {
			log_error_write(srv, __FILE__, __LINE__, "s",
					"sorry, there is no event handler for this system");

			return -1;
		}
	} else {
		/*
		 * User override
		 */

		for (i = 0; event_handlers[i].name; i++) {
			if (0 == strcmp(event_handlers[i].name, srv->srvconf.event_handler->ptr)) {
				srv->event_handler = event_handlers[i].et;
				break;
			}
		}

		if (FDEVENT_HANDLER_UNSET == srv->event_handler) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
					"the selected event-handler in unknown or not supported:",
					srv->srvconf.event_handler );

			return -1;
		}
	}

	if (s->is_ssl) {
		if (buffer_is_empty(s->ssl_pemfile)) {
			/* PEM file is require */

			log_error_write(srv, __FILE__, __LINE__, "s",
					"ssl.pemfile has to be set");
			return -1;
		}

#ifndef USE_OPENSSL
		log_error_write(srv, __FILE__, __LINE__, "s",
				"ssl support is missing, recompile with --with-openssl");

		return -1;
#endif
	}

	return 0;
}