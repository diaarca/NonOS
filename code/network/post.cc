// post.cc
// 	Routines to deliver incoming network messages to the correct
//	"address" -- a mailbox, or a holding area for incoming messages.
//	This module operates just like the US postal service (in other
//	words, it works, but it's slow, and you can't really be sure if
//	your mail really got through!).
//
//	Note that once we prepend the MailHdr to the outgoing message data,
//	the combination (MailHdr plus data) looks like "data" to the Network
//	device.
//
// 	The implementation synchronizes incoming messages with threads
//	waiting for those messages.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "post.h"
#include "copyright.h"
#include "system.h"
#include <string>
#include <strings.h> /* for bzero */
#include <tuple>
#include <vector>

static void NetworkSendWaitHandler(int arg)
{
    static int lastTicksTEMPO = 0;
    static int lastTicksDISCONNECT = 0;

    int currTick = stats->totalTicks;
    if(currTick - lastTicksTEMPO > TEMPO)
    {
        lastTicksTEMPO = currTick;
        for(int i = 0; i < postOffice->numBoxes; i++)
        {
            postOffice->boxes[i].ackLock->Acquire();
            postOffice->boxes[i].ackCond->Broadcast(postOffice->boxes[i].ackLock);
            postOffice->boxes[i].ackLock->Release();
        }
    }
    if(currTick - lastTicksDISCONNECT > DISCONNECT_TEMPO)
    {

        lastTicksDISCONNECT = currTick;
        postOffice->disconnectLock->Acquire();
        postOffice->disconnectCond->Signal(postOffice->disconnectLock);
        postOffice->disconnectLock->Release();
    }
}

//----------------------------------------------------------------------
// Mail::Mail
//      Initialize a single mail message, by concatenating the headers to
//	the data.
//
//	"pktH" -- source, destination machine ID's
//	"mailH" -- source, destination mailbox ID's
//	"data" -- payload data
//----------------------------------------------------------------------

Mail::Mail(Payload *p, char *msgData)
{
    pktHdr = p->pktHdr;
    mailHdr = p->mailHdr;
    // systematically copy the size of a full segment, because we cannot filter whether this is the
    // last segment or not such a filtering will be performed by MailBox::Get later
    bcopy(msgData, data, MaxSegmentSize);
}

//----------------------------------------------------------------------
// MailBox::MailBox
//      Initialize a single mail box within the post office, so that it
//	can receive incoming messages.
//
//	Just initialize a list of messages, representing the mailbox.
//----------------------------------------------------------------------

MailBox::MailBox()
{
    messages = new SynchList();
    ackCond = new Condition("ack mail box cond");
    ackLock = new Lock("ack mail box cond");
    waitedId = 0;
    ackId = -1;
}

//----------------------------------------------------------------------
// MailBox::~MailBox
//      De-allocate a single mail box within the post office.
//
//	Just delete the mailbox, and throw away all the queued messages
//	in the mailbox.
//----------------------------------------------------------------------

MailBox::~MailBox() { delete messages; }

//----------------------------------------------------------------------
// PrintHeader
// 	Print the message header -- the destination machine ID and mailbox
//	#, source machine ID and mailbox #, and message length.
//
//	"pktHdr" -- source, destination machine ID's
//	"mailHdr" -- source, destination mailbox ID's
//----------------------------------------------------------------------

static void PrintHeader(PacketHeader pktHdr, MailHeader mailHdr)
{
    DEBUG('p', "From (%d, %d) to (%d, %d) bytes %d\n", pktHdr.from, mailHdr.from, pktHdr.to,
          mailHdr.to, mailHdr.length);
}

//----------------------------------------------------------------------
// MailBox::Put
// 	Add a message to the mailbox.  If anyone is waiting for message
//	arrival, wake them up!
//
//	We need to reconstruct the Mail message (by concatenating the headers
//	to the data), to simplify queueing the message on the SynchList.
//
//	"pktHdr" -- source, destination machine ID's
//	"mailHdr" -- source, destination mailbox ID's
//	"data" -- payload message data
//----------------------------------------------------------------------

