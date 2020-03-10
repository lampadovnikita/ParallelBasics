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

// Мьютекс для синхронного доступа к очереди в reduce-функцие
pthread_mutex_t mMutex;

// Спинлок для синхронизации работы с контейнерами
pthread_spinlock_t mSpinlock;

// Очередь, в которую помещаем ключи для reduce-функции
queue<char> mKeyQueue;

// Структура, передаваемая в качестве аргумента потока map-функции
struct MapParams {
    // Указатель на элемент массива, с которого поток начнёт применять функцию поэлементно
	float *pFirstElem;
    // Количество элементов на поток
    size_t batchSize;
    // Указатель на контейнер, в который будет записа промежуточный результат map-функции
    multimap<char, float> *pContainer;
};

// Структура, передаваемая в качестве аргумента потока reduce-функции
struct ReduceParams {
    // Контейнер, к которому применяем reduce-функцию
    multimap<char, float> *pSourceContainer;

    // Контейнер, в который записываем результат reduce-функции
    map<char, float> *pTargetContainer;
};

// Map-функция
void *myMap(void *arg) {
    MapParams* params = (MapParams *)arg;

    float result;
    // Считаем значение синуса аоэлементно в заданном промежутке
    for (size_t i = 0; i < params->batchSize; i++) {
        result = sin((params->pFirstElem)[i]);
    
        // Если синус положительный
        if (result >= 0.0f) {
            pthread_spin_lock(&mSpinlock);

            // Записываем в промежуточный контейнер с ключом 'p'
            params->pContainer->insert(pair<char, float>('p', result));
            
            pthread_spin_unlock(&mSpinlock);

        } else {
            pthread_spin_lock(&mSpinlock);

            // Иначе записываем с ключом 'n'
            params->pContainer->insert(pair<char, float>('n', result));
            
            pthread_spin_unlock(&mSpinlock);
        }
    }

    return NULL;
}

// Reduce-функция
void *myReduce(void *arg) {
    int err;
    ReduceParams* params = (ReduceParams *) arg;

    int queueSize;

    // Ключ по которому будет применена reduce-функция
    char key;
    // Значение которое данная reduce-фукция присвоит заданному ключу
    float value;
    while(true) {
        // Захватываем мьютекс, чтобы синхронизировать доступ к очереди
        pthread_mutex_lock(&mMutex);
        if (err != 0)
            err_exit(err, "Cannot lock mutex"); 
        
        queueSize = mKeyQueue.size();
        // Если очередь не пуста, то поток берёт новый ключ
        if (queueSize > 0) {
            key = mKeyQueue.front();
            mKeyQueue.pop();
        }
        
        // Освобождаем мьютекс, т.к. вся необходимая информация получена
        pthread_mutex_unlock(&mMutex);
        if (err != 0)
            err_exit(err, "Cannot unlock mutex");

        // Если была взят ключ, т.е. очередь не была пуста во время обращения
        // в критической секции, то выполняем reduce с заданным ключом
        // Иначе завершаем работу потока, т.к. более нет ключей
        if (queueSize > 0) {
            value = 0.0f;
            // Определяем диапазон элементов с текущим ключом
            auto range = params->pSourceContainer->equal_range(key);
            // Получаем сумму всех значений с заданным ключом
            for (auto it = range.first; it != range.second; it++) {
                value += it->second;
            }

            pthread_spin_lock(&mSpinlock);

            // Записываем (ключ, значение) в контейнер-результат
            params->pTargetContainer->insert(pair<char, float>(key, value));
            
            pthread_spin_unlock(&mSpinlock);

        } else {
            return NULL;
        }
    }
}

