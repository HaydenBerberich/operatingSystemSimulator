//Hayden Berberich
//5/11/24
#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <csignal>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <iomanip>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <random>
#include <string>
#include <fstream>
#include <sys/types.h>
#include <sys/msg.h>


struct PCB { // struct for process control blocks for process table
   bool occupied{}; // either true or false
   pid_t pid{}; // process id of this child
   int startSeconds{}; // time when it was forked
   int startNano{}; // time when it was forked
};


PCB processTable[20];


struct SharedMemory { // struct to hold simulated time in shared memory
   int seconds{};
   int nano{};
};


int shm_id;
SharedMemory *shmp;
int msgid{};


void parseArgs(int argc, char *argv[], int &proc, int &simul, int &timeLimitForChildren, int &intervalInMsToLaunchChildren, std::string &logFile) {
   int option;
   while ((option = getopt(argc, argv, "hn:s:t:i:f:")) != -1) { // receives command line input and assigns to variables
       switch (option) {
           case 'h':
               std::cout << "Usage: " << argv[0] << " [-h] [-n proc] [-s simul] [-t timeLimitForChildren] [-i intervalInMsToLaunchChildren] [-f logFile]" << std::endl;
               exit(EXIT_SUCCESS);
           case 'n':
               proc = std::stoi(optarg);
               break;
           case 's':
               simul = std::stoi(optarg);
               break;
           case 't':
               timeLimitForChildren = std::stoi(optarg);
               break;
           case 'i':
               intervalInMsToLaunchChildren = std::stoi(optarg);
               break;
           case 'f':
               logFile = optarg;
               break;
           default:
               std::cerr << "Usage: " << argv[0] << " [-h] [-n proc] [-s simul] [-t timeLimitForChildren] [-i intervalInMsToLaunchChildren] [-f logFile" << std::endl;
               exit(EXIT_FAILURE);
       }
   }
}


void handleSignal(int signal) { // function to clean on ctrl+c
   if (signal == SIGINT) {
       std::cout << "Ctrl+C pressed. Cleaning up and exiting." << std::endl;


       for (int i{0}; i < 20; ++i) { // kills all active children
           if (processTable[i].occupied) {
               kill(processTable[i].pid, SIGKILL);
           }
       }


       shmdt(shmp); // detaches shared memory
       shmctl(shm_id, IPC_RMID, NULL);


       // Remove the message queue
       if (msgctl(msgid, IPC_RMID, NULL) == -1) {
           perror("msgctl failed");
           exit(EXIT_FAILURE);
       }


       exit(0);
   }
}


static void myhandler(int s) { // kills children and detaches mem if 60 secs pass
   for (int i{0}; i < 20; ++i) {
       if (processTable[i].occupied) {
           kill(processTable[i].pid, SIGKILL);
       }
   }


   shmdt(shmp);
   shmctl(shm_id, IPC_RMID, NULL);


   // Remove the message queue
   if (msgctl(msgid, IPC_RMID, NULL) == -1) {
       perror("msgctl failed");
       exit(EXIT_FAILURE);
   }
}


static int setupinterrupt(void) {
   /* set up myhandler for SIGPROF */
   struct sigaction act;
   act.sa_handler = myhandler;
   act.sa_flags = 0;
   return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}


static int setupitimer(void) {
   /* set ITIMER_PROF for 2-second intervals */
   struct itimerval value;
   value.it_interval.tv_sec = 60;
   value.it_interval.tv_usec = 0;
   value.it_value = value.it_interval;
   return (setitimer(ITIMER_PROF, &value, NULL));
}