void MailBox::Put(Payload *p, char *data)
{
    Mail *mail = new Mail(p, data);

    messages->Append((void *)mail); // put on the end of the list of
                                    // arrived messages, and wake up
                                    // any waiters
}

//----------------------------------------------------------------------
// MailBox::Get
// 	Get a message from a mailbox, parsing it into the packet header,
//	mailbox header, and data.
//
//	The calling thread waits if there are no messages in the mailbox.
//
//	"pktHdr" -- address to put: source, destination machine ID's
//	"mailHdr" -- address to put: source, destination mailbox ID's
//	"data" -- address to put: payload message data
//----------------------------------------------------------------------

bool MailBox::Get(Payload *p, int segmentIndex, char *data)
{
    DEBUG('p', "Waiting for mail in mailbox\n");
    Mail *mail = (Mail *)messages->Remove(); // remove message from list;
                                             // will wait if list is empty
    if(mail == NULL)
    {
        return false;
    }
    // Update the payload from the received mail
    p->UpdatePayload(mail->pktHdr.from, mail->pktHdr.to, mail->mailHdr.from, mail->mailHdr.to,
                     mail->mailHdr.length, mail->mailHdr.messageType);
    p->mailHdr.messageId = mail->mailHdr.messageId;
    // if we are dealing with the last segment of the message
    // then copy only the remaining characters into data
    if(segmentIndex == p->nbSegments - 1)
    {
        bcopy(mail->data, data, p->remainder);
    }
    else
    {
        // otherwise copy a whole segment
        bcopy(mail->data, data, MaxSegmentSize);
    }

    if(DebugIsEnabled('p'))
    {
        DEBUG('p', "Got mail from mailbox: ");
        PrintHeader(p->pktHdr, p->mailHdr);
        DEBUG('p', "[Machine %d] Got segment %d (%s) from machine %d, box %d\n", p->pktHdr.to,
              segmentIndex, mail->data, p->pktHdr.from, p->mailHdr.from);
    }
    // copy the message data into
    // the caller's buffer
    delete mail; // we've copied out the stuff we
                 // need, we can now discard the message
    return true;
}

//----------------------------------------------------------------------
// PostalHelper, ReadAvail, WriteDone
// 	Dummy functions because C++ can't indirectly invoke member functions
//	The first is forked as part of the "postal worker thread; the
//	later two are called by the network interrupt handler.
//
//	"arg" -- pointer to the Post Office managing the Network
//----------------------------------------------------------------------

static void PostalHelper(int arg)
{
    PostOffice *po = (PostOffice *)arg;
    po->PostalDelivery();
}
static void ReadAvail(int arg)
{
    PostOffice *po = (PostOffice *)arg;
    po->IncomingPacket();
}
static void WriteDone(int arg)
{
    PostOffice *po = (PostOffice *)arg;
    po->PacketSent();
}

//----------------------------------------------------------------------
// PostOffice::PostOffice
// 	Initialize a post office as a collection of mailboxes.
//	Also initialize the network device, to allow post offices
//	on different machines to deliver messages to one another.
//
//      We use a separate thread "the postal worker" to wait for messages
//	to arrive, and deliver them to the correct mailbox.  Note that
//	delivering messages to the mailboxes can't be done directly
//	by the interrupt handlers, because it requires a Lock.
//
//	"addr" is this machine's network ID
//	"reliability" is the probability that a network packet will
//	  be delivered (e.g., reliability = 1 means the network never
//	  drops any packets; reliability = 0 means the network never
//	  delivers any packets)
//	"nBoxes" is the number of mail boxes in this Post Office
//----------------------------------------------------------------------

PostOffice::PostOffice(NetworkAddress addr, double reliability, int nBoxes)
{
    // First, initialize the synchronization with the interrupt handlers
    messageAvailable = new Semaphore("message available", 0);
    messageSent = new Semaphore("message sent", 0);
    sendLock = new Lock("message send lock");
    // Second, initialize the mailboxes
    netAddr = addr;
    numBoxes = nBoxes;
    boxes = new MailBox[nBoxes];
    usedBoxes = new BitMap(numBoxes);
    usedBoxes->Mark(LISTEN_BOX);
    connLock = new Lock("conn lock");
    disconnectCond = new Condition("disconnect cond");
    disconnectLock = new Lock("disonnect lock");
    // Third, initialize the network; tell it which interrupt handlers to call
    network = new Network(addr, reliability, ReadAvail, WriteDone, (int)this);

    // Finally, create a thread whose sole job is to wait for incoming messages,
    //   and put them in the right mailbox.
    Thread *t = new Thread("postal worker");
    BroadcastTimer = new Timer(NetworkSendWaitHandler, 0, false);

    t->Fork(PostalHelper, (int)this);
}

