#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <experimental/filesystem>
#include <iomanip>

using namespace std;
namespace fs = std::experimental::filesystem;

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
            cout << "Need to specify root folder for seek." << endl;
        }

        cout << "Need to specify seeked string." << endl;

        exit(0);
    }
     
    for (auto itEntry = fs::recursive_directory_iterator("~/testseek/");
         itEntry != fs::recursive_directory_iterator(); 
         ++itEntry) {
        const auto filenameStr = itEntry->path().filename().string();
        std::cout << std::setw(itEntry.depth()*3) << "";
        std::cout << "dir:  " << filenameStr << '\n';
    }
    string seeked = argv[1];    
    //string seekPath = "~/testseek/";//argv[2];

    vector<size_t> lineNumbers;

    exit(0);
}