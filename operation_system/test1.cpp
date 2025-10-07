#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <queue>
#include <limits>

using namespace std;

// 作业结构体
struct Job {
    int id;             // 作业ID
    int arriveTime;     // 到达时间
    int runTime;        // 估计运行时间
    int startTime;      // 开始时间
    int finishTime;     // 结束时间
    int turnaroundTime; // 周转时间
    double weightedTurnaroundTime; // 带权周转时间
    bool completed;     // 是否完成
    double responseRatio; // 响应比（用于HRRN算法）

    Job(int i, int a, int r) : id(i), arriveTime(a), runTime(r),
        startTime(-1), finishTime(-1),
        turnaroundTime(0), weightedTurnaroundTime(0.0),
        completed(false), responseRatio(0.0) {
    }
};

// 调度算法基类
class Scheduler {
protected:
    vector<Job> jobs;
    int numJobs;

public:
    Scheduler(const vector<pair<int, int>>& jobList) {
        numJobs = jobList.size();
        for (int i = 0; i < numJobs; i++) {
            jobs.push_back(Job(i + 1, jobList[i].first, jobList[i].second));
        }
    }

    virtual void schedule() = 0;

    void printResults(const string& algorithmName, int numChannels) {
        double totalTurnaround = 0.0;
        double totalWeightedTurnaround = 0.0;

        cout << "\n" << algorithmName << "调度算法结果 (" << numChannels << "道环境):" << endl;
        cout << "==================================================================" << endl;
        cout << setw(4) << "作业" << setw(8) << "到达时间" << setw(8) << "运行时间"
            << setw(8) << "开始时间" << setw(8) << "结束时间"
            << setw(8) << "周转时间" << setw(12) << "带权周转时间" << endl;

        for (const auto& job : jobs) {
            totalTurnaround += job.turnaroundTime;
            totalWeightedTurnaround += job.weightedTurnaroundTime;

            cout << setw(4) << job.id << setw(8) << job.arriveTime
                << setw(8) << job.runTime << setw(8) << job.startTime
                << setw(8) << job.finishTime << setw(8) << job.turnaroundTime
                << setw(12) << fixed << setprecision(2) << job.weightedTurnaroundTime << endl;
        }

        cout << "==================================================================" << endl;
        cout << "平均周转时间: " << fixed << setprecision(2) << totalTurnaround / numJobs << endl;
        cout << "平均带权周转时间: " << fixed << setprecision(2) << totalWeightedTurnaround / numJobs << endl;
    }

    void resetJobs() {
        for (auto& job : jobs) {
            job.startTime = -1;
            job.finishTime = -1;
            job.turnaroundTime = 0;
            job.weightedTurnaroundTime = 0.0;
            job.completed = false;
            job.responseRatio = 0.0;
        }
    }

    virtual ~Scheduler() {} // 虚析构函数
};

// 单道环境调度器
class SingleChannelScheduler : public Scheduler {
public:
    SingleChannelScheduler(const vector<pair<int, int>>& jobList) : Scheduler(jobList) {}
};

// 单道FCFS调度器
class FCFSSingleChannelScheduler : public SingleChannelScheduler {
public:
    FCFSSingleChannelScheduler(const vector<pair<int, int>>& jobList)
        : SingleChannelScheduler(jobList) {
    }

    void schedule() override {
        // 按到达时间排序
        vector<Job> sortedJobs = jobs;
        sort(sortedJobs.begin(), sortedJobs.end(),
            [](const Job& a, const Job& b) { return a.arriveTime < b.arriveTime; });

        int currentTime = 0;

        for (int i = 0; i < numJobs; i++) {
            Job& job = sortedJobs[i];
            job.startTime = max(currentTime, job.arriveTime);
            job.finishTime = job.startTime + job.runTime;
            job.turnaroundTime = job.finishTime - job.arriveTime;
            job.weightedTurnaroundTime = static_cast<double>(job.turnaroundTime) / job.runTime;
            job.completed = true;

            currentTime = job.finishTime;

            // 更新原jobs数组
            for (int j = 0; j < numJobs; j++) {
                if (jobs[j].id == job.id) {
                    jobs[j] = job;
                    break;
                }
            }
        }
    }
};

