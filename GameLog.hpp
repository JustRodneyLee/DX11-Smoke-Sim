#include <string>
#include <vector>

using namespace std;

vector<string> gameLog;

void PrintLog(string str) {
    gameLog.push_back(str);
}

void ClearLog() {
    gameLog.clear();
}