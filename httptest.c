/* httptest by james@ustc.edu.cn 2019.04.29 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAXLEN 16384
// 最多下载4MB
#define MAXCONTENTLEN 4194304

int debug = 0, ipv4 = 0, ipv6 = 0, print_content = 0;
int sockfd = -1;
int wait_time = 5;
char check_string[MAXLEN];
char url[MAXLEN];
char ifluxdb_prefix[MAXLEN];

struct timeval lasttv;
float dns_time, connect_time, response_time, transfer_time;
unsigned long int content_len;

void my_exit(int code)
{
	if (code == 0) {
		if (ifluxdb_prefix[0])
			printf("%s code=10,dns_time=%.4f,connect_time=%.4f,response_time=%.4f,transfer_time=%.4f,content_len=%ld,transfer_rate=%.0f\n",
			       ifluxdb_prefix, dns_time, connect_time, response_time, transfer_time, content_len, (float)content_len / transfer_time);
		else
			printf("%d %.4f %.4f %.4f %.4f %ld %.0f %s\n", code, dns_time, connect_time, response_time, transfer_time, content_len,
			       (float)content_len / transfer_time, url);
	} else {
		if (ifluxdb_prefix[0])
			printf("%s code=%d,dns_time=0,connect_time=0,response_time=0,transfer_time=0,content_len=0,transfer_rate=0\n", ifluxdb_prefix,
			       10 - code);
		else
			printf("%d 0 0 0 0 0 %s\n", code, url);
	}
	exit(code);
}

void start_time(void)
{
	gettimeofday(&lasttv, NULL);
	if (debug)
		printf("time: %ld.%06ld\n", lasttv.tv_sec, lasttv.tv_usec);
}

float delta_time(void)
{
	float t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	if (debug)
		printf("time: %ld.%06ld\n", tv.tv_sec, tv.tv_usec);
	t = (tv.tv_sec - lasttv.tv_sec) + (tv.tv_usec - lasttv.tv_usec) / 1000000.0;
	lasttv = tv;
	return t;
}

SSL_CTX *InitCTX(void)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	OpenSSL_add_all_algorithms();	/* Load cryptos, et.al. */
	SSL_load_error_strings();	/* Bring in and register error messages */
	method = TLSv1_2_client_method();	/* Create new client-method instance */
	ctx = SSL_CTX_new(method);	/* Create new context */
	if (ctx == NULL) {
		ERR_print_errors_fp(stdout);
		close(sockfd);
		my_exit(9);
	}
	return ctx;
}