// 单道SJF调度器
class SJFSingleChannelScheduler : public SingleChannelScheduler {
public:
    SJFSingleChannelScheduler(const vector<pair<int, int>>& jobList)
        : SingleChannelScheduler(jobList) {
    }

    void schedule() override {
        vector<Job> workingJobs = jobs; // 工作副本
        sort(workingJobs.begin(), workingJobs.end(),
            [](const Job& a, const Job& b) { return a.arriveTime < b.arriveTime; });

        int currentTime = 0;
        int completedCount = 0;

        while (completedCount < numJobs) {
            // 找到所有已到达且未完成的作业
            vector<Job*> availableJobs;
            for (auto& job : workingJobs) {
                if (!job.completed && job.arriveTime <= currentTime) {
                    availableJobs.push_back(&job);
                }
            }

            // 如果没有可用作业，推进时间
            if (availableJobs.empty()) {
                currentTime++;
                continue;
            }

            // 找到运行时间最短的作业
            Job* shortestJob = availableJobs[0];
            for (auto jobPtr : availableJobs) {
                if (jobPtr->runTime < shortestJob->runTime) {
                    shortestJob = jobPtr;
                }
            }

            // 执行最短作业
            shortestJob->startTime = currentTime;
            shortestJob->finishTime = currentTime + shortestJob->runTime;
            shortestJob->turnaroundTime = shortestJob->finishTime - shortestJob->arriveTime;
            shortestJob->weightedTurnaroundTime = static_cast<double>(shortestJob->turnaroundTime) / shortestJob->runTime;
            shortestJob->completed = true;
            completedCount++;

            // 更新时间
            currentTime = shortestJob->finishTime;

            // 更新原jobs数组
            for (int j = 0; j < numJobs; j++) {
                if (jobs[j].id == shortestJob->id) {
                    jobs[j] = *shortestJob;
                    break;
                }
            }
        }
    }
};

// 单道HRRN调度器
class HRRNSingleChannelScheduler : public SingleChannelScheduler {
private:
    double calculateResponseRatio(const Job& job, int currentTime) {
        if (job.arriveTime > currentTime) return 0;
        int waitTime = currentTime - job.arriveTime;
        return static_cast<double>(waitTime + job.runTime) / job.runTime;
    }

public:
    HRRNSingleChannelScheduler(const vector<pair<int, int>>& jobList)
        : SingleChannelScheduler(jobList) {
    }

    void schedule() override {
        vector<Job> workingJobs = jobs; // 工作副本
        sort(workingJobs.begin(), workingJobs.end(),
            [](const Job& a, const Job& b) { return a.arriveTime < b.arriveTime; });

        int currentTime = 0;
        int completedCount = 0;

        while (completedCount < numJobs) {
            // 找到所有已到达且未完成的作业
            vector<Job*> availableJobs;
            for (auto& job : workingJobs) {
                if (!job.completed && job.arriveTime <= currentTime) {
                    availableJobs.push_back(&job);
                }
            }

            // 如果没有可用作业，推进时间
            if (availableJobs.empty()) {
                currentTime++;
                continue;
            }

            // 计算并找到最高响应比的作业
            Job* highestRRJob = availableJobs[0];
            double highestRatio = calculateResponseRatio(*highestRRJob, currentTime);

            for (auto jobPtr : availableJobs) {
                double ratio = calculateResponseRatio(*jobPtr, currentTime);
                if (ratio > highestRatio) {
                    highestRatio = ratio;
                    highestRRJob = jobPtr;
                }
            }

            // 执行最高响应比作业
            highestRRJob->startTime = currentTime;
            highestRRJob->finishTime = currentTime + highestRRJob->runTime;
            highestRRJob->turnaroundTime = highestRRJob->finishTime - highestRRJob->arriveTime;
            highestRRJob->weightedTurnaroundTime = static_cast<double>(highestRRJob->turnaroundTime) / highestRRJob->runTime;
            highestRRJob->completed = true;
            completedCount++;

            // 更新时间
            currentTime = highestRRJob->finishTime;

            // 更新原jobs数组
            for (int j = 0; j < numJobs; j++) {
                if (jobs[j].id == highestRRJob->id) {
                    jobs[j] = *highestRRJob;
                    break;
                }
            }
        }
    }
};

