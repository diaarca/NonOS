// nettest.cc
//	Test out message delivery between two "Nachos" machines,
//	using the Post Office to coordinate delivery.
//
//	Two caveats:
//	  1. Two copies of Nachos must be running, with machine ID's 0 and 1:
//		./nachos -m 0 -o 1 &
//		./nachos -m 1 -o 0 &
//
//	  2. You need an implementation of condition variables,
//	     which is *not* provided as part of the baseline threads
//	     implementation.  The Post Office won't work without
//	     a correct implementation of condition variables.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "interrupt.h"
#include "limits.h"
#include "network.h"
#include "post.h"
#include "system.h"
#include "ftp.h"
#include <cstdlib>

// Test out message delivery, by doing the following:
//	1. send a message to the machine with ID "farAddr", at mail box #0
//	2. wait for the other machine's message to arrive (in our mailbox #0)
//	3. send an acknowledgment for the other machine's message
//	4. wait for an acknowledgement from the other machine to our
//	    original message

void MailTest(int farAddr)
{
    Payload *plOut = new Payload();
    Payload *plIn = new Payload();
    // PacketHeader outPktHdr, inPktHdr;
    // MailHeader outMailHdr, inMailHdr;
    const char *data =
        "Hello there! This is Alexis and I am trying to send a very long message in order to check "
        "whether it gets split correctly or not. I would really like to know because messages can "
        "get extremely long, since there is no more message size limit now.";
    const char *ack = "Got it!";
    char buffer[strlen(data) + 1];
    // construct packet, mail header for original message
    // To: destination machine, mailbox 0
    // From: our machine, reply to: mailbox 1

    for (int i = 0; i < 3; i++)
    {
        // outPktHdr.to = farAddr;
        // outMailHdr.to = 0;
        // outMailHdr.from = 0;
        // outMailHdr.length = strlen(data) + 1;
        // Send the first message

        // machine 0
        if (farAddr == 1)
        {
            plOut->UpdatePayload(1 - farAddr, farAddr, 0, 0, strlen(data) + 1);
            printf("=====================START SEND=================\n");
            postOffice->SendPayload(plOut, data);
            printf("=====================END SEND=================\n");
            printf("[Machine %d] Sent \"%s\" to machine %d, box %d\n", plOut->pktHdr.from, data,
                   plOut->pktHdr.to, plOut->mailHdr.to);

            printf("=====================START RECEIVE=================\n");
            postOffice->ReceivePayload(plIn, 0, buffer);
            printf("[Machine %d] Got \"%s\" from machine %d, box %d\n", plIn->pktHdr.to, buffer,
                   plIn->pktHdr.from, plIn->mailHdr.from);

            fflush(stdout);
            printf("=====================END RECEIVE=================\n");
        }
        // machine 1
        else
        {

            printf("=====================START RECEIVE=================\n");
            postOffice->ReceivePayload(plIn, 0, buffer);
            printf("[Machine %d] Got \"%s\" from machine %d, box %d\n", plIn->pktHdr.to, buffer,
                   plIn->pktHdr.from, plIn->mailHdr.from);
            fflush(stdout);
            printf("=====================END RECEIVE=================\n");

            plOut->UpdatePayload(plIn->pktHdr.to, plIn->pktHdr.from, plIn->mailHdr.to,
                                 plIn->mailHdr.from, strlen(ack) + 1);
            // outPktHdr.to = inPktHdr.from;
            // outMailHdr.to = inMailHdr.from;
            // outMailHdr.length = strlen(ack) + 1;
            printf("=====================START SEND=================\n");
            postOffice->SendPayload(plOut, ack);
            printf("[Machine %d] Sent \"%s\" to machine %d, box %d\n", plOut->pktHdr.from, ack,
                   plOut->pktHdr.to, plOut->mailHdr.to);
            // Wait for the ack from the other machine to the first message we sent.
            printf("=====================END SEND=================\n");
        }

        // Wait for the first message from the other machine

        // Send acknowledgement to the other machine (using "reply to" mailbox
        // in the message that just arrived
    }
    postOffice->DisconnectPayload(plIn);
    delete plIn;
    delete plOut;
    // Then we're done!
    interrupt->Halt();
}

