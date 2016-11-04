#include "Common.h"

int init();
int processConnections();
DWORD WINAPI workWithClient(LPVOID clientSocket);
void disconnectClient(SOCKET socket);
u_long getRequestCommandCode(char commandPart);
int getRequestCommandPartLength(u_long commandCode);
int handleRequest(string request, u_long commandCode);
string getCurrentTime();
int handleClose();
int handleTime();
int handleEcho(string request);
int handleUpload(string request);
int handleDownload(string request);
string getStringFromBitset(bitset<40> stringBitset);
long getFileSize(string fileName);
int getEcho(unsigned long long size);
int sendEcho(unsigned long long size);
int getFile(unsigned long long size, string fileName);
int sendFile(string request);
int requestPackage(int currentPackageNum);
int getPackageSizeFromString(string packageSizeString);
string getStringFromPackageNum(u_long packageNum);
u_long getPackageNumFromString(string packageNumString);
string getStringFromBitset24(bitset<24> stringBitset);
bitset<24> getBitset24FromString(string string);
void writePackageToFile(string package, int packageSize);
int checkPackageNum(string package, u_long currentPackageNum);
u_long getDownloadPackageNum(string request);
long getDataBlockSize(char* fileBlock);
string getStringFromBitset16(bitset<16> stringBitset);
string getStringFromPackageSize(size_t packageSize);

int main() {
	init();
	return 0;
}

