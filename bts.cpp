#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <signal.h>

#include <pthread.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <unistd.h>

#include <math.h>
#include <time.h>

#include <memory>

#include <iostream>
#include <fstream>
#include <chrono>
#include <sys/time.h>
#include <ctime>
#include <jsoncpp/json/json.h>

#include <../chilkat/include/CkJsonObject.h>
#include <../chilkat/include/CkJsonArray.h>

#include "bts24x.h"
 
#define ESCAPE 27
#define ENTER 10

#define BUFSIZE_IP  80
#define FILENAME_SIZE		255
#define TRACKDATA_SIZE		44
#define IMAGEDATA_SIZE		65556  // includes the image = 65536 and 5 other fields
#define IMAGEONLY_SIZE		65536
#define IMAGEINDEX_SIZE		20

#define CAPTURE_TRACK		0X1	// flag to capture track data only
#define CAPTURE_CAMERA		0X2	// flag to capture image data only
#define CAPTURE_ALL		0X3     // flag indicating both track and image data is to be captured



using namespace std;
 
typedef enum
{
	MENUSTATE_MAIN = 0,
	MENUSTATE_OPENRADAR,
	MENUSTATE_SETIP,
	MENUSTATE_GETIP,
}menustate_t;

typedef enum
{
	NOT_RUNNING = 0,
	RUNNING,
	RUNNING_CAPTURE_BIN,
	RUNNING_CAPTURE_JSON,
}runningstate_t;

runningstate_t  mRunningState;

typedef enum
{
	DATATYPE_NONE = 0,
	DATATYPE_TRACK,
	DATATYPE_IMAGEINDEX,
	DATATYPE_IMAGE,
}datatype_t;

typedef enum
{
	EXECUTE_NONE = 0,
	EXECUTE_MENU,
	EXECUTE_COMMANDLINE,
	EXECUTE_STOPPING,
}executetype_t;

executetype_t mExecuteMode;
bool mExitNow;
bool mDebugFlag;

WINDOW *menu_items[15];
WINDOW *statusbar_win;
WINDOW *dataview_win;

ofstream mFile_Trackbin;
ofstream mFile_Trackjson;
ofstream mFile_ImageIndexbin;
ofstream mFile_ImageIndexjson;
ofstream mFile_Image;
ofstream mFile_debug;

CkJsonArray jsonTrackDataArray;
CkJsonArray jsonImageIndexArray;

unsigned int mTrackDataCount;
unsigned int mImageDataCount;


const char *main_itemset[] = {"",
                    "Open Radar", 
                    "Get IP", 
                    "Set IP", 
                    "Exit"
					};

const char *openradar_itemset[] = {"",
                    "Radar IP", 
                    "Start",
                    "Start and Save Data bin",
		    "Start and Save Data json",
					"Back"
					};					

const char *setip_itemset[] = {"",
                    "Current Radar IP", 
                    "Change Radar IP", 
                    "Change Radar Gateway", 
                    "Set IP",
					"Back"
					};



string mCurrentIP; 
int mCaptureFlag;

BTS24x bts24;
int en_start = 0;

/* title, status will be globall refresh */

char cur_ip[BUFSIZE_IP];
char change_ip[BUFSIZE_IP];
char change_gw[BUFSIZE_IP]; 


void init_screen(void);
void end_screen(void);
static void end_proc(int p_signo);
static void Setsigfatal(void);
void refresh_status(const char *msg);
void menu_list(WINDOW **items, const char **menu, int cnt, int start_col);
int scroll_menu(WINDOW **items,int count,int menu_start_col, int index_num);
void delete_menu(WINDOW **items, int count);
int insert_ip(char* ip, const char* msg);
int get_yn_menu(const char* msg);
void update_status(const char* str_status);
void update_status(const char* str_status, const char* str_value);
void print_dataview(const char* data);
void print_ip(void);

int save_bin_trackdata(bts24x_packet_track_data trackData, int nTotalTracks, int nTrack);
int save_json_trackdata(bts24x_packet_track_data trackData, int nTotalTracks, int nTrack);
int get_data_filename(int nType, char *pszFilename);
int save_bin_imagedata(bts24x_packet_Image imageData);
int save_json_imagedata(bts24x_packet_Image imageData);
int ProcessCommandLineParams(int argc, char **argv);
int start_radar(int& en_start);

int menu_oepnradar(menustate_t& menu_state, int& en_start);
int menu_getip(menustate_t& menu_state);
int menu_setip(menustate_t& menu_state);
void stop_radar(void);