//----------------------------------------------------------------------
// PostOffice::~PostOffice
// 	De-allocate the post office data structures.
//----------------------------------------------------------------------

PostOffice::~PostOffice()
{
    delete network;
    delete[] boxes;
    delete messageAvailable;
    delete messageSent;
    delete sendLock;
    delete BroadcastTimer;
}

//----------------------------------------------------------------------
// PostOffice::PostalDelivery
// 	Wait for incoming messages, and put them in the right mailbox.
//
//      Incoming messages have had the PacketHeader stripped off,
//	but the MailHeader is still tacked on the front of the data.
//----------------------------------------------------------------------

void PostOffice::PostalDelivery()
{
    PacketHeader pktHdr;
    MailHeader mailHdr;
    Payload *p;
    char *buffer = new char[MaxPacketSize];

    for(;;)
    {
        // first, wait for a message
        messageAvailable->P();
        pktHdr = network->Receive(buffer);

        mailHdr = *(MailHeader *)buffer;
        if(DebugIsEnabled('p'))
        {
            DEBUG('p', "Putting mail into mailbox: ");
            PrintHeader(pktHdr, mailHdr);
        }
        // check that arriving message is legal!
        ASSERT(0 <= mailHdr.to && mailHdr.to < numBoxes);
        p = new Payload(pktHdr.from, pktHdr.to, mailHdr.from, mailHdr.to, mailHdr.length,
                        mailHdr.messageType);
        p->mailHdr.messageId = mailHdr.messageId;
        if(mailHdr.messageType == CONN && mailHdr.to == LISTEN_BOX)
        {
            ConnReminder *cRm = new ConnReminder;
            cRm->connTimestamp = *(time_t *)(buffer + sizeof(MailHeader));
            cRm->netFrom = pktHdr.from;
            cRm->netTo = pktHdr.to;
            cRm->mailFrom = mailHdr.from;
            cRm->mailTo = mailHdr.to;
            if(ValidConn(cRm))
            {

                DEBUG('p', "[machine %d] receive CONN message from machine ID %d\n", GetNetAddr(),
                      pktHdr.from);
                boxes[mailHdr.to].Put(p, buffer + sizeof(MailHeader));
            }
            else
            {
                DEBUG('p', "[machine %d] receive invalid CONN message from machine ID %d\n",
                      GetNetAddr(), pktHdr.from);
                delete cRm;
            }
        }
        else if(mailHdr.messageType == DATA && mailHdr.messageId == boxes[mailHdr.to].waitedId)
        {
            // put into mailbox
            DEBUG('p', "[machine %d] receive DATA message with ID %d\n", GetNetAddr(),
                  mailHdr.messageId);
            boxes[mailHdr.to].Put(p, buffer + sizeof(MailHeader));
            boxes[mailHdr.to].waitedId++;
            delete p;
        }
        else if(mailHdr.messageType == ACK)
        {
            boxes[mailHdr.to].ackLock->Acquire();
            boxes[mailHdr.to].ackId = mailHdr.messageId;
            DEBUG('p', "[machine %d] receive ACK message with ID %d\n", GetNetAddr(),
                  mailHdr.messageId);
            boxes[mailHdr.to].ackCond->Broadcast(boxes[mailHdr.to].ackLock);
            boxes[mailHdr.to].ackLock->Release();
        }
        else
        {
            DEBUG('p', "[machine %d] receive message with invalid ID: %d instead of %d\n",
                  GetNetAddr(), mailHdr.messageId, boxes[mailHdr.to].waitedId);
        }
        if(mailHdr.messageType != ACK)
        {
            PacketHeader ackPktHdr;
            MailHeader ackMailHdr;
            ackPktHdr.from = pktHdr.to;
            ackPktHdr.to = pktHdr.from;
            ackPktHdr.length = sizeof(MailHeader);
            ackMailHdr.from = mailHdr.to;
            ackMailHdr.to = mailHdr.from;
            ackMailHdr.messageType = ACK;
            ackMailHdr.messageId = mailHdr.messageId;
            ackMailHdr.length = 0;
            DEBUG('p', "[machine %d] DATA received, send ACK %d\n", GetNetAddr(),
                  ackMailHdr.messageId);
            sendLock->Acquire();
            network->Send(ackPktHdr, (char *)&ackMailHdr);
            messageSent->P();
            sendLock->Release();
        }
        // de-allocate the Payload
    }
}

