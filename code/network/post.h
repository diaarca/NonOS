// post.h
//	Data structures for providing the abstraction of unreliable,
//	ordered, fixed-size message delivery to mailboxes on other
//	(directly connected) machines.  Messages can be dropped by
//	the network, but they are never corrupted.
//
// 	The US Post Office delivers mail to the addressed mailbox.
// 	By analogy, our post office delivers packets to a specific buffer
// 	(MailBox), based on the mailbox number stored in the packet header.
// 	Mail waits in the box until a thread asks for it; if the mailbox
//      is empty, threads can wait for mail to arrive in it.
//
// 	Thus, the service our post office provides is to de-multiplex
// 	incoming packets, delivering them to the appropriate thread.
//
//      With each message, you get a return address, which consists of a "from
// 	address", which is the id of the machine that sent the message, and
// 	a "from box", which is the number of a mailbox on the sending machine
//	to which you can send an acknowledgement, if your protocol requires
//	this.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "timer.h"
#ifndef POST_H
#define POST_H
#include "network.h"
#include "synchlist.h"
#include "bitmap.h"
#include <list>
#include <algorithm>
#include <time.h>
#define LISTEN_BOX 0
#define MAXREEMISSIONS 50
#define TEMPO 10000000
#define DISCONNECT_TEMPO (TEMPO * 4)
// Mailbox address -- uniquely identifies a mailbox on a given machine.
// A mailbox is just a place for temporary storage for messages.
typedef int MailBoxAddress;

// Forward declaration of the Payload class (circular dependencies between classes)

// The following class defines part of the message header.
// This is prepended to the message by the PostOffice, before the message
// is sent to the Network.
typedef enum
{
  ACK,
  DATA,
  CONN,
  FIN
} MessageType;

class MailHeader
{
public:
  MailBoxAddress to;   // Destination mail box
  MailBoxAddress from; // Mail box to reply to
  unsigned length;     // Bytes of message data (excluding the
                       // mail header)
  MessageType messageType;
  int messageId;
};

// Maximum "payload" -- real data -- that can included in a single message
// Excluding the MailHeader and the PacketHeader

#define MaxSegmentSize (MaxPacketSize - sizeof(MailHeader))

// The following class defines the format of an incoming/outgoing
// "Mail" message.  The message format is layered:
//	network header (PacketHeader)
//	post office header (MailHeader)
//	data

class Payload
{
public:
  int msgSize;         // size of the message (without any header)
  int nbSegments;      // number of required segments to contain it
  int remainder;       // number of characters left in the last segment
  PacketHeader pktHdr; // source and dest machines
  MailHeader mailHdr;  // source and dest mail boxes
  Payload();
  // Create a payload and set every data field to 0
  Payload(NetworkAddress netFrom,
          NetworkAddress netTo,
          MailBoxAddress mailFrom,
          MailBoxAddress mailTo,
          unsigned int length);
  // Create a payload and set every data field with the given data + message type to DATA

  Payload(NetworkAddress netFrom,
          NetworkAddress netTo,
          MailBoxAddress mailFrom,
          MailBoxAddress mailTo,
          unsigned int length,
          MessageType messageType);
  // Create a payload and set every data field with the given data
  void UpdatePayloadSize(unsigned int length);
  void UpdatePayload(NetworkAddress netFrom,
                     NetworkAddress netTo,
                     MailBoxAddress mailFrom,
                     MailBoxAddress mailTo,
                     unsigned int length);
  // Update a payload from the given data + message type to DATA

  void UpdatePayload(NetworkAddress netFrom,
                     NetworkAddress netTo,
                     MailBoxAddress mailFrom,
                     MailBoxAddress mailTo,
                     unsigned int length,
                     MessageType messageType);
  // Update a payload from the given data
};

class Mail
{
public:
  Mail(Payload *p, char *msgData);
  // Initialize a mail message by
  // concatenating the headers to the data

  PacketHeader pktHdr;       // Header appended by Network
  MailHeader mailHdr;        // Header appended by PostOffice
  char data[MaxSegmentSize]; // Payload -- message data
};

// The following class defines a single mailbox, or temporary storage
// for messages.   Incoming messages are put by the PostOffice into the
// appropriate mailbox, and these messages can then be retrieved by
// threads on this machine.

class MailBox
{
public:
  MailBox();  // Allocate and initialize mail box
  ~MailBox(); // De-allocate mail box

  void Put(Payload *p, char *data);
  // Atomically put a message into the mailbox
  bool Get(Payload *p, int segmentIndex, char *data);
  // Atomically get a message out of the
  // mailbox (and wait if there is no message
  // to get!)
  SynchList *messages; // A mailbox is just a list of arrived messages
  int waitedId;
  int ackId;
  int timeoutId;
  Condition *ackCond;
  Lock *ackLock;
};

class Connection
{
public:
  Payload *pIn;
  Payload *pOut;
};

class ConnReminder
{
public:
  NetworkAddress netFrom;
  NetworkAddress netTo;
  MailBoxAddress mailFrom;
  MailBoxAddress mailTo;
  time_t connTimestamp;
};

// The following class defines a "Post Office", or a collection of
// mailboxes.  The Post Office is a synchronization object that provides
// two main operations: Send -- send a message to a mailbox on a remote
// machine, and Receive -- wait until a message is in the mailbox,
// then remove and return it.
//
// Incoming messages are put by the PostOffice into the
// appropriate mailbox, waking up any threads waiting on Receive.

class PostOffice
{
public:
  PostOffice(NetworkAddress addr, double reliability, int nBoxes);
  // Allocate and initialize Post Office
  //   "reliability" is how many packets
  //   get dropped by the underlying network
  ~PostOffice(); // De-allocate Post Office data

  void PostalDelivery(); // Wait for incoming messages,
                         // and then put them in the correct mailbox

  void PacketSent();     // Interrupt handler, called when outgoing
                         // packet has been put on network; next
                         // packet can now be sent
  void IncomingPacket(); // Interrupt handler, called when incoming
                         // packet has arrived and can be pulled
                         // off of network (i.e., time to call
                         // PostalDelivery)
  bool SendPayload(Payload *p, const char *data);
  void ReceivePayload(Payload *p, int box, char *data);
  // Receive a payload from the network
  bool ReliableSendSegment(Payload *p, char *data);
  void DisconnectPayload(Payload *inP);
  bool Send(Connection *conn, const char *data, size_t data_size);
  bool Receive(Connection *conn, char *data);
  Connection *Connect(NetworkAddress addr);
  Connection *Listen();
  void Disconnect(Connection *conn);
  Lock *disconnectLock;

  Condition *disconnectCond;
  // Send a segment and wait for the ACK
  NetworkAddress GetNetAddr(); // Return the current network id
  MailBox *boxes;              // Table of mail boxes to hold incoming mail
  void BroadcastBoxes();
  int numBoxes;
  BitMap *usedBoxes;

private:
  Timer *BroadcastTimer;
  Network *network;            // Physical network connection
  NetworkAddress netAddr;      // Network address of this machine
  Semaphore *messageAvailable; // V'ed when message has arrived from network
  Semaphore *messageSent;      // V'ed when next message can be sent to network
  Lock *sendLock;              // Only one outgoing message at a time
  std::list<ConnReminder*> connections; // To track active/received connections
  Lock *connLock;                      // To make the list thread-safe
  bool ValidConn(ConnReminder *conn);
};

typedef struct
{
  SynchList *list;
  int tid;
} SendWaitHandlerArgs;

#endif
