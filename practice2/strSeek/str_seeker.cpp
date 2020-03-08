#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

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

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Too few program arguments:" << endl;
        
        if (argc < 2) {
            cout << "Need to specify root folder for seek.";
        }

        cout << "Need to specify seeked string." << endl;

        exit(0);
    }

    string seeked = argv[1];    
    string seekPath = argv[2];
    vector<size_t> lineNumbers;
    //string fileName = "example.txt";
    
    // if (seekLines(fileName, seeked, lineNumbers) == false) {
    //     cout << "Can't open file: " << fileName << endl;
    //     exit(-1);
    // }

    // for (size_t i = 0; i < lineNumbers.size(); i++) {
    //     cout << lineNumbers[i] << endl;
    // }

    exit(0);
}