void printTable(std::ofstream& log) { //prints process table
   std::cout << "OSS PID: " << getpid()
             << " SysClockS: " << shmp->seconds
             << " SysClockNano: " << shmp->nano << std::endl
             << "Process Table:" << std::endl << std::left
             << std::setw(10) << "Entry"
             << std::setw(10) << "Occupied"
             << std::setw(10) << "PID"
             << std::setw(10) << "StartS"
             << std::setw(10) << "StartN"
             << std::endl;


   for (int i{0}; i < 20; ++i) {
       std::cout << std::left
                 << std::setw(10) << i
                 << std::setw(10) << processTable[i].occupied
                 << std::setw(10) << processTable[i].pid
                 << std::setw(10) << processTable[i].startSeconds
                 << std::setw(10) << processTable[i].startNano
                 << std::endl;
   }


   log << "OSS PID: " << getpid()
             << " SysClockS: " << shmp->seconds
             << " SysClockNano: " << shmp->nano << std::endl
             << "Process Table:" << std::endl << std::left
             << std::setw(10) << "Entry"
             << std::setw(10) << "Occupied"
             << std::setw(10) << "PID"
             << std::setw(10) << "StartS"
             << std::setw(10) << "StartN"
             << std::endl;


   for (int i{0}; i < 20; ++i) {
       log << std::left
                 << std::setw(10) << i
                 << std::setw(10) << processTable[i].occupied
                 << std::setw(10) << processTable[i].pid
                 << std::setw(10) << processTable[i].startSeconds
                 << std::setw(10) << processTable[i].startNano
                 << std::endl;
   }
}


struct msgbuffer {
   long mtype;     
   int mtext;
};


