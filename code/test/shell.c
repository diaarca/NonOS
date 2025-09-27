#include "syscall.h"

#define NB_ELEMENT 50

int strCmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int main() {
    int newProc;
    char line[200];
    int i, j, nbWord, nbTotalWords, fd, value, isQuotes, size;
    char commandLine[3][NB_ELEMENT + 3]; // For the '\0'
    int sizeWords[3];
    int hasQuotes[3];
    char *buffer;

    PutString("Starting the shell !\n", 25);

    while (1) {
        PutString("->", 2);
        GetString(line, 200);

        i = 0;
        nbWord = 0;

        for (i = 0; i < 3; i++) {
            sizeWords[i] = 0;
            hasQuotes[i] = 0;
        }

        isQuotes = 0;
        for (i = 0; line[i] != '\n' && nbWord < 3 && i < 200; i++) {
            if (line[i] == ' ' && !isQuotes) {
                nbWord++;
            } else if (line[i] == '"') {
                isQuotes = !isQuotes;
            }

            if (isQuotes) {

                if (sizeWords[nbWord] < NB_ELEMENT + 3) {
                    sizeWords[nbWord]++;
                }
            } else {

                if (sizeWords[nbWord] < NB_ELEMENT + 1) {
                    sizeWords[nbWord]++;
                }
            }
        }

        if (i > 0 && !isQuotes) {
            nbWord++;

            nbTotalWords = nbWord;

            j = 0;
            nbWord = 0;
            isQuotes = 0;
            for (i = 0; line[i] != '\n' && nbWord < nbTotalWords && i < 200; i++) {
                if (line[i] == ' ' && !isQuotes) {
                    commandLine[nbWord][j] = '\0';
                    j = 0;
                    nbWord++;
                } else if (line[i] == '"') {
                    commandLine[nbWord][j] = line[i];
                    hasQuotes[nbWord] = 1;
                    j++;
                    isQuotes = !isQuotes;
                } else {
                    if ((j < NB_ELEMENT && !isQuotes) ||
                        (j < NB_ELEMENT + 1 &&
                         isQuotes)) { // 50-3 for trailing \0 and if we need to add "
                        commandLine[nbWord][j] = line[i];
                        j++;
                    }
                }
            }

            if (nbWord < 3) {
                commandLine[nbWord][j] = '\0';
            } else if (nbWord >= 3 && line[i] == '\n') {
                nbWord++;
            }

            if (strCmp(*commandLine, "quit") == 0) {
                break;
            } else if (strCmp(*commandLine, "ls") == 0) {
                if (nbTotalWords != 1) {
                    PutString("Not enough arguments for ls\n", 50);
                    PutString("ls\n", 50);
                }
                Listfiles();
            } else if (strCmp(*commandLine, "rm") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for rm\n", 50);
                    PutString("rm <file>\n", 50);
                }
                if (!Remove(commandLine[1])) {
                    PutString("rm didn't work\n", 50);
                }
            } else if (strCmp(*commandLine, "mkdir") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for mkdir\n", 50);
                    PutString("mkdir <directory>\n", 50);
                }
                if (!Mkdir(commandLine[1])) {
                    PutString("mkdir didn't work\n", 50);
                }
            } else if (strCmp(*commandLine, "rmdir") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for rmdir\n", 50);
                    PutString("rmdir <directory>\n", 50);
                }
                if (!Rmdir(commandLine[1])) {
                    PutString("rmdir didn't work\n", 50);
                }
            } else if (strCmp(*commandLine, "cd") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for cd\n", 50);
                    PutString("cd <path>\n", 50);
                }
                if (!Changedir(commandLine[1])) {
                    PutString("cd didn't work\n", 50);
                }
            } else if (strCmp(*commandLine, "touch") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for touch\n", 50);
                    PutString("touch <name>\n", 50);
                }
                if (!Create(commandLine[1])) {
                    PutString("touch didn't work\n", 50);
                }
            } else if (strCmp(*commandLine, "cat") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for cat\n", 50);
                    PutString("cat <file>\n", 50);
                }
                if ((fd = Open(commandLine[1])) == -1) {
                    PutString("The file ", 20);
                    PutString(commandLine[1], sizeWords[1]);
                    PutString(" can't be opened\n", 20);
                } else {

                    while ((value = Read(line, 100, fd)) > 0) {
                        PutString(line, value);
                    }
                    PutChar('\n');

                    Close(fd);
                }
            } else if (strCmp(*commandLine, "echo") == 0) {
                if (nbTotalWords != 3) {
                    PutString("Not enough arguments for echo\n", 50);
                    PutString("echo <text> <file>\n", 50);
                }
                if ((fd = Open(commandLine[2])) == -1) {
                    PutString("The file ", 20);
                    PutString(commandLine[2], sizeWords[2]);
                    PutString(" can't be opened\n", 20);
                } else {
                    if (hasQuotes[1]) {
                        buffer = commandLine[1] + 1;
                        size = sizeWords[1] - 3;
                    } else {
                        buffer = commandLine[1];
                        size = sizeWords[1] - 1;
                    }
                    Write(buffer, size, fd);
                    Close(fd);
                }
            } else if (strCmp(*commandLine, "run") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for run\n", 50);
                    PutString("run <executable>\n", 50);
                }
                newProc = ForkExec(commandLine[1]);
                ProcessJoin(newProc);
            } else if (strCmp(*commandLine, "get") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for run\n", 50);
                    PutString("get <file>\n", 50);
                }

                if (!ReceiveFile(0, commandLine[1])) {
                    PutString("The file has not been received\n", 50);
                } else {
                    PutString("The file has been received\n", 50);
                }
            } else if (strCmp(*commandLine, "send") == 0) {
                if (nbTotalWords != 2) {
                    PutString("Not enough arguments for run\n", 50);
                    PutString("send <file>\n", 50);
                }

                if (!SendFile(0, commandLine[1])) {
                    PutString("The file has not been sent\n", 50);
                } else {
                    PutString("The file has been sent\n", 50);
                }
            } else if (strCmp(*commandLine, "help") == 0) {
                PutString("Available commands: \n", 20);
                    PutString("ls - List all files in the current directory\n", 100);
                    PutString("rm <file> - Remove the file <file> in the current directory\n", 100);
                    PutString("rmdir <directory> - Remove an empty directory <directory> in the current directory\n", 100);
                    PutString("cd <path> - Change the current directory to <path>\n", 100);
                    PutString("touch <name> - Create a new file of name <name>\n", 100);
                    PutString("cat <file> - Display the content of the file <file>\n", 100);
                    PutString("echo <text> <file> - Write <text> into <file>\n", 100);
                    PutString("run <executable> - Run the executable <executable>\n", 100);
                    PutString("get <file> - Get <file> from the server FTP\n", 100);
                    PutString("send <file> - Send <file> to the server FTP\n", 100);
                    PutString("quit - Quit the shell\n", 50);

            } else {
                PutString("Command not found !\n", 25);
            }
        }
    }
}
