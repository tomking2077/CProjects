#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
using namespace std;

//Helper function for real time histogram
void updateCount();

/*Arguments for the worker threads */
class workerArguments
{
public:
    BoundedBuffer* requestBuffer;
    FIFORequestChannel* requestChannel;
    HistogramCollection* hc;
    int bufferCapacity;

    workerArguments(BoundedBuffer* _requestBuffer, FIFORequestChannel* _requestChannel, HistogramCollection* _hc, int _bufferCapacity = MAX_MESSAGE)
    {
        requestBuffer = _requestBuffer;
        requestChannel = _requestChannel;
        hc = _hc;
        bufferCapacity = _bufferCapacity;
    }
};

/*Arguments for the patient threads */
class patientArguments
{
public:
    /*put all the arguments for the patient threads */
    BoundedBuffer* requestBuffer;
    int patientNum;
    int reqCount;

    patientArguments(BoundedBuffer* _requestBuffer, int _patientNum, int _reqCount)
    {
        requestBuffer = _requestBuffer;
        patientNum = _patientNum;
        reqCount = _reqCount;
    }
};

/*Arguments for the file thread */
class fileArguments
{
public:
    BoundedBuffer* requestBuffer;
    string fileName;
    __int64_t fileSize;
    int bufferCapacity;

    fileArguments(BoundedBuffer* _requestBuffer, string _fileName, __int64_t _fileSize, int _bufferCapacity = MAX_MESSAGE)
    {
        requestBuffer = _requestBuffer;
        fileName = _fileName;
        fileSize = _fileSize;
        bufferCapacity = _bufferCapacity;
    }
};


void* patientFunction(void* arg)
{
    //printf("In patient thread\n");

    patientArguments *pArg = (patientArguments *)arg;
    int patientNumber = pArg->patientNum;
    BoundedBuffer *requestBuffer = pArg->requestBuffer;
    int count = pArg->reqCount;

    double t = 0;
    for (int i = 0; i < count; i++)
    {   
        //Using ecgno 1
        datamsg dmsg = datamsg(patientNumber, t, 1);
        //cout << "P: DATAMESSAGE " << dmsg.person << " " << dmsg.seconds << " " << dmsg.ecgno << "\n";

        requestBuffer->push((char *)&dmsg, sizeof(dmsg));
        t += 0.004;
    }
}

void* workerFunction(void* arg)
{
    //printf("In worker thread\n");

    workerArguments *wArg = (workerArguments*)arg;
    BoundedBuffer *requestBuffer = wArg->requestBuffer;
    int bufferCapacity = wArg->bufferCapacity;

    for (;;)
    {
        vector<char> dataReq = wArg->requestBuffer->pop();
        MESSAGE_TYPE *newmessage = (MESSAGE_TYPE *)dataReq.data();

        if (*newmessage == DATA_MSG)
        {
            datamsg dmsg = *(datamsg *)newmessage;
            //cout << "W: DATAMESSAGE " << dmsg.person << " " << dmsg.seconds << " " << dmsg.ecgno << "\n";
            wArg->requestChannel->cwrite((char *)&dmsg, sizeof(dmsg));

            int len;
            char *recvbuf = wArg->requestChannel->cread(&len);
            if (len == 0)
                cout << "DIDNT READ ANYTHING\n";
            else
            {
                double d = *(double *)recvbuf;
                wArg->hc->update(dmsg.person, d);
                //printf("Seconds: %f Value: %f\n",dmsg.seconds, d);
            }
        }
        else if (*newmessage == FILE_MSG)
        {
            //printf("In file write thread\n");
            char* fullBuffer = dataReq.data();
            filemsg *fmsg = (filemsg *)newmessage;
            string fileName = fullBuffer + sizeof(filemsg);
            string path = "received/" + fileName;
            //cout << "W: FILEMESSAGE Offset: " << fmsg->offset << " Length: " << fmsg->length << " FileName: " << fileName << "\n";

            wArg->requestChannel->cwrite((char *)dataReq.data(), dataReq.size());
            int len = 0;
            char *recvbuf = wArg->requestChannel->cread(&len, bufferCapacity);

            if (len > 0)
            {
                //printf("cread len = %d\n", len);
                FILE *copy = fopen(path.c_str(), "rb+");
                if (copy == NULL)
                {
                    printf("Couldn't open file on client side\n");
                    printf("Error %d\n", errno);
                    //exit(0);
                }
                fseek(copy, fmsg->offset, SEEK_SET);
                fwrite(recvbuf, sizeof(char), len, copy);
                fclose(copy);
                updateCount();
            }
            else
            {
                cout << "Didn't read anything\n";
            }

            //printf("Leaving file write thread\n");
        }
        else if (*newmessage == QUIT_MSG)
        {
            wArg->requestChannel->cwrite((char *)newmessage, sizeof(newmessage));
            delete wArg->requestChannel;
            break;
        }
    }
}

