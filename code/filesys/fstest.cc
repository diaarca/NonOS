// fstest.cc
//	Simple test routines for the file system.
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "filesys.h"
#include "stats.h"
#include "system.h"
#include "thread.h"
#include "userthread.h"
#include "utility.h"

#define TransferSize 100 // make it small, just to be difficult

extern void StartProcess(char *file);

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void Copy(const char *from, const char *to) {
    FILE *fp;
    int openFile;
    int amountRead, fileLength;
    char *buffer;

    // Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {
        printf("Copy: couldn't open input file %s\n", from);
        return;
    }

    // Figure out length of UNIX file
    fseek(fp, 0, 2);
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

    // Create a Nachos file of the same length
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, 0)) { // Create Nachos file
        printf("Copy: couldn't create output file %s\n", to);
        fclose(fp);
        return;
    }

    openFile = fileSystem->OpenUser(to);
    ASSERT(openFile != -1);

    // Copy the data in TransferSize chunks
    buffer = new char[TransferSize];
    while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0)
        fileSystem->WriteUser(buffer, amountRead, openFile);
    delete[] buffer;

    // Close the UNIX and the Nachos files
    fileSystem->CloseUser(openFile);
    fclose(fp);
    
    // Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {
        printf("Copy: couldn't open input file %s\n", from);
        return;
    }

    // Figure out length of UNIX file
    fseek(fp, 0, 2);
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

    openFile = fileSystem->OpenUser(to);
    ASSERT(openFile != -1);

    // Copy the data in TransferSize chunks
    char *buffer1 = new char[TransferSize];
    char *buffer2 = new char[TransferSize];
    while ((amountRead = fread(buffer1, sizeof(char), TransferSize, fp)) > 0) {
        fileSystem->ReadUser(buffer2, amountRead, openFile);
    }
    delete[] buffer1;
    delete[] buffer2;

    // Close the UNIX and the Nachos files
    fileSystem->CloseUser(openFile);
    fclose(fp);
}

// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void Print(char *name) {
    OpenFile *openFile;
    int i, amountRead;
    char *buffer;

    if ((openFile = fileSystem->Open(name)) == NULL) {
        printf("Print: unable to open file %s\n", name);
        return;
    }

    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0)
        for (i = 0; i < amountRead; i++)
            printf("%c", buffer[i]);
    delete[] buffer;

    delete openFile; // close the Nachos file
    return;
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName "TestFile"
#define Contents "1234567890"
#define ContentSize strlen(Contents)
#define FileSize ((int)(ContentSize * 5000))

static void FileWrite() {
    int openFile;
    int i, numBytes;

    printf("Sequential write of %d byte file, in %zd byte chunks\n", FileSize, ContentSize);
    if (!fileSystem->Create(FileName, 0)) {
        printf("Perf test: can't create %s\n", FileName);
        return;
    }
    openFile = fileSystem->OpenUser(FileName);
    if (openFile == -1) {
        printf("Perf test: unable to open %s\n", FileName);
        return;
    }

    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = fileSystem->WriteUser(Contents, ContentSize, openFile);
        if (numBytes < 10) {
            printf("Perf test: unable to write %s\n", FileName);
            fileSystem->CloseUser(openFile);
            return;
        }
    }
    fileSystem->CloseUser(openFile); // close file
}

static void FileRead() {
    int openFile;
    char *buffer = new char[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %zd byte chunks\n", FileSize, ContentSize);

    if ((openFile = fileSystem->OpenUser(FileName)) == -1) {
        printf("Perf test: unable to open file %s\n", FileName);
        delete[] buffer;
        return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = fileSystem->ReadUser(buffer, ContentSize, openFile);
        if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
            printf("Perf test: unable to read %s, %d bytes read\n", FileName, numBytes);
            fileSystem->CloseUser(openFile);
            delete[] buffer;
            return;
        }
    }
    delete[] buffer;
    fileSystem->CloseUser(openFile); // close file
}

void PerformanceTest() {
    printf("Starting file system performance test:\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FileName)) {
        printf("Perf test: unable to remove %s\n", FileName);
        return;
    }
    stats->Print();
}