//----------------------------------------------------------------------
// PostOffice::IncomingPacket
// 	Interrupt handler, called when a packet arrives from the network.
//
//	Signal the PostalDelivery routine that it is time to get to work!
//----------------------------------------------------------------------

void PostOffice::IncomingPacket() { messageAvailable->V(); }

//----------------------------------------------------------------------
// PostOffice::PacketSent
// 	Interrupt handler, called when the next packet can be put onto the
//	network.
//
//	The name of this routine is a misnomer; if "reliability < 1",
//	the packet could have been dropped by the network, so it won't get
//	through.
//----------------------------------------------------------------------

void PostOffice::PacketSent() { messageSent->V(); }

NetworkAddress PostOffice::GetNetAddr() { return netAddr; }

// Initialize a payload without data, to default zero values (because lengths are unsigned)
Payload::Payload()
{
    msgSize = 0;
    remainder = 0;
    nbSegments = 0;
    pktHdr.from = 0;
    pktHdr.to = 0;
    pktHdr.length = 0;
    mailHdr.from = 0;
    mailHdr.to = 0;
    mailHdr.length = 0;
    mailHdr.messageType = DATA;
    mailHdr.messageId = 0;
}
Payload::Payload(NetworkAddress netFrom,
                 NetworkAddress netTo,
                 MailBoxAddress mailFrom,
                 MailBoxAddress mailTo,
                 unsigned int length)
{
    UpdatePayload(netFrom, netTo, mailFrom, mailTo, length, DATA);
}

Payload::Payload(NetworkAddress netFrom,
                 NetworkAddress netTo,
                 MailBoxAddress mailFrom,
                 MailBoxAddress mailTo,
                 unsigned int length,
                 MessageType messageType)
{
    UpdatePayload(netFrom, netTo, mailFrom, mailTo, length, messageType);
}

void Payload::UpdatePayloadSize(unsigned int length)
{
    UpdatePayload(pktHdr.from, pktHdr.to, mailHdr.from, mailHdr.to, length, DATA);
}

void Payload::UpdatePayload(NetworkAddress netFrom,
                            NetworkAddress netTo,
                            MailBoxAddress mailFrom,
                            MailBoxAddress mailTo,
                            unsigned int length)
{
    UpdatePayload(netFrom, netTo, mailFrom, mailTo, length, DATA);
}

// Update a payload with data
void Payload::UpdatePayload(NetworkAddress netFrom,
                            NetworkAddress netTo,
                            MailBoxAddress mailFrom,
                            MailBoxAddress mailTo,
                            unsigned int length,
                            MessageType messageType)
{
    // size of the whole message
    msgSize = length;
    // compute the amount of characters remaining in the last segment, if any
    remainder = msgSize % MaxSegmentSize;
    // compute the number of segments of size MaxSegmentSize required for the whole message
    if(remainder == 0)
    {
        nbSegments = msgSize / MaxSegmentSize;
    }
    else
    {
        nbSegments = msgSize / MaxSegmentSize + 1;
    }
    // set the source machine (source machine and length of packet are set in SendPayload)
    pktHdr.from = netFrom;
    // set the destination machine (source machine and length of packet are set in SendPayload)
    pktHdr.to = netTo;
    // set the length of the packet in the PacketHeader
    pktHdr.length = MaxSegmentSize + sizeof(MailHeader);
    // set the source MailBox
    mailHdr.from = mailFrom;
    // set the destination MailBox
    mailHdr.to = mailTo;
    // set the length of the message in the MailHeader
    mailHdr.length = length;
    mailHdr.messageType = messageType;
}