// Основная функия map-reduce
std::map<char, float> mapReduce(float *arr, size_t arrLength, void *(*pMap)(void *), void *(*pReduce)(void *), size_t threadCount) {
    
    int err = pthread_mutex_init(&mMutex, NULL);
    if (err != 0)
        err_exit(err, "Cannot initialize mutex");

    err = pthread_spin_init(&mSpinlock, PTHREAD_PROCESS_PRIVATE);
    if (err != 0)
        err_exit(err, "Cannot initialize spinlock");

    if (threadCount > arrLength) {
        threadCount = arrLength;
    }
    
    // Определяем кол-во элементов для обработки на один поток
    size_t batchSize = arrLength / threadCount;
    // Количество элементов, которые не получилось равномерно разделить между потоками.
    // Потом добавим их в последний поток
    size_t rest = arrLength % threadCount;

    // Потоки
    vector<pthread_t> threads(threadCount);
    
    // Здесь будет храниться результат map-функции
    multimap<char, float> resMap;

    // Параметры для map-функции на каждый поток
    MapParams *mapParams = new MapParams[threadCount];

    // Запускаем map-функции в потоках с заданными параметрами
    for (size_t i = 0; i < threads.size(); i++) {

        mapParams[i].pFirstElem = &arr[i * batchSize];
        mapParams[i].batchSize = batchSize;
        if (i == threadCount - 1) {
            mapParams[i].batchSize += rest;
        }
        mapParams[i].pContainer = &resMap;

        err = pthread_create(&threads[i], NULL, pMap, &mapParams[i]);
        if (err != 0)
            err_exit(err, "Cannot create thread");
    }

    // Ждём завершения map-функций
    for (size_t i = 0; i < threads.size(); i++) {
        pthread_join(threads[i], NULL);
    }

    cout << "Multimap content:" << endl;
    for (const auto &i: resMap) {
        cout << "{ Key: " << i.first << "; Value = " << i.second << " }" << endl;
    }

    delete[] mapParams;
    
    // Добавляем ключи для reduce-функции
    mKeyQueue.push('n');
    mKeyQueue.push('p');

    // Здесь будет хранится результат reduce-функции = итоговый результат
    map<char, float> resReduce;
    // Параметры для каждого потока одинаковы
    ReduceParams reduceParams;
    reduceParams.pSourceContainer = &resMap;
    reduceParams.pTargetContainer = &resReduce;

    // Запускаем reduce-функции в потоках
    for (size_t i = 0; i < threadCount; i++) {
        err = pthread_create(&threads[i], NULL, pReduce, &reduceParams);
        if (err != 0)
            err_exit(err, "Cannot create thread");
    }

    // ждём завершения reduce-функции
    for (size_t i = 0; i < threads.size(); i++) {
        pthread_join(threads[i], NULL);
    }

    // Освобождаем ресурсы, связанные с мьютексом и спинлоком
    pthread_mutex_destroy(&mMutex);
    pthread_spin_destroy(&mSpinlock);

    return resReduce;
}

// Функция для заполнения массива
void fillArr(float *arr, size_t arrLength) {
    float currValue = 0.0;
    for (size_t i = 0; i < arrLength; i++) {
        arr[i] =  currValue;

        currValue += 0.5f;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cout << "Too few program arguments:" << endl;
        
        if (argc < 2) {
            cout << "Need to specify array length." << endl;
        }

        cout << "Need to specify count of threads." << endl;

        exit(0);
    }

    // Размер массива
    size_t arrLength = atoi(argv[1]);    
    // Кол-во потоков
    size_t threadCount = atoi(argv[2]);
    
    // Создаём и заполняем массив
    float* arr = new float[arrLength];
    fillArr(arr, arrLength);
    
    // применяем к массиву map-reduce
    // В нашем конкретном примере map функция считает синус каждого числа
    // и помещает его контейнер, присваивая ключ 'p', если значение синуса >= 0,
    // иначе - ключ 'n'.
    // Reduce-функция суммирует элементы с одинаковым ключом, т.е. получаем две суммы,
    // одна с положительными эл-тами, другая с отрицательными
    map<char, float> result = mapReduce(arr, arrLength, &myMap, &myReduce, threadCount);

    cout << "Result map:" << endl;
    for (const auto &i: result) {
        cout << "{ Key: " << i.first << "; Value: " << i.second << " }" << endl;
    }

    delete[] arr;
}