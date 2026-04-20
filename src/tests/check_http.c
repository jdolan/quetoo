/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "tests.h"

#include "net/net_http.h"

#include <SDL3/SDL_thread.h>

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {

	Mem_Init();

	Fs_Init(FS_NONE);
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

	Fs_Shutdown();

	Mem_Shutdown();
}

// -- Net_HttpUrl --

START_TEST(check_Net_HttpUrl) {

	net_addr_t addr;
	ck_assert(Net_StringToNetaddr("192.168.1.100:1998", &addr));

	char buf[256];
	const int32_t len = Net_HttpUrl(&addr, "maps/test.bsp", buf, sizeof(buf));

	ck_assert_int_gt(len, 0);
	ck_assert_str_eq(buf, "http://192.168.1.100:1998/maps/test.bsp");

} END_TEST

START_TEST(check_Net_HttpUrl_empty_path) {

	net_addr_t addr;
	ck_assert(Net_StringToNetaddr("10.0.0.1:8080", &addr));

	char buf[256];
	const int32_t len = Net_HttpUrl(&addr, "", buf, sizeof(buf));

	ck_assert_int_gt(len, 0);
	ck_assert_str_eq(buf, "http://10.0.0.1:8080/");

} END_TEST

START_TEST(check_Net_HttpUrl_small_buffer) {

	net_addr_t addr;
	ck_assert(Net_StringToNetaddr("127.0.0.1:1998", &addr));

	char buf[16];
	const int32_t len = Net_HttpUrl(&addr, "maps/test.bsp", buf, sizeof(buf));

	// g_snprintf returns the length that would have been written
	ck_assert_int_gt(len, (int32_t) sizeof(buf));
	// buffer should be truncated but null-terminated
	ck_assert(strlen(buf) < sizeof(buf));

} END_TEST

// -- Net_HttpParseRequestLine --

