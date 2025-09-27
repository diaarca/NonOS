#include "ftp.h"
#include <fcntl.h>  // For open()
#include <unistd.h> // For read() and close()

// tests if a file exists and return true in that case, or false otherwise
bool FileHandler::FileExists(const char *fileName)
{   
    return (fileSystem->IsDataFile(fileName) && fileSystem->FileExists(fileName));
}

int FileHandler::CreateFile(const char *fileName) {
    if (fileSystem->FileExists(fileName)) {
        printf("ERROR : file %s already exists\n", fileName);
        return -1;
    }
    if (!fileSystem->Create(fileName, 0)) {
        printf("ERROR : failed to create file %s\n", fileName);
        return -1;
    }
    return 0;
}

// retrieve the file size in bytes and return it, or -1 if the file is invalid
int FileHandler::FileSize(const char *fileName)
{
    if (!FileHandler::FileExists(fileName)) {
        printf("ERROR : File %s doesn't exist or isn't a regular file\n", fileName);
        return -1;
    }
    return fileSystem->GetFileSize(fileName);
}

int FileHandler::OpenFile(const char *fileName)
{
    int fd;
    fd = fileSystem->OpenUser(fileName);
    if(fd < 0)
    {
        printf("ERROR : failed to open file %s\n", fileName);
    }
    return fd;
}

int FileHandler::ReadFile(int fd, char *buffer, int fileSize)
{
    int bytesRead;
    if((bytesRead = fileSystem->ReadUser(buffer, fileSize, fd)) != fileSize)
    {
        printf("ERROR : failed to read the correct amount of bytes\n");
        return -1;
    }
    if((bytesRead = fileSystem->ReadUser(buffer, fileSize, fd)) != 0)
    {
        printf("ERROR : file data remains after reading ended\n");
        return -1;
    }
    return 0;
}

int FileHandler::WriteFile(int fd, const char *buffer, ssize_t fileSize)
{
    int bytesWritten;
    if((bytesWritten = fileSystem->WriteUser(buffer, fileSize, fd)) != fileSize)
    {
        printf("ERROR : failed to write in file\n");
        return -1;
    }
    return 0;
}

int FileHandler::CloseFile(int fd)
{
    int closeStatus;
    if((closeStatus = fileSystem->CloseUser(fd)) < 0)
    {
        printf("ERROR : failed to close file\n");
    }
    return closeStatus;
}

void FileHandler::RemoveFile(const char *fileName)
{
    if(!fileSystem->Remove(fileName))
    {
        printf("ERROR : failed to remove file %s\n", fileName);
    }
    return;
}

Server::Server()
{
    serverAddr = postOffice->GetNetAddr();
    clientMap = new BitMap(MAXCLIENTS);
    // the machine address corresponding to the server is already considered as connected
    clientMap->Mark(serverAddr);
    nbClients = 0;
    for(int index = 0; index < MAXCLIENTS; index++)
    {
        clients[index] = NULL;
    }
}

void Server::ClientConnect(Connection *c)
{
    printf("[SERVER] Connecting to client %d\n", c->pIn->pktHdr.from);
    if(nbClients >= MAXCLIENTS)
    {
        printf("ERROR : Client %d couldn't connect, because the server is already handling its maximum "
               "amount of clients.\n",
               c->pIn->pktHdr.from);
        return;
    }
    // set the bit associated to the client (machine) address
    clientMap->Mark(c->pIn->pktHdr.from);
    // add the client connection to the list of connections on the server side, and increment the
    // number of clients
    clients[c->pIn->pktHdr.from] = c;
    nbClients++;
}

void Server::ClientDisconnect(Connection *c)
{
    printf("[SERVER] Disconnecting from client %d\n", c->pIn->pktHdr.from);
    // client must be already connected (must be 1 in the bitmap)
    ASSERT(clientMap->Test(c->pIn->pktHdr.from));
    // clear the bit associated to the client (machine) address
    clientMap->Clear(c->pIn->pktHdr.from);
    clients[c->pIn->pktHdr.from] = NULL;
    nbClients--;
}

// Routine executed by every created thread
static void ClientHandler(int arg)
{
    FTPEnvironment *env = (FTPEnvironment *)arg;
    env->server->HandleClient(env->c);
}

