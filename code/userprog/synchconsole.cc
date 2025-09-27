
#include "synchconsole.h"
#include "console.h"
#include "copyright.h"
#include "synch.h"
#include "system.h"

static Semaphore *readAvail;
static Semaphore *writeDone;
static Semaphore *semThreads;
static void ReadAvail (int arg) { readAvail->V (); }
static void WriteDone (int arg) { writeDone->V (); }

SynchConsole::SynchConsole (char *readFile, char *writeFile)
{
    readAvail = new Semaphore ("read avail", 0);
    writeDone = new Semaphore ("write done", 0);
    console = new Console (readFile, writeFile, ReadAvail, WriteDone, 0);
    semThreads = new Semaphore ("sem threads", 1);
}

SynchConsole::~SynchConsole ()
{
    delete console;
    delete writeDone;
    delete readAvail;
}

void SynchConsole::doSynchPutChar (const char ch)
{
    console->PutChar (ch);
    writeDone->P ();
}

void SynchConsole::SynchPutChar (const char ch)
{
    semThreads->P ();
    doSynchPutChar (ch);
    semThreads->V ();
}

char SynchConsole::doSynchGetChar ()
{
    readAvail->P ();
    return console->GetChar ();
}

char SynchConsole::SynchGetChar ()
{
    semThreads->P ();
    char c = doSynchGetChar ();
    semThreads->V ();
    return c;
}

void SynchConsole::SynchPutString (const char s[])
{
    int i = 0;
    semThreads->P ();
    while (s[i] != '\0')
    {
        this->doSynchPutChar (s[i]);
        i++;
    }
    semThreads->V ();
    return;
}

void SynchConsole::SynchGetString (char *s, int n)
{
    int i = 0;
    char c;
    semThreads->P ();
    while (i < n && (c = this->doSynchGetChar ()) != EOF)
    {
        s[i] = c;
        i++;
        if (c == '\n')
        {
            break;
        }
    }
    semThreads->V ();
    s[i] = '\0';
}
