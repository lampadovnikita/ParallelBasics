#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <experimental/filesystem>
#include <iomanip>
#include <cstring>
#include <pthread.h>

#define err_exit(code, str)                   \
    {                                         \
        cerr << str << ": " << strerror(code) \
             << endl;                         \
        exit(EXIT_FAILURE);                   \
    }

using namespace std;
namespace fs = std::experimental::filesystem;

// Расширение файлов, которые будем просматривать
const string requiredExtension = ".txt";

// Класс для работы с пулом потоков
class ThreadPool {

private:
    // Идентификаторы потоков в пуле
    vector<pthread_t> mThreads;

    // Очередь, которая содержит аргументы задач = очередь задач
    queue<void*> mTaskArgs;

    // Указатель на функцию-задачу
    void* (*mTaskPtr) (void*);

    pthread_mutex_t mMutex;

    // Внутренняя функция для потоков из пула
    void* threadJob(void* classContext) {
        int err;
        size_t queueSize;
        void* taskArg;

        while(true) {
            // Захватываем мьютекс, чтобы синхронизировать доступ к очереди задач
            pthread_mutex_lock(&mMutex);
            if (err != 0)
                err_exit(err, "Cannot lock mutex"); 
            
            queueSize = mTaskArgs.size();
            // Если очередь не пуста, то поток берёт аргументы новой задачи
            if (queueSize > 0) {
                taskArg = mTaskArgs.front();
                mTaskArgs.pop();
            }
            
            // Освобождаем мьютекс, т.к. вся необходимая информация получена
            pthread_mutex_unlock(&mMutex);
            if (err != 0)
                err_exit(err, "Cannot unlock mutex");

            // Если была взята задача, т.е. очередь не была пуста во время обращения
            // в критической секции, то выполняем задачу со взятыми аргументами
            // Иначе завершаем работу потока, т.к. более нет задач
            if (queueSize > 0) {
                mTaskPtr(taskArg);
            } else {
                return NULL;
            }
        }
    }

public:
    // В конструкторе задаём указатель на функцию-задачу и число потоков
    ThreadPool(void* (*taskPtr) (void*), const size_t threadCount) {

        int err = pthread_mutex_init(&mMutex, NULL);
        if (err != 0)
            err_exit(err, "Cannot initialize mutex");

        mTaskPtr = taskPtr;

        for (size_t i = 0; i < threadCount; i++) {
            pthread_t thread;
            mThreads.push_back(thread);
        }
    }

    // Добавляем задачу в пул
    void addTask(void* taskArg) {
        mTaskArgs.push(taskArg);
    }

    // Запускаем работу потоков
    void run() {
        int err;
        for (size_t i = 0; i < mThreads.size(); i++) {
            err = pthread_create(&mThreads[i], NULL, (void*(*)(void*))&ThreadPool::threadJob, (void*) this);
            if (err != 0)
                err_exit(err, "Cannot create thread");            
        }

        // Дожидаемся завершения всех потоков
        for (size_t i = 0; i < mThreads.size(); i++) {
            err = pthread_join(mThreads[i], NULL);
            if (err != 0)
                err_exit(err, "Cannot join thread");
        }
    }
};

// Функция, которая записывает пути ко всем необходимым файлам
// rRootPath - ссылка на строку с директорией, в которой начинается поиск
// rExtension - ссылка на строку с расширением, которое будем учитывать при поиске файлов
// rPathContainer - ссылка на вектор, в который будут записаны пути к найденым файлам
void collectPaths(const string &rRootPath, const string &rExtension, vector<string> &rPathsContainer) {
    // Используем рекурсивный итератор, чтобы обойти все внутренние директории
    for (auto itEntry = fs::recursive_directory_iterator(rRootPath);
         itEntry != fs::recursive_directory_iterator(); 
         ++itEntry) {

        const auto filePath = itEntry->path();
        const string fileExtension = filePath.extension().string();
        if (fileExtension == rExtension) {
            rPathsContainer.push_back(filePath.string());
        }
    }
}

// Структура, которая соержит параметры, необходимые для выполненния одной
// конкретной задачи пулом потоков
struct ThreadParams {
    // Путь к файлу
    string *pFilePath;
    // Искомая подстрока
    string *pSeeked;
    // Вектор, в который будут записаны номера строк со вхождениями подстроки
    vector<size_t> *pLineNumbers;
};

// Функция-задача, которая выполняется потоком из пула
void* seekSubstr(void *pArg) {
    // Распаковываем параметры из аргумента функции
    ThreadParams* params = (ThreadParams*) pArg;
    string filePath = *params->pFilePath;
    string seeked = *params->pSeeked;
    vector<size_t> *pLineNumbers = params->pLineNumbers;

    ifstream fileInput(filePath, ios_base::in);
    if (fileInput.is_open() == false) {
        return NULL;
    }

    size_t curLine = 0;
    string lineBuf;
    // Пока можем считать строку из файла
    while(getline(fileInput, lineBuf)) {
        // Увеличиваем счётчик строк
        curLine++;

        // Если нашли вхождение подстроки в текущей строке, 
        // то добавляем номер текущей строки
        if (lineBuf.find(seeked, 0) != string::npos) {
            (*pLineNumbers).push_back(curLine);
        }
    }

    fileInput.close();

	return NULL;
}

int main(int argc, char* argv[]) {
    // Необходимо как минимум 3 аргумента
    if (argc < 3) {
        cout << "Too few program arguments:" << endl;
        
        if (argc < 2) {
            cout << "Need to specify root folder for seek." << endl;
        }

        cout << "Need to specify seeked string." << endl;

        exit(0);
    }
    // Искомая подстрока
    string seekedStr = argv[1];    
    
    // Директория, в которой будем искать файлы
    string rootSeekPath = argv[2];
    if (!fs::exists(rootSeekPath)) {
        cout << rootSeekPath << " directory doesn't exist." << endl;
        exit(0);
    }

    // Количество потоков в пуле
    size_t threadCount = argc > 4 ? argc : 1;

    // Ищем нужные нам файлы
    vector<string> filePaths;
    collectPaths(rootSeekPath, requiredExtension, filePaths);

    // Двумерный вектор, каждый подвектор которого содержит
    // номера искомых строк в i-ом файле
    vector<vector<size_t>> entryLineNumbers;
    entryLineNumbers.resize(filePaths.size());

    // Инициализируем наш пул потоков
    ThreadPool threadPool(&seekSubstr, threadCount);

    ThreadParams* params = new ThreadParams[filePaths.size()];
    for (size_t i = 0; i < filePaths.size(); i++) {
        // Инициализируем параметры каждой задачи
        params[i].pFilePath = &filePaths[i];
        params[i].pSeeked = &seekedStr;
        params[i].pLineNumbers = &entryLineNumbers[i];

        // Добавляем задачу в пул
        threadPool.addTask((void*) &params[i]);
    }

    // Запускаем пул потоков
    threadPool.run();

    // Выводим информацию о вхождениях подстроки в каждом подходящем файле
    for (size_t i = 0; i < entryLineNumbers.size(); i++) {
        cout << filePaths[i] << ":" << endl;
        
        if (!entryLineNumbers[i].empty()) {
            for (size_t j = 0; j < entryLineNumbers[i].size(); j++) {
                cout << entryLineNumbers[i][j] << endl;
            }
        } else {
            cout << "No entries found." << endl;
        }

        cout << endl;
    }

    delete[] params;
}

