#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <chrono>
#include <cmath>

using namespace std;

// Структура, передаваемая в качестве аргумента потока
struct thread_args {
	float* first_number_ptr;
    unsigned int batch_size;
    float (*func_ptr)(float);
};

// Функция, которую будет исполнять созданный поток
void* thread_job(void* arg)
{
    int err;

	// Преобразуем аргументы к нужному типу
	thread_args* param = (thread_args*) arg;
	
    // Если не было передано аргументов, то завершаем работу потока
	if (param == NULL) {
		cout << "No args to work" << endl;
		return NULL;
	} else {
        for (unsigned int i = 0; i < param->batch_size; i++) {
            *(param->first_number_ptr + i) = param->func_ptr(*(param->first_number_ptr + i));
        }
    }
	
	return NULL;
}

float executable_function(float input_number) {
    return sqrt(input_number);
}

void initialize_array(unsigned int length, float *arr) {
    for (unsigned int i = 0; i < length; i++) {
        arr[i] = (float)i;
    }
}

int main(int argc, char* argv[]) {
    
    if (argc < 3) {
		cout << "Not enough arguments" << endl;
		return 0;
	}

    unsigned int array_length = atoi(argv[1]);
    unsigned int threads_count = atoi(argv[2]);

    if (threads_count > array_length) {
        threads_count = array_length;
    }

    unsigned int batch_size = array_length / threads_count;
    unsigned int rest = array_length % threads_count;

    pthread_t* thread_arr = new pthread_t[threads_count];
    
    float* number_arr = new float[array_length];
    initialize_array(array_length, number_arr);
    
    thread_args* targs_arr = new thread_args[threads_count];
    
    int err;
    for (unsigned int i = 0; i < threads_count; i++) {
        targs_arr[i].func_ptr = &executable_function;
        targs_arr[i].first_number_ptr = number_arr + i * batch_size;
        targs_arr[i].batch_size = batch_size;
        if (i == threads_count - 1) {
            targs_arr[i].batch_size += rest;
        }

        err = pthread_create(thread_arr + i, NULL, thread_job, (void*) (targs_arr + i));
        if (err != 0) {
			cout << "Cannot create a thread: " << strerror(err) << endl;
			exit(-1);
		}
    }

    for (unsigned int i = 0; i < threads_count; i++) {
        pthread_join(thread_arr[i], NULL);
    }
    
    for (unsigned int i = 0; i < array_length; i++) {
        cout << "arr[" << i << "] = " << number_arr[i] << endl;
    }
    
    delete[] number_arr;
    delete[] targs_arr;
    delete[] thread_arr;
    
	pthread_exit(NULL);
}