void Server::ServerRoutine()
{
    Connection *c;
    char clientThreadName[MAXNAMESIZE];
    printf("[SERVER] Launching server with address %d ...\n", postOffice->GetNetAddr());

    while(true)
    {
        // accept a connection
        c = postOffice->Listen();
        // client failed to connect
        if(c == NULL)
        {
            printf("ERROR : Client failed to connect because of a mailbox shortage\n");
        }
        else if(clientMap->Test(c->pIn->pktHdr.from))
        {
            printf("ERROR : Client %d failed to connect because it is either already connected or this "
                   "machine address is unavailable\n",
                   c->pIn->pktHdr.from);
            // tell the client to disconnect on its side
            NotifyClient(c, DISCONNECT, 0);
            // disconnect on the server side
            postOffice->Disconnect(c);
        }
        // client succeeded to connect
        else
        {
            // build the name string for the current client
            snprintf(clientThreadName, MAXNAMESIZE, "server handler of client %d",
                     c->pIn->pktHdr.from);
            // set up the server structures to identify the current client
            ClientConnect(c);
            // create a new thread and start executing the client handler
            Thread *t = new Thread(clientThreadName);
            clientsThreads[c->pIn->pktHdr.from] = t;
            // build an environment structure to transfer data to the new thread
            FTPEnvironment *env = new FTPEnvironment;
            env->server = this;
            env->c = c;
            t->Fork(ClientHandler, (int)env);
        }
    }
}

void Server::NotifyClient(Connection *c, FTPType type, ssize_t fileSize)
{
    FTPHeader ftpHdr;
    const char *msg = "Server notification";
    char notification[sizeof(FTPHeader) + strlen(msg) + 1];
    ftpHdr.type = type;
    ftpHdr.fileSize = fileSize;
    // write header
    bcopy(&ftpHdr, notification, sizeof(FTPHeader));
    // write message
    bcopy(msg, notification + sizeof(FTPHeader), strlen(msg) + 1);
    // send signal
    postOffice->Send(c, notification, sizeof(FTPHeader) + strlen(msg) + 1);
}

bool Server::ClientConfirmation(Connection *c)
{
    FTPHeader ftpHdr;
    char clientAnswer[sizeof(FTPHeader) + strlen("Client notification") + 1];
    postOffice->Receive(c, clientAnswer);
    ftpHdr = *(FTPHeader *)clientAnswer;
    ASSERT(ftpHdr.type == OK || ftpHdr.type == ERROR);
    if(ftpHdr.type == ERROR)
    {
        return false;
    }
    else
    {
        return true;
    }
}

// handles the requests of a specific client
void Server::HandleClient(Connection *c)
{
    // format of a client message : FTPHeader + data

    FTPHeader ftpHdr;
    // tell the client it can proceed
    NotifyClient(c, OK, 0);

    while(true)
    {
        char buffer[c->pIn->msgSize];
        char data[c->pIn->msgSize - sizeof(FTPHeader)];
        postOffice->Receive(c, buffer);
        // retrieve FTPHeader from the received message
        ftpHdr = *(FTPHeader *)buffer;
        // retrieve data from the received message
        bcopy(buffer + sizeof(FTPHeader), data, c->pIn->msgSize - sizeof(FTPHeader));
        // filter client request
        switch(ftpHdr.type)
        {
        // client wants to receive a file from the server
        case READFILE:
            printf("[SERVER] Received READFILE \"%s\" request from client %d\n", data,
                   c->pIn->pktHdr.from);
            SendFile(c, data);
            break;
        // client wants to send a file to the server
        case WRITEFILE:
            printf("[SERVER] Received WRITEFILE \"%s\" request from client %d\n", data,
                   c->pIn->pktHdr.from);
            ReceiveFile(c, data, ftpHdr.fileSize);
            break;
        // client wants to terminate the connection
        case DISCONNECT:
            printf("[SERVER] Received DISCONNECT request from client %d\n", c->pIn->pktHdr.from);
            NotifyClient(c, DISCONNECT, 0);
            ClientDisconnect(c);
            postOffice->Disconnect(c);
            currentThread->Finish();
            break;
        default:
            printf("ERROR : invalid client request %d\n", ftpHdr.type);
            NotifyClient(c, ERROR, 0);
        }
    }
}