int init() {
	char buf[MAX_BLOCK_SIZE];

	if (WSAStartup(0x0202, (WSADATA *)&buf[0])) {
		printf("wsa startup error %d\n", WSAGetLastError());
		return -1;
	}
	
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket init error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	sockaddr_in localAddress;
	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(PORT_NUM);
	localAddress.sin_addr.S_un.S_addr = 0;

	if (bind(serverSocket, (sockaddr *)&localAddress, sizeof(localAddress))) {
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
	processConnections();

}
int processConnections() {
	SOCKET clientSocket;
	clientAddress;
	clientAddressSize = sizeof(clientAddress);
	while (clientSocket = accept(serverSocket, (sockaddr *)&clientAddress, &clientAddressSize)) {
		if (clientAmount == 1) {
			continue;
		}

		clientAmount++;

		HOSTENT* host;
		host = gethostbyaddr((char *)&clientAddress.sin_addr.S_un.S_addr, 4, AF_INET);
		printf("+%s [%s] new connect\n", (host) ? host->h_name : "", inet_ntoa(clientAddress.sin_addr));

		workWithClient(&clientSocket);
	}
	return 0;
}

DWORD WINAPI workWithClient(LPVOID clientSocket) {
	newSocket = ((SOCKET *)clientSocket)[0];
	send(newSocket, HELLO_MESSAGE, sizeof(HELLO_MESSAGE), 0);
	int receivedBytes;
	string request = "";
	char buffer[MAX_BLOCK_SIZE] = "";
	bool isCommandCodeProcessed = false;
	int commandPartLength = 1;
	int commandCode;


	/*fd_set set;
	struct timeval timeout;
	timeout.tv_sec = CONNECTION_TIMEOUT_SEC;
	timeout.tv_usec = 0;
	FD_ZERO(&set);
	FD_SET(newSocket, &set);*/

	while (true) {
		/*int rv = select(0, &set, NULL, NULL, &timeout);

		if (rv < 0) {
			cout << WSAGetLastError() << endl;
			break;
		}*/

		receivedBytes = recv(newSocket, buffer, MAX_BLOCK_SIZE, 0);

		for (int i = 0; i < receivedBytes; i++) {
			request.push_back(buffer[i]);
			if (!isCommandCodeProcessed) {
				if ((commandCode = getRequestCommandCode(request.at(0))) == -1)
					return -1;

				commandPartLength = getRequestCommandPartLength(commandCode);
				isCommandCodeProcessed = true;
			}
			if (request.length() == commandPartLength) {
				if (handleRequest(request, commandCode) == -1)
					return -1;
				isCommandCodeProcessed = false;
				request.clear();
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

u_long getRequestCommandCode(char commandPart) {
	bitset<BYTE_SIZE> command(commandPart);
	if (command[0] == 0)
		return -1;
	bitset<COMMAND_CODE_SIZE> commandCodeBitset;
	for (int i = 0; i < 4; i++)
		commandCodeBitset[i] = command[i + 1];

	return commandCodeBitset.to_ulong();
}

int getRequestCommandPartLength(u_long commandCode) {
	switch (commandCode) {
	case ECHO_CODE:
		return ECHO_SIZE_CLIENT;
	case TIME_CODE:
		return TIME_SIZE_CLIENT;
	case CLOSE_CODE: 
		return CLOSE_SIZE_CLIENT;
	case UPLOAD_CODE:
		return UPLOAD_SIZE_CLIENT;
	case DOWNLOAD_CODE:
		if (downloadFileName.length())
			return DOWNLOAD_PROGRESS_SIZE_CLIENT;
		return DOWNLOAD_SIZE_CLIENT;
	default:
		return -1;
	}
}

int handleRequest(string request, u_long commandCode) {
	switch (commandCode) {
	case ECHO_CODE:
		cout << "ECHO" << endl;
		if (handleEcho(request) == -1)
			return -1;
		break;
	case TIME_CODE:
		cout << "TIME" << endl;
		if (handleTime() == -1)
			return -1;
		break;
	case CLOSE_CODE:
		cout << "CLOSE" << endl;
		if (handleClose() == -1)
			return -1;
		break;
	case UPLOAD_CODE:
		cout << "UPLOAD" << endl;
		if (handleUpload(request) == -1)
			return -1;
		break;
	case DOWNLOAD_CODE:
		cout << "DOWNLOAD" << endl;
		if (handleDownload(request) == -1)
			return -1;
		break;
	default:
		return -1;
	}
}

string getCurrentTime() {
	time_t seconds = time(NULL);
	tm* timeinfo = localtime(&seconds);
	return string(asctime(timeinfo));
}

int handleClose() {
	bitset<CLOSE_BITSET_SIZE_SERVER> commandBitset(CLOSE_BITSET);
	string response(1, static_cast<unsigned char> (commandBitset.to_ulong()));
	int res = send(newSocket, response.c_str(), response.length(), 0);
	if (res != -1) {
		disconnectClient(newSocket);
	}
	return -1;
}

int handleTime() {
	bitset<TIME_BITSET_SIZE_SERVER> commandBitset(TIME_BITSET);
	string response(1, static_cast<unsigned char> (commandBitset.to_ulong()));
	response += getCurrentTime();
	return send(newSocket, response.c_str(), response.length() - 1, 0);
}

bitset<40> getBitset40FromString(string request) {
	bitset<40> result;
	const char* requestString = request.c_str();
	for (int i = 0; i < 5; i++) {
		bitset<8> charBitset(requestString[i]);
		for (int j = 0; j < 8; j++)
			result[i * 8 + j] = charBitset[j];
	}
	return result;
}

int handleEcho(string request) {
	bitset<ECHO_BITSET_SIZE_SERVER> commandBitset(ECHO_BITSET);
	string response(1, static_cast<unsigned char> (commandBitset.to_ulong()));
	bitset<SIZE_BITSET_SIZE> echoSizeBitset;

	bitset<ECHO_BITSET_SIZE_CLIENT> requestBitset = getBitset40FromString(request);

	for (int i = 0; i < SIZE_BITSET_SIZE; i++) {
		echoSizeBitset[i] = requestBitset[i + 5];
	}

	unsigned long long echoSize = echoSizeBitset.to_ullong();

	if (send(newSocket, response.c_str(), response.length(), 0) == -1)
		return -1;
	if (getEcho(echoSize) == -1)
		return -1;
	return sendEcho(echoSize);
}

int handleUpload(string request) {
	bitset<BYTE_SIZE> commandBitset(UPLOAD_BITSET);
	string response(1, static_cast<unsigned char> (commandBitset.to_ulong()));
	bitset<SIZE_BITSET_SIZE> uploadSizeBitset;
	bitset<UPLOAD_BITSET_SIZE_CLIENT> requestBitset = getBitset40FromString(request.substr(0, 5));

	for (int i = 0; i < SIZE_BITSET_SIZE; i++) {
		uploadSizeBitset[i] = requestBitset[i + 5];
	}

	unsigned long long uploadSize = uploadSizeBitset.to_ullong();
	string currentUploadFileName = request.substr(5, FILE_NAME_SIZE);
	

	if (currentUploadFileName.compare(uploadFileName)) {
		remove(uploadFileName.c_str());
		uploadFileName = currentUploadFileName;
		currentPackageNum = 0;
		currentSize = 0;
	}

	requestPackage(currentPackageNum);

	return getFile(uploadSize, uploadFileName);
}

int handleDownload(string request) {
	if (!downloadFileName.length()) {
		bitset<DOWNLOAD_BITSET_SIZE_SERVER> commandBitset(DOWNLOAD_BITSET);
		downloadFileName = request.substr(1, FILE_NAME_SIZE);
		downloadFileSize = getFileSize(downloadFileName);
		bitset<SIZE_BITSET_SIZE> fileSizeBitset(downloadFileSize);
		for (int i = 0; i < SIZE_BITSET_SIZE; i++)
			commandBitset[i + 5] = fileSizeBitset[i];
		string commandString = getStringFromBitset(commandBitset);
		if (send(newSocket, commandString.c_str(), commandString.length(), 0) == -1)
			return -1;
	}
	else {
		return sendFile(request);
	}
}

string getStringFromBitset(bitset<40> stringBitset) {
	/*string result = "";
	bitset<BYTE_SIZE> charBitset;
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < BYTE_SIZE; j++) {
			charBitset[j] = stringBitset[i * BYTE_SIZE + j];
		}
		result += static_cast<unsigned char> (charBitset.to_ulong());
		charBitset.reset();
	}
	return result;*/
	char* result = new char[5];
	memcpy(&result[0], &stringBitset, 5);
	return string(result, 5);
}

long getFileSize(string fileName) {
	struct stat stat_buf;
	int rc = stat(fileName.c_str(), &stat_buf);
	return rc == 0 ? stat_buf.st_size : -1;
}

int getEcho(unsigned long long size) {
	return 0;
}

int sendEcho(unsigned long long size) {
	return 0;
}

int getFile(unsigned long long size, string fileName) {
	fd_set set;
	struct timeval timeout;
	timeout.tv_sec = SOCKET_READ_TIMEOUT_SEC;
	timeout.tv_usec = 0;
	bool errorFlag = false;
	string package = "";
	int packageSize = MIN_PACKAGE_SIZE;
	char buffer[MAX_BLOCK_SIZE];
	unsigned long long retryCounter = 0;
	bool blabla = false;

	if (!uploadFile.is_open()) {
		if (currentPackageNum == 0)
			uploadFile.open(fileName, ios::out | ios::binary);
		else
			uploadFile.open(fileName, ios::out | ios::binary | ios::app);
	}

	while (true) {
		int receivedBytes = recv(newSocket, buffer, MAX_BLOCK_SIZE, 0);
		if (errorFlag) {
			continue;
		}

		if (receivedBytes == SOCKET_ERROR) {
			cout << WSAGetLastError() << endl;
			disconnectClient(newSocket);
			closesocket(newSocket);
			uploadFile.close();
			
				FD_ZERO(&set);
				FD_SET(serverSocket, &set);
				int rv = select(0, &set, NULL, NULL, &timeout);

				if (rv == 0) {
					return 0; 
				}
				if (rv > 0) {
					newSocket = accept(serverSocket, (sockaddr *)&clientAddress, &clientAddressSize);
					memset(buffer, 0, MAX_BLOCK_SIZE);
					package.clear();
					send(newSocket, HELLO_MESSAGE, sizeof(HELLO_MESSAGE), 0);
					return 0;
				}
		} else {
			package.append(&buffer[0], receivedBytes);
		}

		if (package.length() >= PACKAGE_SIZE_SIZE) {
			packageSize = getPackageSizeFromString(package.substr(0, PACKAGE_SIZE_SIZE));
			if (package.length() >= packageSize) {
				switch (checkPackageNum(package, currentPackageNum)) {
				case -1:
					package.erase(0, packageSize);
					continue;
				case 1:
					errorFlag = true;
					continue;
				}
				writePackageToFile(package, packageSize);
				currentPackageNum++;
				currentSize += packageSize - 5;
				package.erase(0, packageSize);
				if (currentSize >= size - 1) {
					uploadFile.close();
					if (requestPackage(currentPackageNum) == -1) {
						return -1;
					}
					currentPackageNum = 0;
					uploadFileName.clear();
					return 0;
				}
			}
		}
	}
	return -1;
}

int requestPackage(int currentPackageNum) {
	bitset<BYTE_SIZE> commandBitset(UPLOAD_BITSET);
	string response(1, static_cast<unsigned char> (commandBitset.to_ulong()));
	response += getStringFromPackageNum(currentPackageNum);
	return send(newSocket, response.c_str(), response.length(), 0);
}

void writePackageToFile(string package, int packageSize) {
	int dataBlockSize = packageSize - 5;
	uploadFile.write(package.substr(5, dataBlockSize).c_str(), dataBlockSize);
}

int checkPackageNum(string package, u_long currentPackageNum) {
	u_long packageNum = getPackageNumFromString(package.substr(2, 3));
	if (packageNum < currentPackageNum) {
		return -1;
	}
	if (packageNum > currentPackageNum)
		return 1;
	return 0;
}

u_long getPackageNumFromString(string packageNumString) {
	bitset<BYTE_SIZE * 3> packageNumBitset = getBitset24FromString(packageNumString);
	return packageNumBitset.to_ulong();
}

bitset<24> getBitset24FromString(string string) {
	bitset<24> result;
	for (int i = 0; i < 3; i++) {
		bitset<8> byteBitset(string.at(i));
		for (int j = 0; j < 8; j++) {
			result[i * 8 + j] = byteBitset[j];
		}
	}
	return result;
	/*bitset<24> result;
	memcpy(&result, &string[0], 3);
	return result;*/
}

bitset<16> getBitset16FromString(string string) {
	bitset<16> result;
	for (int i = 0; i < 2; i++) {
		bitset<8> byteBitset(string.at(i));
		for (int j = 0; j < 8; j++) {
			result[i * 8 + j] = byteBitset[j];
		}
	}
	return result;

	/*bitset<16> result;
	memcpy(&result, &string[0], 2);
	return result;*/
}

int getPackageSizeFromString(string packageSizeString) {
	bitset<BYTE_SIZE * PACKAGE_SIZE_SIZE> packageSizeBitset = getBitset16FromString(packageSizeString);

	int packageSize = 0;
	int temp = 1;
	for (int i = 15; i >= 0; i--) {
		packageSize += packageSizeBitset[i] * temp;
		temp *= 2;
	}

	return packageSize;
}

string getStringFromPackageNum(u_long packageNum) {
	bitset<BYTE_SIZE * 3> packageNumBitset(packageNum);
	return getStringFromBitset24(packageNumBitset);
}

string getStringFromBitset24(bitset<24> stringBitset) {
	/*
	string result = "";
	bitset<BYTE_SIZE> charBitset;
	for (int i = 0; i < 3; i++) {
 		for (int j = 0; j < BYTE_SIZE; j++) {
			charBitset[j] = stringBitset[i * BYTE_SIZE + j];
		}
		result += static_cast<unsigned char> (charBitset.to_ulong());
		charBitset.reset();
	}
	return result;*/

	char* result = new char[3];
	memcpy(&result[0], &stringBitset, 3);
	return string(result, 3);
}

int sendFile(string request) {
	if (!downloadFile.is_open()) {
		processing = true;
		downloadFile.open(downloadFileName, ios::binary | ios::in);
		downloadBlockCount = downloadFileSize / MAX_DATA_BLOCK_SIZE + 1;
	}

	u_long packageNum = getDownloadPackageNum(request);
	if (packageNum >= downloadBlockCount) {
		downloadFile.close();
		downloadFileName.clear();
		downloadFileSize = 0;
		processing = false;
		return 0;
	}

	downloadFile.seekg(packageNum * MAX_DATA_BLOCK_SIZE, ios::beg);

	char fileBlock[MAX_DATA_BLOCK_SIZE + 1];

	for (u_long currentPackageNum = packageNum; currentPackageNum < downloadBlockCount; currentPackageNum++) {
		cout << currentPackageNum << '/' << downloadBlockCount << endl;

		if (currentPackageNum == downloadBlockCount - 1)
			memset(fileBlock, 0, MAX_DATA_BLOCK_SIZE + 1);

		downloadFile.read(&fileBlock[0], MAX_DATA_BLOCK_SIZE);

		fileBlock[MAX_DATA_BLOCK_SIZE] = '\0';

		long dataBlockSize = currentPackageNum == downloadBlockCount - 1 ? getDataBlockSize(fileBlock)
			: MAX_DATA_BLOCK_SIZE;

		long packageLength = dataBlockSize + PACKAGE_NUM_SIZE_SERVER + PACKAGE_SIZE_SIZE;

		char* pack = new char[packageLength];

		string packageLengthString = getStringFromPackageSize(packageLength);
		string packageNumString = getStringFromPackageNum(currentPackageNum);

		memcpy(&pack[0], packageLengthString.c_str(), PACKAGE_SIZE_SIZE);
		memcpy(&pack[2], packageNumString.c_str(), PACKAGE_NUM_SIZE_SERVER);
		memcpy(&pack[5], &fileBlock[0], dataBlockSize);

		send(newSocket, &pack[0], packageLength, 0);

		delete[] pack;
	}
	processing = true;
	return 0;
}

string getStringFromPackageSize(size_t packageSize) {
	bitset<BYTE_SIZE * 2> packageSizeBitset;

	int i = 15;
	while (packageSize != 0) {
		packageSizeBitset[i--] = packageSize % 2;
		packageSize /= 2;
	}

	return getStringFromBitset16(packageSizeBitset);
}

string getStringFromBitset16(bitset<16> stringBitset) {
	char* result = new char[2];
	memcpy(&result[0], &stringBitset, 2);
	return string(result, 2);
}

long getDataBlockSize(char* fileBlock) {
	long i = MAX_DATA_BLOCK_SIZE;
	while (fileBlock[i--] == '\0');

	if (i > MAX_DATA_BLOCK_SIZE)
		i = MAX_DATA_BLOCK_SIZE;

	return i + 2;
}

u_long getDownloadPackageNum(string request) {
	string packageNumString = request.substr(1, 3);
	bitset<PACKAGE_NUM_SIZE_CLIENT> packageNumBitset = getBitset24FromString(packageNumString);
	u_long packageNum = packageNumBitset.to_ulong();
	return packageNum;
}