void test_callback(char* recvdata, int len_recv);

void* quick_thread(void* arg);

template<typename ... Args>
std::string format_string(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1;
	std::unique_ptr<char[]> buffer(new char[size]);
	snprintf(buffer.get(), size, format.c_str(), args ...);
	return std::string(buffer.get(), buffer.get() + size - 1);
}

int menu_oepnradar(menustate_t& menu_state, int& en_start)
{
	int sel_item;
	int nErr = 0;
	char buf_dataview_win[BUFSIZ];
	// update_status("Select Menu");
	menu_list(menu_items, openradar_itemset, 6, 8);
	sel_item = scroll_menu(menu_items, 6, 5, 0); 

	if (mDebugFlag)
		mFile_debug << "1] menu_openradar sel_item = " << sel_item << " mRunningState: " << mRunningState << endl;

	if (mRunningState > 0 && sel_item == 0)
	{
	//	sel_item = int(mRunningState) + 1;  
		if (mDebugFlag)
			mFile_debug << "2] menu_openradar sel_item = " << sel_item << endl;
	}

	string str_daqlist = "";
	char daqlist[1024];

	

	string str_ip = mCurrentIP;    //cur_ip;  <-orig code
	if (sel_item == 1) // Radar IP
	{
		insert_ip(cur_ip, "insert Radar IP");
		strcpy(buf_dataview_win, "Radar IP : ");
		strcat(buf_dataview_win, cur_ip);
		strcat(buf_dataview_win, "\n");
		print_dataview(buf_dataview_win);
		menu_state = MENUSTATE_OPENRADAR;

		print_ip();
		return 0;
	}
	else if (sel_item >= 2 && sel_item <= 4) // Start
	{
		mFile_debug << "3] menu_openradar sel_item = " << sel_item << endl;
		if (str_ip.size() > 0)
		{
			if (mDebugFlag)
				mFile_debug << "4] menu_openradar str_ip = " << str_ip << endl;
			if (!bts24.OpenRadar(str_ip, test_callback))
			{
				update_status("[ERR] Connect Fail");
			}				
			else
			{
				int pid;
				string str_name;
				bts24.SetDAQDefault();
				if (mDebugFlag)
					mFile_debug << "5] menu_openradar OpenRadar successful! " << endl;
				if (!bts24.StartRadar())
				{
					update_status("[ERR] Start Data Streaming Fail");
				}
				else
				{	if (mDebugFlag)				
						mFile_debug << "6] menu_openradar StartRadar successful " << endl;
					if (sel_item == 2)
						mRunningState = RUNNING;
					else if (sel_item == 3)
					{
						mRunningState = RUNNING_CAPTURE_BIN;
						mTrackDataCount = 0;
						mImageDataCount = 0;
					}
					else if (sel_item == 4)
					{
						mRunningState = RUNNING_CAPTURE_JSON;
						mTrackDataCount = 0;
						mImageDataCount = 0;
					}
					update_status("Press q to Stop Radar");
					en_start = 1;
				}
				menu_state = MENUSTATE_MAIN;
			}
		}		
		else
		{
			update_status("Please Enter Radar IP");
			menu_state = MENUSTATE_OPENRADAR;
		}
	}
	else if (sel_item == 5) // Back
	{
		menu_state = MENUSTATE_MAIN;
	}

	touchwin(stdscr);
	refresh(); 	
	return 1;
}

// Need this so we can get the capturing going with the use of the
// command parameters.
int start_radar(int& en_start)
{
	if (mDebugFlag)
		mFile_debug << "start_radar" << endl;

	int nRet = 0;

	if (!bts24.OpenRadar(mCurrentIP, test_callback))
	{
		update_status("[ERR] Connect Fail");
	}				
	else
	{
		int pid;
		string str_name;
		bts24.SetDAQDefault();

		if (mDebugFlag)
			mFile_debug << "start_radar 1] OpenRadar success  " << endl; 

		if (!bts24.StartRadar())
		{
			update_status("[ERR] Start Data Streaming Fail");
		}
		else
		{					
			if (mDebugFlag)
				mFile_debug << "start_radar 1] StartRadar success  " << endl; 
			mTrackDataCount = 0;
			mImageDataCount = 0;
			update_status("Press 'q' TWICE to Stop Radar and exit application");
			en_start = 1;
		}
	
	}  // end else, radar did open

	return nRet;
}

