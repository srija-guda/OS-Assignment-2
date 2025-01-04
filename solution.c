//use this
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <math.h>

#define MAX_NEW_REQUESTS 30
#define ELEVATOR_MAX_CAP 20
#define PERMS 0644
#define maxPass 4

int found = 0;
pthread_mutex_t lock;

typedef struct PassengerRequest{
	int requestId;
	int startFloor;
	int requestedFloor;
} PassengerRequest;

typedef struct MainSharedMemory {
	char authStrings[100][ELEVATOR_MAX_CAP + 1];
	char elevatorMovementInstructions[100];
	PassengerRequest newPassengerRequest[MAX_NEW_REQUESTS];
	int elevatorFloors[100];
	int droppedPassengers[1000];
	int pickedUpPassengers[1000][2];
} MainSharedMemory;

typedef struct SolverRequest {
	long mtype;
	int elevatorNumber;
	char authStringGuess[ELEVATOR_MAX_CAP + 1];
} SolverRequest;

typedef struct SolverResponse {
	long mtype;
	int guessIsCorrect;
} SolverResponse;

typedef struct TurnChangeResponse {
	long mtype;
	int turnNumber;
	int newPassengerRequestCount;
	int errorOccured;
	int finished;
} TurnChangeResponse;

typedef struct TurnChangeRequest {
	long mtype;
	int droppedPassengersCount;
	int pickedUpPassengersCount;
} TurnChangeRequest;

typedef struct StoringPassengerRequest{
	int StoringRequestId[3000];
	int StoringStartFloor[3000];
	int StoringRequestedFloor[3000];
	int finishedDropRequest[3000];
	int finishedPickUpRequest[3000];
} StoringPassengerRequest;

typedef struct Elevator{
	int elevatorNo[50];
	int floorNo[50];
	char direction[50];
	int NoOfPassengers[50];
    int NoOfPassengersBeforeTurn[50];
    int UsingElevator[50];
    int dropCount[50];
    int PassengerInEle[50][maxPass];
    int PassengerInFl[50][maxPass];
} Elevator;

typedef struct argus{
    char *str;
    int length;
    int solverKey;
    int elevatorNum;
    MainSharedMemory *mainShmPtr;
    Elevator elevator;
    long startNo;
    long endNo;
    int* isFound;
    int solverNum;
    
}argus;

