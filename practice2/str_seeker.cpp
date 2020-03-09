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
            if (err != 0)
                err_exit(err, "Cannot lock mutex");
            

            queueSize = mTaskArgs.size();
            if (queueSize > 0) {
                taskArg = mTaskArgs.front();
                mTaskArgs.pop();
            }

            pthread_mutex_unlock(&mutex);
            if (err != 0)
                err_exit(err, "Cannot unlock mutex");

            if (queueSize > 0) {
                mTaskPtr(taskArg);
            } else {
                return NULL;
            }
        }
    }

public:
    ThreadPool(void* (*taskPtr) (void*), const size_t threadCount) {
        int err = pthread_mutex_init(&mutex, NULL);
        if (err != 0)
            err_exit(err, "Cannot initialize mutex");

        mTaskPtr = taskPtr;

        for (size_t i = 0; i < threadCount; i++) {
            pthread_t thread;
            mThreads.push_back(thread);
        }
    }

    void addTask(void* taskArg) {
        mTaskArgs.push(taskArg);
    }

    void run() {
        int err;
        for (size_t i = 0; i < mThreads.size(); i++) {
            err = pthread_create(&mThreads[i], NULL, (void*(*)(void*))&ThreadPool::thread_job, (void*) this);
            if (err != 0)
                err_exit(err, "Cannot create thread");            
        }

        for (size_t i = 0; i < mThreads.size(); i++) {
            err = pthread_join(mThreads[i], NULL);
            if (err != 0)
                err_exit(err, "Cannot join thread");
        }
    }
};

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

void* seekSubstr(void* arg) {
    ThreadParams* params = (ThreadParams*) arg;
    string filePath = *params->filePath;
    string seeked = *params->seeked;
    vector<size_t> *lineNumbers = params->lineNumbers;

    ifstream fileInput(filePath, ios_base::in);
    if (fileInput.is_open() == false) {
        return NULL;
    }

    size_t curLine = 0;
    string lineBuf;
    while(getline(fileInput, lineBuf)) {
        curLine++;

        if (lineBuf.find(seeked, 0) != string::npos) {
            (*lineNumbers).push_back(curLine);
        }
    }

    fileInput.close();

	return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Too few program arguments:" << endl;
        
        if (argc < 2) {
            cout << "Need to specify root folder for seek." << endl;
        }

        cout << "Need to specify seeked string." << endl;

        exit(0);
    }
     
    string seekedStr = argv[1];    
    
    string rootSeekPath = argv[2];
    if (!fs::exists(rootSeekPath)) {
        cout << rootSeekPath << " directory doesn't exist." << endl;
        exit(0);
    }

    size_t threadCount = argc > 4 ? argc : 1;

    vector<string> filePaths;
    collectPaths(rootSeekPath, requiredExtension, filePaths);

    vector<vector<size_t>> entryLineNumbers;
    entryLineNumbers.resize(filePaths.size());

    ThreadPool threadPool(&seekSubstr, threadCount);
    
    ThreadParams* params = new ThreadParams[filePaths.size()];
    for (size_t i = 0; i < filePaths.size(); i++) {
        params[i].filePath = &filePaths[i];
        params[i].seeked = &seekedStr;
        params[i].lineNumbers = &entryLineNumbers[i];

        threadPool.addTask((void*) &params[i]);
    }

    threadPool.run();

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

