//Hayden Berberich
//5/11/24
#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>


struct msgbuffer {
   long mtype;     
   int mtext;
};


struct SharedMemory {
   int seconds{};
   int nano{};
};


int main(int argc, char *argv[]) {
   // creates key and get shared mem
   const int sh_key = ftok("oss.cpp", 0);
   int shm_id = shmget(sh_key, sizeof(SharedMemory), IPC_CREAT | 0666);


   if (shm_id <= 0) {
       fprintf(stderr, "Shared memory get failed\n");
       exit(1);
   }


   // points to shared mem
   SharedMemory *shmp = static_cast<SharedMemory*>(shmat(shm_id, 0, 0));


   if (shmp == (void *)-1) {
       fprintf(stderr, "Shared memory attach failed\n");
       exit(1);
   }


   int msgid;
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


   int termTimeS = shmp->seconds + std::stoi(argv[1]);
   int termTimeN = shmp->nano + std::stoi(argv[2]);
   //int currentSeconds = shmp->seconds;
   //int secondsPassing{0};
   int i{0};


   // prints worker info
   std::cout << "WORKER PID: " << getpid()
             << " PPID: " << getppid()
             << " SysClockS: " << shmp->seconds
             << " SysClockNano: " << shmp->nano
             << " TermTimeS: " << termTimeS
             << " TermTimeNano: " << termTimeN
             << std::endl
             << "--Just Starting" << std::endl;


   while (termTimeS > shmp->seconds || (termTimeS == shmp->seconds && termTimeN > shmp->nano)) {


       if (msgrcv(msgid, &msg, sizeof(msg.mtext), getpid(), 0) == -1) {
           perror("msgrcv failed");
           exit(EXIT_FAILURE);
       }


       std::cout << "WORKER PID: " << getpid()
                 << " PPID: " << getppid()
                 << " SysClockS: " << shmp->seconds
                 << " SysClockNano: " << shmp->nano
                 << " TermTimeS: " << termTimeS
                 << " TermTimeNano: " << termTimeN
                 << std::endl
                 << "--" << ++i << " iterations have passed since starting" << std::endl;
       // sends message if still running
       if (termTimeS > shmp->seconds || (termTimeS == shmp->seconds && termTimeN > shmp->nano)) {
           msg.mtype = getppid();
           msg.mtext = 1;


           if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
               perror("msgsnd failed");
               exit(EXIT_FAILURE);
           }
       }


   }


   std::cout << "WORKER PID: " << getpid()
             << " PPID: " << getppid()
             << " SysClockS: " << shmp->seconds
             << " SysClockNano: " << shmp->nano
             << " TermTimeS: " << termTimeS
             << " TermTimeNano: " << termTimeN
             << std::endl
             << "--Terminating after sending message back to oss after " << i << " iterations." << std::endl;


   msg.mtype = getppid();
   msg.mtext = 0;


   // sends message that it's terminating
   if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
       perror("msgsnd failed");
       exit(EXIT_FAILURE);
   }


   return 0;
}
