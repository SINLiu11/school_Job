#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include <climits>
#include <sstream>

using namespace std;

vector<int> experimentData1;
vector<int> beladyData;

void initData() {
    int data1[] = { 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 };
    int data2[] = { 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 };

    experimentData1.assign(data1, data1 + sizeof(data1) / sizeof(data1[0]));
    beladyData.assign(data2, data2 + sizeof(data2) / sizeof(data2[0]));
}

string intToString(int num) {
    stringstream ss;
    ss << num;
    return ss.str();
}

int OPT(const vector<int>& pages, int frames) {
    set<int> memory;
    map<int, int> nextUse;
    int pageFaults = 0;

    for (int i = 0; i < (int)pages.size(); i++) {
        int page = pages[i];

        if (memory.find(page) == memory.end()) {
            pageFaults++;

            if ((int)memory.size() == frames) {
                int farthest = -1, pageToRemove = -1;

                for (set<int>::iterator it = memory.begin(); it != memory.end(); ++it) {
                    int p = *it;
                    int nextPos = INT_MAX;
                    for (int j = i + 1; j < (int)pages.size(); j++) {
                        if (pages[j] == p) {
                            nextPos = j;
                            break;
                        }
                    }

                    if (nextPos == INT_MAX) {
                        pageToRemove = p;
                        break;
                    }

                    if (nextPos > farthest) {
                        farthest = nextPos;
                        pageToRemove = p;
                    }
                }

                memory.erase(pageToRemove);
            }

            memory.insert(page);
        }
    }

    return pageFaults;
}

int FIFO(const vector<int>& pages, int frames) {
    queue<int> memoryQueue;
    set<int> memorySet;
    int pageFaults = 0;

    for (int i = 0; i < (int)pages.size(); i++) {
        int page = pages[i];

        if (memorySet.find(page) == memorySet.end()) {
            pageFaults++;

            if ((int)memoryQueue.size() == frames) {
                int oldest = memoryQueue.front();
                memoryQueue.pop();
                memorySet.erase(oldest);
            }

            memoryQueue.push(page);
            memorySet.insert(page);
        }
    }

    return pageFaults;
}

int LRU(const vector<int>& pages, int frames) {
    vector<int> memory;
    map<int, int> recentUse;
    int time = 0;
    int pageFaults = 0;

    for (int i = 0; i < (int)pages.size(); i++) {
        int page = pages[i];
        time++;

        bool found = false;
        for (int j = 0; j < (int)memory.size(); j++) {
            if (memory[j] == page) {
                found = true;
                break;
            }
        }

        if (!found) {
            pageFaults++;

            if ((int)memory.size() == frames) {
                int lruPage = -1;
                int minTime = INT_MAX;

                for (int j = 0; j < (int)memory.size(); j++) {
                    int p = memory[j];
                    if (recentUse[p] < minTime) {
                        minTime = recentUse[p];
                        lruPage = p;
                    }
                }

                for (vector<int>::iterator it = memory.begin(); it != memory.end(); ++it) {
                    if (*it == lruPage) {
                        memory.erase(it);
                        break;
                    }
                }
            }

            memory.push_back(page);
        }

        recentUse[page] = time;
    }

    return pageFaults;
}

void printLine(int length) {
    for (int i = 0; i < length; i++) {
        cout << "=";
    }
    cout << endl;
}

void printHeader(const string& title) {
    cout << endl;
    printLine(60);
    cout << " " << title << endl;
    printLine(60);
}

void printResults(const vector<int>& frames, const vector<int>& optResults,
    const vector<int>& fifoResults, const vector<int>& lruResults) {
    cout << "\n页面访问序列: ";
    for (int i = 0; i < (int)experimentData1.size(); i++) {
        cout << experimentData1[i] << " ";
    }
    cout << endl;

    cout << "\n" << setw(10) << "页框数"
        << setw(15) << "OPT缺页数"
        << setw(15) << "FIFO缺页数"
        << setw(15) << "LRU缺页数" << endl;

    for (int i = 0; i < 55; i++) cout << "-";
    cout << endl;

    for (int i = 0; i < (int)frames.size(); i++) {
        cout << setw(10) << frames[i]
            << setw(15) << optResults[i]
            << setw(15) << fifoResults[i]
            << setw(15) << lruResults[i] << endl;
    }
}

