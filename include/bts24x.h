#pragma once



#ifdef _WIN32
#pragma comment (lib, "ws2_32.lib")
#include <WinSock2.h>
#define BTS24X_DLLEXPORT __declspec(dllexport)
#elif __linux__
#define BTS24X_DLLEXPORT
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif


#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <thread>
#include <queue>

#include "bts24x_setting.h"

using namespace std;

#ifdef _WIN32

#elif defined __linux__
typedef unsigned int		UINT;
typedef unsigned char       BYTE;
typedef struct sockaddr_in	SOCKADDR_IN;
typedef struct sockaddr		SOCKADDR;
typedef int					SOCKET;
typedef struct hostent 		PHOSTENT;
#define INVALID_SOCKET		-1
#define closesocket			close
#define Sleep(x)			usleep(x*1000)
#define SD_BOTH				SHUT_RDWR
#endif

typedef void(*callback_recv) (char* recvdata, int len_recvdata);
//typedef std::function<void(char*, int)> callback_recv;


typedef enum bts24x_working_status
{
	BTS24X_STATUS_CLOSE = 0,
	BTS24X_STATUS_OPEN,
	BTS24X_STATUS_START,
	BTS24X_STATUS_STOP,
}BTS24X_STATUS_T;

typedef struct bts24x_daq_list
{
	int n_pid;
	vector<int> list_pid;
	vector<string> list_pidname;
	BYTE* active_daq;
}BTS24X_DAQ_LIST_T;

typedef struct bts24x_packet_track_data
{
	float xpos;
	float ypos;

	float xspeed;
	float yspeed;

	float TTLC;
	float length;

	unsigned int ID;
	unsigned int status;
	unsigned int pwdb;
	unsigned int lane;

	float timestamp;

}BTS24X_PACKET_TRACK_DATA_T;

typedef struct bts24x_packet_Image
{
	int timestamp;
	int hor;
	int ver;
	int format;
	int length;
	char image[65536];

}BTS24X_PACKET_IMAGE_T;

class semaphore
{
private:
	std::mutex mutex_;
	std::condition_variable condition_;
	unsigned long count_ = 0; // Initialized as locked.

public:
	void notify() {
		std::lock_guard<decltype(mutex_)> lock(mutex_);
		++count_;
		condition_.notify_one();
	}

	void wait() {
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		while (!count_) // Handle spurious wake-ups.
			condition_.wait(lock);
		--count_;
	}

	bool try_wait() {
		std::lock_guard<decltype(mutex_)> lock(mutex_);
		if (count_) {
			--count_;
			return true;
		}
		return false;
	}
};

template <typename T>
class waitqueue
{
private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable condition_;
	unsigned long count_ = 0; // Initialized as locked.

public:
	void send(T data) {
		queue_.push(data);
		std::lock_guard<decltype(mutex_)> lock(mutex_);
		++count_;
		condition_.notify_one();
	}

	void recv(T& data) {
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		while (!count_) // Handle spurious wake-ups.
			condition_.wait(lock);
		--count_;
		data = queue_.front();
		queue_.pop();
	}

	int size() {
		return queue_.size();
	}

	bool empty() {
		return queue_.empty();
	}
};

class BTS24x
{
public:
	BTS24X_STATUS_T radar_status;
	BTS24X_DAQ_LIST_T daq_list;

public:
	BTS24X_DLLEXPORT BTS24x(void);
	BTS24X_DLLEXPORT ~BTS24x(void);

	BTS24X_DLLEXPORT bool OpenRadar(string str_ip, callback_recv callback_func);
	BTS24X_DLLEXPORT bool OpenRadar(string str_ip, int port, callback_recv callback_func);
	BTS24X_DLLEXPORT bool CloseRadar(void);

	BTS24X_DLLEXPORT bool StartRadar(void);
	BTS24X_DLLEXPORT bool StopRadar(void);

	BTS24X_DLLEXPORT bool GetTrackdata(char* recvdata, int len_recv, vector <BTS24X_PACKET_TRACK_DATA_T>& track_data);
	BTS24X_DLLEXPORT bool GetImage(char* recvdata, int len_recv, BTS24X_PACKET_IMAGE_T& image);

	BTS24X_DLLEXPORT bool GetIP(vector<string>& ip);
	BTS24X_DLLEXPORT bool SetIP(string target_ip, string ip, string gateway);

	BTS24X_DLLEXPORT bool SetDAQDefault(void);

private:

	bool ConnectRadar(void);
	bool DisconnectRadar(void);

	void ShutdownConnect(void);

	bool CmdConnect(void);
	bool CmdDisconnect(void);
	bool CmdGetStatus(void);
	bool CmdGetDAQlist(void);
	bool CmdSetDAQlist(void);
	bool CmdSetSync(int bStart);
	bool CmdSync(void);

	bool GetIP(vector<string>& ip, string gateway);

	bool RegistryCallback(callback_recv callback_func);
	//bool RegistryCallback(handleClass handle_class);

	bool GetDAQList(char* daq_info);

private:

	bool m_connect_status;
	bool m_sync_status;
	int m_thread_status;

	char* m_recvdata;
	char* m_daqlist;
	int m_sz_daqlist;

	unsigned int m_cnt_cro;
	unsigned int m_cnt_dto;


	callback_recv m_callback_recv;

	thread m_thread_recv;

	SOCKET	m_sock_radar;
	SOCKADDR_IN m_target;

	semaphore m_sem_thread_recv_start;
	bool m_run_recv_thread;

	vector<int> m_daqlist_pid;
	vector<string> m_daqlist_name;

	double prv_time;
	double cur_time;

	//WORD cycletime;

};