void http_test(void)
{
	int i, n;
	int https = 0;
	char hostname[MAXLEN];
	char port[MAXLEN];
	char *uri, *p = NULL;
	struct addrinfo hints, *res;
	char buf[MAXCONTENTLEN];
	SSL_CTX *ctx;
	SSL *ssl;

	if (strlen(url) > MAXLEN)
		my_exit(10);

	if (memcmp(url, "https://", 8) == 0) {
		p = url + 8;
		https = 1;
	} else if (memcmp(url, "http://", 7) != 0) {
		printf("only support http:// and https:// \n");
		my_exit(10);
	} else
		p = url + 7;
	hostname[0] = 0;
	i = 0;
	if (*p == '[') {	// ipv6 addr
		p++;
		while (*p && (*p != ']') && i < MAXLEN - 1) {
			hostname[i] = *p;
			i++;
			p++;
		}
		if (*p != ']') {	// ipv6 addr error 
			printf("url %s error\n", url);
			my_exit(10);
		}
		p++;
	} else {
		while (*p && (*p != '/') && (*p != ':') && i < MAXLEN - 1) {
			hostname[i] = *p;
			i++;
			p++;
		}
	}
	hostname[i] = 0;

	if (hostname[0] == 0)
		my_exit(10);

	port[0] = 0;
	i = 0;
	if (*p == ':') {
		p++;
		while (*p && (*p != '/') && i < MAXLEN - 1) {
			port[i] = *p;
			i++;
			p++;
		}
		port[i] = 0;
	}
	if (port[0] == 0) {
		if (https)
			strcpy(port, "443");
		else
			strcpy(port, "80");
	}
	uri = p;

	if (uri[0] == 0)
		uri = "/";

	if (debug) {
		printf("url: %s\nhostname: %s port: %s uri: %s\n", url, hostname, port, uri);
		printf("begin dns lookup\n");
	}
	if (https) {
		SSL_library_init();
		ctx = InitCTX();
		ssl = SSL_new(ctx);
	}

	start_time();
	if ((n = getaddrinfo(hostname, port, &hints, &res)) != 0) {
		if (debug)
			printf("getaddrinfo error for %s\n", hostname);
		my_exit(1);
	}
	dns_time = delta_time();
	if (debug) {
		printf("end dns lookup\n");
		printf("begin tcp connect\n");
	}

	do {
		int serport;
		char seraddr[INET6_ADDRSTRLEN];
		if (res->ai_family == AF_INET) {	// IPv4
			if (ipv6)
				continue;
			struct sockaddr_in *sinp;
			sinp = (struct sockaddr_in *)res->ai_addr;
			serport = ntohs(sinp->sin_port);
			inet_ntop(sinp->sin_family, &sinp->sin_addr, seraddr, INET6_ADDRSTRLEN);
		} else if (res->ai_family == AF_INET6) {
			if (ipv4)
				continue;
			struct sockaddr_in6 *sinp;
			sinp = (struct sockaddr_in6 *)res->ai_addr;
			serport = ntohs(sinp->sin6_port);
			inet_ntop(sinp->sin6_family, &sinp->sin6_addr, seraddr, INET6_ADDRSTRLEN);
		} else
			continue;
		if (debug) {
			printf("server address: %s\n", seraddr);
			printf("server port: %d\n", serport);
		}

		if ((sockfd = socket(res->ai_family, SOCK_STREAM, 0)) < 0) {
			printf("create socket failed: %s\n", strerror(errno));
			my_exit(2);
		}

		struct timeval timeout;
		timeout.tv_sec = wait_time;
		timeout.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

		if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
			if (debug)
				printf("can't connect to %s:%d %s\n", seraddr, serport, strerror(errno));
			close(sockfd);
			sockfd = -1;
		} else {
			if (debug)
				printf("connect ok\n");
			break;
		}
	}
	while ((res = res->ai_next) != NULL);

	if (sockfd < 0) {
		if (debug)
			printf("tcp connect error\n");
		my_exit(2);
	}

	int flag = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

	if (https) {
		SSL_set_fd(ssl, sockfd);
		if (SSL_connect(ssl) == -1) {
			if (debug)
				ERR_print_errors_fp(stdout);
			close(sockfd);
			my_exit(9);
		}
	}

	connect_time = delta_time();
	if (debug) {
		printf("end tcp & https connect\n");
		printf("begin http get request\n");
	}

	snprintf(buf, MAXLEN,
		 "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/73.0.3683.103 Safari/537.36\r\nConnection: close\r\n\r\n",
		 uri, hostname);
	if (https)
		n = SSL_write(ssl, buf, strlen(buf));
	else
		n = write(sockfd, buf, strlen(buf));
	if (debug)
		printf("end send request %d\n", n);
	if (https)
		n = SSL_read(ssl, buf, 12);
	else
		n = read(sockfd, buf, 12);
	response_time = delta_time();
	if (n <= 0) {
		if (debug)
			printf("get response: %d\n", n);
		close(sockfd);
		my_exit(3);
	}
	buf[n] = 0;
	/* HTTP/1.1 200 */
	if ((memcmp(buf, "HTTP/1", 6) != 0) || (memcmp(buf + 9, "200", 3) != 0)) {
		buf[12] = 0;
		if (debug)
			printf("get response: %s\n", buf);
		close(sockfd);
		my_exit(4);
	}
	content_len = n;
	while (n < MAXCONTENTLEN) {
		if (https)
			n = SSL_read(ssl, buf + content_len, MAXCONTENTLEN - content_len - 1);
		else
			n = read(sockfd, buf + content_len, MAXCONTENTLEN - content_len - 1);
		if (n <= 0)
			break;
		content_len += n;
	}
	buf[content_len] = 0;
	transfer_time = delta_time();
	close(sockfd);
	if (debug) {
		printf("read: %ld\n", content_len);
	}
	if (print_content)
		printf("%s\n", buf);

	if (check_string[0]) {
		if (debug)
			printf("check string: %s\n", check_string);
		char *p = strstr(buf, check_string);
		if (p == NULL) {
			if (debug)
				printf("check string not found, return -1\n");
			my_exit(5);
		}
	}
	if (debug)
		printf("All OK, return 0\n");
	my_exit(0);
}

void usage(void)
{
	printf("httptest: http get test\n\n");
	printf("httptest  [ -d ] [ -4 ] [ -6 ] [ -p ] [ -i prefix ] [ -w wait_time ] [ -r check_string ] url\n\n");
	printf("    -d               print debug message\n");
	printf("    -4               force ipv4\n");
	printf("    -6               force ipv6\n");
	printf("    -p               print response content\n");
	printf("    -i prefix        influxdb output\n");
	printf("    -w wait_time     max conntion time\n");
	printf("    -r check_string  check_string\n\n");
	printf("exit code:\n");
	printf("  0  get 200 response and match the check_string in response\n");
	printf("  1  dns error\n");
	printf("  2  tcp connect error\n");
	printf("  3  read tcp error\n");
	printf("  4  not 200 response\n");
	printf("  5  check_string not found\n");
	printf("  8  help or cmdline errror\n");
	printf("  9  https connect error\n");
	printf(" 10  url error\n\n");
	printf("output:\n");
	printf("code dns_time tcp_connect_time response_time transfer_time transfer_rate url\n");
	printf("            s                s             s             s        byte/s\n");
	exit(8);
}

int main(int argc, char *argv[])
{
	int c;
	strcpy(url, "NULL");

	while ((c = getopt(argc, argv, "d4p6w:i:r:h")) != EOF)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case '4':
			ipv4 = 1;
			break;
		case '6':
			ipv6 = 1;
			break;
		case 'p':
			print_content = 1;
			break;
		case 'w':
			wait_time = atoi(optarg);
			break;
		case 'i':
			strncpy(ifluxdb_prefix, optarg, MAXLEN);
			break;
		case 'r':
			strncpy(check_string, optarg, MAXLEN);
			break;
		case 'h':
			usage();
		};

	if (optind != argc - 1)
		usage();

	strncpy(url, argv[optind], MAXLEN);

	http_test();
	exit(8);
}