START_TEST(check_Net_HttpParseRequestLine_get) {

	char method[16], path[256];

	const bool ok = Net_HttpParseRequestLine("GET /maps/test.bsp HTTP/1.0\r\n",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(ok);
	ck_assert_str_eq(method, "GET");
	ck_assert_str_eq(path, "maps/test.bsp");

} END_TEST

START_TEST(check_Net_HttpParseRequestLine_post) {

	char method[16], path[256];

	const bool ok = Net_HttpParseRequestLine("POST /upload HTTP/1.1\r\n",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(ok);
	ck_assert_str_eq(method, "POST");
	ck_assert_str_eq(path, "upload");

} END_TEST

START_TEST(check_Net_HttpParseRequestLine_strip_slash) {

	char method[16], path[256];

	const bool ok = Net_HttpParseRequestLine("GET /textures/common/clip.png HTTP/1.0\r\n",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(ok);
	ck_assert_str_eq(path, "textures/common/clip.png");

} END_TEST

START_TEST(check_Net_HttpParseRequestLine_no_slash) {

	char method[16], path[256];

	const bool ok = Net_HttpParseRequestLine("GET maps/test.bsp HTTP/1.0\r\n",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(ok);
	ck_assert_str_eq(path, "maps/test.bsp");

} END_TEST

START_TEST(check_Net_HttpParseRequestLine_malformed_no_space) {

	char method[16], path[256];

	const bool ok = Net_HttpParseRequestLine("GARBAGE",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(!ok);

} END_TEST

START_TEST(check_Net_HttpParseRequestLine_malformed_no_version) {

	char method[16], path[256];

	const bool ok = Net_HttpParseRequestLine("GET /path",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(!ok);

} END_TEST

START_TEST(check_Net_HttpParseRequestLine_method_overflow) {

	char method[3], path[256]; // too small for "GET\0"

	const bool ok = Net_HttpParseRequestLine("GET /path HTTP/1.0\r\n",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(!ok);

} END_TEST

START_TEST(check_Net_HttpParseRequestLine_path_overflow) {

	char method[16], path[4]; // too small for "maps/test.bsp\0"

	const bool ok = Net_HttpParseRequestLine("GET /maps/test.bsp HTTP/1.0\r\n",
	                                         method, sizeof(method),
	                                         path, sizeof(path));
	ck_assert(!ok);

} END_TEST

// -- Net_HttpFormatResponse --

START_TEST(check_Net_HttpFormatResponse_200) {

	char buf[512];

	const int32_t len = Net_HttpFormatResponse(200, "OK",
	                                           "application/octet-stream", 12345,
	                                           buf, sizeof(buf));
	ck_assert_int_gt(len, 0);
	ck_assert(strstr(buf, "HTTP/1.0 200 OK\r\n") == buf);
	ck_assert(strstr(buf, "Content-Length: 12345\r\n") != NULL);
	ck_assert(strstr(buf, "Content-Type: application/octet-stream\r\n") != NULL);
	ck_assert(strstr(buf, "Connection: close\r\n") != NULL);
	// must end with blank line
	const size_t buf_len = strlen(buf);
	ck_assert(buf_len >= 4);
	ck_assert_str_eq(buf + buf_len - 4, "\r\n\r\n");

} END_TEST

START_TEST(check_Net_HttpFormatResponse_404) {

	char buf[512];

	const int32_t len = Net_HttpFormatResponse(404, "Not Found", NULL, 0,
	                                           buf, sizeof(buf));
	ck_assert_int_gt(len, 0);
	ck_assert(strstr(buf, "HTTP/1.0 404 Not Found\r\n") == buf);
	ck_assert(strstr(buf, "Content-Length: 0\r\n") != NULL);
	ck_assert(strstr(buf, "Content-Type") == NULL);

} END_TEST

START_TEST(check_Net_HttpFormatResponse_no_content) {

	char buf[512];

	const int32_t len = Net_HttpFormatResponse(204, "No Content", "text/plain", 0,
	                                           buf, sizeof(buf));
	ck_assert_int_gt(len, 0);
	ck_assert(strstr(buf, "HTTP/1.0 204 No Content\r\n") == buf);
	ck_assert(strstr(buf, "Content-Length: 0\r\n") != NULL);
	ck_assert(strstr(buf, "Content-Type: text/plain\r\n") != NULL);

} END_TEST

START_TEST(check_Net_HttpFormatResponse_large_content) {

	char buf[512];

	const int64_t large_size = 1073741824LL; // 1 GiB
	const int32_t len = Net_HttpFormatResponse(200, "OK",
	                                           "application/octet-stream", large_size,
	                                           buf, sizeof(buf));
	ck_assert_int_gt(len, 0);
	ck_assert(strstr(buf, "Content-Length: 1073741824\r\n") != NULL);

} END_TEST

// -- End-to-end round-trip test --

typedef struct {
	int32_t listen_sock;
	in_port_t port;
	const void *payload;
	size_t payload_len;
	char parsed_method[16];
	char parsed_path[256];
	bool ok;
} http_server_t;

static int SDLCALL http_server_thread(void *data) {
	http_server_t *ctx = data;

	// Make listen socket blocking so accept() waits for the client
	Net_SetNonBlocking(ctx->listen_sock, false);

	net_addr_t from;
	const int32_t client = Net_Accept(ctx->listen_sock, &from);
	if (client < 0) {
		ctx->ok = false;
		return 1;
	}

	// Net_Accept sets client non-blocking; switch to blocking for this test
	Net_SetNonBlocking(client, false);

	// Read the HTTP request
	char request[1024];
	const ssize_t n = Net_Recv(client, request, sizeof(request) - 1);
	if (n <= 0) {
		Net_CloseSocket(client);
		ctx->ok = false;
		return 1;
	}
	request[n] = '\0';

	// Parse request line
	if (!Net_HttpParseRequestLine(request, ctx->parsed_method, sizeof(ctx->parsed_method),
	                              ctx->parsed_path, sizeof(ctx->parsed_path))) {
		Net_HttpSendError(client, 400, "Bad Request");
		Net_CloseSocket(client);
		ctx->ok = false;
		return 1;
	}

	// Send HTTP response with the payload
	char header[512];
	const int32_t hlen = Net_HttpFormatResponse(200, "OK", "application/octet-stream",
	                                            (int64_t) ctx->payload_len,
	                                            header, sizeof(header));
	Net_Send(client, header, hlen);
	Net_Send(client, ctx->payload, ctx->payload_len);

	Net_CloseSocket(client);
	ctx->ok = true;
	return 0;
}

START_TEST(check_Net_Http_roundtrip) {

	Net_Init();

	const in_port_t port = 39981;

	const int32_t listen_sock = Net_SocketListen(NULL, port, 1);
	ck_assert_msg(listen_sock >= 0, "Net_SocketListen failed on port %d", port);

	const char payload[] = "Hello, Quetoo!\nThis is a test file download.\n";

	http_server_t server = {
		.listen_sock = listen_sock,
		.port = port,
		.payload = payload,
		.payload_len = sizeof(payload) - 1,
		.ok = false,
	};

	SDL_Thread *thread = SDL_CreateThread(http_server_thread, "http_server", &server);
	ck_assert_msg(thread != NULL, "SDL_CreateThread failed");

	// Client: download via Net_HttpGet (Objectively URLSession / libcurl)
	char url[128];
	g_snprintf(url, sizeof(url), "http://127.0.0.1:%d/test.txt", port);

	void *data = NULL;
	size_t length = 0;
	const int32_t status = Net_HttpGet(url, &data, &length);

	ck_assert_int_eq(status, 200);
	ck_assert_uint_eq(length, sizeof(payload) - 1);
	ck_assert(data != NULL);
	ck_assert(memcmp(data, payload, length) == 0);

	Mem_Free(data);

	// Join server thread and verify it parsed the request correctly
	int thread_status;
	SDL_WaitThread(thread, &thread_status);
	ck_assert_int_eq(thread_status, 0);
	ck_assert(server.ok);
	ck_assert_str_eq(server.parsed_method, "GET");
	ck_assert_str_eq(server.parsed_path, "test.txt");

	Net_CloseSocket(listen_sock);
	Net_Shutdown();

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_http");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Net_HttpUrl);
	tcase_add_test(tcase, check_Net_HttpUrl_empty_path);
	tcase_add_test(tcase, check_Net_HttpUrl_small_buffer);

	tcase_add_test(tcase, check_Net_HttpParseRequestLine_get);
	tcase_add_test(tcase, check_Net_HttpParseRequestLine_post);
	tcase_add_test(tcase, check_Net_HttpParseRequestLine_strip_slash);
	tcase_add_test(tcase, check_Net_HttpParseRequestLine_no_slash);
	tcase_add_test(tcase, check_Net_HttpParseRequestLine_malformed_no_space);
	tcase_add_test(tcase, check_Net_HttpParseRequestLine_malformed_no_version);
	tcase_add_test(tcase, check_Net_HttpParseRequestLine_method_overflow);
	tcase_add_test(tcase, check_Net_HttpParseRequestLine_path_overflow);

	tcase_add_test(tcase, check_Net_HttpFormatResponse_200);
	tcase_add_test(tcase, check_Net_HttpFormatResponse_404);
	tcase_add_test(tcase, check_Net_HttpFormatResponse_no_content);
	tcase_add_test(tcase, check_Net_HttpFormatResponse_large_content);

	Suite *suite = suite_create("check_http");
	suite_add_tcase(suite, tcase);

	tcase = tcase_create("check_http_server");
	tcase_add_checked_fixture(tcase, setup, teardown);
	tcase_set_timeout(tcase, 10);
	tcase_add_test(tcase, check_Net_Http_roundtrip);
	suite_add_tcase(suite, tcase);

	// Run with CK_NOFORK because Net_HttpGet uses libcurl, which is not fork-safe
	SRunner *runner = srunner_create(suite);
	srunner_set_fork_status(runner, CK_NOFORK);
	srunner_run_all(runner, CK_VERBOSE);
	const int32_t failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	Test_Shutdown();
	return failed;
}
