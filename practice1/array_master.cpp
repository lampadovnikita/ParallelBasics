#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <chrono>
#include <cmath>

using namespace std;

const unsigned int ARRAY_LENGTH = 10;

// Структура, передаваемая в качестве аргумента потока
struct thread_args {
	float *number_ptr;
    float (*func_ptr)(float*);
};

// Функция, которую будет исполнять созданный поток
void* thread_job(void* arg)
{
    int err;

	// Преобразуем аргументы к нужному типу
	thread_args *param = (thread_args *) arg;
	
    // Если не было передано аргументов, то завершаем работу потока
	if (param == NULL) {
		cout << "No args to work" << endl;
		return NULL;
	} else {
        //cout << "Input number: " <<*(param->number_ptr) << endl;
        *param->number_ptr = param->func_ptr(param->number_ptr);
    }
	
	return NULL;
}

float executable_function(float* input_number) {
    return sqrt(*input_number);
}

void initialize_array(unsigned int length, float *arr) {
    for (unsigned int i = 0; i < length; i++) {
        arr[i] = (float)i;
    }
}

int main()
{
	int err;

    pthread_t* thread_arr = new pthread_t[ARRAY_LENGTH];
    
    float* arr = new float[ARRAY_LENGTH];
    initialize_array(ARRAY_LENGTH, arr);
    
    thread_args* targs_arr = new thread_args[ARRAY_LENGTH];
    
    for (unsigned int i = 0; i < ARRAY_LENGTH; i++) {
        
        //thread_args targs;
        targs_arr[i].func_ptr = &executable_function;
        targs_arr[i].number_ptr = arr + i;

        err = pthread_create(thread_arr + i, NULL, thread_job, (void*) (targs_arr + i));
        if (err != 0) {
			cout << "Cannot create a thread: " << strerror(err) << endl;
			exit(-1);
		}
    }

    for (unsigned int i = 0; i < ARRAY_LENGTH; i++) {
        pthread_join(thread_arr[i], NULL);

        cout << "arr[" << i << "] = " << arr[i] << endl;
    }
    
    delete[] arr;
    delete[] targs_arr;

	pthread_exit(NULL);
    
    
    
}
