#ifndef __FTP_H__
#define __FTP_H__

#include <sys/stat.h>
#include "network.h"
#include "bitmap.h"
#include "system.h"
#include "post.h"

#define MAXNAMESIZE 1024
#define MAXCLIENTS 50
extern bool FTPClientAction(int servAddr, char readwrite, char *fileName);
extern void startFTPserver();

// type used to determine the nature of a FTP message
typedef enum {
    READFILE,
    WRITEFILE,
    CONNECT,
    DISCONNECT,
    OK,
    ERROR,
    FILEDATA
} FTPType;

// file modes (read only or read/write)
typedef enum {
    READ,
    READWRITE
} FileMode;

// FTP header, added on top of the data (under PacketHeader and MailHeader)
class FTPHeader {
    public:
        FTPType type;
        ssize_t fileSize;
};

// utility class to wrap file-related system calls and handle errors
class FileHandler {
    public:
        static bool FileExists(const char *fileName);
        static int CreateFile(const char *fileName);
        static int FileSize(const char *fileName);
        static int OpenFile(const char *fileName);
        static int ReadFile(int fd, char *buffer, int fileSize);
        static int WriteFile(int fd, const char *buffer, int fileSize);
        static int CloseFile(int fd);
        static void RemoveFile(const char *fileName);
};

// FTP server class
class Server {
    private:
        NetworkAddress serverAddr;
        BitMap *clientMap; // keeps track of connected clients
        Connection *clients[MAXCLIENTS]; // stores client connections (not used for now)
        unsigned int nbClients; // number of connected clients
        Thread *clientsThreads[MAXCLIENTS]; // stores threads of currently connected clients (not used for now)
    public:
        Server();
        // Initialize structures to acknowledge the connection of a new client
        void ClientConnect(Connection *c);
        // Reset structures associated to a client that disconnected
        void ClientDisconnect(Connection *c);
        // Main server routine (never stops unless Ctrl-C)
        void ServerRoutine();
        // Send a notification to the current client
        void NotifyClient(Connection *c, FTPType type, ssize_t fileSize);
        // Wait for a confirmation from the current client
        bool ClientConfirmation(Connection *c);
        // Method executed by other threads created in ServerRoutine() to handle client requests
        void HandleClient(Connection *c);
        // Send a file to the current client
        bool SendFile(Connection *c, char *name);
        // Receive a file from the current client
        bool ReceiveFile(Connection *c, char *name, ssize_t fileSize);
};

// FTP client class
class Client {
    private:
        NetworkAddress serverAddr;  // address of the current server
        Connection *conn;   // current connection object
    public:
        Client();
        // Connect the client to the current server
        void Connect(NetworkAddress serverAddress);
        // Disconnect the client from the current server
        void Disconnect();
        // Send a notification to the current server
        void NotifyServer(FTPType type);
        // Wait from a confirmation from the current server
        bool ServerConfirmation(ssize_t *fileSize);
        // Send a disconnection request to the current server
        void SendDisconnectRequest();
        // Send a file transfer (read or write) request to the current server
        bool SendFileRequest(FTPType requestType, const char *fileName);
        // Send a file to the current server
        bool SendFile(const char *name);
        // Receive a file from the current server
        bool ReceiveFile(const char *name, ssize_t fileSize);
};

// Structure used to fork new client handler threads and get structures back from there
// (since we can only pass a single argument)
typedef struct {
    Server *server;
    Connection *c;
} FTPEnvironment;

#endif