// 先来先服务调度算法（多道环境）
class FCFSMultiChannelScheduler : public Scheduler {
private:
    int numChannels;

public:
    FCFSMultiChannelScheduler(const vector<pair<int, int>>& jobList, int channels)
        : Scheduler(jobList), numChannels(channels) {
    }

    void schedule() override {
        vector<Job> sortedJobs = jobs;
        sort(sortedJobs.begin(), sortedJobs.end(),
            [](const Job& a, const Job& b) { return a.arriveTime < b.arriveTime; });

        vector<int> channelFinishTime(numChannels, 0);
        int completedCount = 0;
        int currentIndex = 0;

        while (completedCount < numJobs) {
            // 找到最早空闲的通道
            int minFinishTime = numeric_limits<int>::max();
            int selectedChannel = -1;

            for (int i = 0; i < numChannels; i++) {
                if (channelFinishTime[i] < minFinishTime) {
                    minFinishTime = channelFinishTime[i];
                    selectedChannel = i;
                }
            }

            // 选择下一个可执行的作业
            if (currentIndex < numJobs) {
                Job& job = sortedJobs[currentIndex];

                job.startTime = max(channelFinishTime[selectedChannel], job.arriveTime);
                job.finishTime = job.startTime + job.runTime;
                job.turnaroundTime = job.finishTime - job.arriveTime;
                job.weightedTurnaroundTime = static_cast<double>(job.turnaroundTime) / job.runTime;
                job.completed = true;

                channelFinishTime[selectedChannel] = job.finishTime;
                currentIndex++;
                completedCount++;

                // 更新原jobs数组
                for (int i = 0; i < numJobs; i++) {
                    if (jobs[i].id == job.id) {
                        jobs[i] = job;
                        break;
                    }
                }
            }
            else {
                break;
            }
        }
    }
};

// 短作业优先调度算法（多道环境）
class SJFMultiChannelScheduler : public Scheduler {
private:
    int numChannels;

public:
    SJFMultiChannelScheduler(const vector<pair<int, int>>& jobList, int channels)
        : Scheduler(jobList), numChannels(channels) {
    }

    void schedule() override {
        vector<Job> workingJobs = jobs; // 工作副本

        vector<int> channelFinishTime(numChannels, 0);
        int completedCount = 0;
        int currentTime = 0;

        while (completedCount < numJobs) {
            // 找到已到达且未完成的作业
            vector<Job*> availableJobs;
            for (auto& job : workingJobs) {
                if (!job.completed && job.arriveTime <= currentTime) {
                    availableJobs.push_back(&job);
                }
            }

            // 如果没有可用作业，时间前进到下一个作业到达时间或通道空闲时间
            if (availableJobs.empty()) {
                int nextArrival = numeric_limits<int>::max();
                for (const auto& job : workingJobs) {
                    if (!job.completed && job.arriveTime > currentTime) {
                        nextArrival = min(nextArrival, job.arriveTime);
                    }
                }

                int nextChannelFree = *min_element(channelFinishTime.begin(), channelFinishTime.end());
                currentTime = max(currentTime + 1, min(nextArrival, nextChannelFree));
                continue;
            }

            // 按运行时间排序（短作业优先）
            sort(availableJobs.begin(), availableJobs.end(),
                [](const Job* a, const Job* b) { return a->runTime < b->runTime; });

            // 分配作业到空闲通道
            for (int i = 0; i < min(numChannels, (int)availableJobs.size()); i++) {
                Job* job = availableJobs[i];
                if (job->completed) continue;

                // 找到最早空闲的通道
                int earliestChannel = 0;
                for (int j = 1; j < numChannels; j++) {
                    if (channelFinishTime[j] < channelFinishTime[earliestChannel]) {
                        earliestChannel = j;
                    }
                }

                // 如果通道空闲，分配作业
                if (channelFinishTime[earliestChannel] <= currentTime) {
                    job->startTime = max(channelFinishTime[earliestChannel], job->arriveTime);
                    job->finishTime = job->startTime + job->runTime;
                    job->turnaroundTime = job->finishTime - job->arriveTime;
                    job->weightedTurnaroundTime = static_cast<double>(job->turnaroundTime) / job->runTime;
                    job->completed = true;

                    channelFinishTime[earliestChannel] = job->finishTime;
                    completedCount++;

                    // 更新原jobs数组
                    for (int j = 0; j < numJobs; j++) {
                        if (jobs[j].id == job->id) {
                            jobs[j] = *job;
                            break;
                        }
                    }
                }
            }

            currentTime++;
        }
    }
};