void* fileFunction(void* arg)
{
    //printf("In file thread\n");

    fileArguments *fArg = (fileArguments *)arg;
    __int64_t fileSize = fArg->fileSize;
    string fileName = fArg->fileName;
    int bufferCapacity = fArg->bufferCapacity;

    //Setting up buffer
    char fileBuffer[sizeof(filemsg) + sizeof(fileName)];
    memset(fileBuffer, 0, sizeof(fileBuffer));
    strcpy(fileBuffer + sizeof(filemsg), fileName.c_str());

    int offset = 0;
    int length = 0;
    int bytesRemaining = fileSize;

    while (bytesRemaining != 0)
    {
        length = min(bytesRemaining, bufferCapacity);
        filemsg fileReqSeg = filemsg(offset, length);
        memcpy(fileBuffer, &fileReqSeg, sizeof(fileReqSeg));

        //printf("F: FILEMESSAGE %d %d\n", offset, length);
        fArg->requestBuffer->push((char *)&fileBuffer, sizeof(fileBuffer));

        offset += length;
        bytesRemaining -= length;
    }
}

//globals for real time histogram/file transfer view
HistogramCollection hc;
int partsCopied;
int totalParts;
double percent;
bool fileMode;

void signalHandler(int signo){
    if(signo == SIGALRM){
        //printf("SIGALRM called\n");
        system("clear");
        if(fileMode){
            percent = (double)partsCopied/(double)totalParts;
            percent*=100;
            printf("%f% (%d / %d) copied\n", percent, partsCopied, totalParts);
        }
        else{
            hc.print();
        }
    }
}

void updateCount(){
    mutex mtx;
    unique_lock<mutex> lock(mtx);
    partsCopied+=1;
    lock.unlock();
}