void FileSystemTest() {
    int nbWord;
    int i, j;
    int fd, value;
    char **commandLine;
    char line[100];
    char buffer[100];
    int newProcessId;

    synchconsole = new SynchConsole(NULL, NULL);

    while (1) {
        synchconsole->SynchPutChar('>');
        synchconsole->SynchGetString(line, 100);

        nbWord = 0;
        for (i = 0; line[i] != '\0'; i++) {
            if (line[i] == ' ') {
                nbWord++;
            }
        }

        commandLine = new char *[nbWord];

        if (i == 0) {
            // Break nothing
        } else {
            nbWord++;

            char *word = strtok(line, " \n");
            j = 0;
            while (word != NULL) {
                DEBUG('f', "The word %s has been cut\n", word);
                commandLine[j] = word;
                word = strtok(NULL, " \n");
                j++;
            }

            DEBUG('f', "Number of words: %d\n", nbWord);
            DEBUG('f', "command: ");
            for (int l = 0; l < nbWord; l++) {
                DEBUG('f', "'%s'", commandLine[l]);
            }
            DEBUG('f', "\n");

            if (!strcmp(*commandLine, "ls")) {
                fileSystem->List();
            } else if (!strcmp(*commandLine, "cp")) {
                ASSERT(nbWord > 2);
                Copy(commandLine[1], commandLine[2]);
            } else if (!strcmp(*commandLine, "rm")) {
                ASSERT(nbWord > 1);
                fileSystem->Remove(commandLine[1]);
            } else if (!strcmp(*commandLine, "mkdir")) {
                ASSERT(nbWord > 1);
                fileSystem->CreateDir(commandLine[1]);
            } else if (!strcmp(*commandLine, "rmdir")) {
                ASSERT(nbWord > 1);
                fileSystem->RemoveDir(commandLine[1]);
            } else if (!strcmp(*commandLine, "cd")) {
                ASSERT(nbWord > 1);
                fileSystem->ChangeDir(commandLine[1]);
            } else if (!strcmp(*commandLine, "p")) {
                fileSystem->Print();
            } else if (!strcmp(*commandLine, "touch")) {
                ASSERT(nbWord > 1);
                fileSystem->Create(commandLine[1], 0);
            } else if (!strcmp(*commandLine, "cat")) {
                ASSERT(nbWord > 1);
                if ((fd = fileSystem->OpenUser(commandLine[1])) == -1) {
                    printf("The file Test can't be opened\n");
                    break;
                }

                while ((value = fileSystem->ReadUser(buffer, 100, fd)) > 0) {
                    buffer[value] = '\0';
                    synchconsole->SynchPutString(buffer);
                }
                synchconsole->SynchPutChar('\n');
                fileSystem->CloseUser(fd);
            } else if (!strcmp(*commandLine, "echo")) {
                ASSERT(nbWord > 2);
                if ((fd = fileSystem->OpenUser(commandLine[2])) == -1) {
                    printf("The file Test can't be opened\n");
                    break;
                }

                fileSystem->WriteUser(commandLine[1], strlen(commandLine[1]), fd);
                fileSystem->CloseUser(fd);
            } else if (!strcmp(*commandLine, "test")) {
                if (fileSystem->Create("Test", 0) == FALSE) {
                    printf("The file Test can't be opened\n");
                    break;
                }

                if ((fd = fileSystem->OpenUser("Test")) == -1) {
                    printf("The file Test can't be opened\n");
                    break;
                }

                value = fileSystem->WriteUser("Hello, World!", 14, fd);
                if (value < 0) {
                    printf("The write failed\n");
                    break;
                }

                value = fileSystem->SeekUser(fd, 0);

                value = fileSystem->ReadUser(buffer, 14, fd);
                if (value < 0) {
                    printf("The read failed\n");
                    break;
                }

                printf("'%s'\n", buffer);

                fileSystem->CloseUser(fd);
            } else if (!strcmp(*commandLine, "run")) {
                ASSERT(nbWord > 1);
                newProcessId = do_ForkExec(commandLine[1]);
                
                do_ProcessJoin(newProcessId);
            } else if (!strcmp(*commandLine, "quit")) {
                printf("End\n");
                return;
            }
        }
    }
}
