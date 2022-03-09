#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_LINE_SIZE 256

void read_input_vector(const char* filename, int n, int* array){
	FILE *fp;
	char *line = malloc(MAX_LINE_SIZE+1);
	size_t len = MAX_LINE_SIZE;
	ssize_t read;

	fp = strcmp(filename, "-") ? fopen(filename, "r") : stdin;

	assert(fp != NULL && line != NULL);

	int index = 0;

	while ((read = getline(&line, &len, fp)) != -1){
		array[index] = atoi(line);
		index++;
	}

	free(line);
	fclose(fp);
}

int length; //length of the file/input array
int num_of_threads; //the number of threads the program is allowed to use
int* input; //the input array

int threads_avaiable; //the number of threads currently avaiable to use

pthread_mutex_t lock_threads_avaiable = PTHREAD_MUTEX_INITIALIZER; //lock for the threads_avaible
pthread_cond_t condition_all_threads_completed = PTHREAD_COND_INITIALIZER; //cond for main continueing

int num_of_B_threads; //the total number of B threads
int thread_length; //the mode* of the lengths of the B Threads (At worse, one B Thread will be shorter)

struct B_thread_chunk_header* chunk_header_list; //An array of B Thread Headers
pthread_mutex_t* lock_chunk_header_list; //An array of locks for the B Thread Headers

void C_thread_terminate();
void* B_thread_create_chunk_header(int starting_index, int ending_index, int chunk_id);
void* C_thread_create_increase(int current, int INCREASE_BY, int stop);
void* C_thread_apply_increase(void* arguments);
void* B_thread_work(void* arguments);

struct B_thread_chunk_header {
	bool finished;
	bool added_sum;

	bool wait_for_other_chunk;
	pthread_cond_t start_summing_again;

	int current_index;
	int current_sum;

	int starting_index;
	int ending_index;

	int chunk_id;
};

struct C_thread_increase_header {
	int current;
	int INCREASE_BY;
	int stop;
};

int main(int argc, char* argv[]) {
	char* filename = argv[1]; //file of prefix-sum
	length = atoi(argv[2]); //length of the list in the file
	num_of_threads = atoi(argv[3]); //Number of allowed threads

	//If there are less than two items in the list
	if (length < 2){
		//Why are you even getting the pre-fix sum?
		exit(EXIT_FAILURE);
	}

	input = malloc(sizeof(int) * length);

	//Set the number of threads avaible to the number of possible threads
	threads_avaiable = num_of_threads;

	//Read item list into input
	read_input_vector(filename, length, input);

	//If there are more threads than items in the list
	if (length <= num_of_threads){
		//set the num of B threads to the number of items in the list
		num_of_B_threads = length;
		//You have a thread length of 1 then
		thread_length = 1;
	} else {
		//Set the thread length to the length divided by the number of threads
		//ceil this value (one thread will be shorter than the rest)
		thread_length = (int)(ceil((double)length / num_of_threads));
		//Calculate the number of B threads
		//If you have a remainder, then add one extra thread (the shorter thread)
		num_of_B_threads = (length % thread_length) == 0 ? length / thread_length : (length / thread_length) + 1;
	}

	//Allocat memory for the B Thread chunks and their locks
	chunk_header_list = calloc(num_of_B_threads, sizeof(struct B_thread_chunk_header));
	lock_chunk_header_list = calloc(num_of_B_threads, sizeof(pthread_mutex_t));

	//Store the current chunk index and the remaining items to cover
	int current_index = 0;
	int remaining = length;

	//For all the B Threads
	for (int i = 0; i < num_of_B_threads; i++){
		//Create a lock at their appriote id in the lock array
		lock_chunk_header_list[i] = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

		//Get the chunk size (need to test for the potenial shorter chunk)
		int size = thread_length;

		if (remaining - thread_length < 0){
			size = remaining;
		} else {
			remaining-=size;
		}

		//Create a new chunk header object with the current data and put it into the array
		chunk_header_list[i] = *(struct B_thread_chunk_header*)(B_thread_create_chunk_header(current_index, current_index + size - 1, i));
		//Increase the current_index to account for the new chunk
		current_index+=(size);

		//Lock/Unlock the avaible threads
		pthread_mutex_lock(&lock_threads_avaiable);
		//Decrease the avaiable threads
		threads_avaiable--;
		pthread_mutex_unlock(&lock_threads_avaiable);

	}

	for(int i = 0; i < num_of_B_threads; i++){
		pthread_t B_thread;
		pthread_create(&B_thread, NULL, B_thread_work, (void*)&chunk_header_list[i]);
	}

	//Lock/Unlock the avaible threads
	pthread_mutex_lock(&lock_threads_avaiable);
	//If the number of threads avaiable is not equal to the num of threads
	while (threads_avaiable != num_of_threads){
		//Wait for a signal to check again
		pthread_cond_wait(&condition_all_threads_completed, &lock_threads_avaiable);
	}

	pthread_mutex_unlock(&lock_threads_avaiable);

	for(int i = 0; i < length; i++){
		printf("%d\n", input[i]);
	}
}

