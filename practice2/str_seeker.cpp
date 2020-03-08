#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <experimental/filesystem>
#include <iomanip>

using namespace std;
namespace fs = std::experimental::filesystem;

const string requiredExtension = ".txt";

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
    string rootSeekPath = "/home/nikita/testseek/";//argv[2];

    vector<size_t> entryLineNumbers;
    vector<string> pathsForSeek;
    collectPaths(rootSeekPath, requiredExtension, pathsForSeek);

    for (size_t i = 0; i < pathsForSeek.size(); i++) {
        if (seekLines(pathsForSeek[i], seekedStr, entryLineNumbers) != true) {
            cout << "Can't open file " << pathsForSeek[i] << endl;
            exit(-1);
        }
        
        cout << pathsForSeek[i] << ":" << endl;
        
        if (!entryLineNumbers.empty()) {
            for (size_t j = 0; j < entryLineNumbers.size(); j++) {
                cout << entryLineNumbers[j] << endl;
            }

            entryLineNumbers.clear();
        } else {
            cout << "No entries found." << endl;
        }

        cout << endl;
    }

    exit(0);
}