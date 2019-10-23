#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

vector<string> parseQuotes(string& command){
    bool singleQuote = false;
    bool doubleQuote = false;
    int doubleIndex = -1;
    int singleIndex = -1;
    vector<string> quotes;
    string commandCopy = command;

    cerr << "Length: " << command.length() << endl;
    for(int i = 0; i < command.length(); i++){
        cerr << "Parsing: " << i << " " << command[i] << endl;
        if(command[i] == '\"'){
            //cerr << "Caught: " << command.substr(i-1, 2) << endl;
            if(doubleQuote){
                //Store prev to now in new var and push back
                int difference = (i-1) - (doubleIndex + 1) + 1;
                int edifference = difference + 2;
                string quote = command.substr(doubleIndex+1, difference);
                //cerr << "Quote: " << quote << endl;
                quotes.push_back(quote);

                //Erase from string
                commandCopy.erase(doubleIndex,edifference);

                //Replace with tilde
                commandCopy.insert(doubleIndex, "`");

                doubleQuote = false;
            }else{
                doubleQuote = true;
                doubleIndex = i;
            }
        }
        else if(command[i] == '\''){
            //cerr << "Caught: " << command.substr(i-1, 2) << endl;
            if(singleQuote){
                //Store prev to now in new var and push back
                int difference = (i-1) - (singleIndex + 1) + 1;
                int edifference = difference + 2;
                string quote = command.substr(singleIndex+1, difference);
                //cerr << "Quote: " << quote << endl;
                quotes.push_back(quote);

                //Erase from string
                commandCopy.erase(singleIndex,edifference);

                //Replace with tilde
                commandCopy.insert(singleIndex, "`");

                singleQuote = false;
            }else{
                singleQuote = true;
                singleIndex = i;
            }
        }
    }

    command = commandCopy;
    
    return quotes;
}

void putBack(string& parsedCommand, vector<string> quotesRemoved){
    int replaced = 0;
    for(int i = 0; i < parsedCommand.size(); i++){
        if(parsedCommand[i] == '`'){
            parsedCommand.erase(i,1);
            parsedCommand.insert(i, quotesRemoved[replaced]);
            replaced++;
        }
    }
}

int main(){

    int stdin_backup = dup (0);
    int stdout_backup = dup (1);

    
    int fd = open("foo.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    dup2(fd, 1);
    if(fork() == 0){
        execlp("ls", "ls", "-l", "-a", NULL); //CHILD
    }else{
        wait(0); //PARENT
    }
    
    dup2(stdin_backup, 0);
    dup2(stdout_backup, 1);

    cerr << "Im here!\n";

    /* 
    char buf[10];
    int fds[2];
    pipe(fds);
    printf("Sending message: Hi\n");
    write(fds[1], "Hi", 3);
    read(fds[0], buf, 3);
    printf("Received message: %s\n", buf);

    if(!fork()){
        dup2(fds[1],1);
        execlp("ls", "ls", "-l", NULL);
    }else{
        dup2(fds[0],0);
        execlp("grep", "grep", "foo", NULL);
    }
    */

    string command;
    while(getline(cin, command)){
        vector<string> parsed = parseQuotes(command);
        cerr << "Parsed: " << command << endl;

        cerr << "Putting it back together" << endl;
        putBack(command, parsed);
        cerr << "Reset: " << command << endl;
    }


    return 0;
}