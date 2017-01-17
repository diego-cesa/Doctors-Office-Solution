#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#define MAX_SLEEP 5

#define NAPPING 0
#define TREATING 1

struct WaitingRoom{
    int iID;
    sem_t called;
    sem_t treated;
    struct WaitingRoom* next;
};

struct Doctor{
    int iID;
    int iStatus;
    sem_t wake;
    struct Doctor* next;
};

struct WaitingRoom *waitingRoom = NULL;
struct Doctor *doctor = NULL;
int finished;
int iMaxChair;

/**
* This function is used to represent treating your patients
*/
int treat_patient(void) {
	int iTime = (rand() % 5) + 4;
	sleep(iTime);
	return iTime;
}

/**
 * This thread is responsible for getting patients from the waiting room
 * to treat and sleep when there are no patients.
 */
void *doctor_thread(void *arg) {
    int iID = *((int*)arg);

	printf("[DOC] Doctor %d arrived\n", iID);
    // Create new doctors
    struct Doctor *newDoctor = malloc(sizeof(struct Doctor));
    newDoctor->iID = iID;
    newDoctor->iStatus = TREATING;
    sem_init(&newDoctor->wake, 0, 0);

    if(doctor == NULL){
        doctor = newDoctor;
        doctor->next = doctor;
    }
    else{
        newDoctor->next = doctor->next;
        doctor->next= newDoctor;
    }


	while(finished > 0) {			//exit your doctor threads when no more patients coming (amount specified on cammond line)

        if(waitingRoom == NULL){
            newDoctor->iStatus = NAPPING;
            sem_wait(&newDoctor->wake);
            newDoctor->iStatus = TREATING;
        }
        else{
            struct WaitingRoom* nextPatient;
            nextPatient = waitingRoom;

            if(waitingRoom->next != NULL){
               waitingRoom = waitingRoom->next;
            }
            else{
                waitingRoom = NULL;
            }
            // Call patient
            sem_post(&nextPatient->called);
            printf("[DOC] Doctor %d called patient %d\n", newDoctor->iID, nextPatient->iID);
            int iTime = treat_patient();
            printf("[DOC] Doctor %d treated Patient %d in %d seconds\n",newDoctor->iID, nextPatient->iID, iTime);
            // Dismiss patient
            sem_post(&nextPatient->treated);
            finished--;
        }
	}

	// Wake-up other doctors to leave the office
	doctor = newDoctor;
	while(doctor->next != newDoctor){
        doctor = doctor->next;
        sem_post(&doctor->wake);
    }

	printf("[DOC] Doctor %d left clinic\n", iID);
	return NULL;
}

/**
 * This thread is responsible for acting as a patient, waking up doctors, waiting for doctors
 * and be treated.
 */
void *patient_thread(void *arg) {

    int iID = *((int*)arg);

    struct WaitingRoom* newPatient;
    newPatient = malloc(sizeof(struct WaitingRoom));
    newPatient->iID = iID;
    sem_init(&newPatient->called, 0, 0);
    sem_init(&newPatient->treated, 0, 0);

    if(waitingRoom == NULL){
        waitingRoom = newPatient;
        printf("[PAT] Patient %d sat in chair: 1\n", iID);
    }
    else{
        struct WaitingRoom* nextPatient;
        int iContPatients = 1;

        nextPatient = waitingRoom;

        while(nextPatient->next != NULL){
            nextPatient = nextPatient->next;
            iContPatients++;
        }
        if(iContPatients >= iMaxChair){
            printf("[PAT] Waiting room is full. Patient %d left.\n", iID);
            finished--;
            return NULL;
        }

        nextPatient->next = newPatient;
        printf("[PAT] Patien %d sat in chair: %d\n", iID, iContPatients+1);

    }

    // Wake-up doctor
    struct Doctor* nextDoctor;
    nextDoctor = doctor;

    if(nextDoctor == NULL){
        printf("Error: there is no doctor!\n");
        return NULL;
    }
    else{
        while(nextDoctor->iStatus == TREATING && nextDoctor->next != doctor){
            nextDoctor = nextDoctor->next;
        }
        if(nextDoctor->iStatus == NAPPING){
            printf("[PAT] Patient %d woke-up doctor %d\n", iID, nextDoctor->iID);
            sem_post(&nextDoctor->wake);
            doctor = nextDoctor->next;
        }
    }

    // Wait to be called
    printf("[PAT] Patient %d waiting call\n", iID);
    sem_wait(&newPatient->called);

    // Wait to be treated
    sem_wait(&newPatient->treated);

    printf("[PAT] Patient %d left clinic\n", iID);
	return NULL;
}

int main(int argc, char **argv) {

	if(argc != 4){
		printf("Usage: DoctorsOffice <waitingSize> <patients> <doctors>\n");
		exit(0);
	}

	int iPatient, iDoc;

	iMaxChair = strtol(argv[1],NULL, 10);
	iPatient = strtol(argv[2],NULL, 10);
	iDoc = strtol(argv[3],NULL, 10);

	finished = iPatient;

	pthread_t threadDoc[iDoc], threadPatient[iPatient];
    int iDocRet, iPatientRet;

	// Start Doctor Threads
	int iDocID[iDoc];
	for(int i = 0; i < iDoc; i++){
        iDocID[i] = i+1;
        iDocRet = pthread_create(&threadDoc[i], NULL, doctor_thread, (void*)&iDocID[i]);
        if(iDocRet != 0){
            printf("Error: doctor_thread %d not created\n", i);
        }
     }

    // Start Patient Threads
    int iPatientID[iPatient];
    for(int i = 0; i < iPatient; i++){
        iPatientID[i] = i+1;

        // Patients will arrive between every 1 to 5 seconds
        int iArrive = rand() % MAX_SLEEP + 1;
        sleep(iArrive);
        printf("[PAT] Patient %d arrived in %d seconds\n", i+1, iArrive);

        iPatientRet = pthread_create(&threadPatient[i], NULL, patient_thread, (void*)&iPatientID[i]);
        if(iPatientRet != 0){
            printf("Error: patient_thread %d not created\n", i);
        }
    }

	// Clean up
    for(int i = 0; i < iPatient; i++){
        pthread_join(threadPatient[i], NULL);
    }
    for(int i = 0; i < iDoc; i++){
        pthread_join(threadDoc[i], NULL);
    }

    while(waitingRoom != NULL){
        struct WaitingRoom* temp = waitingRoom;
        waitingRoom = waitingRoom->next;
        free(temp);
    }

    struct Doctor* previous = doctor;
    doctor = doctor->next;
    while(doctor != previous){

        struct Doctor* temp = doctor;
        doctor = doctor->next;
        previous->next = doctor;

        free(temp);
    }
    free(doctor);

	return 0;
}