long custom_pow(int base, int exp) {
    long result = 1;
    for (int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

void computeIndices(int *indices, int num, int base, int length) {
    num -= 1;
    for (int i = length - 1; i >= 0; i--) {
        indices[i] = num % base;
        num /= base;
    }
}

void generateAuthString(char *str, int length, int solverKey, int elevatorNum, MainSharedMemory *mainShmPtr, Elevator elevator, long startNo, long endNo, int * (isFound), int solverNum)
{   
    int n = strlen(str);  
    char currentGuess[100] = ""; 
    int indices[length];  

    struct SolverRequest solverRequest;
    struct SolverResponse solverResponse;
    solverRequest.mtype = 2;
    solverRequest.elevatorNumber = elevatorNum;

    if (msgsnd(solverKey, &solverRequest, sizeof(solverRequest) - sizeof(solverRequest.mtype), 0) == -1) {
        perror("Error sending target elevator message");
        exit(1);
    }

    computeIndices(indices, startNo, n, length);

    for (int num = startNo; num <= endNo; num++) {

        for (int i = 0; i < length; i++) {
            currentGuess[i] = str[indices[i]];
        }
        currentGuess[length] = '\0'; 
            if(*(isFound) == 1)
        {
            pthread_mutex_unlock(&lock);
            str = NULL;
            return;
        }

        solverRequest.mtype = 3;
        strncpy(solverRequest.authStringGuess, currentGuess, ELEVATOR_MAX_CAP);
        if (msgsnd(solverKey, &solverRequest, sizeof(solverRequest) - sizeof(solverRequest.mtype), 0) == -1) {
            perror("Error sending guess message");
            exit(1);
        }
        if (msgrcv(solverKey, &solverResponse, sizeof(solverResponse) - sizeof(solverResponse.mtype), 4, 0) == -1) {
            perror("Error in receiving response");
            exit(1);
        }

       if (solverResponse.guessIsCorrect)
        {   
            pthread_mutex_lock (&lock);
            found = 1;
            pthread_mutex_unlock (&lock);
            strncpy(mainShmPtr->authStrings[elevatorNum], currentGuess, ELEVATOR_MAX_CAP);
            return;
        }

        for (int i = length - 1; i >= 0; i--) {
            if (indices[i] < n - 1) {
                indices[i]++;  
                break;
            } else {
                indices[i] = 0;  
            }
        }
    }
    pthread_exit(NULL);
    return;
}


void * func(void* ptr)
{
    argus* arguments = (argus*) ptr;
    generateAuthString(arguments->str, arguments->length, arguments->solverKey, arguments->elevatorNum, arguments->mainShmPtr, arguments->elevator, arguments->startNo, arguments->endNo, arguments->isFound, arguments->solverNum);
}

int iSPassInEle(int Eleids[maxPass], int reqId)
{
    for(int i=0; i<maxPass; i++)
    {
        if(Eleids[i]==reqId)
        {
            return 1;
        }
    }
    return 0;
}

void removeEleId(int Eleids[maxPass], int reqId)
{
    for(int i=0; i<maxPass; i++)
    {
        if(Eleids[i]==reqId)
        {
            Eleids[i] = -1;
        }
    }
    int NotNeg[maxPass];
    int idx = 0;
    for(int i=0; i<maxPass; i++)
    {
        if(Eleids[i]!= -1)
        {
            NotNeg[idx++] = Eleids[i];
        }
    }
    for(int i=0; i<maxPass; i++)
    {
        if(i<idx)
        {
            Eleids[i] = NotNeg[i];
        }
        else
        {
            Eleids[i] = -1;
        }
        
    }
    
}

void removeEleFl(int Eleids[maxPass], int reqId)
{
    for(int i=0; i<maxPass; i++)
    {
        if(Eleids[i]==reqId)
        {
            Eleids[i] = -1;
        }
    }
    int NotNeg[maxPass];
    int idx = 0;
    for(int i=0; i<maxPass; i++)
    {
        if(Eleids[i]!= -1)
        {
            NotNeg[idx++] = Eleids[i];
        }
    }
    for(int i=0; i<maxPass; i++)
    {
        if(i<idx)
        {
            Eleids[i] = NotNeg[i];
        }
        else
        {
            Eleids[i] = -1;
        }
        
    }
    
}

int main()
{
    FILE *input = fopen("input.txt", "r");
	int numbers[9];
	for(int i = 0; i<6 ; i++)
	{
		fscanf(input, "%d", &numbers[i]);
	}
	int N = numbers[0];
	int K = numbers[1];
	int M = numbers[2];
	int T = numbers[3];
	key_t shm_key = numbers[4];
	key_t main_mq_key = numbers[5];
	key_t keys[M];
    int solverIds[M];
	for(int i = 0; i<M ; i++)
	{
		fscanf(input, "%d", &keys[i]);
        int msqid;
        if ((msqid = msgget(keys[i], IPC_CREAT|PERMS)) == -1) {
            perror("Error in getting message queue ID");
            exit(1);
        }
        solverIds[i] = msqid;
    
	}
    if(pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("Error Initializing Mutex\n");
        return -1;
    }
    int shmId;
	MainSharedMemory* mainShmPtr;
	if((shmId = shmget(shm_key, sizeof(MainSharedMemory), PERMS))==-1)
	{
		printf("Error in shmget");
		exit(1);
	}
	if((mainShmPtr = shmat(shmId, NULL, 0)) == (void*)-1)
	{
		printf("Error in shmat ");
		return 1;
	}
    struct Elevator elevator;
    struct StoringPassengerRequest storingpassengerrequest;
    struct TurnChangeResponse turnchangeresponse;
    int currentLengthOfStoring = 0;
	for(int i=0; i<N; i++)
	{
		elevator.direction[i] = 'n';
		elevator.NoOfPassengers[i] = 0;
        elevator.elevatorNo[i] = i;
        elevator.NoOfPassengersBeforeTurn[i] = 0;
        elevator.UsingElevator[i] = 0;
        elevator.dropCount[i] = 0;
        for(int j=0; j<maxPass; j++)
        {
            elevator.PassengerInEle[i][j]=-1;
            elevator.PassengerInFl[i][j]=-1;
        }
	}
	for(int i=0; i<3000; i++)
	{
		storingpassengerrequest.finishedDropRequest[i] = 0;
		storingpassengerrequest.finishedPickUpRequest[i] = 0;
	}

    turnchangeresponse.finished = 0;
    int t = 0;
    while(turnchangeresponse.finished != 1) //turnchangeresponse.finished != 0
    {
        t++;
        
        struct TurnChangeResponse turnchangeresponse;
		int mainmsqid;
		if ((mainmsqid = msgget(main_mq_key, IPC_CREAT | PERMS)) == -1)
		{
			printf("Error in getting message queue ID");
			exit(1);
		}

		if(msgrcv(mainmsqid, &turnchangeresponse, sizeof(turnchangeresponse) - sizeof(turnchangeresponse.mtype), 2, 0)==-1)
		{
			printf("Error in receiving the message");
			exit(1);
		}
        if(turnchangeresponse.finished == 1)
        {
            break;
        }
        
        if(turnchangeresponse.errorOccured == 1)
		{
		    perror("Turn Change Response Error\n");
			break;
		}
        int newRequestsNum = turnchangeresponse.newPassengerRequestCount;
		for(int i=0; i<N; i++)
		{
			elevator.floorNo[i] = mainShmPtr->elevatorFloors[i];
            strncpy(mainShmPtr->authStrings[i], "", 0);
		}
        
        for(int i=0; i<newRequestsNum; i++)
		{
			storingpassengerrequest.StoringRequestId[currentLengthOfStoring] = mainShmPtr-> newPassengerRequest[i].requestId;
			storingpassengerrequest.StoringStartFloor[currentLengthOfStoring] = mainShmPtr-> newPassengerRequest[i].startFloor;
			storingpassengerrequest.StoringRequestedFloor[currentLengthOfStoring] = mainShmPtr -> newPassengerRequest[i].requestedFloor;

			currentLengthOfStoring++;
		}
        int dcount = 0;
        int pcount = 0;
        //pick up and drop
        for(int i=0; i<N; i++)
		{
			for(int j=0; j<currentLengthOfStoring; j++)
			{
                
				if(iSPassInEle(elevator.PassengerInEle[i], storingpassengerrequest.StoringRequestId[j]) && elevator.floorNo[i] == storingpassengerrequest.StoringRequestedFloor[j] && storingpassengerrequest.finishedDropRequest[j] == 0 && storingpassengerrequest.finishedPickUpRequest[j] == 1)
				{

					mainShmPtr->droppedPassengers[dcount] = storingpassengerrequest.StoringRequestId[j];
					storingpassengerrequest.finishedDropRequest[j] = 1;
					elevator.NoOfPassengers[i]--;
                    removeEleFl(elevator.PassengerInFl[i], storingpassengerrequest.StoringRequestedFloor[j]);
                    removeEleId(elevator.PassengerInEle[i], storingpassengerrequest.StoringRequestId[j]);
					dcount++;
					break;	
				}
				
				else if(elevator.floorNo[i] == storingpassengerrequest.StoringStartFloor[j] && elevator.NoOfPassengers[i]<maxPass && storingpassengerrequest.finishedPickUpRequest[j] == 0 )
				{
					mainShmPtr->pickedUpPassengers[pcount][0] = storingpassengerrequest.StoringRequestId[j];
					mainShmPtr->pickedUpPassengers[pcount][1] = elevator.elevatorNo[i];
					storingpassengerrequest.finishedPickUpRequest[j] = 1;
                    elevator.PassengerInEle[i][elevator.NoOfPassengers[i]] = storingpassengerrequest.StoringRequestId[j];
                    elevator.PassengerInFl[i][elevator.NoOfPassengers[i]] = storingpassengerrequest.StoringRequestedFloor[j];
                    elevator.NoOfPassengers[i]++;
					pcount++;
					break;
				}
			}
		}
        for(int i=0; i<N; i++)
        {
            if(elevator.UsingElevator[i]==1 && elevator.NoOfPassengers[i]==0)
            {
                elevator.UsingElevator[i] = 0;
            }
        }
        //if there are no passengers then elevator direction can be anything
        for(int i=0; i<N; i++)
        {
            if(elevator.NoOfPassengers[i]==0 && elevator.UsingElevator[i]==0)
            {
                elevator.direction[i] = 'n';
            }
        }

        int unfinishedDrop = 0;
        int unfinishedPickup = 0;

        for (int j = 0; j < currentLengthOfStoring; j++)
        {
            if (storingpassengerrequest.finishedDropRequest[j] == 0)
            {
                unfinishedDrop++;
            }
            if (storingpassengerrequest.finishedPickUpRequest[j] == 0)
            {
                unfinishedPickup++;
            }
        }

        int PassRemaining = newRequestsNum;
// To find closest elevator
        for (int j = 0; j < currentLengthOfStoring; j++)
        {
            if (storingpassengerrequest.finishedPickUpRequest[j] == 0)
            { 
                int requestedStartFloor = storingpassengerrequest.StoringStartFloor[j];
                int requestedDropFloor = storingpassengerrequest.StoringRequestedFloor[j];
                int closestUpElevator = -1;
                int closestDownElevator = -1;
                int minUpDistance = INT_MAX;
                int minDownDistance = INT_MAX;

                for (int i = 0; i < N; i++)
                {
                    if(elevator.NoOfPassengers[i]!=0)
                    {
                        if (elevator.NoOfPassengers[i]<maxPass && unfinishedPickup> 0 && (elevator.direction[i] == 'u'||elevator.direction[i] == 'n') && (requestedStartFloor >= elevator.floorNo[i]) && (requestedDropFloor>requestedStartFloor)&& elevator.floorNo[i]<K)
                        {
                        int distance = requestedStartFloor - elevator.floorNo[i];
                        if (distance < minUpDistance)
                        {
                            minUpDistance = distance;
                            closestUpElevator = i;
                        }
                        
                        }
                    else if (elevator.NoOfPassengers[i]<maxPass && unfinishedPickup> 0 &&(elevator.direction[i] == 'd'||elevator.direction[i] == 'n') && (requestedStartFloor <= elevator.floorNo[i]) && (requestedStartFloor>requestedDropFloor) && elevator.floorNo[i]>0)
                        {
                        int distance = elevator.floorNo[i] - requestedStartFloor;
                        if (distance < minDownDistance)
                        {
                            minDownDistance = distance;
                            closestDownElevator = i;
                        }
                        PassRemaining--;
                        }
                    }
                    else //lift is empty
                    {
                        if (unfinishedPickup> 0 && (elevator.direction[i] == 'u'||elevator.direction[i] == 'n') && elevator.floorNo[i]<K)
                        {
                            int distance = requestedStartFloor - elevator.floorNo[i];
                            if(distance<0)
                            {
                                if(-1*(distance) < minDownDistance)
                                {
                                    minDownDistance = distance;
                                    closestDownElevator = i;
                                }
                            }
                            else
                            {
                                if (distance < minUpDistance)
                                {
                                    minUpDistance = distance;
                                    closestUpElevator = i;
                                }
                            }
                        
                        }
                    }

                    
                }

                if (closestUpElevator != -1)
                {
                    mainShmPtr->elevatorMovementInstructions[closestUpElevator] = 'u';
                    elevator.UsingElevator[closestUpElevator] = 1;
                    unfinishedPickup--;
                }
                if (closestDownElevator != -1)
                {
                    mainShmPtr->elevatorMovementInstructions[closestDownElevator] = 'd';
                    elevator.UsingElevator[closestDownElevator] = 1;
                    unfinishedPickup--;
                }
            }
        }
        
        for(int i=0; i<N; i++)
            {
                    if(elevator.NoOfPassengers[i]>0 && elevator.floorNo[i]==0)
                    {
                        mainShmPtr->elevatorMovementInstructions[i] = 'u';
                    }
                    else if(elevator.NoOfPassengers[i]>0 && elevator.floorNo[i]==K-1)
                    {
                        mainShmPtr->elevatorMovementInstructions[i] = 'd';
                    }
            }
        for(int i=0; i<N; i++)
        {
            for(int j=0; j<maxPass; j++)
            {
                if(elevator.PassengerInFl[i][j]!= -1)
                {
                    int floor = elevator.PassengerInFl[i][j];
                    if(elevator.floorNo[i]>floor)
                    {
                        mainShmPtr->elevatorMovementInstructions[i] = 'd';
                    }
                    else if(elevator.floorNo[i]<floor)
                    {
                        mainShmPtr->elevatorMovementInstructions[i] = 'u';
                    }
                    break;
                }
            }
        }



        for(int i=0; i<N; i++)
		{
			if(elevator.UsingElevator[i]==0 && elevator.NoOfPassengers[i]==0)
            {
                mainShmPtr->elevatorMovementInstructions[i] = 's';
            }
		}
        // //print elevator instr
        // for(int i=0; i<N; i++)
		// {
		// 	//printf("Ele Instr%d %c\n", i, mainShmPtr->elevatorMovementInstructions[i]);
		// }
        // //print no of people in each elevator
        // for(int i=0; i<N; i++)
        // {
        //     //printf("Ele Passengers %d \n", elevator.NoOfPassengers[i]);
        // }

        int usingSolver[M]; //at the start of every turn the solver is free
		for(int i=0; i<M; i++)
		{
			usingSolver[i] = 0;
		}
        argus newArray[M];
		int j=0;
        pthread_t threads[M];
        char str[] = "abcdef";
        int threadsSent=0;
        for(int i=0; i<N; i++)
		{
			if(elevator.NoOfPassengersBeforeTurn[i] > 0)
			{
                long startNo = 1;
                long endNo = 0;
                long numPossibilities = custom_pow(6, elevator.NoOfPassengersBeforeTurn[i]);
                
                for(int var = 0; var<M; var++)
                {

                    if(var>numPossibilities)
                    {
                        break;
                    }
                    endNo+= numPossibilities/M +1;
                    if(var>0)
                    {
                        startNo+= numPossibilities/M +1;
                        if(endNo >= numPossibilities)
                        {
                            endNo = numPossibilities;
                        }
                    }
                    if(startNo>numPossibilities)
                    {
                        break;
                    }
                    threadsSent++;
                    argus ptr;
                    newArray[var].str = (char*)malloc(sizeof(10));
                    strncpy((newArray[var].str), str, strlen(str));
                    newArray[var].startNo = startNo;
                    newArray[var].length = elevator.NoOfPassengersBeforeTurn[i];
                    newArray[var].solverKey = solverIds[var];
                    newArray[var].elevatorNum = i;
                    newArray[var].mainShmPtr = mainShmPtr;
                    newArray[var].elevator = elevator;
                    newArray[var].endNo = endNo;
                    newArray[var].isFound = &found;
                    newArray[var].solverNum = var;

                    pthread_create(&threads[var], NULL, func, (void *)&newArray[var]);
                }
                for(int var=0; var<threadsSent; var++)
                {
                    pthread_join(threads[var], NULL);
                }
                threadsSent = 0;
                found = 0;
                
			}
		}


        struct TurnChangeRequest turnchangerequest;
		turnchangerequest.droppedPassengersCount = dcount; 
		turnchangerequest.pickedUpPassengersCount = pcount; 
		
		if ((mainmsqid = msgget(main_mq_key, PERMS)) == -1)
		{
			perror("Error in getting message queue ID");
			exit(1);
		}
		
		turnchangerequest.mtype = 1;
		if (msgsnd(mainmsqid, &turnchangerequest, sizeof(turnchangerequest) - sizeof(turnchangerequest.mtype), 0) == -1)
		{
			perror("Error in sending the message 2");
			exit(1);
		}
        for(int i=0; i<N; i++)
        {
             if(elevator.dropCount[i] != 0)
            {
                 elevator.NoOfPassengersBeforeTurn[i] = elevator.NoOfPassengers[i]+elevator.dropCount[i];
            }
             else
            {
                elevator.NoOfPassengersBeforeTurn[i] = elevator.NoOfPassengers[i];
            }

        }
    }
}