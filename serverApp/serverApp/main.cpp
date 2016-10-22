#include <iostream>
#include <fstream>
#include <ctime>
#include <string.h>
#include <string>
#pragma comment (lib, "ws2_32.lib")

#include <windows.h>
#include <conio.h>
#include <io.h>

using namespace std;

#define PORT_NUM 2222
#define QUEUE_LENGTH 20
#define HELLO_STRING "\1Hi, this is server!\n"
#define ECHO_CODE 1
#define TIME_CODE 2
#define CLOSE_CODE 3
#define UPLOAD_CODE 4
#define DOWNLOAD_CODE 5

#define SOCKET_READ_TIMEOUT_SEC 1

#define BEGIN_PACKAGE_CODE 17
#define BEGIN_DATA_CODE 18
#define END_PACKAGE_CODE 19
#define ACK_CODE 6
#define END_OF_FILE_CODE 21

#define BLOCK_SIZE 1024

char* cbuf = "";
DWORD WINAPI workWithClient(LPVOID clientSocket);
DWORD WINAPI checkCommandBuffer(LPVOID clientSocket);
int sendMessage(char commandCode, string message);
string getCurrentTime();
void disconnectClient(SOCKET socket);
int getFile(string parameters);
long getFileSize(string fileName);

int clientAmount = 0;
string clientName = "";
SOCKET newSocket;

int proccessCommand(string command);
int sendHeading(char commandCode, string message);