bool PostOffice::ReliableSendSegment(Payload *p, char *data)
{
    int nReemissions = 0;
    int sentId = p->mailHdr.messageId;
    while(nReemissions < MAXREEMISSIONS)
    {
        boxes[p->mailHdr.from].ackLock->Acquire();
        sendLock->Acquire();
        network->Send(p->pktHdr, data);
        messageSent->P();
        sendLock->Release();
        DEBUG('p', "[machine %d] Emission %d in machine %d in box %d with messageId %d\n",
              GetNetAddr(), nReemissions, p->pktHdr.to, p->mailHdr.to, p->mailHdr.messageId);

        boxes[p->mailHdr.from].ackCond->Wait(boxes[p->mailHdr.from].ackLock);
        int ackId = boxes[p->mailHdr.from].ackId;
        boxes[p->mailHdr.from].ackLock->Release();

        if(ackId == sentId)
        {
            DEBUG('p', "[machine %d] ACK received\n", GetNetAddr());
            return true;
        }
        else if(ackId != -1)
        {
            DEBUG('p', "[machine %d] Received invalid ACK(ignored) %d instead of %d\n ",
                  GetNetAddr(), ackId, sentId);
        }

        DEBUG('p', "[machine %d] NO ACK received, reemission\n", GetNetAddr());
        nReemissions++;
    }

    return false;
}

bool PostOffice::SendPayload(Payload *p, const char *data)
{
    char *buffer = new char[MaxPacketSize]; // space to hold concatenated
                                            // mailHdr + data

    if(DebugIsEnabled('p'))
    {
        DEBUG('p', "Post send: ");
        PrintHeader(p->pktHdr, p->mailHdr);
    }
    ASSERT(0 <= p->mailHdr.to && p->mailHdr.to < numBoxes);

    // fill in pktHdr, for the Network layer
    ASSERT(p->pktHdr.from == netAddr);
    ASSERT(p->pktHdr.length == MaxSegmentSize + sizeof(MailHeader));

    // split the message into segments
    for(int segIndex = 0; segIndex < p->nbSegments; segIndex++)
    {
        // reset the buffer before the upcoming iteration
        bzero(buffer, MaxPacketSize);
        // write MailHeader first, before the data
        bcopy(&(p->mailHdr), buffer, sizeof(MailHeader));
        // if we are dealing with the last segment of a message
        // then only write the remaining characters into buffer
        if(segIndex == p->nbSegments - 1)
        {
            bcopy(data + segIndex * MaxSegmentSize, buffer + sizeof(MailHeader), p->remainder);
        }
        else
        {
            // otherwise write a whole segment
            bcopy(data + segIndex * MaxSegmentSize, buffer + sizeof(MailHeader), MaxSegmentSize);
        }
        if(DebugIsEnabled('p'))
        {
            DEBUG('p', "[Machine %d] Sent segment %d (%s) to machine %d, box %d\n", p->pktHdr.from,
                  segIndex, buffer + sizeof(MailHeader), p->pktHdr.to, p->mailHdr.to);
        }

        if(!ReliableSendSegment(p, buffer))
        {
            delete[] buffer;
            return false;
        }
        p->mailHdr.messageId++;
    }
    delete[] buffer; // we've sent the message, so
                     // we can delete our buffer
    return true;
}

// Receive a payload from the network
void PostOffice::ReceivePayload(Payload *p, int box, char *data)
{
    ASSERT((box >= 0) && (box < numBoxes));
    bzero(data, strlen(data) + 1);
    // first call to Get() in order to initialize the current payload
    boxes[box].Get(p, 0, data);
    // Browse through every other segment index to reconstitute data
    for(int segIndex = 1; segIndex < p->nbSegments; segIndex++)
    {
        boxes[box].Get(p, segIndex, data + segIndex * MaxSegmentSize);
    }
}