void testBeladyPhenomenon() {
    printHeader("测试FIFO算法的Belady现象");

    cout << "Belady现象数据序列: ";
    for (int i = 0; i < (int)beladyData.size(); i++) {
        cout << beladyData[i] << " ";
    }
    cout << endl;

    cout << "\n测试不同页框数下的FIFO算法缺页次数:" << endl;
    cout << setw(10) << "页框数" << setw(15) << "缺页次数" << endl;

    for (int i = 0; i < 25; i++) cout << "-";
    cout << endl;

    vector<int> frameSizes;
    frameSizes.push_back(2);
    frameSizes.push_back(3);
    frameSizes.push_back(4);
    frameSizes.push_back(5);

    vector<int> faults;

    for (int i = 0; i < (int)frameSizes.size(); i++) {
        int frames = frameSizes[i];
        int faultCount = FIFO(beladyData, frames);
        faults.push_back(faultCount);
        cout << setw(10) << frames << setw(15) << faultCount << endl;
    }

    cout << "\n分析Belady现象:" << endl;
    bool hasBelady = false;
    for (int i = 1; i < (int)faults.size(); i++) {
        if (faults[i] > faults[i - 1]) {
            hasBelady = true;
            cout << "发现Belady现象: 页框从" << frameSizes[i - 1]
                << "增加到" << frameSizes[i]
                << "时，缺页次数从" << faults[i - 1]
                << "增加到" << faults[i] << endl;
        }
    }

    if (!hasBelady) {
        cout << "在此数据序列中未发现Belady现象" << endl;
    }
}

void demonstrateAlgorithm(const vector<int>& pages, int frames, const string& algorithm) {
    stringstream title;
    title << algorithm << "算法执行过程（页框数=" << frames << "）";
    printHeader(title.str());

    cout << "页面访问序列: ";
    for (int i = 0; i < (int)pages.size(); i++) {
        cout << pages[i] << " ";
    }
    cout << endl;

    int faults;
    if (algorithm == "FIFO") {
        faults = FIFO(pages, frames);
    }
    else if (algorithm == "LRU") {
        faults = LRU(pages, frames);
    }
    else {
        faults = OPT(pages, frames);
    }

    cout << "总缺页次数: " << faults << endl;
    double hitRate = (1.0 - (double)faults / pages.size()) * 100;
    cout << "命中率: " << fixed << setprecision(2) << hitRate << "%" << endl;
}

int main() {
    initData();

    vector<int> frameSizes;
    frameSizes.push_back(2);
    frameSizes.push_back(3);
    frameSizes.push_back(4);

    vector<int> optResults, fifoResults, lruResults;

    printHeader("实验(1): FIFO和LRU算法对比（以OPT为参考）");

    for (int i = 0; i < (int)frameSizes.size(); i++) {
        int frames = frameSizes[i];
        optResults.push_back(OPT(experimentData1, frames));
        fifoResults.push_back(FIFO(experimentData1, frames));
        lruResults.push_back(LRU(experimentData1, frames));
    }

    printResults(frameSizes, optResults, fifoResults, lruResults);

    printHeader("实验(2): FIFO算法在不同页框数下的表现");
    for (int i = 0; i < (int)frameSizes.size(); i++) {
        int frames = frameSizes[i];
        int faults = FIFO(experimentData1, frames);
        double hitRate = (1.0 - (double)faults / experimentData1.size()) * 100;
        cout << "页框数=" << frames << ": 缺页次数=" << faults
            << ", 命中率=" << fixed << setprecision(2) << hitRate << "%" << endl;
    }

    printHeader("实验(3): LRU算法在不同页框数下的表现");
    for (int i = 0; i < (int)frameSizes.size(); i++) {
        int frames = frameSizes[i];
        int faults = LRU(experimentData1, frames);
        double hitRate = (1.0 - (double)faults / experimentData1.size()) * 100;
        cout << "页框数=" << frames << ": 缺页次数=" << faults
            << ", 命中率=" << fixed << setprecision(2) << hitRate << "%" << endl;
    }

    testBeladyPhenomenon();

    cout << "\n\n是否查看算法详细执行过程？(y/n): ";
    char choice;
    cin >> choice;

    if (choice == 'y' || choice == 'Y') {
        cout << endl;
        demonstrateAlgorithm(experimentData1, 3, "FIFO");
        demonstrateAlgorithm(experimentData1, 3, "LRU");
        demonstrateAlgorithm(experimentData1, 3, "OPT");
    }

    cout << "\n实验完成！" << endl;

    return 0;
}