//B THREAD METHODS
void* B_thread_create_chunk_header(int starting_index, int ending_index, int chunk_id){
	struct B_thread_chunk_header* chunk = malloc(sizeof(struct B_thread_chunk_header));
	//Lost data?

	chunk->finished = false;
	chunk->added_sum = false;

	chunk->wait_for_other_chunk = false;
	chunk->start_summing_again = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

	chunk->current_index = starting_index;
	chunk->current_sum = 0;

	chunk->starting_index = starting_index;
	chunk->ending_index = ending_index;
	chunk->chunk_id = chunk_id;

	return (void*)(chunk);
}
void* B_thread_work(void* arguments){
	bool at_end_of_chunk = false;
	//Get the B thread chunk header from the passed void pointer
	struct B_thread_chunk_header *passed_arguments = arguments;

	pthread_mutex_lock(&lock_chunk_header_list[passed_arguments->chunk_id]);

	while(!at_end_of_chunk){

		//Lock the current chunk header
		if (!passed_arguments->wait_for_other_chunk){
			passed_arguments->current_sum+= input[passed_arguments->current_index];
			input[passed_arguments->current_index] = passed_arguments->current_sum;

			if (passed_arguments->current_index == passed_arguments->ending_index){
				//Set the end of chunk variable to be truev=
				at_end_of_chunk = true;
				//Set the chunk to be finsihed reading
				passed_arguments->finished = true;
			} else {
				passed_arguments->current_index++;
			}
		}


		while (passed_arguments->wait_for_other_chunk){
			pthread_cond_wait(&passed_arguments->start_summing_again, &lock_chunk_header_list[passed_arguments->chunk_id]);
		}
	}

	pthread_mutex_unlock(&lock_chunk_header_list[passed_arguments->chunk_id]);
	int current_next_chunk_id = passed_arguments->chunk_id + 1;
	int C_thread_to = -1;
	bool found_next_chunk = false;

	while (current_next_chunk_id < num_of_B_threads && !found_next_chunk){

		chunk_header_list[current_next_chunk_id].wait_for_other_chunk = true;
		pthread_mutex_lock(&lock_chunk_header_list[current_next_chunk_id]);
		if (!(chunk_header_list[current_next_chunk_id].added_sum)){
			//Lock my own chunk again
			pthread_mutex_lock(&lock_chunk_header_list[passed_arguments->chunk_id]);
			//Set found a next chunk to true
			found_next_chunk = true;

			//increase the chunk's current sum to inlcude my own
			chunk_header_list[current_next_chunk_id].current_sum+=passed_arguments->current_sum;
			//Define C thread to as the next chunk's current index
			C_thread_to = chunk_header_list[current_next_chunk_id].current_index - 1;

			//Set my added sum variable to true
			passed_arguments->added_sum = true;

			if (!(chunk_header_list[current_next_chunk_id].added_sum) && chunk_header_list[current_next_chunk_id].finished){
				input[chunk_header_list[current_next_chunk_id].current_index]+=passed_arguments->current_sum;
			}

			pthread_mutex_unlock(&lock_chunk_header_list[passed_arguments->chunk_id]);
		}

		chunk_header_list[current_next_chunk_id].wait_for_other_chunk = false;

		pthread_cond_signal(&chunk_header_list[current_next_chunk_id].start_summing_again);
		pthread_mutex_unlock(&lock_chunk_header_list[current_next_chunk_id]);
		//Increase the chunk I'm looking at
		current_next_chunk_id++;
	}

	//If found a next chunk
	if (found_next_chunk && (passed_arguments->ending_index != C_thread_to) && passed_arguments->current_sum != 0){
		C_thread_apply_increase(C_thread_create_increase(passed_arguments->ending_index + 1, passed_arguments->current_sum, C_thread_to));
	} else {
		//If not, just terminate
		C_thread_terminate();
	}

	return NULL;
}

//C THREAD METHODS
void* C_thread_create_increase(int current, int INCREASE_BY, int stop){
	struct C_thread_increase_header* args = malloc(sizeof(struct C_thread_increase_header));
	//printf("creating a new sub task from (%d to %d) on Threads %ld\n", current, stop, pthread_self());
	args->current = current;
	args->INCREASE_BY = INCREASE_BY;
	args->stop = stop;

	return (void*)(args);
}
void* C_thread_apply_increase(void* arguments){
	//Type casting the passed arguments into the C_Thread arguments
	struct C_thread_increase_header *passed_arguments = arguments;

	//If there are still more work to be done
	while(passed_arguments->current <= passed_arguments->stop){

		input[passed_arguments->current]+=passed_arguments->INCREASE_BY;
		//Store this local values
		int new_stop = passed_arguments->stop;
		int next = passed_arguments->current + 1;

		//Lock the number of threads aaiable for a second
		pthread_mutex_lock(&lock_threads_avaiable);

		//If there isn't one more location left and threads avaiable is greater than 0
		if (!(next >= new_stop) && threads_avaiable > 0){
			//Do the math to create a new thread
			int offset = ceil((passed_arguments->stop - next)/2);
			int sub_Thread_stop = passed_arguments->stop;
			new_stop = next + offset;

			//Decrease the number of avaible threads
			threads_avaiable--;

			pthread_t sub_Thread;
			//Create a new pthread to handle the work
			pthread_create(&sub_Thread, NULL, C_thread_apply_increase, C_thread_create_increase(new_stop + 1, passed_arguments->INCREASE_BY, sub_Thread_stop));

		}
		pthread_mutex_unlock(&lock_threads_avaiable);

		//Set the arguments to their updated ones
		passed_arguments->current = next;
		passed_arguments->stop = new_stop;

		//Start to work again
	}
	//Free the memory location of the arguments
	free(passed_arguments);
	//Termiante the C Thread
	C_thread_terminate();

	return NULL;
}
void C_thread_terminate(){
	bool signal_to_A_thread = false;

	pthread_mutex_lock(&lock_threads_avaiable);

	threads_avaiable++;

	if (threads_avaiable == num_of_threads){
		signal_to_A_thread = true;
	}

	pthread_mutex_unlock(&lock_threads_avaiable);

	if (signal_to_A_thread){
		pthread_cond_signal(&condition_all_threads_completed);
	}
	//Time for this C thread to die
}