void PostOffice::DisconnectPayload(Payload *inP)
{
    DEBUG('p', "START DISCONNECT\n");
    int box = inP->mailHdr.to;
    while(!boxes[box].messages->IsEmpty())
    {
        boxes[box].messages->Remove();
    }
    disconnectLock->Acquire();
    disconnectCond->Wait(disconnectLock); // Be sure that DISONNECT_TEMPO is reached
    do
    {
        disconnectCond->Wait(disconnectLock);
    } while(boxes[box].messages->RemoveNoWaiting() != NULL);
    disconnectLock->Release();
    boxes[box].waitedId = 0;
    usedBoxes->Clear(box);
    DEBUG('p', "END DISCONNECT\n");
}

Connection *PostOffice::Connect(NetworkAddress addr)
{
    DEBUG('p', "Start connect\n");
    if(addr == postOffice->GetNetAddr())
    {
        printf("ERROR : a machine cannot connect to itself !\n");
        return NULL;
    }
    char buffer[MaxSegmentSize];
    time_t timestamp = time(0);
    int box = usedBoxes->Find();
    if(box == -1)
    {
        return NULL;
    }
    Connection *c = new Connection;
    c->pOut = new Payload;
    c->pIn = new Payload;
    boxes[box].waitedId = 0;
    c->pOut->UpdatePayload(GetNetAddr(), addr, box, LISTEN_BOX, sizeof(time_t), CONN);
    SendPayload(c->pOut, (char *)&timestamp);
    ReceivePayload(c->pIn, box, buffer);
    ASSERT(!strcmp(buffer, "C"));
    c->pOut->UpdatePayload(GetNetAddr(), addr, box, c->pIn->mailHdr.from, 0);
    DEBUG('p', "End connect\n");
    return c;
}

Connection *PostOffice::Listen()
{
    char buffer[MaxSegmentSize];
    int box = usedBoxes->Find();
    if(box == -1)
    {
        return NULL;
    }

    Connection *c = new Connection;
    c->pOut = new Payload;
    c->pIn = new Payload;
    boxes[box].waitedId = 1;
    ReceivePayload(c->pIn, LISTEN_BOX, buffer);
    ASSERT(c->pIn->mailHdr.messageType == CONN);
    c->pOut->UpdatePayload(GetNetAddr(), c->pIn->pktHdr.from, box, c->pIn->mailHdr.from, 2);
    SendPayload(c->pOut, "C");
    return c;
}

bool PostOffice::Send(Connection *conn, const char *data, size_t data_size)
{
    if(conn == NULL)
    {
        return false;
    }
    conn->pOut->UpdatePayloadSize(data_size);
    return SendPayload(conn->pOut, data);
}

bool PostOffice::Receive(Connection *conn, char *data)
{
    if(conn == NULL)
    {
        return false;
    }
    ReceivePayload(conn->pIn, conn->pOut->mailHdr.from, data);
    return true;
}

void PostOffice::Disconnect(Connection *conn)
{
    if(conn == NULL)
    {
        return;
    }
    DisconnectPayload(conn->pIn);
    delete conn;
}

bool PostOffice::ValidConn(ConnReminder *conn)
{
    connLock->Acquire(); // Ensure thread safety
    bool validConn;
    // Check if the connection already exists
    auto it = std::find_if(connections.begin(), connections.end(),
                           [conn](const ConnReminder *existingConn)
                           {
                               return (existingConn->netFrom == conn->netFrom &&
                                       existingConn->netTo == conn->netTo &&
                                       existingConn->mailFrom == conn->mailFrom &&
                                       existingConn->mailTo == conn->mailTo &&
                                       existingConn->connTimestamp >= conn->connTimestamp);
                           });

    if(it == connections.end())
    {
        // Connection is not in the list, add it
        DEBUG('p', "The conn : %d %d %d %d %ld is valid\n", conn->netFrom, conn->netTo,
              conn->mailFrom, conn->mailTo, conn->connTimestamp);
        connections.push_back(conn);
        validConn = true;
    }
    else
    {
        // Connection is a duplicate; optionally log or handle it
        DEBUG('p', "The conn : %d %d %d %d %ld is duplicated\n", conn->netFrom, conn->netTo,
              conn->mailFrom, conn->mailTo, conn->connTimestamp);
        validConn = false;
    }

    connLock->Release(); // Release the lock
    return validConn;
}
