#include <iostream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <chrono>

using namespace std;

bool checkifredirect(string command);

string trim(string input)
{
    int i = 0;
    while (i < input.size() && input[i] == ' ')
        i++;
    if (i < input.size())
        input = input.substr(i);
    else
    {
        return "";
    }

    i = input.size() - 1;
    while (i >= 0 && input[i] == ' ')
        i--;
    if (i >= 0)
        input = input.substr(0, i + 1);
    else
        return "";

    return input;
}

vector<string> split(string line, string separator = " ")
{
    vector<string> result;
    while (line.size())
    {
        size_t found = line.find(separator);
        if (found == string::npos)
        {
            string lastpart = trim(line);
            if (lastpart.size() > 0)
            {
                result.push_back(lastpart);
            }
            break;
        }
        string segment = trim(line.substr(0, found));
        //cout << "line: " << line << "found: " << found << endl;
        line = line.substr(found + 1);

        //cout << "[" << segment << "]"<< endl;
        if (segment.size() != 0)
            result.push_back(segment);

        //cout << line << endl;
    }
    return result;
}

void printVector(vector<string> &input)
{
    //cerr << "Printing vector\n";
    for (int i = 0; i < input.size(); i++)
    {
        cerr << "[" << i << "] : " << input[i] << '\n';
    }
}

char **vec_to_char_array(vector<string> parts)
{
    char **result = new char *[parts.size() + 1]; // add 1 for the NULL at the end
    for (int i = 0; i < parts.size(); i++)
    {
        // allocate a big enough string
        result[i] = new char[parts[i].size() + 1]; // add 1 for the NULL byte
        strcpy(result[i], parts[i].c_str());
    }
    result[parts.size()] = NULL;
    return result;
}

string removeQuotes(string command)
{
    vector<string> noDQuotes = split(command, "\"");
    vector<string> noSQuotes = split(command, "\'");

    string quoteRemoved;

    if (noDQuotes[0].size() > noSQuotes.size())
    {
        quoteRemoved = noSQuotes[0];
    }
    else
    {
        quoteRemoved = noDQuotes[0];
    }

    return quoteRemoved;
}

vector<string> parseQuotes(string& command){
    bool singleQuote = false;
    bool doubleQuote = false;
    int doubleIndex = -1;
    int singleIndex = -1;
    int alreadyErased = 0;
    vector<string> quotes;
    string commandCopy = command;

    //cerr << "Length: " << command.length() << endl;
    for(int i = 0; i < command.length(); i++){
        //cerr << "Parsing: " << i << " " << command[i] << endl;
        if(command[i] == '\"'){
            //cerr << "Caught: " << command.substr(i-1, 2) << endl;
            if(doubleQuote){
                //Store prev to now in new var and push back
                int difference = (i-1) - (doubleIndex + 1) + 1;
                int edifference = difference + 2;
                string quote = command.substr(doubleIndex+1, difference);
                //cerr << "Quote: " << quote << endl;
                quotes.push_back(quote);

                //Erase from string - important to keep track of index after erasing
                int adjustedIndex = doubleIndex - alreadyErased;
                
                //cerr << "doubleIndex: " << doubleIndex << " adjustedIndex: " << adjustedIndex << endl;
                commandCopy.erase(doubleIndex - alreadyErased ,edifference);

                //Replace with tilde
                commandCopy.insert(adjustedIndex, "`");

                //One erase One insert
                alreadyErased += edifference;
                alreadyErased -= 1;
                //cerr << "alreadyErased: " << alreadyErased << endl;

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

                //Erase from string - important to keep track of index after erasing
                int adjustedIndex = singleIndex - alreadyErased;

                //cerr << "singleIndex: " << singleIndex << " adjustedIndex: " << adjustedIndex << endl;
                commandCopy.erase(singleIndex -  alreadyErased ,edifference);            

                //Replace with tilde
                commandCopy.insert(adjustedIndex, "`");
                
                //One erase One insert
                alreadyErased += edifference;   
                alreadyErased -= 1;
                //cerr << "alreadyErased: " << alreadyErased << endl; 

                singleQuote = false;
            }else{
                singleQuote = true;
                singleIndex = i;
            }
        }
    }

    //cerr << "All done\n";
    command = commandCopy;
    
    return quotes;
}


void putBackQuotes(vector<string>& args, vector<string> quotes){
    int replaced = 0; 
    for(int i = 0; i < args.size(); i++){
        if(args[i] == "`"){
            args[i] = quotes[replaced];
            replaced++;
        }
    }
}



//Redo by replacing strings with tilde character
//Store in vector of strings
//if in io redirect, use last element of vector for filename and then remove from vector
//Replace the tilde character with quoted elements in row before executing

