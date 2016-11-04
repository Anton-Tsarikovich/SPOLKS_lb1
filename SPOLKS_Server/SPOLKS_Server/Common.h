#include <iostream>
#include <fstream>
#include <ctime>
#include <string.h>
#include <string>
#include <bitset>
#pragma comment (lib, "ws2_32.lib")

using namespace std;

#include <windows.h>
#include <conio.h>
#include <io.h>

#define PORT_NUM 2222
#define BYTE_SIZE 8
#define COMMAND_CODE_SIZE 4
#define SIZE_BITSET_SIZE 35
#define FILE_NAME_SIZE 16

#define ECHO_SIZE_CLIENT 5
#define TIME_SIZE_CLIENT 1
#define CLOSE_SIZE_CLIENT 1
#define UPLOAD_SIZE_CLIENT 21
#define DOWNLOAD_SIZE_CLIENT 17
#define DOWNLOAD_PROGRESS_SIZE_CLIENT 4

#define ECHO_BITSET_SIZE_CLIENT 40
#define ECHO_BITSET_SIZE_SERVER 8
#define TIME_BITSET_SIZE_CLIENT 8
#define TIME_BITSET_SIZE_SERVER 8
#define CLOSE_BITSET_SIZE_CLIENT 8
#define CLOSE_BITSET_SIZE_SERVER 8
#define UPLOAD_BITSET_SIZE_CLIENT 40
#define UPLOAD_BITSET_SIZE_SERVER 32
#define DOWNLOAD_BITSET_SIZE_CLIENT 8
#define DOWNLOAD_BITSET_SIZE_SERVER 40
#define HELLO_MESSAGE "HELLO I'M SERVER\n"

#define ECHO_BITSET 0x3
#define TIME_BITSET 0x5
#define CLOSE_BITSET 0x7
#define UPLOAD_BITSET 0x9
#define DOWNLOAD_BITSET 0xB

#define ECHO_CODE 1
#define TIME_CODE 2
#define CLOSE_CODE 3
#define UPLOAD_CODE 4
#define DOWNLOAD_CODE 5
#define DATA_CODE 0

#define MAX_BLOCK_SIZE 65534
#define MAX_DATA_BLOCK_SIZE 65529
#define QUEUE_LENGTH 1

#define SOCKET_READ_TIMEOUT_SEC 120
#define CONNECTION_TIMEOUT_SEC 5
#define PACKAGE_SIZE_SIZE 2
#define PACKAGE_NUM_SIZE_CLIENT 24
#define PACKAGE_NUM_SIZE_SERVER 3

#define MIN_PACKAGE_SIZE 3
SOCKET serverSocket;
SOCKET newSocket;
int clientAmount = 0;
sockaddr_in clientAddress;
int clientAddressSize;
bool processing;

//UPLOAD
u_long currentPackageNum;
unsigned long long currentSize;
string uploadFileName;
ofstream uploadFile;

//DOWNLOAD
string downloadFileName = "";
long downloadFileSize;
ifstream downloadFile;
long downloadBlockCount;