// nType is whether it is Track = 1 or ImageIndex = 2, Image = 3
int get_data_filename(int nType, char *pszFilename)
{
	int nErr = 0;
	time_t  tNow = time(NULL);
	struct tm tmNow = *localtime(&tNow);
	char szType[20];

	memset(szType, 0, sizeof(szType));

	if (nType == DATATYPE_TRACK)
		strcpy(szType, "Track");
	else if (nType == DATATYPE_IMAGE)
	{
		sprintf(pszFilename, "Data/img/Image%d%02d%02d_%02d%02d%02d.bin", tmNow.tm_year+1900, tmNow.tm_mon+1, tmNow.tm_mday,
				tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
	}
	else if (nType == DATATYPE_IMAGEINDEX)
		strcpy(szType,  "ImageIndex");
		

	if (mRunningState == RUNNING_CAPTURE_BIN)
	{
		if (nType == DATATYPE_TRACK)
			sprintf(pszFilename, "Data/bin/%s%d%02d%02d_%02d%02d%02d.bin", szType, tmNow.tm_year+1900, tmNow.tm_mon+1, tmNow.tm_mday,
				tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
		else if (nType == DATATYPE_IMAGEINDEX) // we need and index file name too
			sprintf(pszFilename, "Data/bin/%s%d%02d%02d_%02d%02d%02d.bin", szType, tmNow.tm_year+1990, tmNow.tm_mon+1, tmNow.tm_mday,
				tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);

	}
	else if (mRunningState == RUNNING_CAPTURE_JSON)
	{
		if (nType == DATATYPE_TRACK)
			sprintf(pszFilename, "Data/json/%s%d%02d%02d_%02d%02d%02d.json", szType, tmNow.tm_year+1900, tmNow.tm_mon+1, tmNow.tm_mday,
				tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
		else if (nType == DATATYPE_IMAGEINDEX) // we need and index file name too
			sprintf(pszFilename, "Data/json/%s%d%02d%02d_%02d%02d%02d.json", szType, tmNow.tm_year+1900, tmNow.tm_mon+1, tmNow.tm_mday,
				tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);

	}

	return nErr;
}

int save_bin_trackdata(bts24x_packet_track_data trackData, int nTotalTracks, int nTrack)
{
	char szFilename[FILENAME_SIZE];
	int nRet = 0;
	char szData[TRACKDATA_SIZE];
	int nIndex = 0;

	memset(szFilename, 0, sizeof(szFilename));
	memset(szData, 0, sizeof(szData));
	

	if (mFile_Trackbin.is_open() == 0)
	{
		nRet = get_data_filename(DATATYPE_TRACK, szFilename);
		mFile_Trackbin.open(szFilename, ios::out | ios::binary);
	}

	memcpy(szData+nIndex, &trackData.timestamp, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.ID, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.status, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.pwdb, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.lane, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.xpos, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.ypos, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.xspeed, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.yspeed, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.TTLC, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &trackData.length, 4);
	nIndex +=4;

	mFile_Trackbin.write(szData, nIndex);

	return nRet;
}

int save_json_trackdata(bts24x_packet_track_data trackData, int nTotalTracks, int nTrack)
{
	char szFilename[FILENAME_SIZE];
	int nRet = 0;
	bool bSuccess = false;
	struct timeval time_now{};

	gettimeofday(&time_now, nullptr);
	memset(szFilename, 0, sizeof(szFilename));


	if (mFile_Trackjson.is_open() == 0)
	{
		nRet = get_data_filename(1, szFilename);
		mFile_Trackjson.open(szFilename, ios::out | ios::binary);
	}

	// 0 is the first track, from the index of the loop
	if (nTrack == 0) // need to add a new top most json object for the array
	{
		jsonTrackDataArray.AddObjectAt(mTrackDataCount-1);
		CkJsonObject *jsonTrackData = jsonTrackDataArray.ObjectAt(mTrackDataCount-1);
		bSuccess = jsonTrackData->UpdateInt("trackdatanum", mTrackDataCount);
		bSuccess = jsonTrackData->UpdateInt("seconds", time_now.tv_sec);
		bSuccess = jsonTrackData->UpdateInt("microsecs", time_now.tv_usec);
		jsonTrackDataArray.AddArrayAt(mTrackDataCount-1);

		delete jsonTrackData;

	}
	// need to add a lower json object to the tracks array

	
	CkJsonArray *aTrack = jsonTrackDataArray.ArrayAt(mTrackDataCount-1);
	aTrack->AddObjectAt(-1);
	CkJsonObject *oTrack = aTrack->ObjectAt(aTrack->get_Size()-1);
	bSuccess = oTrack->AddIntAt(-1, "TrackNum", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "timestamp", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "ID", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "Status", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "pwdb", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "Lane", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "xPos", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "yPos", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "xSpeed", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "ySeed", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "TTLC", nTrack);	
	bSuccess = oTrack->AddIntAt(-1, "Length", nTrack);	

	
	delete aTrack;
	delete oTrack;

	return nRet;
}

int save_bin_imagedata(bts24x_packet_Image imageData)
{
	char szFilename[FILENAME_SIZE];
	int nRet = 0;
	char szData[IMAGEINDEX_SIZE];
	int nIndex = 0;
	struct timeval time_now{};

	gettimeofday(&time_now, nullptr);

	memset(szFilename, 0, sizeof(szFilename));
	memset(szData, 0, sizeof(szData));
	

	if (mFile_ImageIndexbin.is_open() == 0)
	{
		nRet = get_data_filename(DATATYPE_IMAGEINDEX, szFilename);
		mFile_ImageIndexbin.open(szFilename, ios::out | ios::binary);
	}

	if (mFile_Image.is_open() == 0)
	{
		nRet = get_data_filename(DATATYPE_IMAGE, szFilename);
		mFile_Image.open(szFilename, ios::out | ios::binary);
	}

	memcpy(szData+nIndex, &imageData.timestamp, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &imageData.hor, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &imageData.ver, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &imageData.format, 4);
	nIndex +=4;
	memcpy(szData+nIndex, &imageData.length, 4);
	nIndex +=4;
	
	mFile_ImageIndexbin.write(szData, nIndex);
	mFile_Image.write(imageData.image, imageData.length);

	return nRet;
}

int save_json_imagedata(bts24x_packet_Image imageData)
{
	char szFilename[FILENAME_SIZE];
	
	int nRet = 0;
	bool bSuccess = false;
	struct timeval time_now{};

	gettimeofday(&time_now, nullptr);
	memset(szFilename, 0, sizeof(szFilename));
	
//	mFile_debug << "1] save_json_imagedata timestamp = " << imageData.timestamp << " mImageDataCount: " << mImageDataCount << endl;
	jsonImageIndexArray.AddObjectAt(mImageDataCount-1);
	CkJsonObject *jsonImg = jsonImageIndexArray.ObjectAt(mImageDataCount-1);

	if (mFile_ImageIndexjson.is_open() == 0)
	{
		nRet = get_data_filename(DATATYPE_IMAGEINDEX, szFilename);
		mFile_ImageIndexjson.open(szFilename, ios::out | ios::binary);
	}

	if (mFile_Image.is_open() == 0)
	{
		nRet = get_data_filename(DATATYPE_IMAGE, szFilename);
		mFile_Image.open(szFilename, ios::out | ios::binary);
	}

	mFile_Image.write(imageData.image, imageData.length);

	bSuccess = jsonImg->UpdateInt("imagenum", mImageDataCount);
	bSuccess = jsonImg->UpdateInt("seconds", time_now.tv_sec);
	bSuccess = jsonImg->UpdateInt("micorsecs", time_now.tv_usec);
	bSuccess = jsonImg->UpdateInt("timestamp", imageData.timestamp);
	bSuccess = jsonImg->UpdateInt("hor", imageData.hor);
	bSuccess = jsonImg->UpdateInt("ver", imageData.ver);
	bSuccess = jsonImg->UpdateInt("format", imageData.format);	
	bSuccess = jsonImg->UpdateInt("length", imageData.length);

//	mFile_debug << "2] save_json_imagedata timestamp = " << imageData.timestamp << endl;
	delete jsonImg;	
	
	return nRet;
}

int menu_getip(menustate_t& menu_state)
{
	update_status("Waiting Get IP");
	vector<string> get_ip_set;

	char ip_buf[1024];
	string str_ip_buf = "";
	bts24.GetIP(get_ip_set);

	menu_state = MENUSTATE_MAIN;

	if (get_ip_set.size() == 0)
	{
		str_ip_buf = "Not Found Radar";
		strcpy(ip_buf, str_ip_buf.c_str());
		print_dataview(ip_buf);
		touchwin(stdscr);
		refresh(); 		

		return 0;
	}

	str_ip_buf += "Get IP List\n";
	for (int idx=0;idx < get_ip_set.size();idx++)
	{
		str_ip_buf += format_string("%d:", idx);
		str_ip_buf += get_ip_set[idx];
		str_ip_buf += "\n";
	}

	strcpy(ip_buf, str_ip_buf.c_str());
	print_dataview(ip_buf);
	touchwin(stdscr);
	refresh(); 			

	return 1;
}

int menu_setip(menustate_t& menu_state)
{
	int sel_item;

	string str_curip, str_chgip, str_chggw;

	// update_status("Select Menu");
	menu_list(menu_items, setip_itemset, 6, 8);
	sel_item = scroll_menu(menu_items, 6, 5, 0); 
	if (sel_item == 1) // Current Radar IP
	{
		insert_ip(cur_ip, "insert Current Radar IP");		
		print_ip();
		menu_state = MENUSTATE_SETIP;
	}
	else if (sel_item == 2) // Change Radar IP
	{
		insert_ip(change_ip, "insert Change Radar IP");
		print_ip();
		menu_state = MENUSTATE_SETIP;
	}
	else if (sel_item == 3) // Change Radar Gateway
	{
		insert_ip(change_gw, "insert Change Radar Gateway");
		print_ip();
		menu_state = MENUSTATE_SETIP;
	}
	else if (sel_item == 4) // Set IP
	{
		if ((strlen(cur_ip) == 0) || (strlen(change_ip) == 0) || (strlen(change_gw) == 0))
		{
			menu_state = MENUSTATE_SETIP;
			update_status("Please Enter All");
		}
		else
		{
			str_curip = cur_ip;
			str_chgip = change_ip;
			str_chggw = change_gw;
			bts24.SetIP(str_curip, str_chgip, str_chggw);
			menu_state = MENUSTATE_MAIN;
			update_status("Reset Radar to Apply New IP");
		}
	}
	else if (sel_item == 5) // Back
	{
		menu_state = MENUSTATE_MAIN;
	}

	touchwin(stdscr);
	refresh(); 	

	return 1;
}

void stop_radar(void)
{
	if (mDebugFlag)	
		mFile_debug << "stop_radar " << endl;
	if (mFile_Trackbin.is_open())
	{
		mFile_Trackbin.close();
		if (mDebugFlag)
			mFile_debug << "Close Track bin file, number of entries: " << mTrackDataCount << endl;
	}
	if (mFile_Trackjson.is_open())
	{
		jsonTrackDataArray.put_EmitCompact(false);
		mFile_Trackjson << jsonTrackDataArray.emit() << endl;
		mFile_Trackjson.close();
		if (mDebugFlag)
			mFile_debug << "Close Track Json file, number of entries: " << mTrackDataCount << endl;
	}
	if (mFile_ImageIndexbin.is_open())
	{
		mFile_ImageIndexbin.close();
		if (mDebugFlag)
			mFile_debug << "Close ImageIndex bin file, number of entries: " << mImageDataCount << endl;
	}
	if (mFile_ImageIndexjson.is_open())
	{
		jsonImageIndexArray.put_EmitCompact(false);
		mFile_ImageIndexjson << jsonImageIndexArray.emit() << endl;
		mFile_ImageIndexjson.close();
		if (mDebugFlag)
			mFile_debug << "Close ImageIndex Json file, number of entries: " << mImageDataCount << endl;
	}
	if (mFile_Image.is_open())
	{
		mFile_Image.close();
	}
	if (mDebugFlag)
		mFile_debug << "stop_radar 6] end" << endl;
	bts24.StopRadar();
	bts24.CloseRadar();
	if (mDebugFlag)
		mFile_debug << "stop_radar 7] end" << endl;
}

void test_callback(char* recvdata, int len_recv)
{
	vector<BTS24X_PACKET_TRACK_DATA_T> track_data;
	BTS24X_PACKET_IMAGE_T image = { 0, 0, 0, 0, 0 };
	int nErr = 0;

	char track_buf[1024*1024];
	string str_track= "";

	if (en_start == 1)
	{
		if (bts24.GetTrackdata(recvdata, len_recv, track_data) && (mCaptureFlag & CAPTURE_TRACK))
		{
			// str_track = "ID\txspeed\t\txpos\t\tyspeed\t\typos\t\tTTLC\t\tlane\tlength\tPWDB\ttimestamp\n";
			str_track = "ID\txspeed\t\txpos\t\tyspeed\t\typos\t\tlane\n";
			mTrackDataCount++;			
			for (int i = 0; i < track_data.size(); i++)
			{
				if (track_data[i].ypos > 0)
				{
					if (mRunningState == RUNNING_CAPTURE_BIN)
					{
						nErr = save_bin_trackdata(track_data[i], track_data.size(), i);
						
					}
					else if (mRunningState == RUNNING_CAPTURE_JSON)
					{
						nErr = save_json_trackdata(track_data[i], track_data.size(), i);
					
					}

					str_track += format_string("%d\t", track_data[i].ID);
					str_track += format_string("%.4f\t", track_data[i].xspeed);
					if (fabs(track_data[i].xspeed) < 10)
						str_track += "\t";
					str_track += format_string("%.4f\t", track_data[i].xpos);
					if (fabs(track_data[i].xpos) < 10)
						str_track += "\t";			
					str_track += format_string("%.4f\t", track_data[i].yspeed);
					if (track_data[i].yspeed < 100)
						str_track += "\t";		
					str_track += format_string("%.4f\t", track_data[i].ypos);
					if (track_data[i].ypos < 100)
						str_track += "\t";
					// str_track += format_string("%.4f\t", track_data[i].TTLC);
					if (track_data[i].lane != 255)
					{
						str_track += format_string("%d\t", track_data[i].lane);
					}
					else
					{
						str_track += "\t";
					}
					// str_track += format_string("%.4f\t", track_data[i].length);
					// str_track += format_string("%d\t", track_data[i].pwdb);
					// str_track += format_string("%.4f\t", track_data[i].timestamp);
				}

			}

			strcpy(track_buf, str_track.c_str());
			print_dataview(track_buf);
			touchwin(stdscr);
			refresh(); 
		}
		else if (bts24.GetImage(recvdata, len_recv, image) && (mCaptureFlag & CAPTURE_CAMERA))
		{
			mImageDataCount++;
			if (mRunningState == RUNNING_CAPTURE_BIN)
			{
				nErr = save_bin_imagedata(image);

			}
			else if (mRunningState == RUNNING_CAPTURE_JSON)
			{
				nErr = save_json_imagedata(image);
			
			}
			//FILE* file = fopen("image.jpeg", "wb");
			//if (file == NULL)
			//{
			//	printf("Fail file open\n");
			//}
			//else
			//{
			//	fwrite(image.image, 1,image.length, file);
			//}
			//fclose(file);
		} // end else if image data
	}

}


void init_screen(void)
{
    initscr();
    start_color();
    noecho();
    keypad(stdscr, TRUE);
}
 
void end_screen(void)
{
    delwin(statusbar_win);
    delwin(dataview_win);
    endwin();
 
    exit(0);
}
 
static void end_proc(int p_signo)
{
    char        _Log_Buf [BUFSIZ];
 
    strcat(_Log_Buf, " Exit");
    refresh_status(_Log_Buf);
    end_screen();
}
 
static void        Setsigfatal(void)
{
    signal (SIGINT,     end_proc);
    signal (SIGKILL,    end_proc);
    signal (SIGQUIT,    end_proc);
    signal (SIGILL,     end_proc);
    signal (SIGTERM,    end_proc);
    signal (SIGBUS,     end_proc);
    signal (SIGSEGV,    end_proc);
    signal (SIGHUP,     end_proc);
 
    return;
}
 
void refresh_status(const char *msg)
{
    char blank[100];
 
    memset(blank, 0x20, sizeof(blank));
 
    wbkgd(statusbar_win, A_REVERSE);
    wmove(statusbar_win, 0, 1);
    wprintw(statusbar_win,"> ");
    wmove(statusbar_win, 0, 4);
    wprintw(statusbar_win, blank);
    wmove(statusbar_win, 0, 4);
    wprintw(statusbar_win, msg);
    wrefresh(statusbar_win);
}

 
void menu_list(WINDOW **items, const char **menu, int cnt, int start_col)
{
    int i;
 
    items[0]= newwin(cnt+1, 40, 3,start_col);
    wborder(items[0], '|', '|', '-', '-', '-', '-','-', '-');
 
    for (i =1; i < cnt; i++)
        items[i]=subwin(items[0], 1, 30, i+3, start_col+1);
 
    for (i =1; i < cnt; i++)
        wprintw((WINDOW *)items[i], " %s",menu[i]);
 
    wbkgd(items[1], A_UNDERLINE|A_STANDOUT);
    wrefresh(items[0]);
 
    return;
}
 
int scroll_menu(WINDOW **items,int count,int menu_start_col, int index_num)
{
    int key;
    int selected=1, before = 1;
 
    while (1) 
    {
        halfdelay(100);
        switch((key=getch())) {
            case KEY_DOWN:
                selected=(selected+1) % count;
                if (selected == 0)
                    selected = 1;    
                break;
            case KEY_UP: 
                selected=(selected+count-1) % count;
                if (selected == 0)
                    selected = count - 1;    
                break;
            case KEY_LEFT: 
                return key;
            case KEY_RIGHT: 
                return key;
            case ESCAPE: 
                return -1;
            case ENTER: 
                return selected;
            default:
                return -1;
        }
 
        if (selected != before)
        {
            wbkgd(items[before], COLOR_PAIR(0));
            wnoutrefresh(items[before]);
            before = selected;     
        }
        wbkgd(items[selected],A_REVERSE);
        wnoutrefresh(items[selected]);
        doupdate();
    }
}
 
void delete_menu(WINDOW **items, int count)
{
    int i;
 
    for (i = 0; i < count; i++)
        delwin((WINDOW *)items[i]);
}

int insert_ip(char* ip, const char* msg)
{
    WINDOW *win_ip;
    int key;

	echo();
 
    win_ip = newwin(4, 30, 4, 10);
	wmove(win_ip, 1, 4);
	wprintw(win_ip, msg); 
    wmove(win_ip, 2, 4);
	wborder(win_ip, '|', '|', '-', '-', '|', '|', '|', '|');
    refresh();
	wgetstr(win_ip, ip);
    // delwin(win_ip);
	endwin();

	noecho();

	if (strlen(ip) == 0)
	    return 0;
	else if (strlen(ip) > 0)
		return 1;

	return 0;
}

void print_ip(void)
{
	char buf_dataview_win[BUFSIZ];
	strcpy(buf_dataview_win, "Radar IP : ");
	strcat(buf_dataview_win, cur_ip);
	strcat(buf_dataview_win, "\n");						
	strcat(buf_dataview_win, "Change Radar IP : ");
	strcat(buf_dataview_win, change_ip);
	strcat(buf_dataview_win, "\n");			
	strcat(buf_dataview_win, "Change Radar GW : ");
	strcat(buf_dataview_win, change_gw);
	strcat(buf_dataview_win, "\n");		
	print_dataview(buf_dataview_win);
	touchwin(stdscr);
	refresh(); 
}

int get_yn_menu(const char* msg)
{
    WINDOW *check;
    int key;

	mFile_debug << "get_yn_menu " << endl; 
    check = newwin(3, 30, 4, 10);
    wmove(check, 1, 4);
    wprintw(check, msg); 
	wborder(check, '|', '|', '-', '-', '|', '|', '|', '|');
    refresh();
    key = wgetch(check);
    delwin(check);
    if (key == 'y')
    {

        return 1;
    }
    else 
        return 0;
}

void update_status(const char* str_status)
{
	update_status(str_status, "");
}

void update_status(const char* str_status, const char* str_value)
{
	char _Log_Buf [BUFSIZ];
	strcpy(_Log_Buf, str_status);
	strcat(_Log_Buf, str_value);
	refresh_status(_Log_Buf);
	touchwin(stdscr);
	refresh(); 
}
 
void print_dataview(const char* data)
{
	wclear(dataview_win);
	wprintw(dataview_win, data); 
}


//
//	First argument is always the program name
//	Second argument should be IP:  "192.168.7.2"
//	Third argument shuold be run type, 1 = Running, 2 = Running and caputure Bin, 3 = running and capture json
//  	Forth argument is a binary flag indicating which data to collect, 1=Track, 2=Camera, 3=Both
//
int ProcessCommandLineParams(int argc, char **argv)
{
	int nRet = 0;
	char szDataType;

	if (mDebugFlag)
		mFile_debug << "ProcessCommandLineParams" << endl;

	mCurrentIP = "192.168.7.2";    //"192.168.10.150";  <-  radar default

	// IP then runningstate_t

	if (argc > 1)
	{
		mCurrentIP = argv[1];
		mRunningState =  (runningstate_t)atoi(argv[2]);
		mCaptureFlag = atoi(argv[3]);
		mExecuteMode = EXECUTE_COMMANDLINE;
	}

	if (mDebugFlag)
		mFile_debug << "CurrentIP:  " << mCurrentIP << "  Running State: " << mRunningState << " Capture Flag: " << mCaptureFlag << endl; 

	return nRet;
}


int main(int argc, char **argv)
{
    int key;
    int sel;
    int sitem = 0; 
    menustate_t menu_state = MENUSTATE_MAIN;

    int exit_flag = 0;

    pthread_t hnd_quick_thread;

	pthread_create(&hnd_quick_thread, NULL, &quick_thread, (void*)&en_start);
 
    Setsigfatal ();
    init_screen();

	mRunningState = NOT_RUNNING;
	mCaptureFlag = CAPTURE_ALL;	
	mExecuteMode = EXECUTE_MENU;
	mExitNow = false;
	mDebugFlag = true;

	//if (mDebugFlag)
		mFile_debug.open("radar_debug.log");

	ProcessCommandLineParams(argc, argv);
	
	if (mRunningState > NOT_RUNNING)
		menu_state = MENUSTATE_OPENRADAR;

    statusbar_win = subwin(stdscr, 1, 100, 12, 1);
	dataview_win = subwin(stdscr, 20, 80, 13, 1);
	
	update_status("Select Menu");
    refresh();

    while(exit_flag == 0 && mExitNow == false)
    {
//	if (mDebugFlag)
//		mFile_debug << "main .1] inside while mExecuteMode= " << mExecuteMode << " mExitNow= " << mExitNow << endl;
		if (en_start == 0 && mExecuteMode != EXECUTE_STOPPING)
		{
			if (mDebugFlag)
				mFile_debug << "main .2] inside first if" << endl;
			if (mRunningState > NOT_RUNNING && mExecuteMode == EXECUTE_COMMANDLINE)
			{
				if (mDebugFlag)
					mFile_debug << "main 1]CurrentIP:  " << mCurrentIP << "  Running State: " << mRunningState << endl; 
				start_radar(en_start);
			}
			if (mDebugFlag)
				mFile_debug << "main 2] menu_state= " << menu_state << endl;
			   switch (menu_state)
			   {
				case MENUSTATE_MAIN:
					update_status("Select Menu");
					menu_list(menu_items, main_itemset, 5, 8);
					sitem = scroll_menu(menu_items, 5, 5, 0); 
					if (sitem == 1) // Open Radar
					{
						menu_state = MENUSTATE_OPENRADAR;
						print_ip();
					}
					else if (sitem == 2) // Get IP
					{
						menu_state = MENUSTATE_GETIP;
					}
					else if (sitem == 3) // Set IP
					{
						menu_state = MENUSTATE_SETIP;
						print_ip();
					}
					else if (sitem == 4) // Exit
					{
						sel = get_yn_menu(" Exit program (y/n) ? ");
						if (sel == 1)
							exit_flag = 1;
						
					}
					break;
				case MENUSTATE_OPENRADAR:
					menu_oepnradar(menu_state, en_start);
					break;

				case MENUSTATE_GETIP:
					menu_getip(menu_state);
					break;

				case MENUSTATE_SETIP:
					menu_setip(menu_state);
					break;
				default:
					break;
			   }// end switch
			
			if (mDebugFlag)
				mFile_debug << "main 3]" << endl;
			halfdelay(100);   
	
			touchwin(stdscr);
			refresh(); 
			if (mDebugFlag)
				mFile_debug << "main 4]" << endl;
		}
		else
		{
			if (mDebugFlag)
//				mFile_debug << "main 5] else" << endl;
			usleep(1000);
		}
    } // end while not told to exit
	update_status("Exiting");
	if (mDebugFlag)
		mFile_debug << "main 6] Exiting!" << endl;
	if (mFile_debug.is_open())
		mFile_debug.close(); 
    end_screen();
    return 0;
}


void* quick_thread(void* arg)
{
	int* en_start = (int*)arg;
	int key;

    while(1)
    {
		if (en_start[0] == 1)
		{
			key = getch();
			if (key == 'q' && mExecuteMode == EXECUTE_MENU)
			{
				en_start[0] = 2;
				if (get_yn_menu(" Stop Radar (y/n) ? "))
				{
					en_start[0] = 0;
					stop_radar();
				}					
				else
				{
					en_start[0] = 1;
				}
			}
			else if (key == 'q' && mExecuteMode == EXECUTE_COMMANDLINE)
			{
				if (mDebugFlag)
					mFile_debug << "quick_thread 3] q pressed" << endl;
				mExecuteMode = EXECUTE_STOPPING;
				en_start[0] = 0;
				stop_radar();
				mExitNow = true;
				if (mDebugFlag)
					mFile_debug << "quick_thread 4] after ExitNow set" << endl;
			}
		}
        usleep(1000);
    }
}
