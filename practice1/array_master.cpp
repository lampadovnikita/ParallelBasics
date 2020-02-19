#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <chrono>
#include <cmath>

using namespace std;

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
    unsigned int array_length;
    cout << "Введите число потоков:";
    cin >> array_length;


    int err;

    pthread_t* thread_arr = new pthread_t[array_length];
    
    float* number_arr = new float[array_length];
    initialize_array(array_length, number_arr);
    
    thread_args* targs_arr = new thread_args[array_length];
    
    for (unsigned int i = 0; i < array_length; i++) {
        targs_arr[i].func_ptr = &executable_function;
        targs_arr[i].number_ptr = number_arr + i;

        err = pthread_create(thread_arr + i, NULL, thread_job, (void*) (targs_arr + i));
        if (err != 0) {
			cout << "Cannot create a thread: " << strerror(err) << endl;
			exit(-1);
		}
    }

    for (unsigned int i = 0; i < array_length; i++) {
        pthread_join(thread_arr[i], NULL);

        cout << "arr[" << i << "] = " << number_arr[i] << endl;
    }
    
    delete[] number_arr;
    delete[] targs_arr;
    delete[] thread_arr;
    
	pthread_exit(NULL);
}
