#include <pthread.h>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>

using namespace std;

#define err_exit(code, str)                   \
    {                                         \
        cerr << str << ": " << strerror(code) \
             << endl;                         \
        exit(EXIT_FAILURE);                   \
    }

// Типы синхронизации потоков
// LACK - отсутствие синхронизации
enum SyncTypes { LACK, MUTEX, SPINLOCK };  

// Тип синхронизации, который мы используем 
const SyncTypes mSyncType = SyncTypes::MUTEX;

pthread_mutex_t mMutex;
pthread_spinlock_t mSpinlock;

// Функция, которая имитирует долгие вычисления
void do_task()
{
    float result = 0;

    int repeat_count = 1000000;
    for (int i = 0; i < repeat_count; i++) {
        result += sin(i);
    }
}

// Функция-поток
void *thread_job(void *params)
{
    int err;
    
    pthread_t thread = pthread_self();

    // Блокируем участок используемым типом синхронизации
    if (mSyncType == SyncTypes::MUTEX) {
        err = pthread_mutex_lock(&mMutex);
        if (err != 0)
            err_exit(err, "Cannot lock mutex");
    }     
    else if (mSyncType == SyncTypes::SPINLOCK) {
        err = pthread_spin_lock(&mSpinlock);
        if (err != 0)
            err_exit(err, "Cannot lock mutex");
    }

    do_task();

    // Освобождаем участок используемым типом синхронизации
    if (mSyncType == SyncTypes::MUTEX) {
        err = pthread_mutex_unlock(&mMutex);
        if (err != 0)
            err_exit(err, "Cannot unlock mutex");    
    }
    else if (mSyncType == SyncTypes::SPINLOCK) {
        err = pthread_spin_unlock(&mSpinlock);
        if (err != 0)
            err_exit(err, "Cannot unlock spinlock");
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cout << "Too few program arguments:" << endl;
        cout << "Need to specify the count of threads." << endl;

        exit(0);
    }

    // Количество потоков считываем как аргумент программы
    int threads_count = atoi(argv[1]);
    if (threads_count <= 0) {
        cout << "Count of threads must be > 0" << endl;

        exit(0);
    }

    // Код ошибки
    int err;
    // Идентификаторы потоков
    pthread_t* threads = new pthread_t[threads_count];

    // Инициализируем мьютекс
    err = pthread_mutex_init(&mMutex, NULL);
    if (err != 0)
        err_exit(err, "Cannot initialize mutex");

    // Инициализируем спинлок    
    err = pthread_spin_init(&mSpinlock, PTHREAD_PROCESS_PRIVATE);
    if (err != 0)
        err_exit(err, "Cannot initialize spinlock");
    
    // Начинаем замер времени как раз до создания первого потока
    auto begin = chrono::steady_clock::now();

    // Создаём потоки
    for (int i = 0; i < threads_count; i++) {
        err = pthread_create(threads + i, NULL, &thread_job, NULL);
        if (err != 0) {
            char err_str[35];
            snprintf(err_str, strlen(err_str) * sizeof(char), "Cannot create thread %d", i);
            err_exit(err, err_str);
        }
    }

    // Дожидаемся завершения всех потоков
    for (int i = 0; i < threads_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // Заканчиваем замер времени после завершения всех потоков
    auto end = chrono::steady_clock::now();
    
    auto elapsed_time = chrono::duration_cast<std::chrono::microseconds>(end - begin);
    cout << "A program was running " << elapsed_time.count() << " us" <<  endl;

    // Освобождаем ресурсы, связанные с мьютексом и спинлоком
    pthread_mutex_destroy(&mMutex);
    pthread_spin_destroy(&mSpinlock);

    // Освобождаем ресурсы связанные с хранением информации о потоках
    delete[] threads;
}