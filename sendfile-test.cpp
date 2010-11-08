#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/sendfile.h>

#include <pthread.h>
#include <fcntl.h>

class SendFileTest
{
public:
	SendFileTest(std::string const & serverSocketPath, std::string const & testFilePath);
	bool runTest();
	
private:
	std::string
		serverSocketPath,
		testFilePath;
		
	sockaddr_un serverAddress;
	
	size_t fileSize;
		
	static void * clientThreadHandler(void * argument);
	bool runClient();
};

SendFileTest::SendFileTest(std::string const & serverSocketPath, std::string const & testFilePath):
	serverSocketPath(serverSocketPath),
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
	unlink(serverSocketPath.c_str());
	int serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(serverSocket == -1)
	{
		std::cout << "Failed to create the UNIX server socket" << std::endl;
		return false;
	}
	serverAddress.sun_family = PF_UNIX;
	strcpy(serverAddress.sun_path, serverSocketPath.c_str());
	int result = bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress));
	if(result == -1)
	{
		std::cout << "Failed to bind the UNIX server socket" << std::endl;
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
	
	sockaddr_un clientAddress;
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
	int clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(clientSocket == -1)
	{
		std::cout << "Failed to create the UNIX client socket" << std::endl;
		return false;
	}
	
	int result = connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress));
	if(result == -1)
	{
		std::cout << "Failed to connect to the UNIX server socket" << std::endl;
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
		std::cout << argv[0] << " <name of the server socket to be used> <path to file to run the test on>" << std::endl;
		return 1;
	}
	
	std::string
		serverSocketPath = argv[1],
		testFilePath = argv[2];
		
	SendFileTest test(serverSocketPath, testFilePath);
	bool success = test.runTest();
	return success ? 0 : 1;
}
