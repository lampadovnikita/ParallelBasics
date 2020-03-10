#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include <queue>
#include <pthread.h>

using namespace std;

#define err_exit(code, str)                   \
    {                                         \
        cerr << str << ": " << strerror(code) \
             << endl;                         \
        exit(EXIT_FAILURE);                   \
    }

pthread_mutex_t mMutex;

queue<char> mKeyQueue;

// Структура, передаваемая в качестве аргумента потока
struct MapParams {
    // Указатель на элемент массива, с которого поток начнёт применять функцию поэлементно
	float *pFirstElem;
    // Количество элементов на поток
    size_t batchSize;
    // Указатель на контейнер
    multimap<char, float> *pContainer;
};

// Структура, передаваемая в качестве аргумента потока
struct ReduceParams {
    multimap<char, float> *pSourceContainer;

    // Указатель на контейнер
    map<char, float> *pTargetContainer;
};


void *myMap(void *arg) {
    MapParams* params = (MapParams *)arg;

    float result;
    for (size_t i = 0; i < params->batchSize; i++) {
        //cout << "i = " << i << endl;
        //cout << "arr[i] = " << *(params->pFirstElem + i) << endl; 
        result = sin((params->pFirstElem)[i]);

        //cout << "complete" << endl;
        //cout << "res[i] = " << (params->pFirstElem)[i] << endl; 

        if (result >= 0.0f) {
            params->pContainer->insert(pair<char, float>('p', result));
        } else {
            params->pContainer->insert(pair<char, float>('n', result));
        }
    }

    return NULL;
}

void *myReduce(void *arg) {
    int err;
    ReduceParams* params = (ReduceParams *) arg;

    float value;
    int queueSize;
    char key;
    while(true) {
        // Захватываем мьютекс, чтобы синхронизировать доступ к очереди задач
        pthread_mutex_lock(&mMutex);
        if (err != 0)
            err_exit(err, "Cannot lock mutex"); 
        
        queueSize = mKeyQueue.size();
        // Если очередь не пуста, то поток берёт аргументы новой задачи
        if (queueSize > 0) {
            key = mKeyQueue.front();
            mKeyQueue.pop();
        }
        
        // Освобождаем мьютекс, т.к. вся необходимая информация получена
        pthread_mutex_unlock(&mMutex);
        if (err != 0)
            err_exit(err, "Cannot unlock mutex");

        // Если была взята задача, т.е. очередь не была пуста во время обращения
        // в критической секции, то выполняем задачу со взятыми аргументами
        // Иначе завершаем работу потока, т.к. более нет задач
        if (queueSize > 0) {
            value = 0.0f;
            auto range = params->pSourceContainer->equal_range(key);
            for (auto it = range.first; it != range.second; it++) {
                value += it->second;
            }

            params->pTargetContainer->insert(pair<char, float>(key, value));
        } else {
            return NULL;
        }
    }
}

std::map<char, float> mapReduce(float *arr, size_t arrLength, void *(*pMap)(void *), void *(*pReduce)(void *), size_t threadCount) {
    
    int err = pthread_mutex_init(&mMutex, NULL);
    if (err != 0)
        err_exit(err, "Cannot initialize mutex");

    if (threadCount > arrLength) {
        threadCount = arrLength;
    }
    
    // Определяем кол-во элементов для обработки на один поток
    size_t batch_size = arrLength / threadCount;
    // Количество элементов, которые не получилось равномерно разделить между потоками.
    // Потом добавим их в последний поток
    size_t rest = arrLength % threadCount;

    vector<pthread_t> threads(threadCount);
    vector<MapParams> mapParamsVector;
    
    multimap<char, float> resMap;

    MapParams *mapParams = new MapParams[threadCount];

    for (size_t i = 0; i < threads.size(); i++) {

        mapParams[i].pFirstElem = &arr[i * batch_size];
        mapParams[i].batchSize = batch_size;
        if (i == threadCount - 1) {
            mapParams[i].batchSize += rest;
        }
        mapParams[i].pContainer = &resMap;

        err = pthread_create(&threads[i], NULL, pMap, &mapParams[i]);
        if (err != 0)
            err_exit(err, "Cannot create thread");
    }

    for (size_t i = 0; i < threads.size(); i++) {
        pthread_join(threads[i], NULL);
    }

    cout << "Multimap content:" << endl;
    for (const auto &i: resMap) {
        cout << "{ Key: " << i.first << "; Value = " << i.second << " }" << endl;
    }

    delete[] mapParams;
    
    mKeyQueue.push('n');
    mKeyQueue.push('p');

    map<char, float> resReduce;
    ReduceParams reduceParams;
    reduceParams.pSourceContainer = &resMap;
    reduceParams.pTargetContainer = &resReduce;

    for (size_t i = 0; i < threadCount; i++) {
        err = pthread_create(&threads[i], NULL, pReduce, &reduceParams);
        if (err != 0)
            err_exit(err, "Cannot create thread");
    }

    for (size_t i = 0; i < threads.size(); i++) {
        pthread_join(threads[i], NULL);
    }

    return resReduce;
}

void fillArr(float *arr, size_t arrLength) {
    float currValue = 0.0;
    for (size_t i = 0; i < arrLength; i++) {
        arr[i] =  currValue;

        currValue += 0.5f;
    }
}

int main(int argc, char *argv[]) {
    // Необходимо как минимум 3 аргумента
    if (argc < 3) {
        cout << "Too few program arguments:" << endl;
        
        if (argc < 2) {
            cout << "Need to specify array length." << endl;
        }

        cout << "Need to specify count of threads." << endl;

        exit(0);
    }

    size_t arrLength = atoi(argv[1]);    
    size_t threadCount = atoi(argv[2]);
    
    float* arr = new float[arrLength];
    fillArr(arr, arrLength);

    map<char, float> result = mapReduce(arr, arrLength, &myMap, &myReduce, threadCount);

    cout << "Result map:" << endl;
    for (const auto &i: result) {
        cout << "{ Key: " << i.first << "; Value: " << i.second << " }" << endl;
    }

    delete[] arr;
}