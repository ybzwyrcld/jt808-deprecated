#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <thread>

#include <iostream>
#include <fstream>
#include <sstream>

#include <jt808_service.h>
#include <unix_socket.h>
#include <bcd.h>

using std::ifstream;
using std::string;
using std::stringstream;


static int epoll_register(const int &epoll_fd, const int &fd)
{
	struct epoll_event ev;
	int ret, flags;

	/* important: make the fd non-blocking */
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	ev.events = EPOLLIN;
	//ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = fd;
	do {
	    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

static int epoll_deregister(const int &epoll_fd, const int &fd)
{
	int ret;

	do {
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

static inline uint16_t endian_swap16(const uint16_t &v)
{
	assert(sizeof(v) == 2);

	return ((v & 0xff) << 8) | (v >> 8);
}

static inline uint32_t endian_swap32(const uint32_t &v)
{
	assert(sizeof(v) == 4);

	return (v >> 24)
			| ((v & 0x00ff0000) >> 8)
			| ((v & 0x0000ff00) << 8)
			| (v << 24);
}

static uint8_t bcc_sum(uint8_t *src, const int16_t &len)
{
    int16_t i = 0;
	uint8_t checksum = 0;

	for (i = 0; i < len; ++i) {
		checksum = checksum ^ src[i];
	}

	return checksum;
}

static uint16_t escape(uint8_t *data, const uint16_t &len)
{
	uint16_t i, j;

	uint8_t *buf = (uint8_t *) new char[len * 2];
	memset(buf, 0x0, len * 2);

	for (i = 0, j = 0; i < len; ++i) {
		if (data[i] == PROTOCOL_SIGN) {
			buf[j++] = PROTOCOL_ESCAPE;
			buf[j++] = PROTOCOL_ESCAPE_SIGN;
		} else if(data[i] == PROTOCOL_ESCAPE) {
			buf[j++] = PROTOCOL_ESCAPE;
			buf[j++] = PROTOCOL_ESCAPE_ESCAPE;
		} else {
			buf[j++] = data[i];
		}
	}

	memcpy(data, buf, j);
	delete [] buf;
	return j;
}

static uint16_t reverse_escape(uint8_t *data, const uint16_t &len)
{
	uint16_t i, j;

	uint8_t *buf = (uint8_t *) new char[len * 2];
	memset(buf, 0x0, len * 2);

	for (i = 0, j = 0; i < len; ++i) {
		if ((data[i] == PROTOCOL_ESCAPE) && (data[i+1] == PROTOCOL_ESCAPE_SIGN)) {
			buf[j++] = PROTOCOL_SIGN;
			++i;
		} else if ((data[i] == PROTOCOL_ESCAPE) && (data[i+1] == PROTOCOL_ESCAPE_ESCAPE)) {
			buf[j++] = PROTOCOL_ESCAPE;
			++i;
		} else {
			buf[j++] = data[i];
		}
	}

	memcpy(data, buf, j);
	delete [] buf;
	return j;
}

jt808_service::jt808_service()
{
	m_listen_sock = 0;
	m_epoll_fd = 0;
	m_max_count = 0;
	m_epoll_events = NULL;
	m_msgflownum = 0;
}

jt808_service::~jt808_service()
{
	if(m_listen_sock > 0){
		close(m_listen_sock);
	}

	if(m_epoll_fd > 0){
		close(m_epoll_fd);
	}
}

void jt808_service::read_devices_list(void)
{
	ifstream ifs;
	string s, s1;
	string::size_type spos, epos;
	stringstream ss;
	node_t node;
	union {
		uint32_t a;
		uint8_t b[4];
	}code;

	ifs.open(devices_file_path, std::ios::binary | std::ios::in);
	if (ifs.is_open()) {
		while (getline(ifs, s)) {
			memset(&node, 0x0, sizeof(node_t));
			memset(&code, 0x0, sizeof(code));
			ss.clear();
			spos = 0;
			epos = s.find(";");
			s1 = s.substr(spos, epos - spos);
			//std::cout << s1 << std::endl;
			s1.copy((char *)node.pnum, s1.length(), 0);
			++epos;
			spos = epos;
			epos = s.find(";", spos);
			s1 = s.substr(spos, epos - spos);
			//std::cout << s1 << std::endl;
			ss << s1;
			ss >> code.a;
			//printf("%02x, %02x, %02x, %02x\n", code.b[0], code.b[1], code.b[2], code.b[3]);
			memcpy(node.acode, code.b, 4);
			node.sockfd = -1;
			m_list.push_back(node);
			ss.str("");
		}
		ifs.close();
	}

	return ;
}

bool jt808_service::init(int port , int sock_count)
{
	m_max_count = sock_count;
	struct sockaddr_in caster_addr;
	memset(&caster_addr, 0, sizeof(struct sockaddr_in));
	caster_addr.sin_family = AF_INET;
	caster_addr.sin_port = htons(port);
	caster_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(m_listen_sock == -1) {
		exit(1);
	}

	if(bind(m_listen_sock, (struct sockaddr*)&caster_addr, sizeof(struct sockaddr)) == -1){
		exit(1);
	}

	if(listen(m_listen_sock, 5) == -1){
		exit(1);
	}

	m_epoll_events = new struct epoll_event[sock_count];
	if (m_epoll_events == NULL){
		exit(1);
	}

	m_epoll_fd = epoll_create(sock_count);
	epoll_register(m_epoll_fd, m_listen_sock);

	read_devices_list();

	m_socket_fd = serv_listen(sock_path);
	epoll_register(m_epoll_fd, m_socket_fd);

	return true;
}

bool jt808_service::init(const char *ip, int port , int sock_count)
{
	m_max_count = sock_count;
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(ip);

	m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_listen_sock == -1) {
		exit(1);
	}

	if (bind(m_listen_sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1) {
		exit(1);
	}

	if (listen(m_listen_sock, 5) == -1){
		exit(1);
	}

	m_epoll_events = new struct epoll_event[sock_count];
	if (m_epoll_events == NULL){
		exit(1);
	}

	m_epoll_fd = epoll_create(sock_count);
	epoll_register(m_epoll_fd, m_listen_sock);

	read_devices_list();

	m_socket_fd = serv_listen(sock_path);
	epoll_register(m_epoll_fd, m_socket_fd);

	return true;
}

int jt808_service::accept_new_client()
{
	uint16_t cmd = 0;
	struct sockaddr_in client_addr;
	node_t node;
	propara_t propara;
	message_t msg;
	uint8_t pnum[6] = {0};
	decltype (m_list.begin()) it;

	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	socklen_t clilen = sizeof(struct sockaddr);

	int new_sock = accept(m_listen_sock, (struct sockaddr*)&client_addr, &clilen);

	int keepalive = 1;  /* enable keepalive attributes. */
	int keepidle = 60;  /* time out for starting detection. */
	int keepinterval = 5;  /* time interval for sending packets during detection. */
	int keepcount = 3;  /* max times for sending packets during detection. */

	setsockopt(new_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive, sizeof(keepalive));
	setsockopt(new_sock, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle, sizeof(keepidle));
	setsockopt(new_sock, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval, sizeof(keepinterval));
	setsockopt(new_sock, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount, sizeof(keepcount));

	if (!recv_frame_data(new_sock, m_msg)) {
		cmd = jt808_frame_parse(m_msg, propara);
		switch (cmd) {
			case UP_REGISTER:
				memset(msg.data, 0x0, MAX_PROFRAMEBUF_LEN);
				msg.len = jt808_frame_pack(msg, DOWN_REGISTERRSPONSE, propara);
				send_frame_data(new_sock, msg);
				if (propara.bpara1 != 0x0) {
					close(new_sock);
					new_sock = -1;
					break;
				}

				if (!recv_frame_data(new_sock, msg)) {
					cmd = jt808_frame_parse(msg, propara);
					if (cmd != UP_AUTHENTICATION) {
						close(new_sock);
						new_sock = -1;
						break;
					}
				}
			case UP_AUTHENTICATION:
				memset(msg.data, 0x0, MAX_PROFRAMEBUF_LEN);
				m_msg.len = jt808_frame_pack(msg, DOWN_UNIRESPONSE, propara);
				send_frame_data(new_sock, msg);
				if (propara.bpara1 != 0x0) {
					close(new_sock);
					new_sock = -1;
				} else if (!m_list.empty()) {
					it = m_list.begin();
					while (it != m_list.end()) {
						str2bcd_compress(it->pnum, pnum);
						if (!strncmp((char *)pnum, (char *)propara.strpara1, 6))
							break;
						else
							++it;
					}
					if (it != m_list.end()) {
						it->sockfd = new_sock;
						node = *it;
						m_list.erase(it);
						m_list.push_back(node);
					}
				}
				break;
			default:
				close(new_sock);
				new_sock = -1;
				break;
		}
	}

	if (new_sock > 0)
		epoll_register(m_epoll_fd, new_sock);

	return new_sock;
}

int jt808_service::jt808_service_wait(int time_out)
{
	return epoll_wait(m_epoll_fd, m_epoll_events, m_max_count, time_out);
}

int jt808_service::accept_new_cmd_client(void)
{
	memset(&m_uid, 0x0, sizeof(uid_t));
	m_client_fd = serv_accept(m_socket_fd, &m_uid);
	return m_client_fd;
}

void jt808_service::run(int time_out)
{
	char recv_buff[1024] = {0};
	node_t node;
	propara_t propara;
	message_t msg;

	while(1){
		int ret = jt808_service_wait(time_out);
		if(ret == 0){
			//printf("time out\n");
			continue;
		} else {
			for (int i = 0; i < ret; i++) {
				if (m_epoll_events[i].data.fd == m_listen_sock) {
					if (m_epoll_events[i].events & EPOLLIN) {
						accept_new_client();
					}
				} else if (m_epoll_events[i].data.fd == m_socket_fd) {
					if (m_epoll_events[i].events & EPOLLIN) {
						accept_new_cmd_client();
						epoll_register(m_epoll_fd, m_client_fd);
					}
				} else if (m_epoll_events[i].data.fd == m_client_fd) {
					memset(recv_buff, 0x0, sizeof(recv_buff));
					if ((ret = recv(m_client_fd, recv_buff, sizeof(recv_buff), 0)) > 0) {
						parse_commond(recv_buff);
					} else if (ret == 0) {
						epoll_deregister(m_epoll_fd, m_client_fd);
						close(m_client_fd);
					}
				} else if ((m_epoll_events[i].events & EPOLLIN) && !m_list.empty()) {
					auto it = m_list.begin();
					while (it != m_list.end()) {
						if (m_epoll_events[i].data.fd == it->sockfd) {
							if (!recv_frame_data(m_epoll_events[i].data.fd, msg)) {
								jt808_frame_parse(msg, propara);
							} else {
								epoll_deregister(m_epoll_fd, m_epoll_events[i].data.fd);
								close(m_epoll_events[i].data.fd);
								it->sockfd = -1;
								node = *it;
								m_list.erase(it);
								m_list.push_back(node);;
							}
							break;
						} else {
							++it;
						}
					}
				} else {
				}
			}
		}
	}
}

int jt808_service::send_frame_data(const int &fd, const message_t &msg)
{
	int ret = -1;

	signal(SIGPIPE, SIG_IGN);
	ret = send(fd, msg.data, msg.len, 0);
	if (ret < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
			//printf("%s[%d]: no data to send, continue\n", __FILE__, __LINE__);
			ret = 0;
		} else {
			if (errno == EPIPE) {
				printf("%s[%d]: remote socket close!!!\n", __FILE__, __LINE__);
			}
			ret = -1;
			printf("%s[%d]: send data failed!!!\n", __FILE__, __LINE__);
		}
	} else if (ret == 0) {
		printf("%s[%d]: connection disconect!!!\n", __FILE__, __LINE__);
		ret = -1;
	}

	return ret >= 0 ? 0 : ret;
}

int jt808_service::recv_frame_data(const int &fd, message_t &msg)
{
	int ret = -1;

	memset(msg.data, 0x0, MAX_PROFRAMEBUF_LEN);
	ret = recv(fd, msg.data, MAX_PROFRAMEBUF_LEN, 0);
	if (ret < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
			//printf("%s[%d]: no data to receive, continue\n", __FILE__, __LINE__);
			ret = 0;
		} else {
			ret = -1;
			printf("%s[%d]: recv data failed!!!\n", __FILE__, __LINE__);
		}
		msg.len = 0;
	} else if(ret == 0) {
		ret = -1;
		msg.len = 0;
		printf("%s[%d]: connection disconect!!!\n", __FILE__, __LINE__);
	} else {
		msg.len = ret;
	}

	return ret >= 0 ? 0 : ret;
}

int jt808_service::jt808_frame_pack(message_t &msg, const uint16_t &cmd, const propara_t &propara)
{
	struct message_head *msghead_ptr;
	uint8_t *msg_body;
	uint16_t u16temp;

	msghead_ptr = (struct message_head *)&msg.data[1];
	msghead_ptr->id = endian_swap16(cmd);
	msghead_ptr->attribute.val = 0x0000;
	msghead_ptr->attribute.bit.encrypt = 0;
	if (propara.wpara3 > 1) {
		msghead_ptr->attribute.bit.package = 1;
		msg_body = &msg.data[MSGBODY_PACKAGE_POS];
		u16temp = propara.wpara3;
		u16temp = endian_swap16(u16temp);
		memcpy(&msg.data[13], &u16temp, 2);
		u16temp = propara.wpara4;
		u16temp = endian_swap16(u16temp);
		memcpy(&msg.data[15], &u16temp, 2);
		msg.len = 16;
	} else {
		msghead_ptr->attribute.bit.package = 0;
		msg_body = &msg.data[MSGBODY_NOPACKAGE_POS];
		msg.len = 12;
	}

	m_msgflownum++;
	msghead_ptr->msgflownum = endian_swap16(m_msgflownum);
	memcpy(msghead_ptr->phone, propara.strpara1, 6);

	switch (cmd) {
		case DOWN_UNIRESPONSE:
			*(uint16_t *)msg_body = endian_swap16(propara.wpara1);
			msg_body += 2;
			*(uint16_t *)msg_body = endian_swap16(propara.wpara2);
			msg_body += 2;
			*msg_body = propara.bpara1;
			msg_body++;
			msghead_ptr->attribute.bit.msglen = 5;
			msghead_ptr->attribute.val = endian_swap16(msghead_ptr->attribute.val);
			msg.len += 5;
			break;
		case DOWN_REGISTERRSPONSE:
			*(uint16_t *)msg_body = endian_swap16(propara.wpara1);
			msg_body += 2;
			*msg_body = propara.bpara1;
			msg_body++;
			if (!propara.bpara1) {
				memcpy(msg_body, propara.strpara2, 4);
				msg_body += 4;
				msghead_ptr->attribute.bit.msglen = 7;
				msg.len += 7;
			} else {
				msghead_ptr->attribute.bit.msglen = 3;
				msg.len += 3;
			}
			msghead_ptr->attribute.val = endian_swap16(msghead_ptr->attribute.val);
			break;
		case DOWN_UPDATEPACKAGE:
			*msg_body = propara.bpara1;
			msg_body ++;
			msg_body += 5;
			msg.len += 6;
			*msg_body = 8;
			msg_body++;
			msg.len++;
			memcpy(msg_body, propara.strpara2, strlen((char *)propara.strpara2));
			msg_body += strlen((char *)propara.strpara2);
			msg.len += strlen((char *)propara.strpara2);
			*(uint32_t*)msg_body = endian_swap32(propara.wpara1);
			msg_body += 4;
			msg.len += 4;
			memcpy(msg_body, propara.strpara3, propara.wpara1);
			msg_body += propara.wpara1;
			msg.len += propara.wpara1;
			msghead_ptr->attribute.bit.msglen = 11 + strlen((char *)propara.strpara2) + propara.wpara1;
			msghead_ptr->attribute.val = endian_swap16(msghead_ptr->attribute.val);
			break;
		default:
			break;
	}

	*msg_body = bcc_sum(&msg.data[1], msg.len);
	msg.len++;

	msg.len = escape(msg.data + 1, msg.len);
	msg.data[0] = PROTOCOL_SIGN;
	msg.len++;
	msg.data[msg.len++] = PROTOCOL_SIGN;

#if 0
	printf("%s[%d]: socket-send:\n", __FILE__, __LINE__);
	for (uint16_t i = 0; i < msg.len; ++i) {
		printf("%02X ", msg.data[i]);
	}
	printf("\r\n");
#endif

	return msg.len;
}

uint16_t jt808_service::jt808_frame_parse(message_t &msg, propara_t &propara)
{
	struct message_head *msghead_ptr;
	uint8_t *msg_body;
	uint8_t pnum[12] = {0};
	uint16_t u16temp;
	uint16_t msglen;
	uint32_t u32temp;
	double latitude;
	double longitude;
	float altitude;
	float speed;
	float bearing;
	char timestamp[6];
	decltype (m_list.begin()) it;

	msg.len = reverse_escape(&msg.data[1], msg.len);
	msghead_ptr = (struct message_head *)&msg.data[1];
	msghead_ptr->attribute.val = endian_swap16(msghead_ptr->attribute.val);

	if (msghead_ptr->attribute.bit.package) {
		msg_body = &msg.data[MSGBODY_PACKAGE_POS];
	} else {
		msg_body = &msg.data[MSGBODY_NOPACKAGE_POS];
	}

	msghead_ptr->msgflownum = endian_swap16(msghead_ptr->msgflownum);
	msghead_ptr->id = endian_swap16(msghead_ptr->id);
	msglen = (uint16_t)(msghead_ptr->attribute.bit.msglen);
	switch (msghead_ptr->id) {
		case UP_REGISTER:
			memset(&propara, 0x0, sizeof(struct propara_t));
			propara.wpara1 = msghead_ptr->msgflownum;
			memcpy(propara.strpara1, msghead_ptr->phone, 6);
			if (!m_list.empty()) {
			it = m_list.begin();
				while (it != m_list.end()) {
					str2bcd_compress(it->pnum, pnum);
					if (!strncmp((char *)pnum, (char *)msghead_ptr->phone, 6))
						break;
					else
						++it;
				}
				if (it != m_list.end()) {
					if (it->sockfd == -1) {
						propara.bpara1 = 0;
						memcpy(propara.strpara2, it->acode, 4);
					} else {
						propara.bpara1 = 3;
					}
				} else {
					propara.bpara1 = 4;
				}
			} else {
				propara.bpara1 = 2;
			}
			break;
		case UP_AUTHENTICATION:
			memset(&propara, 0x0, sizeof(struct propara_t));
			propara.wpara1 = msghead_ptr->msgflownum;
			propara.wpara2 = msghead_ptr->id;
			memcpy(propara.strpara1, msghead_ptr->phone, 6);
			if (!m_list.empty()) {
				it = m_list.begin();
				while (it != m_list.end()) {
					str2bcd_compress(it->pnum, pnum);
					if (!strncmp((char *)pnum, (char *)msghead_ptr->phone, 6))
						break;
					else
						++it;
				}
				if ((it != m_list.end()) && (msglen == 4)
						&& !strncmp((char *)it->acode, (char *)msg_body, 4)) {
					propara.bpara1 = 0;
				} else {
					propara.bpara1 = 1;
				}
			} else {
				propara.bpara1 = 1;
			}
			break;
		case UP_UNIRESPONSE:
			memcpy(&u16temp, &msg_body[2], 2);
			u16temp = endian_swap16(u16temp);
			switch(u16temp) {
				case DOWN_UPDATEPACKAGE:
					printf("%s[%d]: received updatepackage respond: ", __FILE__, __LINE__);
					break;
				default:
					break;
			}
			if (msg_body[4] == 0x00) {
				printf("normal\r\n");
			} else if(msg_body[4] == 0x01) {
				printf("failed\r\n");
			} else if(msg_body[4] == 0x02) {
				printf("message has something wrong\r\n");
			} else if(msg_body[4] == 0x03) {
				printf("message not support\r\n");
			}
			break;
		case UP_UPDATERESULT:
			printf("%s[%d]: received updateresult respond: ", __FILE__, __LINE__);
			if (msg_body[4] == 0x00) {
				printf("normal\r\n");
			} else if(msg_body[4] == 0x01) {
				printf("failed\r\n");
			} else if(msg_body[4] == 0x02) {
				printf("message has something wrong\r\n");
			} else if(msg_body[4] == 0x03) {
				printf("message not support\r\n");
			}
			break;
		case UP_POSITIONREPORT:
			printf("%s[%d]: received position report respond:\n", __FILE__, __LINE__);
			memset(pnum, 0x0, sizeof(pnum));
			bcd2str_compress(msghead_ptr->phone, pnum, 6);
			printf("\tdevice: %s\n", pnum);
			memcpy(&u32temp, &msg_body[12], 4);
			latitude = endian_swap32(u32temp) / 1000000.0;
			printf("\tlatitude: %lf\n", latitude);
			memcpy(&u32temp, &msg_body[8], 4);
			longitude = endian_swap32(u32temp) / 1000000.0;
			printf("\tlongitude: %lf\n", longitude);
			memcpy(&u16temp, &msg_body[16], 2);
			altitude = endian_swap16(u16temp);
			printf("\taltitude: %f\n", altitude);
			memcpy(&u16temp, &msg_body[18], 2);
			speed = endian_swap16(u16temp) / 10.0;
			printf("\tspeed: %f\n", speed);
			memcpy(&u16temp, &msg_body[20], 2);
			bearing = endian_swap16(u16temp);
			printf("\tbearing: %f\n", bearing);
			timestamp[0] = bcd2hex(msg_body[22]);
			timestamp[1] = bcd2hex(msg_body[23]);
			timestamp[2] = bcd2hex(msg_body[24]);
			timestamp[3] = bcd2hex(msg_body[25]);
			timestamp[4] = bcd2hex(msg_body[26]);
			timestamp[5] = bcd2hex(msg_body[27]);
			printf("\ttimestamp: 20%02d-%02d-%02d, %02d:%02d:%02d\n", timestamp[0], timestamp[1], timestamp[2]
					, timestamp[3], timestamp[4], timestamp[5]);
			printf("%s[%d]: received position report respond end\n", __FILE__, __LINE__);
		default:
			break;
	}

	return msghead_ptr->id;
}

int jt808_service::parse_commond(const char *buffer)
{
	string s, s1;
	string::size_type pos, spos, epos;
	stringstream ss;
	decltype (m_list.begin()) it;

	ss.clear();
	ss << buffer;
	ss >> s;
	if (s == "update") {
		ss >> s;
		//std::cout << s << std::endl;
		if (!m_list.empty()) {
			it = m_list.begin();
			while (it != m_list.end()) {
				s1 = (char *)it->pnum;
				if (s == s1) {
					printf("find phone num\n");
					break;
				} else {
					++it;
				}
			}
			if (it != m_list.end()) {
				ss >> s;
				//std::cout << s << std::endl;
				if ((s == "device") || (s == "gpsfw")) {
					if (s == "device")
						it->type = 0x0;
					else
						it->type = 0x34;

					ss >> s;
					//std::cout << s << std::endl;
					memset(it->verid, 0x0, sizeof(it->verid));
					s.copy((char *)it->verid, s.length(), 0);

					ss >> s;
					//std::cout << s << std::endl;
					memset(file_path, 0x0, sizeof(file_path));
					s.copy(file_path, s.length(), 0);

					node_t node = *it;
					m_list.erase(it);
					m_list.push_back(node);

					while (!m_mtx.try_lock()) ;
					has_update_node = node;

					/* start thread. */
					std::thread sut(start_update_thread, this);
					sut.detach();
				}
			}
		}
		ss.str("");
	}


	return 0;
}

void jt808_service::update_handler(void)
{
	message_t msg;
	ifstream ifs;
	propara_t propara;
	char *data = nullptr;
	uint32_t len, max_data_len;
	decltype (m_list.begin()) it;
	node_t node;

	node = has_update_node;
	m_mtx.unlock();

	if (!m_list.empty()) {
		it = m_list.begin();
		while (it != m_list.end()) {
			if (!strncmp((char *)it->pnum, (char *)node.pnum, 12)) {
				node = *it;
				break;
			} else {
				++it;
			}
		}
	}

	if (it == m_list.end())
		return ;

	memcpy(propara.strpara2, node.verid, strlen((char *)it->verid));
	max_data_len = 1023 - 11 - strlen((char *)it->verid);
	epoll_deregister(m_epoll_fd, it->sockfd);
	//printf("path: %s\n", file_path);
	ifs.open(file_path, std::ios::binary | std::ios::in);
	if (ifs.is_open()) {
		ifs.seekg(0, std::ios::end);
		len = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		data = new char[len];
		ifs.read(data, len);
		ifs.close();

		propara.wpara3 = len/max_data_len + 1;
		propara.wpara4 = 1;
		propara.bpara1 = it->type;

		propara.strpara3 = new unsigned char [1024];
		while (len > 0) {
			memset(msg.data, 0x0, MAX_PROFRAMEBUF_LEN);
			if (len > max_data_len)
				propara.wpara1 = max_data_len;
			else
				propara.wpara1 = len;
			len -= propara.wpara1;
			//printf("sum_packet = %d, sub packet = %d\n", propara.wpara3, propara.wpara4);
			memset(propara.strpara3, 0x0, 1024);
			memcpy(propara.strpara3, data + max_data_len * (propara.wpara4 - 1), propara.wpara1);
			msg.len = jt808_frame_pack(msg, DOWN_UPDATEPACKAGE, propara);
			if (send_frame_data(node.sockfd, msg)) {
				close(node.sockfd);
				node.sockfd = -1;
				m_list.erase(it);
				m_list.push_back(node);
				break;
			} else {
				while (1) {
					if (recv_frame_data(node.sockfd, msg)) {
						close(node.sockfd);
						node.sockfd = -1;
						m_list.erase(it);
						m_list.push_back(node);
						break;
					} else if (msg.len) {
						jt808_frame_parse(msg, propara);
						break;
					}
				}

				if (node.sockfd == -1)
					break;

				++propara.wpara4;
				usleep(1000);
			}
		}

		if (node.sockfd > 0) {
			epoll_register(m_epoll_fd, node.sockfd);
		}

		delete [] data;
		delete [] propara.strpara3;
		data = 0;
	}

	return ;
}

void *jt808_service::start_update_thread(void *arg)
{
	jt808_service *_this = (jt808_service *)arg;
	_this->update_handler();

	return nullptr;
}

