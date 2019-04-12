#ifndef __JT808_SERVICE_H__
#define __JT808_SERVICE_H__

#include <string>
#include <list>
#include <jt808_util.h>
#include <mutex>

using std::list;

struct node_t {
	uint8_t pnum[12];
	uint8_t acode[16];
	uint8_t verid[20];
	uint8_t type;
	int16_t sockfd;
};

class jt808_service {
public:
	jt808_service();
	virtual ~jt808_service();
	bool init(int port, int sock_count);
	bool init(const char *ip, int port, int sock_count);
	int accept_new_client(void);
	int accept_new_cmd_client(void);
	int jt808_service_wait(int time_out);
	void run(int time_out);
	int send_frame_data(const int &fd, const message_t &msg);
	int recv_frame_data(const int &fd, message_t &msg);
	int jt808_frame_pack(message_t &msg, const uint16_t &cmd, const propara_t &propara);
	uint16_t jt808_frame_parse(message_t &msg, propara_t &propara);
	void read_devices_list(void);
	int parse_commond(const char *buffer);
	static void *start_update_thread(void *arg);
	void update_handler(void);

private:
	int m_listen_sock;
	int m_epoll_fd;
	int m_max_count;
	int m_msgflownum;
	int m_socket_fd;
	int m_client_fd;
	std::mutex m_mtx;
	uid_t m_uid;
	node_t has_update_node;
	message_t m_msg;
	list<node_t> m_list;
	char file_path[256];
	struct epoll_event *m_epoll_events;
	const char *devices_file_path = "./devices/devices.list";
	const char *sock_path = "/tmp/jt808cmd.sock";
};

#endif // #ifndef __JT808_SERVICE_H__
