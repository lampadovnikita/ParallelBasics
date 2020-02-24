#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <chrono>
#include <cmath>

using namespace std;

// Структура, передаваемая в качестве аргумента потока
struct thread_params {
    // Указатель на элемент массива, с которого поток начнёт применять функцию поэлементно
	float* first_number_ptr;
    // Количество элементов на поток
    unsigned int batch_size;
    // Указатель на функцию
    float (*func_ptr)(float);
};

// Функция, которую будет исполнять созданный поток
void* thread_job(void* arg)
{
    int err;

	// Преобразуем аргумент к нужному типу
	thread_params* params = (thread_params*) arg;
	
    // Если не было передано аргумента, то завершаем работу потока
	if (params == NULL) {
		cout << "No arg to work in thread" << endl;
		return NULL;
	} else {
        // Применяем функцию поэлементно
        for (unsigned int i = 0; i < params->batch_size; i++) {
            *(params->first_number_ptr + i) = params->func_ptr(*(params->first_number_ptr + i));
        }
    }
	
	return NULL;
}

// Функция, которую применяем над каждым элементом массива 
float executable_function(float input_number) {
    return sqrt(input_number);
}

// Иницифализация массива с числами
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

    // Определяем кол-во элементов для обработки на один поток
    unsigned int batch_size = array_length / threads_count;
    // Количество элементов, которые не получилось равномерно разделить между потоками.
    // Потом добавим их в последний поток
    unsigned int rest = array_length % threads_count;

    // Массив идентификаторов потоков
    pthread_t* thread_arr = new pthread_t[threads_count];
    
    // Массив чисел
    float* number_arr = new float[array_length];
    initialize_array(array_length, number_arr);
    
    // Массив параметров потоков
    thread_params* params_arr = new thread_params[threads_count];
    
    int err;
    for (unsigned int i = 0; i < threads_count; i++) {
        // Задаём параметры структуры, которую передадим в поток
        params_arr[i].func_ptr = &executable_function;
        params_arr[i].first_number_ptr = number_arr + i * batch_size;
        params_arr[i].batch_size = batch_size;
        // Если последний поток, то добавим ему на обработку остаток массива
        if (i == threads_count - 1) {
            params_arr[i].batch_size += rest;
        }

        err = pthread_create(thread_arr + i, NULL, thread_job, (void*) (params_arr + i));
        if (err != 0) {
			cout << "Cannot create a thread: " << strerror(err) << endl;
			exit(-1);
		}
    }

    // Ждём завершения всех дочерних потоков
    for (unsigned int i = 0; i < threads_count; i++) {
        pthread_join(thread_arr[i], NULL);
    }

    for (unsigned int i = 0; i < array_length; i++) {
        cout << "arr[" << i << "] = " << number_arr[i] << endl;
    }
    
    delete[] number_arr;
    delete[] params_arr;
    delete[] thread_arr;
}
