#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>

sem_t *nurse_vac1;
sem_t *nurse_vac2;
sem_t *empty;
sem_t *mutex;
sem_t *full;
sem_t *counter;
int pip[2];
int pipeforvac[2];
sem_t *citizenmutex;
int counterint=0;

int sem_val(sem_t* sem ){
	int retval;
	if(sem_getvalue(sem,&retval)<0){
		perror("sem_val error");
		exit(EXIT_FAILURE);
	}
	return retval;
}

void swait(sem_t *sem){
	if(sem_wait(sem)<0){
		perror("sem_wait error");
		exit(EXIT_FAILURE);
	}
}

void spost(sem_t *sem){
	if(sem_post(sem)<0){
		perror("sem_post error");
		exit(EXIT_FAILURE);
	}
}

void nurse(int fd,pid_t pid,int index){
	char *e=(char*)calloc(1,sizeof(char));
    int sz=read(fd,e,1);
    while(strcmp(e,"\n") != 0){
        if(sz==0){
            break;
        }
    	swait(empty);
    	
    	
    	if(strcmp(e,"1") == 0){
    		sem_post(nurse_vac1);
    		printf("Nurse %d ( pid = %d) brought a vaccine1: the clinic has %d vaccine1 and %d vaccine2.\n",index,pid,sem_val(nurse_vac1),sem_val(nurse_vac2));
    	}
    	else if(strcmp(e,"2") == 0){
    		sem_post(nurse_vac2);
    		printf("Nurse %d ( pid = %d) brought a vaccine2: the clinic has %d vaccine1 and %d vaccine2.\n",index,pid,sem_val(nurse_vac1),sem_val(nurse_vac2));
    	}
        swait(mutex);
    	sz=read(fd,e,1);
    	spost(mutex);
        spost(full);
    }
}

void vaccinator(pid_t pid,int time,int temp,int index){
	while(sem_val(counter)<time){ 
		swait(full);
        
		//Critical Region
		spost(counter);  
       
        swait(nurse_vac1);
        swait(nurse_vac2);
		swait(mutex);
        swait(citizenmutex);
        read(pip[0],&temp,sizeof(temp));
        printf("Vaccinator %d (pid = %d) is inviting Citizen pid=%d to the clinic.\n",index,pid,temp);
        printf("Citizen ( pid = %d) is vaccinated: the clinic has %d vaccine1 and %d vaccine2.\n",temp,sem_val(nurse_vac1),sem_val(nurse_vac2));
        counterint++;
		//leaving critical region
        spost(mutex);
        spost(empty);

	}
}