int main() {
	char buf[BLOCK_SIZE];

	if (WSAStartup(0x0202, (WSADATA *) &buf[0])) {
		printf("wsa startup error %d\n", WSAGetLastError());
		return -1;
	}

	SOCKET serverSocket;
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket init error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	sockaddr_in localAddress;
	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(PORT_NUM);
	localAddress.sin_addr.S_un.S_addr = 0;

	if (bind(serverSocket, (sockaddr *) &localAddress, sizeof(localAddress))) {
		printf("bind socket to localAddress error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	if (listen(serverSocket, QUEUE_LENGTH)) {
		printf("listen error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	printf("waiting for connection...\n");

	SOCKET clientSocket;
	sockaddr_in clientAddress;
	int clientAddressSize = sizeof(clientAddress);

	while (clientSocket = accept(serverSocket, 
								(sockaddr *) &clientAddress, 
								&clientAddressSize)) {
		if (clientAmount == 1) {
			continue;
		}

		clientAmount++;

		HOSTENT* host;
		host = gethostbyaddr((char *) &clientAddress.sin_addr.S_un.S_addr, 4, AF_INET);
		printf("+%s [%s] new connect\n", (host) ? host->h_name : "", inet_ntoa(clientAddress.sin_addr));

		workWithClient(&clientSocket);
	}

	return 0;
}

DWORD WINAPI workWithClient(LPVOID clientSocket) {
	newSocket = ((SOCKET *)clientSocket)[0];
	send(newSocket, HELLO_STRING, sizeof(HELLO_STRING), 0);

	int receivedBytes;
	string command = "";

	while (true) {
		char buffer[BLOCK_SIZE] = "";
		receivedBytes = recv(newSocket, buffer, sizeof(buffer), 0);

		for (int i = 0; i < sizeof(buffer); i++) {
			if (buffer[i] == '\0') {
				break;
			}

			command += buffer[i];
			
			if (buffer[i] == '\n' || buffer[i] == TIME_CODE || buffer[i] == CLOSE_CODE) {
				if (proccessCommand(command)) {
					return 0;
				}
				command = "";
			}
		}
	}

	disconnectClient(newSocket);

	return 0;
}

void disconnectClient(SOCKET socket) {
	clientAmount--;
	closesocket(socket);
	printf("disconnect client...\n");
}

int proccessCommand(string command) {
	string foo = command.substr(1, command.size() - 1);

	switch (command.at(0)) {
	case ECHO_CODE:
		cout << "ECHO command with " << foo << endl;
		if (sendMessage(ECHO_CODE, foo) == SOCKET_ERROR) {
			cout << "ERROR sending echo" << endl;
		};
		return 0;
	case TIME_CODE: printf("TIME command\n");
		if (sendMessage(TIME_CODE, getCurrentTime()) == SOCKET_ERROR) {
			cout << "ERROR sending time" << endl;
		};
		return 0;
	case CLOSE_CODE: printf("CLOSE command\n");
		if (sendMessage(CLOSE_CODE, "") == SOCKET_ERROR) {
			cout << "ERROR sending close" << endl;
		};
		disconnectClient(newSocket);
		return 1;
	case UPLOAD_CODE: cout << "UPLOAD command with " << foo << endl;
		if (getFile(foo) == SOCKET_ERROR) {
			cout << "ERROR getting file" << endl;
		}
		return 0;
	case DOWNLOAD_CODE: cout << "DOWNLOAD command with " << foo << endl;
		return 0;
	}
}

long fileSize, blockNum;
string fileName;
ofstream file;

int getFile(string parameters) {
	fileSize = stol(parameters.substr(1, parameters.find_last_of(' ') - 1));
	fileName = parameters.substr(parameters.find_last_of(' ') + 1, parameters.size() - parameters.find_last_of(' ') - 2);
	//check if reconnect 
	blockNum = 0;
	fd_set set;
	struct timeval timeout;
	timeout.tv_sec = SOCKET_READ_TIMEOUT_SEC;
	timeout.tv_usec = 0;
	bool errorFlag = false;



	while (true) {
		FD_ZERO(&set);
		FD_SET(newSocket, &set);
		int rv = select(0, &set, NULL, NULL, &timeout);

		if (rv == SOCKET_ERROR) {
			int a = WSAGetLastError();
			return -1;
		}
		if (rv == 0) {
			//request new data
			string request = "";
			request += ACK_CODE;
			request += to_string(blockNum);
			request += '\n';
			send(newSocket, request.c_str(), request.size(), 0);
			errorFlag = false;
		}
		else {
			if (!file.is_open()) {
				file.open(fileName, ios::out | ios::binary);
			}
			char buffer[BLOCK_SIZE] = "";
			int receivedBytes = recv(newSocket, buffer, sizeof(buffer), 0);
			cout << receivedBytes << endl;
			if (errorFlag) {
				continue;
			}
			if (receivedBytes == SOCKET_ERROR) {
				return -1;
			}
			if (receivedBytes == 0) {
				disconnectClient(newSocket);
				return 0;
			}
			else {
				string block = "";
				for (int i = 0; i < sizeof(buffer); i++) {
					block.push_back(buffer[i]);

					if (buffer[i] == END_PACKAGE_CODE || buffer[i] == END_OF_FILE_CODE) {
						if (blockNum != stol(block.substr(1, 7)) || block.at(0) != BEGIN_PACKAGE_CODE || block.at(8) != BEGIN_DATA_CODE) {
							errorFlag = true;
							break;
						}

						string dataPart = block.substr(9, block.size() - 10);

						file.write(dataPart.c_str(), dataPart.size());

						blockNum++;

						if (buffer[i] == END_OF_FILE_CODE) {
							file.close();
							string uploadResponse = "";
							uploadResponse += UPLOAD_CODE;
							uploadResponse += '\n';
							send(newSocket, uploadResponse.c_str(), uploadResponse.size(), 0);
							/*if (getFileSize(fileName) != fileSize) {
								return -1;
							}*/
							return 0;
						}
					}

					/*if (buffer[i] == '\n' || buffer[i] == TIME_CODE || buffer[i] == CLOSE_CODE) {
						if (proccessCommand(block)) {
							return 0;
						}
						command = "";
					}*/
				}
			}
		}
	}

	return 0;
}

/*long getFileSize(ofstream file) {
	file.seekg(0, ios::end);
	int fileSize = file.tellg();
	file.close();

	return fileSize;
}*/

string getCurrentTime() {
	time_t seconds = time(NULL);
	tm* timeinfo = localtime(&seconds);
	return string(asctime(timeinfo));
}

int sendMessage(char commandCode, string message) {
	int blocksCount = ((message.size() + 1) / BLOCK_SIZE) + 1;
	int blockBegin = 0;
	string block;

	for (int i = 0; i < blocksCount - 1; i++) {
		block = message.substr(blockBegin, BLOCK_SIZE);

		if (blockBegin == 0) {
			if (sendHeading(commandCode, message.substr(blockBegin, BLOCK_SIZE - 1)) == SOCKET_ERROR) {
				return -1;
			}
			blockBegin += BLOCK_SIZE - 1;
			continue;
		}

		if (send(newSocket, block.c_str(), BLOCK_SIZE, 0) == SOCKET_ERROR) {
			return -1;
		}

		blockBegin += BLOCK_SIZE;
	}

	block = message.substr(blockBegin, message.size() - blockBegin);

	if (blockBegin == 0) {
		if (sendHeading(commandCode, message.substr(blockBegin, BLOCK_SIZE - 1)) == SOCKET_ERROR) {
			return -1;
		}
		return 0;
	}

	if (send(newSocket, block.c_str(), block.size(), 0) == SOCKET_ERROR) {
		return -1;
	}

	return 0;
}

int sendHeading(char commandCode, string message) {
	string response(1, commandCode);
	response.append(message);
	return send(newSocket, response.c_str(), response.size(), 0);
}