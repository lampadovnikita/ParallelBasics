#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include <pthread.h>

using namespace std;

#define err_exit(code, str)                   \
    {                                         \
        cerr << str << ": " << strerror(code) \
             << endl;                         \
        exit(EXIT_FAILURE);                   \
    }

pthread_mutex_t mMutex;

// Структура, передаваемая в качестве аргумента потока
struct MapParams {
    // Указатель на элемент массива, с которого поток начнёт применять функцию поэлементно
	float *pFirstElem;
    // Количество элементов на поток
    size_t batchSize;
    // Указатель на контейнер
    multimap<char, float> *pContainer;
};

void *myMap(void *arg) {
    MapParams* params = (MapParams *)arg;
    
    pthread_mutex_lock(&mMutex);

    float result;
    for (size_t i = 0; i < params->batchSize; i++) {
        //cout << "i = " << i << endl;
        //cout << "arr[i] = " << *(params->pFirstElem + i) << endl; 
        result = sin((params->pFirstElem)[i]);

        //cout << "complete" << endl;
        //cout << "res[i] = " << (params->pFirstElem)[i] << endl; 

        if (result >= 0.0f) {
            (*params->pContainer).insert(pair<char, float>('p', result));
        } else {
            (*params->pContainer).insert(pair<char, float>('n', result));
        }
    }

    pthread_mutex_unlock(&mMutex);

    return NULL;
}

void *myReduce(void *arg) {
    return NULL;
}

std::map<string, float> mapReduce(float *arr, size_t arrLength, void *(*pMap)(void *), void *(*pReduce)(void *), size_t threadCount) {
    
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
    // MapParams mapParams;
    
    multimap<char, float> resMap;

    MapParams *mapParams = new MapParams[threadCount];

    for (size_t i = 0; i < threads.size(); i++) {
        // mapParams.pFirstElem = &arr[i * batch_size];
        // mapParams.batchSize = batch_size;
        // if (i == threadCount - 1) {
        //     mapParams.batchSize += rest;
        // }
        // mapParams.pContainer = &resMap;
        //
        // mapParamsVector.push_back(mapParams);

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

    for (const auto &i: resMap) {
        cout << "First: " << i.first << "; Second = " << i.second << endl;
    }

    delete[] mapParams;
    
    std::map<string, float> result;
    return result;
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

    mapReduce(arr, arrLength, &myMap, &myReduce, threadCount);

    delete[] arr;
}