/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/19
 */
#include "common.h"
#include "FIFOreqchannel.h"

using namespace std;


int main(int argc, char *argv[]){
    int n = 100;    // default number of requests per "patient"
	int p = 1;		// default patient
    double time = 0.004;
    int ecgno = 1;
    int m = MAX_MESSAGE;
    struct timeval start, end;

    bool dataRequest = false;
    bool fileTransfer = false;
    bool newChannel = false;
    string fileName = "not set";

    int opt = 0;
    while ((opt = getopt(argc, argv, "dp:t:e:f:m:ch")) != -1)
    {
        switch (opt)
        {
        case 'd':
            dataRequest = true;
            break;
        case 'p':
            p = atoi(optarg);
            break;
        case 't':
            time = atof(optarg);
            break;
        case 'e':
            if(atoi(optarg) != 1 && atoi(optarg) != 2){
                cerr << "Enter 1 or 2 for ecgno\n";
                exit(0);
            }else{
                ecgno = atoi(optarg);
            }
            break;
        case 'f':
            fileTransfer = true;
            fileName = optarg;
            break;
        case 'm':
            m = atoi(optarg);
            break;
        case 'c':
            newChannel = true;
            break;
        case 'h':
        default:
            printf("Program can be invoked with -d (datarequestsequence) -p patientnum -t time -e ecgno -f (file name) -m message size -n (new channel) \n");
            exit(0);
        }
    }

    printf("Parsed args d = %d, p = %d, t = %f, e = %d, f = %d, c = %d, filename = %s\n", dataRequest, p, time, ecgno, fileTransfer, newChannel, fileName.c_str());

    int pid = fork();
    if (pid == 0)
    {
        printf("Spawning server from client\n");
        string fileBufSize = to_string(m);
        execl("dataserver", "dataserver", (char *)fileBufSize.c_str(),  NULL);
    }

    srand(time_t(NULL));
    FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);

    gettimeofday(&start, 0);
    gettimeofday(&end, 0);

    if (dataRequest)
    {
        printf("Transferring entire patient %d file\n", p);
        string path = "received/" + to_string(p) + ".csv";
        cout << path << endl;
        ofstream myfile;
        myfile.open (path);

        gettimeofday(&start, 0);

        for(double t = 0; t < 59.996; t+=0.004){
            datamsg d1(p, t, 1);
            datamsg d2(p, t, 2);

            chan.cwrite((char *)&d1, sizeof(d1));
            double val1 = *(double *)chan.cread();

            chan.cwrite((char *)&d2, sizeof(d2));
            double val2 = *(double *)chan.cread();

            myfile << t << "," << val1 << "," << val2 << "\n";
        }

        gettimeofday(&end, 0);

        myfile.close();

        cout << "Comparing the two files using diff -s\n";
        string diff = "diff -s BIMDC/" + to_string(p) + ".csv" + " received/" + to_string(p) + ".csv";
        system(diff.c_str());
    }
    else if(fileTransfer){
        filemsg fileRequest = filemsg(0, 0);
        char fileBuffer[sizeof(fileRequest) + sizeof(fileName)];
        memset(fileBuffer, 0, sizeof(fileBuffer));

        memcpy(fileBuffer, &fileRequest, sizeof(fileRequest));
        strcpy(fileBuffer + sizeof(fileRequest), fileName.c_str());

        chan.cwrite(fileBuffer, sizeof(fileBuffer));

        int len = 0;
        char *recvbuf = chan.cread(&len, m);
        if (len > 0)
        {
            __int64_t *response = (__int64_t *)recvbuf;
            printf("Filesize %d\n", *response);

            int iterations = ceil(*response / m);

            //open file (also creates)
            string path = "received/" + fileName;

            //clearing file
            FILE *clear = fopen(path.c_str(), "wb");
            fclose(clear);

            FILE *copy = fopen(path.c_str(), "ab+");
            if (copy == NULL)
            {
                printf("Couldn't open file on client side\n");
                printf("Error %d \n", errno);
            }
            else
            {
                gettimeofday(&start, 0);

                int offset = 0;
                int length = 0;
                int bytesRemaining = *response;
                //char receiveBuffer[MAX_MESSAGE];

                int i = 1;

                while (bytesRemaining != 0)
                {
                    length = min(bytesRemaining, m);
                    filemsg fileReqSeg = filemsg(offset, length);
                    //printf("Requesting offset %d length %d\n", offset, length);
                    memcpy(fileBuffer, &fileReqSeg, sizeof(fileReqSeg));

                    int len = 0;

                    chan.cwrite(fileBuffer, sizeof(fileBuffer));
                    char *recvbuf = chan.cread(&len, m);

                    printf("Got segment %d with size %d\n", i, len);

                    fwrite (recvbuf , sizeof(char), len, copy);
                    //fprintf(copy, recvbuf);

                    offset += length;
                    bytesRemaining -= length;
                    i++;
                }
                fclose(copy);

                gettimeofday(&end, 0);

                
                cout << "Comparing the two files using diff -s\n";
                string diff = "diff -s BIMDC/" + fileName + " received/" + fileName;
                system(diff.c_str());
            }
        }
        else
        {
            printf("Read returned nothing. Does file exist on server side?\n");
        }
    }
    else if (newChannel)
    {
        MESSAGE_TYPE newChanMessage = NEWCHANNEL_MSG;
        chan.cwrite((char *)&newChanMessage, sizeof(newChanMessage));

        int len = 0;
        char *recvBuf = chan.cread(&len);
        if (len > 0)
        {
            string newName(recvBuf);
            FIFORequestChannel newChan(newName, FIFORequestChannel::CLIENT_SIDE);

            printf("Requesting original channel for data: patient: %d time: %f ecgno: %d\n", p, time, ecgno);
            datamsg dataRequest(p, time, ecgno);
            chan.cwrite((char *)&dataRequest, sizeof(dataRequest));
            printf("Read %f from old channel\n", *(double *)chan.cread());

            newChan.cwrite((char *)&dataRequest, sizeof(dataRequest));
            printf("Read %f from new channel\n", *(double *)newChan.cread());

            //sending quit to new channel
            MESSAGE_TYPE quitMsg = QUIT_MSG;
            newChan.cwrite((char *)&quitMsg, sizeof(quitMsg));
        }
        else
        {
            cout << "Didn't read anything\n";
        }
    }
    else{
        printf("Printing value for patient: %d time: %f ecgno: %d\n", p, time, ecgno);
        datamsg d(p, time, ecgno);
        chan.cwrite((char *)&d, sizeof(d));
        cout << *(double *)chan.cread() << endl;
    }

    if(fileTransfer || dataRequest){
        int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int)1e6;
        int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int)1e6);
        cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
    }

    //quit
    MESSAGE_TYPE quitMsg = QUIT_MSG;
    chan.cwrite((char *)&quitMsg, sizeof(quitMsg));
    printf("Exiting client\n");

	return 0;
}