int main(int argc, char *argv[])
{
    int n = 100;    //default number of requests per "patient"
    int p = 10;     // number of patients [1,15]
    int w = 100;    //default number of worker threads
    int b = 20; 	// default capacity of the request buffer
	int m = MAX_MESSAGE; 	// default capacity of the file buffer
    fileMode = false; //file read mode
    srand(time_t(NULL));
    string fileName = "not set";

    int opt = 0;
    while ((opt = getopt(argc, argv, "n:p:w:b:m:f:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            n = atoi(optarg);
            break;
        case 'p':
            p = atoi(optarg);
            break;
        case 'w':
            w = atoi(optarg);
            if (w > 5000){
                printf("Input w < 5000\n");
                exit(0);
            }
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 'm':
            m = atoi(optarg);
            break;
        case 'f':
            fileMode = true;
            fileName = optarg;
            break;
        default:
            printf("Program can be invoked with -n requests -p patients -w workers -b bounded buffer size -m file buffer size -f fileName \n");
            exit(0);
        }
    }
    
    printf("Parsed args n = %d, p = %d, w = %d, b = %d, m = %d, f = %d, FileName = %s\n", n, p, w, b, m, fileMode, fileName.c_str());
    
    int pid = fork();
    if (pid == 0){
		// modify this to pass along m
        string fileBufSize = to_string(m);
        execl ("dataserver", "dataserver", (char *)fileBufSize.c_str(), (char *)NULL);
        
    }

    signal(SIGALRM, signalHandler);
    percent = 0;
    partsCopied = 0;
    totalParts = 0;
    
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer requestBuffer(b);

    struct timeval start, end;

    vector<workerArguments> wArgs;
    printf("Populating worker args\n");
    for (int i = 0; i < w; i++)
    {
        MESSAGE_TYPE newmessage = NEWCHANNEL_MSG;
        chan->cwrite((char *)&newmessage, sizeof(newmessage));
        char *recvbuf;
        int len = 0;
        recvbuf = chan->cread(&len);
        string channelName = recvbuf;
        //cout << "ChannelName: " << channelName << endl;
        FIFORequestChannel *workerChannel = new FIFORequestChannel(channelName, FIFORequestChannel::CLIENT_SIDE);
        workerArguments wArg(&requestBuffer, workerChannel, &hc, m);
        wArgs.push_back(wArg);
    }

    vector<patientArguments> pArgs;
    vector<fileArguments> fArgs;

    if(fileMode){
        filemsg fileRequest = filemsg(0, 0);
        char fileBuffer[sizeof(fileRequest) + sizeof(fileName)];
        memset(fileBuffer, 0, sizeof(fileBuffer));
        memcpy(fileBuffer, &fileRequest, sizeof(fileRequest));
        strcpy(fileBuffer + sizeof(fileRequest), fileName.c_str());

        cout << "FileName: " << fileName << endl;

        int len = 0;
        chan->cwrite(fileBuffer, sizeof(fileBuffer));
        char *recvbuf = chan->cread(&len);

        if (len > 0){
            __int64_t *response = (__int64_t *)recvbuf;
            __int64_t fileSize = *response;
            printf("Filesize %d\n", *response);

            totalParts = round((double)fileSize/(double)m);
            __int64_t estimatedSize = totalParts * m;
            //printf("Estimated Size: %d\n", estimatedSize);

            //open file (also creates)
            string path = "received/" + fileName;

            //clearing file
            FILE *clear = fopen(path.c_str(), "wb");
            if (clear == NULL)
            {
                printf("Couldn't open file on client side\n");
                printf("Error %d \n", errno);
            }
            fclose(clear);
            
            fileArguments fArg(&requestBuffer, fileName, fileSize, m);
            fArgs.push_back(fArg);
        }
        else{
            printf("Read returned nothing. Does file exist on server side?\n");
            exit(0);
        }
    }
    else{
        printf("Initializing histogram\n");
        for (int i = 0; i < p; i++)
        {
            Histogram* h = new Histogram(10, -3, 3);
            hc.add(h);
        }

        printf("Populating patient args\n");
        for (int i = 0; i < p; i++)
        {
            patientArguments pArg(&requestBuffer, i + 1, n);
            pArgs.push_back(pArg);
        }
    }

    pthread_t fileThread;
    pthread_t patientThreads[p];
    pthread_t workerThreads[w];

    //Time set up for Real Time Statistics

    timer_t timerID;

    if(timer_create(CLOCK_MONOTONIC, NULL, &timerID) != 0){
        printf("timer_create error\n");
        exit(0);
    }

    struct itimerspec time;
    time.it_value.tv_sec = 0;
    time.it_value.tv_nsec = 1;
    time.it_interval.tv_sec = 0;
    time.it_interval.tv_nsec = 500000000;
    
    if(timer_settime(timerID, 0, &time, NULL) != 0){
        printf("timer_settime error\n");
        exit(0);
    }
    
    gettimeofday(&start, 0);
    /* Start all threads here */

    if(fileMode){
        printf("Starting file thread\n");
        pthread_create(&fileThread, NULL, &fileFunction, (void *)&fArgs[0]);
    }
    else{
        printf("Starting patient threads\n");
        for (int i = 0; i < p; i++)
        {
            pthread_create(&patientThreads[i], NULL, &patientFunction, (void *)&pArgs[i]);
        }
    }

    printf("Starting worker threads\n");
    for (int i = 0; i < w; i++)
    {
        pthread_create(&workerThreads[i], NULL, &workerFunction, (void *)&wArgs[i]);
    }

    if(fileMode){
        int ret = pthread_join(fileThread, NULL);
        printf("File thread joined\n");
    }
    else{
        for (int i = 0; i < p; i++)
        {
            int ret = pthread_join(patientThreads[i], NULL);
        }
        printf("All patient threads joined\n");
    }

    //Pushing w quits
    for (int i = 0; i < w; i++)
    {
        MESSAGE_TYPE quitMSG = QUIT_MSG;
        requestBuffer.push((char *)&quitMSG, sizeof(quitMSG));
    }

    //Join worker threads
    for (int i = 0; i < w; i++)
    {
        int ret = pthread_join(workerThreads[i], NULL);
    }

    printf("All worker threads joined\n");

    gettimeofday(&end, 0);
    timer_delete(timerID);
    system("clear");

    if(fileMode){
        printf("%d / %d copied\n", partsCopied, totalParts);
        printf("Transfer complete\n");
    }
    else{
        hc.print();
    }
    
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int)1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int)1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;    
}