int main(int argc, char *argv[]) {
   std::signal(SIGINT, handleSignal);


   // to terminate at 60 secs
   if (setupinterrupt() == -1) {
       perror("Failed to set up handler for SIGPROF");
       return 1;
   }
   if (setupitimer() == -1) {
       perror("Failed to set up the ITIMER_PROF interval timer");
       return 1;
   }


   int totalChildren{};
   int simultaneousChildren{};
   int timeLimitForChildren{};
   int intervalInMsToLaunchChildren{};
   std::string logFile{};
   int launchedChildren{};
   int currentChildren{};
   long milliseconds{};


   parseArgs(argc, argv, totalChildren, simultaneousChildren, timeLimitForChildren, intervalInMsToLaunchChildren, logFile);


   // error handling
   if (totalChildren < 1) {
       totalChildren = 1;
       std::cout << "Set -n to 1" << std::endl;
   }


   if (simultaneousChildren < 1 || simultaneousChildren > 15) {
       simultaneousChildren = 1;
       std::cout << "Set -s to 1" << std::endl;
   }


   if (timeLimitForChildren < 1) {
       timeLimitForChildren = 1;
       std::cout << "Set -t to 1" << std::endl;
   }


   if (logFile.empty()) {
       logFile = "log.txt";
       std::cout << "Set -f to log.txt" << std::endl;
   }


   std::ofstream log(logFile);


   if (!log.is_open()) {
       std::cerr << "Failed to open log file" << std::endl;
       exit(1);
   }


   // make shared mem key
   const int sh_key = ftok("oss.cpp", 0);


   // get shared mem
   shm_id = shmget(sh_key, sizeof(SharedMemory), IPC_CREAT | 0666);


   if (shm_id <= 0) {
       fprintf(stderr, "Shared memory get failed\n");
       exit(1);
   }


   // attach pointer to shared mem
   shmp = static_cast<SharedMemory*>(shmat(shm_id, 0, 0));


   if (shmp == (void *)-1) {
       fprintf(stderr, "Shared memory attach failed\n");
       exit(1);
   }


   int nextChild{0};
   int currentChild{0};
   int worker{-1};
   msgbuffer msg;


   // get message key
   key_t key = ftok("oss.cpp", 'R');
   if (key == -1) {
       perror("ftok failed");
       exit(EXIT_FAILURE);
   }


   // get message queue
   msgid = msgget(key, 0666 | IPC_CREAT);
   if (msgid == -1) {
       perror("msgget failed");
       exit(EXIT_FAILURE);
   }


   while (true) {
       // simulated time
       shmp->nano += 1000000;
       if (shmp->nano % 1000000000 == 0) {
           shmp->seconds++;
           shmp->nano = 0;
       }


       if (shmp->nano % 1000000 == 0) {
           milliseconds++;
       }


       if (shmp->nano % 500000000 == 0) {
           printTable(log);
       }


       // find next child to send message to
       do {
           ++worker;
           if (worker == 20) {
               worker = -1;
               break;
           }
           if (processTable[worker].occupied) {
               nextChild = processTable[worker].pid;
               break;
           }
       } while (nextChild == currentChild);


       currentChild = nextChild;


       if (worker != -1 && nextChild != 0) {
           log << "OSS: Sending message to worker " << worker << " PID " << nextChild << " at time " << shmp->seconds << ":" << shmp->nano << std::endl;
           std::cout << "OSS: Sending message to worker " << worker << " PID " << nextChild << " at time " << shmp->seconds << ":" << shmp->nano << std::endl;


           msg.mtype = nextChild;
           msg.mtext = 1;


           // send message
           if (msgsnd(msgid, &msg, sizeof(int), 0) == -1) {
               perror("msgsnd failed");
               exit(EXIT_FAILURE);
           }


           // receive message
           if (msgrcv(msgid, &msg, sizeof(msg.mtext), getpid(), 0) == -1) {
               perror("msgrcv failed");
               exit(EXIT_FAILURE);
           }


           log << "OSS: Receiving message " << msg.mtext << " from worker " << worker << " PID " << nextChild << " at time " << shmp->seconds << ":" << shmp->nano << std::endl;
           std::cout << "OSS: Receiving message " << msg.mtext << " from worker " << worker << " PID " << nextChild << " at time " << shmp->seconds << ":" << shmp->nano << std::endl;


           // for when child terminates
           if (msg.mtext == 0) {
               std::cout << "Child " << nextChild << " has terminated" << std::endl;
               log << "Child " << nextChild << " has terminated" << std::endl;
               wait(0);
               for (int i{0}; i < 20; ++i) {
                   if (processTable[i].pid == nextChild) {
                       processTable[i].occupied = false;
                       processTable[i].pid = 0;
                       processTable[i].startSeconds = 0;
                       processTable[i].startNano = 0;
                   }
               }
               currentChildren--;
           }
       }


       if ((launchedChildren < totalChildren) && (currentChildren < simultaneousChildren) && ((intervalInMsToLaunchChildren == 0) || (milliseconds % intervalInMsToLaunchChildren == 0))) {
           pid_t pid = fork();
           if (pid == 0) { // child
               std::random_device rd; // creates random secs and nano and execs to worker
               std::mt19937 gen(rd());
               std::uniform_int_distribution<> dis(1, timeLimitForChildren);
               int randomSeconds = dis(gen);
               std::uniform_int_distribution<> disNano(1, 1000000000);
               int randomNano = disNano(gen);
               execlp("./worker", "./worker", std::to_string(randomSeconds).c_str(), std::to_string(randomNano).c_str(), NULL);
               std::cerr << "exec failed\n";
               exit(1);
           } else if (pid > 0) { // parent
               for (int i{0}; i < 20; ++i) {
                   if (!processTable[i].occupied) { // enters into process table
                       processTable[i].occupied = true;
                       processTable[i].pid = pid;
                       processTable[i].startSeconds = shmp->seconds;
                       processTable[i].startNano = shmp->nano;
                       break;
                   }
               }
               launchedChildren++;
               currentChildren++;
           }
       }


       if (launchedChildren == totalChildren && currentChildren == 0) {
           break; // breaks when all processes have run
       }
   }


   log.close();


   // remove the message queue
   if (msgctl(msgid, IPC_RMID, NULL) == -1) {
       perror("msgctl failed");
       exit(EXIT_FAILURE);
   }


   shmdt(shmp); // detaches mem
   shmctl(shm_id, IPC_RMID, NULL);


   return 0;
}