// send a file to the client
bool Server::SendFile(Connection *c, char *fileName)
{
    off_t fileSize;
    FTPHeader ftpHdr;
    // retrieve file size
    if((fileSize = FileHandler::FileSize(fileName)) < 0)
    {
        NotifyClient(c, ERROR, 0);
        return false;
    }
    printf("Size finded\n");
    // open file
    int fd;
    if((fd = FileHandler::OpenFile(fileName)) < 0)
    {
        NotifyClient(c, ERROR, 0);
        return false;
    }
    // read file content
    char *buffer = new char[fileSize];
    if(FileHandler::ReadFile(fd, buffer, fileSize) < 0)
    {
        NotifyClient(c, ERROR, 0);
        FileHandler::CloseFile(fd);
        return false;
    }
    // tell client to prepare for file data transmision
    printf("[SERVER] File \"%s\" initialized - notifying client %d ...\n", fileName,
           c->pIn->pktHdr.from);
    NotifyClient(c, OK, fileSize);
    // receive client confirmation that it is ready
    printf("[SERVER] Waiting for client confirmation from %d ...\n", c->pIn->pktHdr.from);
    if(!ClientConfirmation(c))
    {
        printf("ERROR : client couldn't open file \"%s\"\n", fileName);
        FileHandler::CloseFile(fd);
        delete[] buffer;
        return false;
    }
    printf("[SERVER] Got confirmation from %d\n", c->pIn->pktHdr.from);
    // send file content
    char *fileData = new char[sizeof(FTPHeader) + fileSize];
    ftpHdr.type = FILEDATA;
    ftpHdr.fileSize = fileSize;
    // write header
    bcopy(&ftpHdr, fileData, sizeof(FTPHeader));
    // write data
    bcopy(buffer, fileData + sizeof(FTPHeader), fileSize);
    // send to client
    printf("[SERVER] Sending file data to client %d ...\n", c->pIn->pktHdr.from);
    postOffice->Send(c, fileData, sizeof(FTPHeader) + fileSize);
    // receive confirmation from client
    printf("[SERVER] Waiting for client confirmation from %d ...\n", c->pIn->pktHdr.from);
    if(!ClientConfirmation(c))
    {
        printf("ERROR : client couldn't write file \"%s\"\n", fileName);
        FileHandler::CloseFile(fd);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[SERVER] Got confirmation from %d\n", c->pIn->pktHdr.from);
    // Close the file descriptor
    printf("[SERVER] Ending transmission, closing file \"%s\" ...\n", fileName);
    if(FileHandler::CloseFile(fd) < 0)
    {
        NotifyClient(c, ERROR, 0);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[SERVER] File \"%s\" successfully transferred ! Notifying client %d ...\n", fileName,
           c->pIn->pktHdr.from);
    NotifyClient(c, OK, 0);
    delete[] buffer;
    delete[] fileData;
    return true;
}

bool Server::ReceiveFile(Connection *c, char *fileName, ssize_t fileSize)
{
    FTPHeader ftpHdr;
    // create file
    if(FileHandler::CreateFile(fileName) < 0)
    {
        NotifyClient(c, ERROR, 0);
        return false;
    }
    // open file
    int fd;
    if((fd = FileHandler::OpenFile(fileName)) < 0)
    {
        NotifyClient(c, ERROR, 0);
        return false;
    }
    // tell client that server is ready to receive file data
    printf("[SERVER] File \"%s\" initialized - notifying client %d ...\n", fileName,
           c->pIn->pktHdr.from);
    NotifyClient(c, OK, 0);
    // wait for client to be ready (receive OK or ERROR)
    printf("[SERVER] Waiting for client confirmation from %d ...\n", c->pIn->pktHdr.from);
    if(!ClientConfirmation(c))
    {
        printf("ERROR : client couldn't send file \"%s\"\n", fileName);
        FileHandler::CloseFile(fd);
        FileHandler::RemoveFile(fileName);
        return false;
    }
    printf("[SERVER] Got confirmation from %d\n", c->pIn->pktHdr.from);
    // receive file data from client
    char *buffer = new char[sizeof(FTPHeader) + fileSize];
    char *fileData = new char[fileSize];
    printf("[SERVER] Receiving file data from client %d ...\n", c->pIn->pktHdr.from);
    postOffice->Receive(c, buffer);
    // retrieve FTPHeader from the received message
    ftpHdr = *(FTPHeader *)buffer;
    // retrieve data from the received message
    bcopy(buffer + sizeof(FTPHeader), fileData, fileSize);
    ASSERT(ftpHdr.type == FILEDATA);
    ASSERT(ftpHdr.fileSize == fileSize);
    // write file data on server side
    if(FileHandler::WriteFile(fd, fileData, fileSize) < 0)
    {
        NotifyClient(c, ERROR, 0);
        FileHandler::CloseFile(fd);
        FileHandler::RemoveFile(fileName);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[SERVER] Successfully wrote file data from %d - notifying client ...\n",
           c->pIn->pktHdr.from);
    NotifyClient(c, OK, 0);
    // wait for client notification (OK or ERROR)
    printf("[SERVER] Waiting for client confirmation from %d ...\n", c->pIn->pktHdr.from);
    if(!ClientConfirmation(c))
    {
        printf("ERROR : client couldn't confirm end of transmission for file %s", fileName);
        FileHandler::CloseFile(fd);
        FileHandler::RemoveFile(fileName);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[SERVER] Got confirmation from %d\n", c->pIn->pktHdr.from);
    // close file on server side
    if(FileHandler::CloseFile(fd) < 0)
    {
        FileHandler::RemoveFile(fileName);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[SERVER] File \"%s\" successfully transferred !\n", fileName);
    fileSystem->PrintDirectory();
    delete[] buffer;
    delete[] fileData;
    return true;
}

Client::Client()
{
    conn = NULL;
    serverAddr = -1;
}

void Client::Connect(NetworkAddress serverAddress)
{
    FTPHeader ftpHdr;
    char serverAnswer[sizeof(FTPHeader) + strlen("Server notification") + 1];
    // establish a connection with server
    printf("[CLIENT %d] Connecting to server ...\n", postOffice->GetNetAddr());
    Connection *c = postOffice->Connect(serverAddress);
    if(c == NULL)
    {
        printf("ERROR : couldn't connect to server %d\n", serverAddress);
        interrupt->Halt();
        return;
    }
    // wait for the server answer (OK or DISCONNECT)
    postOffice->Receive(c, serverAnswer);
    ftpHdr = *(FTPHeader *)serverAnswer;
    ASSERT(ftpHdr.type == OK || ftpHdr.type == DISCONNECT);
    // server cannot handle the connection and asks for a disconnection
    if(ftpHdr.type == DISCONNECT)
    {
        printf(
            "ERROR : connection to server %d succeeded on client side but server is unavailable\n",
            serverAddress);
        postOffice->Disconnect(c);
        return;
    }
    // connection succeeded
    else
    {
        serverAddr = serverAddress;
        conn = c;
        return;
    }
}

void Client::Disconnect()
{
    ASSERT(conn != NULL);
    printf("[CLIENT %d] Disconnecting from server ...\n", postOffice->GetNetAddr());
    // ask server for disconnection
    SendDisconnectRequest();
    // remove server address
    serverAddr = -1;
    // end connection on client side
    postOffice->Disconnect(conn);
}

void Client::NotifyServer(FTPType type)
{
    ASSERT(conn != NULL);
    FTPHeader ftpHdr;
    const char *msg = "Client notification";
    char notification[sizeof(FTPHeader) + strlen(msg) + 1];
    ftpHdr.type = type;
    ftpHdr.fileSize = 0;
    // write header
    bcopy(&ftpHdr, notification, sizeof(FTPHeader));
    // write message
    bcopy(msg, notification + sizeof(FTPHeader), strlen(msg) + 1);
    // send signal
    postOffice->Send(conn, notification, sizeof(FTPHeader) + strlen(msg) + 1);
}

bool Client::ServerConfirmation(ssize_t *fileSize)
{
    ASSERT(conn != NULL);
    FTPHeader ftpHdr;
    char serverAnswer[sizeof(FTPHeader) + strlen("Server notification") + 1];
    postOffice->Receive(conn, serverAnswer);
    ftpHdr = *(FTPHeader *)serverAnswer;
    ASSERT(ftpHdr.type == OK || ftpHdr.type == ERROR);
    if(ftpHdr.type == ERROR)
    {
        return false;
    }
    else
    {
        // store file size if needed
        if(fileSize != NULL)
        {
            *fileSize = ftpHdr.fileSize;
        }
        return true;
    }
}

void Client::SendDisconnectRequest()
{
    ASSERT(conn != NULL);
    FTPHeader ftpHdrOut, ftpHdrIn;
    char request[sizeof(FTPHeader) + strlen("Client request") + 1];
    char serverAnswer[strlen("Server notification") + 1];
    // build up FTPHeader for the request
    ftpHdrOut.type = DISCONNECT;
    ftpHdrOut.fileSize = 0;
    // write FTPHeader
    bcopy(&ftpHdrOut, request, sizeof(FTPHeader));
    // write data
    bcopy("Client request", request + sizeof(FTPHeader), strlen("Client request") + 1);
    // send signal
    postOffice->Send(conn, request, sizeof(FTPHeader) + strlen("Client request") + 1);
    // receive server answer (must be DISCONNECT)
    postOffice->Receive(conn, serverAnswer);
    ftpHdrIn = *(FTPHeader *)serverAnswer;
    ASSERT(ftpHdrIn.type == DISCONNECT);
}

bool Client::SendFileRequest(FTPType requestType, const char *fileName)
{
    ASSERT(conn != NULL);
    FTPHeader ftpHdrOut;
    char request[sizeof(FTPHeader) + strlen(fileName) + 1];
    ssize_t fileSize;

    // the request must be related to file transfer
    ASSERT(requestType == READFILE || requestType == WRITEFILE);

    ftpHdrOut.type = requestType;
    if(requestType == READFILE)
    {
        printf("[CLIENT %d] Sending a READFILE request to server for file \"%s\" ...\n",
               postOffice->GetNetAddr(), fileName);
        ftpHdrOut.fileSize = 0;
    }
    else
    {
        printf("[CLIENT %d] Sending a WRITEFILE request to server for file \"%s\" ...\n",
               postOffice->GetNetAddr(), fileName);
        if ((ftpHdrOut.fileSize = FileHandler::FileSize(fileName)) < 0) {
            return false;
        }
    }
    // write header
    bcopy(&ftpHdrOut, request, sizeof(FTPHeader));
    // write message
    bcopy(fileName, request + sizeof(FTPHeader), strlen(fileName) + 1);
    // send signal
    postOffice->Send(conn, request, sizeof(FTPHeader) + strlen(fileName) + 1);
    // wait for server answer (OK or ERROR)
    printf("[CLIENT %d] Waiting for server confirmation ...\n", postOffice->GetNetAddr());
    if(!ServerConfirmation(&fileSize))
    {
        printf("ERROR : server couldn't initialize file \"%s\"\n", fileName);
        return false;
    }
    printf("[CLIENT %d] Got confirmation from server\n", postOffice->GetNetAddr());
    // if everything is OK, begin transmission
    if(requestType == READFILE)
    {
        return ReceiveFile(fileName, fileSize);
    }
    else
    {
        return SendFile(fileName);
    }
}

bool Client::SendFile(const char *fileName)
{
    ASSERT(conn != NULL);
    off_t fileSize;
    FTPHeader ftpHdr;

    // retrieve file size
    if((fileSize = FileHandler::FileSize(fileName)) < 0)
    {
        NotifyServer(ERROR);
        return false;
    }
    // open file
    int fd;
    if((fd = FileHandler::OpenFile(fileName)) < 0)
    {
        NotifyServer(ERROR);
        return false;
    }
    // read file content
    char *buffer = new char[fileSize];
    if(FileHandler::ReadFile(fd, buffer, fileSize) < 0)
    {
        NotifyServer(ERROR);
        FileHandler::CloseFile(fd);
        delete[] buffer;
        return false;
    }
    // send confirmation to server
    printf("[CLIENT %d] Initialized file \"%s\" on client side - notifying server ...\n",
           postOffice->GetNetAddr(), fileName);
    NotifyServer(OK);
    // send file content
    char *fileData = new char[sizeof(FTPHeader) + fileSize];
    ftpHdr.type = FILEDATA;
    ftpHdr.fileSize = fileSize;
    // write header
    bcopy(&ftpHdr, fileData, sizeof(FTPHeader));
    // write data
    bcopy(buffer, fileData + sizeof(FTPHeader), fileSize);
    // send to server
    printf("[CLIENT %d] Sending file \"%s\" to server ...\n", postOffice->GetNetAddr(), fileName);
    postOffice->Send(conn, fileData, sizeof(FTPHeader) + fileSize);
    // wait for server confirmation
    printf("[CLIENT %d] Waiting for server confirmation ...\n", postOffice->GetNetAddr());
    if(!ServerConfirmation(NULL))
    {
        FileHandler::CloseFile(fd);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[CLIENT %d] Got confirmation from server\n", postOffice->GetNetAddr());
    // Close the file descriptor
    if(FileHandler::CloseFile(fd) < 0)
    {
        NotifyServer(ERROR);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[CLIENT %d] File \"%s\" successfully transferred ! Notifying server ...\n",
           postOffice->GetNetAddr(), fileName);
    NotifyServer(OK);
    delete[] buffer;
    delete[] fileData;
    return true;
}

bool Client::ReceiveFile(const char *fileName, ssize_t fileSize)
{
    ASSERT(conn != NULL);
    FTPHeader ftpHdr;

    // create file
    if(FileHandler::CreateFile(fileName) < 0)
    {
        NotifyServer(ERROR);
        return false;
    }    
    // open file
    int fd;
    if((fd = FileHandler::OpenFile(fileName)) < 0)
    {
        NotifyServer(ERROR);
        return false;
    }
    // tell server that client is ready to receive file data
    printf("[CLIENT %d] Initialized file \"%s\" on client side - notifying server ...\n",
           postOffice->GetNetAddr(), fileName);
    NotifyServer(OK);
    // receive file data from client
    printf("[CLIENT %d] Receiving file \"%s\" from server ...\n", postOffice->GetNetAddr(),
           fileName);
    char *buffer = new char[sizeof(FTPHeader) + fileSize];
    char *fileData = new char[fileSize];
    postOffice->Receive(conn, buffer);
    // retrieve FTPHeader from the received message
    ftpHdr = *(FTPHeader *)buffer;
    // retrieve data from the received message
    bcopy(buffer + sizeof(FTPHeader), fileData, fileSize);
    ASSERT(ftpHdr.type == FILEDATA);
    ASSERT(ftpHdr.fileSize == fileSize);
    // write file data on client side
    if(FileHandler::WriteFile(fd, fileData, fileSize) < 0)
    {
        NotifyServer(ERROR);
        FileHandler::CloseFile(fd);
        FileHandler::RemoveFile(fileName);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    // tell server that write operation succeeded
    printf("[CLIENT %d] Successfully wrote file \"%s\" on client side - notifying server ...\n",
           postOffice->GetNetAddr(), fileName);
    NotifyServer(OK);
    // wait for server confirmation (OK or ERROR)
    printf("[CLIENT %d] Waiting for server confirmation ...\n", postOffice->GetNetAddr());
    if(!ServerConfirmation(NULL))
    {
        printf("ERROR : server couldn't confirm end of transmission for file %s", fileName);
        FileHandler::CloseFile(fd);
        FileHandler::RemoveFile(fileName);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[CLIENT %d] Got confirmation from server\n", postOffice->GetNetAddr());
    // close file on client side
    if(FileHandler::CloseFile(fd) < 0)
    {
        FileHandler::RemoveFile(fileName);
        delete[] buffer;
        delete[] fileData;
        return false;
    }
    printf("[CLIENT %d] File \"%s\" transferred successfully !\n", postOffice->GetNetAddr(),
           fileName);
    delete[] buffer;
    delete[] fileData;
    return true;
}


bool FTPClientAction(int servAddr, char readwrite, char *fileName) {
    bool res;
    Client *client = new Client();
    client->Connect(servAddr);
    if (readwrite == 'r') {
        res = client->SendFileRequest(READFILE, fileName);
    } else if (readwrite == 'w') {
        res = client->SendFileRequest(WRITEFILE, fileName);
    }
    client->Disconnect();
    delete client;
    return res;
}

// Server routine that loops endlessly to accept new connections
void startFTPserver() {
    Server *server = new Server();
    server->ServerRoutine();
}