// 响应比高者优先调度算法（多道环境）
class HRRNMultiChannelScheduler : public Scheduler {
private:
    int numChannels;

    double calculateResponseRatio(const Job& job, int currentTime) {
        if (job.arriveTime > currentTime) return 0;
        int waitTime = currentTime - job.arriveTime;
        return static_cast<double>(waitTime + job.runTime) / job.runTime;
    }

public:
    HRRNMultiChannelScheduler(const vector<pair<int, int>>& jobList, int channels)
        : Scheduler(jobList), numChannels(channels) {
    }

    void schedule() override {
        vector<Job> workingJobs = jobs; // 工作副本

        vector<int> channelFinishTime(numChannels, 0);
        int completedCount = 0;
        int currentTime = 0;

        while (completedCount < numJobs) {
            // 找到已到达且未完成的作业，计算响应比
            vector<pair<double, Job*>> availableJobs; // <响应比, 作业指针>
            for (auto& job : workingJobs) {
                if (!job.completed && job.arriveTime <= currentTime) {
                    double ratio = calculateResponseRatio(job, currentTime);
                    availableJobs.push_back({ ratio, &job });
                }
            }

            // 如果没有可用作业，时间前进
            if (availableJobs.empty()) {
                int nextArrival = numeric_limits<int>::max();
                for (const auto& job : workingJobs) {
                    if (!job.completed && job.arriveTime > currentTime) {
                        nextArrival = min(nextArrival, job.arriveTime);
                    }
                }

                int nextChannelFree = *min_element(channelFinishTime.begin(), channelFinishTime.end());
                currentTime = max(currentTime + 1, min(nextArrival, nextChannelFree));
                continue;
            }

            // 按响应比排序（高者优先）
            sort(availableJobs.begin(), availableJobs.end(),
                [](const pair<double, Job*>& a, const pair<double, Job*>& b) {
                    return a.first > b.first;
                });

            // 分配作业到空闲通道
            for (int i = 0; i < min(numChannels, (int)availableJobs.size()); i++) {
                Job* job = availableJobs[i].second;
                if (job->completed) continue;

                // 找到最早空闲的通道
                int earliestChannel = 0;
                for (int j = 1; j < numChannels; j++) {
                    if (channelFinishTime[j] < channelFinishTime[earliestChannel]) {
                        earliestChannel = j;
                    }
                }

                // 如果通道空闲，分配作业
                if (channelFinishTime[earliestChannel] <= currentTime) {
                    job->startTime = max(channelFinishTime[earliestChannel], job->arriveTime);
                    job->finishTime = job->startTime + job->runTime;
                    job->turnaroundTime = job->finishTime - job->arriveTime;
                    job->weightedTurnaroundTime = static_cast<double>(job->turnaroundTime) / job->runTime;
                    job->completed = true;

                    channelFinishTime[earliestChannel] = job->finishTime;
                    completedCount++;

                    // 更新原jobs数组
                    for (int j = 0; j < numJobs; j++) {
                        if (jobs[j].id == job->id) {
                            jobs[j] = *job;
                            break;
                        }
                    }
                }
            }

            currentTime++;
        }
    }
};

// 性能分析函数
void analyzePerformance(const vector<pair<int, int>>& jobList) {
    cout << "作业流分析:" << endl;
    cout << "作业数量: " << jobList.size() << endl;

    int totalRunTime = 0;
    int maxArriveTime = 0;
    for (const auto& job : jobList) {
        totalRunTime += job.second;
        maxArriveTime = max(maxArriveTime, job.first);
    }

    cout << "总运行时间: " << totalRunTime << "分钟" << endl;
    cout << "最大到达时间: " << maxArriveTime << "分钟" << endl;
    cout << "平均运行时间: " << static_cast<double>(totalRunTime) / jobList.size() << "分钟" << endl;
}