void RingTest(int farAddr)
{
    Payload *plOut = new Payload();
    Payload *plIn = new Payload();
    const char *data =
        "Hello there! This is Alexis and I am trying to send a very long message in order to check "
        "whether it gets split correctly or not. I would really like to know because messages can "
        "get extremely long, since there is no more message size limit now.";
    char buffer[strlen(data) + 1];
    // construct packet, mail header for original message
    // To: destination machine, mailbox 0
    // From: our machine, reply to: mailbox 1
    //
    if (postOffice->GetNetAddr() == 0)
    {
        char oldBuffer[strlen(data) + 1];
        strncpy(oldBuffer, data, strlen(data) + 1);
        plOut->UpdatePayload(0, farAddr, 0, 0, strlen(data) + 1);

        // Send the first message
        postOffice->SendPayload(plOut, data);

        printf("[Machine %d] Sent \"%s\" to machine %d, box %d\n", plOut->pktHdr.from, data,
               plOut->pktHdr.to, plOut->mailHdr.to);
        postOffice->ReceivePayload(plIn, 0, buffer);

        printf("[Machine %d] End of the ring, received \"%s\" from machine %d, box %d\n",
               plIn->pktHdr.to, buffer, plIn->pktHdr.from, plIn->mailHdr.from);
        ASSERT(!strcmp(oldBuffer, buffer));
    }
    else
    {
        postOffice->ReceivePayload(plIn, 0, buffer);
        printf("[Machine %d] Received \"%s\" from machine %d\n", plIn->pktHdr.to, buffer,
               plIn->pktHdr.from);
        plOut->UpdatePayload(postOffice->GetNetAddr(), farAddr, 0, 0, strlen(data) + 1);
        printf("[Machine %d] Sent \"%s\" to machine %d\n", plOut->pktHdr.from, buffer,
               plOut->pktHdr.to);
        postOffice->SendPayload(plOut, buffer);
    }
    delete plIn;
    delete plOut;
    interrupt->Halt();
}

void ConnTest(int farAddr)
{
    Connection *c = NULL;
    const char *data =
        "Hello there! This is Thomas and I am trying to send a very long message in order to check "
        "whether it gets split correctly or not. I would really like to know because messages can "
        "get extremely long, since there is no more message size limit now.";
    const char *ack = "Got it!";
    char buffer[strlen(data) + 1];
    for (int i = 0; i < 3; i++)
    {
        if (farAddr == 1)
        {
            c = postOffice->Connect(farAddr);
            printf("=====================START SEND=================\n");
            postOffice->Send(c, data, strlen(data) + 1);
            printf("=====================END SEND=================\n");
            printf("[Machine %d] Sent \"%s\" to machine %d, box %d\n", c->pOut->pktHdr.from, data,
                   c->pOut->pktHdr.to, c->pOut->mailHdr.to);

            printf("=====================START RECEIVE=================\n");
            postOffice->Receive(c, buffer);
            printf("[Machine %d] Got \"%s\" from machine %d, box %d\n", c->pIn->pktHdr.to, buffer,
                   c->pIn->pktHdr.from, c->pIn->mailHdr.from);

            fflush(stdout);
            printf("=====================END RECEIVE=================\n");
        }
        else
        {
            c = postOffice->Listen();
            printf("=====================START RECEIVE=================\n");
            postOffice->Receive(c, buffer);
            printf("[Machine %d] Got \"%s\" from machine %d, box %d\n", c->pIn->pktHdr.to, buffer,
                   c->pIn->pktHdr.from, c->pIn->mailHdr.from);
            fflush(stdout);
            printf("=====================END RECEIVE=================\n");
            printf("=====================START SEND=================\n");
            postOffice->Send(c, ack, strlen(ack) + 1);
            printf("[Machine %d] Sent \"%s\" to machine %d, box %d\n", c->pOut->pktHdr.from, ack,
                   c->pOut->pktHdr.to, c->pOut->mailHdr.to);
            // Wait for the ack from the other machine to the first message we sent.
            printf("=====================END SEND=================\n");
        }
        postOffice->Disconnect(c);
    }

    interrupt->Halt();
}

// First argument : address of the server machine
// Second argument : r - read a file from the server
//                   w - write a file to the server
// Third argument : name of the file
extern void FTPTestClient(int servAddr, char readwrite, char *fileName) {
    Client *client = new Client();
    client->Connect(servAddr);
    if (readwrite == 'r') {
        client->SendFileRequest(READFILE, fileName);
    } else if (readwrite == 'w') {
        client->SendFileRequest(WRITEFILE, fileName);
    }
    fileSystem->PrintDirectory();
    client->Disconnect();
    interrupt->Halt();
}

// Server routine that loops endlessly to accept new connections
extern void FTPTestServer() {
    Server *server = new Server();
    server->ServerRoutine();
}