void execute(string command)
{
    bool argInQuotes = false;
    string inQuotes = "";

    //Removing ampersand for background
    command = split(command,"&")[0];

    vector<string> quotes = parseQuotes(command);

    vector<string> argstrings = split(command, " "); // split the command into space-separated parts
    if (checkifredirect(command))
    {
        cerr << "Redirecting: " << command << endl;

        //Removing arrow
        vector<string> input = split(command, "<");
        vector<string> output = split(command, ">");

        /* 
        cerr << "Printing input vector\n";
        printVector(input);*/

        if (command.find("<") != string::npos)
        {
            cerr << "in input redirection loop\n";
            cerr << "Printing input vector\n";
            printVector(input);

            string fileName;

            if(input[1] == "`"){
                fileName = quotes.back();
                quotes.pop_back();
            }else{
                fileName = input[1];
            }

            cerr << "File: " << fileName << endl;

            int fd = open(fileName.c_str(), O_RDONLY);
            dup2(fd, 0);
            vector<string> inputArgs = split(input[0]);

            putBackQuotes(inputArgs, quotes);

            cerr << "Printing exec vector\n";
            printVector(inputArgs);

            char **args = vec_to_char_array(inputArgs);
            execvp(args[0], args);      
        }
        else if (command.find(">") != string::npos)
        {
            cerr << "in output redirection loop\n";
            cerr << "Printing output vector\n";
            printVector(output);

            string fileName;

            if(output[1] == "`"){
                fileName = quotes.back();
                quotes.pop_back();
            }else{
                fileName = output[1];
            }

            cerr << "File: " << fileName << endl;

            int fd = open(fileName.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(fd, 1);
            vector<string> outputArgs = split(output[0]);

            putBackQuotes(outputArgs, quotes);

            cerr << "Printing exec vector\n";
            printVector(outputArgs);

            char **args = vec_to_char_array(outputArgs);
            execvp(args[0], args);
        }
        else
        {
            cerr << "Redirect command doesnt exist? - BUG\n";
        }
    }
    else
    {
        putBackQuotes(argstrings, quotes);
        cerr << "Executing without redirect\n";
        printVector(argstrings);

        char **args = vec_to_char_array(argstrings); // convert vec<string> into an array of char*
        execvp(args[0], args);
    }
}

struct functionType
{
    bool background;
    bool custom;

    functionType(bool _background = false, bool _custom = false)
    {
        background = _background;
        custom = _custom;
    }
};

functionType classify(string command)
{

    //cerr << "Classifying: " << command << endl;
    functionType type;
    vector<string> argstrings = split(command, " ");

    /* 
    cerr << "last element: " << argstrings[argstrings.size()] << endl;
    printVector(argstrings);
    */

    if (argstrings[argstrings.size()-1] == "&")
    {
        type.background = true;
    }
    if (argstrings[0] == "cd")
    {
        type.custom = true;
    }

    return type;
}

bool checkifredirect(string command)
{
    vector<string> argstrings = split(command, " ");
    for (int i = 0; i < argstrings.size(); i++)
    {
        if (argstrings[i] == ">" || argstrings[i] == "<")
        {
            return true;
        }
    }
    return false;
}

void getPrompt()
{
    char buf[FILENAME_MAX];

    getcwd(buf, FILENAME_MAX);
    string cwd(buf);

    getlogin_r(buf, FILENAME_MAX);
    string user(buf);

    auto start = std::chrono::system_clock::now();
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    time_t end_time = std::chrono::system_clock::to_time_t(end);

    string ctimeString = ctime(&end_time);
    int length = ctimeString.size() - 1;
    ctimeString[length] = '\0';

    cerr << user << " | " << cwd << " | " << ctimeString << '>';
}

string getCWD(){
    char buf[FILENAME_MAX];
    getcwd(buf, FILENAME_MAX);
    string cwd(buf);
    return buf;
}

int main()
{

    int stdin_backup = dup(0);
    int stdout_backup = dup(1);

    dup2(stdin_backup, 0);
    dup2(stdout_backup, 1);

    while (true)
    { // repeat this loop until the user presses Ctrl + C

        string commandline = ""; /*get from STDIN, e.g., "ls  -la |   grep Jul  | grep . | grep .cpp" */
        string prevCWD = getCWD();

        getPrompt();

        while (getline(cin, commandline))
        {
            bool background = false;
            bool custom = false;

            // split the command by the "|", which tells you the pipe levels
            vector<string> tparts = split(commandline, "|");

            printVector(tparts);

            //Parse for custom command
            //Parse for bg here
            //Only check for background and custom commands if this is the case
            if (tparts.size() == 1)
            {
                cerr << "In classify section\n";
                functionType type = classify(tparts[0]);
                if (type.background == true)
                {
                    background = true;
                }
                if (type.custom == true)
                {
                    custom = true;
                }
            }

            if (custom)
            {
                vector<string> argstrings = split(tparts[0]);
                if (argstrings[0] == "cd")
                {
                    if(argstrings[1] != "-"){
                        prevCWD = getCWD();
                        chdir(argstrings[1].c_str());
                    }
                    else{
                        //cerr << "Changing to prev directory: " << prevCWD << endl;
                        chdir(prevCWD.c_str());
                    }
                }
                else if (argstrings[0] == "echo")
                {
                    vector<string> printStatement = split(commandline, "\"");
                    cerr << printStatement[1] << endl;
                }
                else if (argstrings[0] == "pwd")
                {
                    char buf[FILENAME_MAX];
                    getcwd(buf, FILENAME_MAX);
                    cerr << string(buf) << endl;
                }
                else
                {
                    cerr << "Custom command doesnt exist? - BUG\n";
                }
            }
            else
            {

                // for each pipe, do the following:
                for (int i = 0; i < tparts.size(); i++)
                {
                    // make pipe
                    int fd[2];
                    pipe(fd);
                    if (!fork())
                    {
                        // redirect output to the next level
                        // unless this is the last level
                        if (i < tparts.size() - 1)
                        {
                            // redirect STDOUT to fd[1], so that it can write to the other side
                            dup2(fd[1], 1);
                            close(fd[1]); // STDOUT already points fd[1], which can be closed
                        }
                        //execute function that can split the command by spaces to
                        // find out all the arguments, see the definition
                        execute(tparts[i]); // this is where you execute
                    }
                    else
                    {
                        if(!background){
                            cerr << "Waiting for child\n";
                            wait(0); // wait for the child process
                        }
                        else{
                            waitpid(-1, NULL, WNOHANG);
                        }

                        // then do other redirects
                        dup2(fd[0], 0);
                        close(fd[1]);
                    }
                }
            }

            dup2(stdin_backup, 0);
            dup2(stdout_backup, 1);

            getPrompt();
        }
    }
}