int main() {
    // 示例作业数据：{到达时间, 运行时间}
    vector<pair<int, int>> jobList = {
        {0, 5},   // 作业1：0分钟到达，运行5分钟
        {1, 3},   // 作业2：1分钟到达，运行3分钟
        {2, 8},   // 作业3：2分钟到达，运行8分钟
        {3, 2},   // 作业4：3分钟到达，运行2分钟
        {4, 4}    // 作业5：4分钟到达，运行4分钟
    };
    vector<pair<int, int>> jobList1 = {
    {0, 2},   // 作业1
    {1, 1},   // 作业2
    {2, 3},   // 作业3
    {3, 1},   // 作业4
    {4, 2}    // 作业5
    };
    vector<pair<int, int>> jobList2 = {
    {0, 10},  // 作业1
    {1, 2},   // 作业2
    {2, 2},   // 作业3
    {3, 1},   // 作业4
    {4, 3}    // 作业5
    };
    vector<pair<int, int>> jobList5 = {
    {0, 10},    // 作业1：短作业，立即到达
    {1, 200},   // 作业2：长作业，稍后到达（SJF会优先执行短作业，导致此作业饥饿）
    {2, 20},   // 作业3：中等作业，在长作业之后到达
    {3, 100},  // 作业4：长作业，进一步加剧SJF的饥饿问题
    {4, 300}   // 作业5：超长作业，SJF会将其无限推迟
    };
    vector<pair<int, int>> jobList3 = {
    {0, 3},   // 作业1
    {0, 2},   // 作业2
    {0, 4},   // 作业3
    {0, 1},   // 作业4
    {0, 5}    // 作业5
    };
    vector<pair<int, int>> jobList4 = {
    {0, 5},   // 作业1
    {2, 1},   // 作业2
    {4, 8},   // 作业3
    {6, 2},   // 作业4
    {8, 3}    // 作业5
    };
    cout << "操作系统作业调度算法模拟" << endl;
    cout << "=========================" << endl;

    const vector<pair<int, int>> jobLists[] = { jobList1, jobList2, jobList3, jobList4, jobList5 };
    const string names[] = { "短作业流", "长作业流", "突发到达作业流", "混合作业流" , "新长作业流"};

    for (int i = 0; i < 5; i++) {
        analyzePerformance(jobLists[i]);

        FCFSSingleChannelScheduler fcfs(jobLists[i]);
        fcfs.schedule();
        fcfs.printResults(names[i] + "-FCFS", 1);

        SJFSingleChannelScheduler sjf(jobLists[i]);
        sjf.schedule();
        sjf.printResults(names[i] + "-SJF", 1);

        HRRNSingleChannelScheduler hrrn(jobLists[i]);
        hrrn.schedule();
        hrrn.printResults(names[i] + "-HRRN", 1);
    }


    // 性能分析
    analyzePerformance(jobList);

    // 单道环境测试
    FCFSSingleChannelScheduler fcfsSingle(jobList);
    fcfsSingle.schedule();
    fcfsSingle.printResults("单道环境FCFS", 1);

    SJFSingleChannelScheduler sjfSingle(jobList);
    sjfSingle.schedule();
    sjfSingle.printResults("单道环境SJF", 1);

    HRRNSingleChannelScheduler hrrnSingle(jobList);
    hrrnSingle.schedule();
    hrrnSingle.printResults("单道环境HRRN", 1);

    // 多道环境测试（2道）
    int numChannels = 2;

    // FCFS算法
    FCFSMultiChannelScheduler fcfsScheduler(jobList, numChannels);
    fcfsScheduler.schedule();
    fcfsScheduler.printResults("多道环境FCFS", numChannels);
    fcfsScheduler.resetJobs();

    // SJF算法
    SJFMultiChannelScheduler sjfScheduler(jobList, numChannels);
    sjfScheduler.schedule();
    sjfScheduler.printResults("多道环境SJF", numChannels);
    sjfScheduler.resetJobs();

    // HRRN算法
    HRRNMultiChannelScheduler hrrnScheduler(jobList, numChannels);
    hrrnScheduler.schedule();
    hrrnScheduler.printResults("多道环境HRRN", numChannels);

    cout << "\n算法优劣分析:" << endl;
    cout << "1. FCFS算法：简单公平，但短作业等待时间长" << endl;
    cout << "2. SJF算法：平均周转时间最优，但可能导致长作业饥饿" << endl;
    cout << "3. HRRN算法：兼顾等待时间和运行时间，避免饥饿现象" << endl;
    cout << "4. 多道环境相比单道环境能显著提高系统吞吐量" << endl;

    return 0;
}