void citizen(int vaccine_times,pid_t pid){
    int j;
    for(j=0;j<vaccine_times;j++){
        spost(citizenmutex);
        write(pip[1],&pid,sizeof(pid));
    }
}
int main(int argc, char *argv[])
{
	int opt; 
	int nurse_number=0,vaccinator_number=0,citizen_number=0,buffer_size,vaccine_times=0;
    char *file_name="";
    while((opt = getopt(argc, argv, ":n:v:c:b:t:i:")) != -1) 
    { 
        switch(opt) 
        { 
            case 'n':
            	nurse_number=atoi(optarg);
            	break;
            case 'v':
            	vaccinator_number=atoi(optarg);
            	break;
            case 'c':
            	citizen_number=atoi(optarg);
            	break;
            case 'b':
            	buffer_size=atoi(optarg);
            	break;
            case 't':
            	vaccine_times=atoi(optarg);
            	break;
            case 'i':
            	file_name=optarg;
            	break;
            case '?':
                break; 
        } 
    }
    if(buffer_size<(vaccine_times*citizen_number)+1){
        perror("Buffer size should be greater than tc+1");
        exit(EXIT_FAILURE);
    }
    if(nurse_number<2){
        perror("Nurse number should be greater than 2");
        exit(EXIT_FAILURE);
    }
    if(vaccinator_number<2){
        perror("Vaccinator number should be greater than 2");
        exit(EXIT_FAILURE);
    }
    if(vaccine_times<1){
        perror("Vaccine time should be greater than 1");
        exit(EXIT_FAILURE);
    }
    if(citizen_number<3){
        perror("Citizen number should be greater than 2");
        exit(EXIT_FAILURE);
    }
    /*
        Initializing Values
    */
    int fd=open(file_name,O_RDONLY);
    if (fd ==-1)
    {
        perror("Inout file could");  
        exit(EXIT_FAILURE);
    }
    int i=0;
    pid_t pid_Nurse[nurse_number];
    pid_t pid_Vaccinator[vaccinator_number];
    pid_t pid_Citizen[citizen_number];
    printf("Welcome to the GTU344 clinic.Numbers of citizens to vaccinate c=%d with t=%d doses.\n",citizen_number,vaccine_times);
    nurse_vac1=sem_open("nurse_vac12", O_CREAT | O_EXCL,0644,0);
    nurse_vac2=sem_open("nurse_vac22", O_CREAT | O_EXCL,0644,0);
    empty=sem_open("empty2",O_CREAT | O_EXCL,0644,buffer_size);
    mutex=sem_open("mutex2",O_CREAT | O_EXCL,0644,1);
    full=sem_open("full2",O_CREAT | O_EXCL,0644,0);
    counter=sem_open("counter",O_CREAT | O_EXCL,0644,0);
    citizenmutex=sem_open("citizenmutex",O_CREAT | O_EXCL,0644,1);

    pipe(pip);
    pipe(pipeforvac);
    
    /*
		NURSE PROCESESSES

    */

    for(i=0;i<nurse_number;++i){
    	switch (pid_Nurse[i]=fork())
        {
        case -1:
            perror("program | main ");
            exit(EXIT_FAILURE);
            break;
        case 0:
            nurse(fd,getpid(),i);
            exit(EXIT_SUCCESS);
        default:
            break;
        }
    }
    /*
        VACCINATOR PROCESESSES

    */
    for(i=0;i<vaccinator_number;i++){
    	switch (pid_Vaccinator[i]=fork())
        {
        case -1:
            perror("program | main ");
            exit(EXIT_FAILURE);
            break;
        case 0:
            vaccinator(getpid(),citizen_number*vaccine_times,0,i);
            write(pipeforvac[1],&counterint,sizeof(counterint));
            exit(EXIT_SUCCESS);
        default:
            break;
        }
    }
    /*
        CITIZEN PROCESESSES

    */
    for(i=0;i<citizen_number;i++){
        switch (pid_Citizen[i]=fork())
        {
        case -1:
            perror("program | main ");
            exit(EXIT_FAILURE);
            break;
        case 0:
            citizen(vaccine_times,getpid());
            exit(EXIT_SUCCESS);
        default:
            break;
        }
    }


    sem_close(nurse_vac1);
    sem_unlink("nurse_vac12");
    sem_close(nurse_vac2);
    sem_unlink("nurse_vac22");
    sem_close(empty);
    sem_unlink("empty2");
    sem_close(full);
    sem_unlink("full2");
    sem_close(mutex);
    sem_unlink("mutex2");
    sem_close(counter);
    sem_unlink("counter");
    sem_close(citizenmutex);
    sem_unlink("citizenmutex");
    int stat;

    for(i=0;i<nurse_number;i++){
    	waitpid(pid_Nurse[i],&stat,0);
    }

    for(i=0;i<vaccinator_number;i++){
    	waitpid(pid_Vaccinator[i],&stat,0);
    }
    for(i=0;i<citizen_number;i++){
        waitpid(pid_Citizen[i],&stat,0);
    }
    int red;
    for(i=0;i<vaccinator_number;i++){
        read(pipeforvac[0],&red,sizeof(red));
        printf("Vaccinator %d vaccinated %d doses. ",i,red);
    }
    printf("The clinic is now closed. Stay healthy.\n");

}
