/*
 *  Copyright (c) 2016 - 2025 MaiKe Labs
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/
#include "httpd.h"

irom void httpd_handle_root(void *arg)
{
	char resp[256];
	char body[] = "<html><head><title>Noduino</title></head>"
				"<body><center>Welcome to Noduino!</body></html>";
	os_sprintf(resp, HTTP_OK_HDR, "text/html",
			os_strlen(body), body);

	struct espconn *pespconn = (struct espconn *)arg;
	espconn_sent(pespconn, resp, os_strlen(resp));
}

irom void httpd_handle_dev(void *arg, char *data)
{
	struct espconn *pespconn = (struct espconn *)arg;

	char *resp = (char *)os_zalloc(256);
	char body[128] = { 0 };
	if (resp == NULL) {
		HTTPD_INFO("resp mem alloc failed when response upnp ctrl\r\n");
		return;
	}

	char *p = (char *)os_strstr(data, "GET /dev?ops=");
	if (p != NULL) {
		if (*(p+13) == 'r') {

			HTTPD_INFO("RX read operation\r\n");
			os_sprintf(body,
					"<html><head><title>Noduino</title></head>"
					"<body><center>RX dev read operation</body></html>"
					);

		} else if (*(p+13) == 'w') {

			HTTPD_INFO("RX write operation\r\n");
			os_sprintf(body,
					"<html><head><title>Noduino</title></head>"
					"<body><center>RX dev write operation</body></html>"
					);
		} else {

			HTTPD_INFO("Unsupported operation: %c\r\n", *(p+13));
			os_sprintf(body,
					"<html><head><title>Noduino</title></head>"
					"<body><center>Unsupported operation</body></html>"
					);
		}
	} else {
		os_sprintf(body,
			"<html><head><title>Noduino</title></head>"
			"<body><center>Welcome to Noduino!</body></html>"
			);
	}
	os_sprintf(resp, HTTP_OK_HDR, "text/html",
			os_strlen(body), body);

	espconn_sent(pespconn, resp, os_strlen(resp));

	os_free(resp);
	resp = NULL;
}

irom void httpd_handle_bad_req(void *arg)
{
	char head[128];
	os_sprintf(head, 
			"HTTP/1.0 400 BadRequest\r\n"
			"Content-Length: 0\r\n"
			"Server: lwIP/1.4.0\r\n\r\n");
	int len = os_strlen(head);

	HTTPD_INFO("HTTP bad request\r\n");
	struct espconn *pespconn = (struct espconn *)arg;
	espconn_sent(pespconn, head, len);
}

irom void httpd_not_found(void *arg)
{
	char resp[256];
	char body[] = "Not Found :(";
	os_sprintf(resp, 
			"HTTP/1.0 404 Not Found\r\n"
			"Content-Length: %d\r\n"
			"Server: lwIP/1.4.0\r\n"
			"Content-type: text/plain\r\n"
			"Connection: close\r\n\r\n%s",
			os_strlen(body), body);

	struct espconn *pespconn = (struct espconn *)arg;
	espconn_sent(pespconn, resp, os_strlen(resp));
}

irom void tcp_srv_recv_cb(void *arg, char *data, uint16_t len)
{
	char *d = data;

	if (strncmp(d, "GET / ", 6) == 0 ) {
		httpd_handle_root(arg);
	} else if (strncmp(d, "GET /dev", 8) == 0) {
		HTTPD_INFO("------------ TCP Recv: GET /dev\r\n%s\r\n", d);
		httpd_handle_dev(arg, data);
	} else {
		HTTPD_INFO("------- TCP recv: -------\r\n%s\r\n------------------\r\n", data);
		httpd_not_found(arg);
	}
}

irom void tcp_srv_sent_cb(void *arg)
{
	//data sent successfully
	HTTPD_INFO("TCP sent success\r\n");
}

irom void tcp_srv_discon_cb(void *arg)
{
	//tcp disconnect successfully
	HTTPD_INFO("TCP disconnect succeed\r\n");
}

irom void tcp_srv_recon_cb(void *arg, sint8 err)
{
	//error occured , tcp connection broke.
	HTTPD_INFO("TCP reconnect callback, error code %d\r\n", err);
}

irom void tcp_srv_listen(void *arg)
{
	struct espconn *pesp_conn = (struct espconn *)arg;
	HTTPD_DEBUG("TCP is listening ... \r\n");

	espconn_regist_recvcb(pesp_conn, tcp_srv_recv_cb);
	espconn_regist_reconcb(pesp_conn, tcp_srv_recon_cb);
	espconn_regist_disconcb(pesp_conn, tcp_srv_discon_cb);

	espconn_regist_sentcb(pesp_conn, tcp_srv_sent_cb);
}

int httpd_start(httpd_t *d)
{
	if (d->esp_conn == NULL) {
		d->esp_conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
		// Check memory
		if (d->esp_conn == NULL) {
			HTTPD_INFO("%s: not enough memory\r\n", __func__);
			return -1;
		}
	}
	d->esp_conn->type = ESPCONN_TCP;
	d->esp_conn->state = ESPCONN_NONE;
	d->esp_conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	if (d->esp_conn->proto.tcp == NULL) {
		HTTPD_INFO("%s: not enough memory, alloc for esp_tcp failed\r\n", __func__);
		return -1;
	}	

	d->esp_conn->reverse = (void *)d;
	d->esp_conn->proto.tcp->local_port = d->port;

	espconn_regist_connectcb(d->esp_conn, tcp_srv_listen);
	uint8_t ret = espconn_accept(d->esp_conn);
	HTTPD_DEBUG("espconn_accept [%d] !!! \r\n", ret);
	return ret;
}

irom void httpd_stop(httpd_t *d)
{
	if (d->esp_conn != NULL) {
		espconn_delete(d->esp_conn);

		if (d->esp_conn->proto.tcp != NULL) {
			os_free(d->esp_conn->proto.tcp);
			d->esp_conn->proto.tcp = NULL;
		}

		os_free(d->esp_conn);
		d->esp_conn = NULL;
	}
}
