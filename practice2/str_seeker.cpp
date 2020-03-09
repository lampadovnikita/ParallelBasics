#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <experimental/filesystem>
#include <iomanip>
#include <pthread.h>

using namespace std;
namespace fs = std::experimental::filesystem;

const string requiredExtension = ".txt";


class ThreadPool {

private:
    vector<pthread_t> mThreads;

     queue<void*> mTaskArgs;

     void* (*mTaskPtr) (void*);

    pthread_mutex_t mutex;

     void* thread_job(void* classContext) {
        int err;
        size_t queueSize;
        void* taskArg;
        while(true) {
            pthread_mutex_lock(&mutex);
            if (err != 0) {
                cout << "Cannot lock mutex" << endl;
                exit(-1);
            }

            queueSize = mTaskArgs.size();
            if (queueSize > 0) {
                taskArg = mTaskArgs.front();
                mTaskArgs.pop();
            }

            pthread_mutex_unlock(&mutex);
            if (err != 0) {
                cout << "Cannot unlock mutex" << endl;
                exit(-1);
            }

            if (queueSize > 0) {
                mTaskPtr(taskArg);
            } else {
                return NULL;
            }
        }
    }

public:
    ThreadPool(void* (*taskPtr) (void*), const size_t threadCount) {
        int err;
              // Инициализируем мьютекс
        err = pthread_mutex_init(&mutex, NULL);
        if (err != 0) {
            cout << "Cannot initialize mutex" << endl;
            exit(-1);
        }

        mTaskPtr = taskPtr;

        for (size_t i = 0; i < threadCount; i++) {
            pthread_t thread;
            mThreads.push_back(thread);
        }

        // for (size_t i = 0; i < threadCount; i++) { 
        //     err = pthread_create(&mThreads[i], NULL, taskPtr, arg);
        //     // Если при создании потока произошла ошибка, выводим
        //     // сообщение об ошибке и прекращаем работу программы
        //     if (err != 0) {
        //         cout << "Cannot create a thread: " << mThreads[i] << endl;
        //         exit(-1); 
        //     }
        // }

        // delete[] mThreads;
    }

    void addTask(void* taskArg) {
        mTaskArgs.push(taskArg);
    }

    void run() {
        int err;
        for (size_t i = 0; i < mThreads.size(); i++) {
            err = pthread_create(&mThreads[i], NULL, (void*(*)(void*))&ThreadPool::thread_job, (void*) this);
            // Если при создании потока произошла ошибка, выводим
            // сообщение об ошибке и прекращаем работу программы
            if (err != 0) {
                cout << "Cannot create a thread: " << mThreads[i] << endl;
                exit(-1); 
            }
        }

        for (size_t i = 0; i < mThreads.size(); i++) {
            pthread_join(mThreads[i], NULL);
        }
    }
};

bool seekLines(const string &rFileName, const string &rSeeked, vector<size_t> &rLineNumbers) {
    ifstream fileInput(rFileName, ios_base::in);
    if (fileInput.is_open() == false) {
        return false;
    }

    size_t curLine = 0;
    string lineBuf;
    while(getline(fileInput, lineBuf)) {
        curLine++;

        if (lineBuf.find(rSeeked, 0) != string::npos) {
            rLineNumbers.push_back(curLine);
        }
    }

    fileInput.close();

    return true;
}

void collectPaths(const string &rRootPath, const string &rExtension, vector<string> &rPathsContainer) {
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

struct ThreadParams {
    string *filePath;
    string *seeked;
    vector<size_t> *lineNumbers;
};

void* mock(void* arg)
{
    ThreadParams* params = (ThreadParams*) arg;

    int err;
	pthread_t thread;
	thread = pthread_self();


    cout << *params->filePath << endl;

	return NULL;
}

int main(int argc, char* argv[]) {

    pthread_mutex_t mutex;
    // Инициализируем мьютекс
    int err = pthread_mutex_init(&mutex, NULL);
    if (err != 0)
        cout << "fuuuu" << endl;

    if (argc < 3) {
        cout << "Too few program arguments:" << endl;
        
        if (argc < 2) {
            cout << "Need to specify root folder for seek." << endl;
        }

        cout << "Need to specify seeked string." << endl;

        exit(0);
    }
     
    
    string seekedStr = argv[1];    
    string rootSeekPath = "/home/nikita/testseek/";//argv[2];

    vector<size_t> entryLineNumbers;
    vector<string> filePaths;
    collectPaths(rootSeekPath, requiredExtension, filePaths);

    // for (size_t i = 0; i < pathsForSeek.size(); i++) {
    //     if (seekLines(pathsForSeek[i], seekedStr, entryLineNumbers) != true) {
    //         cout << "Can't open file " << pathsForSeek[i] << endl;
    //         exit(-1);
    //     }
        
    //     cout << pathsForSeek[i] << ":" << endl;
        
    //     if (!entryLineNumbers.empty()) {
    //         for (size_t j = 0; j < entryLineNumbers.size(); j++) {
    //             cout << entryLineNumbers[j] << endl;
    //         }

    //         entryLineNumbers.clear();
    //     } else {
    //         cout << "No entries found." << endl;
    //     }

    //     cout << endl;
    // }

    vector<vector<size_t>> lineNumbers;
    lineNumbers.resize(filePaths.size());

    ThreadPool threadPool(&mock, 3);
    
    ThreadParams* params = new ThreadParams[10];
    for (size_t i = 0; i < filePaths.size(); i++) {
        params[i].filePath = &filePaths[i];
        params[i].seeked = &seekedStr;
        params[i].lineNumbers = &lineNumbers[i];

        threadPool.addTask((void*) &params[i]);
    }

    threadPool.run();

    for (size_t i = 0; i < filePaths.size(); i++) {
        
    }

    delete[] params;
}

