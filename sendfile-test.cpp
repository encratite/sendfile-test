#include <iostream>
#include <string>

#include <stdlib.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>

class SendFileTest
{
public:
	SendFileTest(short port, std::string const & testFilePath);
	bool runTest();
	
private:
	short port;
	std::string testFilePath;
		
	sockaddr_in serverAddress;
	
	size_t fileSize;
		
	static void * clientThreadHandler(void * argument);
	bool runClient();
};

SendFileTest::SendFileTest(short port, std::string const & testFilePath):
	port(port),
	testFilePath(testFilePath)
{
}

void * SendFileTest::clientThreadHandler(void * argument)
{
	SendFileTest & test = *static_cast<SendFileTest *>(argument);
	test.runClient();
	pthread_exit(0);
	return NULL;
}

bool SendFileTest::runTest()
{
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket == -1)
	{
		std::cout << "Failed to create the TCP IPv4 server socket" << std::endl;
		return false;
	}
	serverAddress.sin_family = PF_INET;
	serverAddress.sin_port = htons(port);
	serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

	int result = bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress));
	if(result == -1)
	{
		std::cout << "Failed to bind the TCP IPv4 server socket to port " << port << std::endl;
		return false;
	}
	
	int fileHandle = open(testFilePath.c_str(), O_RDONLY | O_NONBLOCK);
	if(fileHandle == -1)
	{
		std::cout << "Failed to open " << testFilePath << " to fstat it" << std::endl;
		return false;
	}
	
	struct stat fileStatistics;
	result = fstat(fileHandle, &fileStatistics);
	if(result == -1)
	{
		std::cout << "Failed to run fstat on " << testFilePath << std::endl;
		return false;
	}
	
	fileSize = static_cast<size_t>(fileStatistics.st_size);
	std::cout << testFilePath << " has a size of " << fileSize << " byte(s)" << std::endl;
	
	pthread_t clientThread;
	
	result = pthread_create(&clientThread, NULL, &SendFileTest::clientThreadHandler, this);
	if(result != 0)
	{
		std::cout << "Failed to create the client thread" << std::endl;
		return false;
	}
	
	result = listen(serverSocket, 1);
	if(result == -1)
	{
		std::cout << "Failed to listen on the server socket" << std::endl;
		return false;
	}
	
	sockaddr_in clientAddress;
	socklen_t clientSize;
	int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientSize);
	if(clientSocket == -1)
	{
		std::cout << "Failed to accept the client" << std::endl;
		return false;
	}
	
	char * buffer = new char[fileSize];
	size_t bytesReceived = recv(clientSocket, buffer, fileSize, 0);
	std::cout << "Received " << bytesReceived << " from the client:" << std::endl;
	std::string content(buffer, bytesReceived);
	delete[] buffer;
	std::cout << "\"" << content << "\"" << std::endl;
	
	close(clientSocket);
	
	close(serverSocket);
	
	return true;
}

bool SendFileTest::runClient()
{
	int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(clientSocket == -1)
	{
		std::cout << "Failed to create the TCP IPv4 client socket" << std::endl;
		return false;
	}
	
	int result = connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress));
	if(result == -1)
	{
		std::cout << "Failed to connect to the TCP IPv4 server on port " << port << std::endl;
		return false;
	}
	
	int fileHandle = open(testFilePath.c_str(), O_RDONLY | O_NONBLOCK);
	if(fileHandle == -1)
	{
		std::cout << "Failed to open " << testFilePath << " to sendfile it" << std::endl;
		return false;
	}
	
	size_t bytesSent = sendfile(clientSocket, fileHandle, 0, fileSize);
	std::cout << "Sent " << fileSize << " byte(s) to the server" << std::endl;
	
	close(fileHandle);
	
	close(clientSocket);
	
	return true;
}

int main(int argc, char ** argv)
{
	if(argc != 3)
	{
		std::cout << "Usage:" << std::endl;
		std::cout << argv[0] << " <TCP port of the local server> <path to file to run the test on>" << std::endl;
		return 1;
	}
	
	int port = atoi(argv[1]);
	std::string testFilePath = argv[2];
		
	SendFileTest test(port, testFilePath);
	bool success = test.runTest();
	return success ? 0 : 1;
}
