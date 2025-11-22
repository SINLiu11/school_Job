#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <algorithm>
#include <iomanip>

using namespace std;

// PCB状态枚举
enum ProcessState {
    READY,
    RUNNING,
    WAITING,
    SUSPENDED,
    TERMINATED
};

// PCB结构体
struct PCB {
    int pid;                    // 进程ID
    ProcessState state;         // 进程状态
    int priority;               // 优先级
    int remainingTime;          // 剩余执行时间
    int totalTime;              // 总需要时间
    PCB* next;                  // 下一个PCB指针
    PCB* prev;                  // 前一个PCB指针
    int memorySize;             // 内存大小（用于伙伴系统）
    bool allocated;             // 是否已分配
    PCB* buddy;                 // 伙伴指针

    PCB(int id, int pri, int time) {
        pid = id;
        priority = pri;
        remainingTime = time;
        totalTime = time;
        state = READY;
        next = prev = nullptr;
        memorySize = 0;
        allocated = false;
        buddy = nullptr;
    }
};

// 进程管理类
class ProcessManager {
private:
    PCB* totalChainHead;        // PCB总链头指针
    PCB* totalChainTail;        // PCB总链尾指针
    PCB* readyQueueHead;        // 就绪队列头指针
    PCB* readyQueueTail;        // 就绪队列尾指针
    PCB* waitingQueueHead;      // 等待队列头指针
    PCB* waitingQueueTail;      // 等待队列尾指针
    PCB* suspendedQueueHead;    // 挂起队列头指针
    PCB* suspendedQueueTail;    // 挂起队列尾指针
    PCB* runningProcess;        // 当前运行进程

    // 伙伴系统相关
    vector<PCB*> freeBlocks;    // 空闲块列表
    map<int, PCB*> allocatedBlocks; // 已分配块映射
    int totalMemory;            // 总内存大小
    int minBlockSize;           // 最小块大小

    int nextPid;                // 下一个进程ID
    int timeSlice;              // 时间片大小

public:
    ProcessManager(int memory = 1024, int minSize = 16) {
        totalChainHead = totalChainTail = nullptr;
        readyQueueHead = readyQueueTail = nullptr;
        waitingQueueHead = waitingQueueTail = nullptr;
        suspendedQueueHead = suspendedQueueTail = nullptr;
        runningProcess = nullptr;

        totalMemory = memory;
        minBlockSize = minSize;
        nextPid = 1;
        timeSlice = 2;

        // 初始化伙伴系统空闲列表
        initializeBuddySystem();
    }

    // 初始化伙伴系统
    void initializeBuddySystem() {
        // 创建初始空闲块
        PCB* initialBlock = new PCB(-1, 0, 0);
        initialBlock->memorySize = totalMemory;
        initialBlock->allocated = false;
        initialBlock->buddy = nullptr;
        freeBlocks.push_back(initialBlock);

        // 添加到总链
        addToTotalChain(initialBlock);
    }

    // 添加到总链
    void addToTotalChain(PCB* pcb) {
        if (!totalChainHead) {
            totalChainHead = totalChainTail = pcb;
            pcb->next = pcb->prev = nullptr;
        }
        else {
            totalChainTail->next = pcb;
            pcb->prev = totalChainTail;
            totalChainTail = pcb;
            pcb->next = nullptr;
        }
    }

    // 从总链移除
    void removeFromTotalChain(PCB* pcb) {
        if (pcb->prev) pcb->prev->next = pcb->next;
        if (pcb->next) pcb->next->prev = pcb->prev;
        if (pcb == totalChainHead) totalChainHead = pcb->next;
        if (pcb == totalChainTail) totalChainTail = pcb->prev;
    }

    // 添加到就绪队列
    void addToReadyQueue(PCB* pcb) {
        pcb->state = READY;
        if (!readyQueueHead) {
            readyQueueHead = readyQueueTail = pcb;
            pcb->next = nullptr;
        }
        else {
            readyQueueTail->next = pcb;
            pcb->prev = readyQueueTail;
            readyQueueTail = pcb;
            pcb->next = nullptr;
        }
    }

    // 从就绪队列移除
    void removeFromReadyQueue(PCB* pcb) {
        if (pcb->prev) pcb->prev->next = pcb->next;
        if (pcb->next) pcb->next->prev = pcb->prev;
        if (pcb == readyQueueHead) readyQueueHead = pcb->next;
        if (pcb == readyQueueTail) readyQueueTail = pcb->prev;
    }

    // 添加到等待队列
    void addToWaitingQueue(PCB* pcb) {
        pcb->state = WAITING;
        if (!waitingQueueHead) {
            waitingQueueHead = waitingQueueTail = pcb;
            pcb->next = nullptr;
        }
        else {
            waitingQueueTail->next = pcb;
            pcb->prev = waitingQueueTail;
            waitingQueueTail = pcb;
            pcb->next = nullptr;
        }
    }

    // 从等待队列移除
    void removeFromWaitingQueue(PCB* pcb) {
        if (pcb->prev) pcb->prev->next = pcb->next;
        if (pcb->next) pcb->next->prev = pcb->prev;
        if (pcb == waitingQueueHead) waitingQueueHead = pcb->next;
        if (pcb == waitingQueueTail) waitingQueueTail = pcb->prev;
    }

    // 添加到挂起队列
    void addToSuspendedQueue(PCB* pcb) {
        pcb->state = SUSPENDED;
        if (!suspendedQueueHead) {
            suspendedQueueHead = suspendedQueueTail = pcb;
            pcb->next = nullptr;
        }
        else {
            suspendedQueueTail->next = pcb;
            pcb->prev = suspendedQueueTail;
            suspendedQueueTail = pcb;
            pcb->next = nullptr;
        }
    }

    // 从挂起队列移除
    void removeFromSuspendedQueue(PCB* pcb) {
        if (pcb->prev) pcb->prev->next = pcb->next;
        if (pcb->next) pcb->next->prev = pcb->prev;
        if (pcb == suspendedQueueHead) suspendedQueueHead = pcb->next;
        if (pcb == suspendedQueueTail) suspendedQueueTail = pcb->prev;
    }

    // 创建进程
    void createProcess(int priority, int executionTime, int memoryNeeded = 64) {
        // 使用伙伴系统分配内存
        PCB* memoryBlock = allocateMemory(memoryNeeded);
        if (!memoryBlock) {
            cout << "内存分配失败，无法创建进程!" << endl;
            return;
        }

        PCB* newPCB = new PCB(nextPid++, priority, executionTime);
        newPCB->memorySize = memoryNeeded;
        newPCB->allocated = true;

        // 关联内存块和进程
        allocatedBlocks[newPCB->pid] = memoryBlock;

        // 添加到总链和就绪队列
        addToTotalChain(newPCB);
        addToReadyQueue(newPCB);

        cout << "创建进程 PID: " << newPCB->pid << " 优先级: " << priority
            << " 执行时间: " << executionTime << " 内存: " << memoryNeeded << endl;
    }

    // 撤销进程
    void terminateProcess(int pid) {
        PCB* pcb = findPCB(pid);
        if (!pcb) {
            cout << "进程 " << pid << " 不存在!" << endl;
            return;
        }

        cout << "撤销进程 PID: " << pid << endl;

        // 从相应队列移除
        switch (pcb->state) {
        case READY:
            removeFromReadyQueue(pcb);
            break;
        case RUNNING:
            runningProcess = nullptr;
            break;
        case WAITING:
            removeFromWaitingQueue(pcb);
            break;
        case SUSPENDED:
            removeFromSuspendedQueue(pcb);
            break;
        default:
            break;
        }

        // 从总链移除
        removeFromTotalChain(pcb);

        // 释放内存
        if (allocatedBlocks.find(pid) != allocatedBlocks.end()) {
            freeMemory(allocatedBlocks[pid]);
            allocatedBlocks.erase(pid);
        }

        // 删除PCB
        delete pcb;

        // 如果有运行进程被撤销，调度新进程
        if (!runningProcess && readyQueueHead) {
            schedule();
        }
    }

    // 时间片到
    void timeSliceExpired() {
        if (runningProcess) {
            cout << "时间片到，进程 " << runningProcess->pid << " 回到就绪队列" << endl;
            runningProcess->state = READY;
            addToReadyQueue(runningProcess);
            runningProcess = nullptr;
        }
        schedule();
    }

    // 挂起进程
    void suspendProcess(int pid) {
        PCB* pcb = findPCB(pid);
        if (!pcb) {
            cout << "进程 " << pid << " 不存在!" << endl;
            return;
        }

        if (pcb->state == RUNNING) {
            runningProcess = nullptr;
        }
        else if (pcb->state == READY) {
            removeFromReadyQueue(pcb);
        }
        else if (pcb->state == WAITING) {
            removeFromWaitingQueue(pcb);
        }

        addToSuspendedQueue(pcb);
        cout << "挂起进程 PID: " << pid << endl;

        if (!runningProcess && readyQueueHead) {
            schedule();
        }
    }

    // 激活进程
    void activateProcess(int pid) {
        PCB* pcb = findPCB(pid);
        if (!pcb) {
            cout << "进程 " << pid << " 不存在!" << endl;
            return;
        }

        if (pcb->state == SUSPENDED) {
            removeFromSuspendedQueue(pcb);
            addToReadyQueue(pcb);
            cout << "激活进程 PID: " << pid << endl;
        }
    }

    // 进程调度
    void schedule() {
        if (runningProcess || !readyQueueHead) return;

        // 简单的时间片轮转调度
        PCB* selected = readyQueueHead;
        removeFromReadyQueue(selected);

        selected->state = RUNNING;
        runningProcess = selected;

        cout << "调度进程 PID: " << selected->pid << " 运行" << endl;
    }

    // 执行一个时间单位
    void executeTimeUnit() {
        if (runningProcess) {
            runningProcess->remainingTime--;
            cout << "进程 " << runningProcess->pid << " 执行，剩余时间: "
                << runningProcess->remainingTime << endl;

            if (runningProcess->remainingTime <= 0) {
                cout << "进程 " << runningProcess->pid << " 执行完成" << endl;
                terminateProcess(runningProcess->pid);
            }
        }
        else {
            cout << "没有进程在执行" << endl;
        }
    }

    // 伙伴系统内存分配
    PCB* allocateMemory(int size) {
        // 找到合适大小的块
        int requiredSize = minBlockSize;
        while (requiredSize < size) {
            requiredSize *= 2;
        }

        for (auto it = freeBlocks.begin(); it != freeBlocks.end(); ++it) {
            if ((*it)->memorySize == requiredSize && !(*it)->allocated) {
                PCB* block = *it;
                freeBlocks.erase(it);
                block->allocated = true;
                return block;
            }
        }

        // 没有合适块，尝试分裂更大的块
        for (int i = 0; i < freeBlocks.size(); i++) {
            PCB* block = freeBlocks[i];
            if (!block->allocated && block->memorySize > requiredSize) {
                // 分裂块
                while (block->memorySize > requiredSize) {
                    PCB* buddy = new PCB(-1, 0, 0);
                    buddy->memorySize = block->memorySize / 2;
                    buddy->allocated = false;
                    block->memorySize /= 2;

                    block->buddy = buddy;
                    buddy->buddy = block;

                    addToTotalChain(buddy);
                    freeBlocks.push_back(buddy);
                }

                freeBlocks.erase(freeBlocks.begin() + i);
                block->allocated = true;
                return block;
            }
        }

        return nullptr; // 内存不足
    }

    // 伙伴系统内存释放
    void freeMemory(PCB* block) {
        block->allocated = false;
        freeBlocks.push_back(block);

        // 尝试合并伙伴块
        mergeBuddies();
    }

    // 合并伙伴块
    void mergeBuddies() {
        bool merged;
        do {
            merged = false;
            for (int i = 0; i < freeBlocks.size(); i++) {
                PCB* block = freeBlocks[i];
                if (block->buddy && !block->buddy->allocated) {
                    // 找到伙伴也在空闲列表中
                    auto it = find(freeBlocks.begin(), freeBlocks.end(), block->buddy);
                    if (it != freeBlocks.end()) {
                        // 合并块
                        PCB* largerBlock = (block->memorySize < block->buddy->memorySize) ? block->buddy : block;
                        largerBlock->memorySize *= 2;

                        // 从总链移除较小的块
                        PCB* smallerBlock = (block == largerBlock) ? block->buddy : block;
                        removeFromTotalChain(smallerBlock);

                        // 从空闲列表移除两个块
                        freeBlocks.erase(it);
                        freeBlocks.erase(find(freeBlocks.begin(), freeBlocks.end(), block));

                        // 添加合并后的块
                        freeBlocks.push_back(largerBlock);

                        merged = true;
                        break;
                    }
                }
            }
        } while (merged);
    }

    // 查找PCB
    PCB* findPCB(int pid) {
        PCB* current = totalChainHead;
        while (current) {
            if (current->pid == pid) {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }

    // 显示所有队列状态
    void displayStatus() {
        cout << "\n====== 系统状态 ======" << endl;

        cout << "总链队列: ";
        displayQueue(totalChainHead);

        cout << "就绪队列: ";
        displayQueue(readyQueueHead);

        cout << "等待队列: ";
        displayQueue(waitingQueueHead);

        cout << "挂起队列: ";
        displayQueue(suspendedQueueHead);

        if (runningProcess) {
            cout << "运行进程: PID " << runningProcess->pid << endl;
        }
        else {
            cout << "运行进程: 无" << endl;
        }

        cout << "空闲内存块: ";
        for (PCB* block : freeBlocks) {
            if (!block->allocated) {
                cout << "[" << block->memorySize << "] ";
            }
        }
        cout << endl;
    }

    // 显示队列
    void displayQueue(PCB* head) {
        PCB* current = head;
        while (current) {
            cout << "P" << current->pid;
            if (current->next) cout << " -> ";
            current = current->next;
        }
        cout << endl;
    }

    // 显示菜单
    void displayMenu() {
        cout << "\n====== 进程管理菜单 ======" << endl;
        cout << "1. 创建进程" << endl;
        cout << "2. 撤销进程" << endl;
        cout << "3. 时间片到" << endl;
        cout << "4. 挂起进程" << endl;
        cout << "5. 激活进程" << endl;
        cout << "6. 执行时间单位" << endl;
        cout << "7. 显示状态" << endl;
        cout << "8. 退出" << endl;
        cout << "请选择操作: ";
    }
};

// 主函数
int main() {
    ProcessManager manager;
    int choice;

    // 初始化一些进程用于演示
    cout << "初始化演示进程..." << endl;
    manager.createProcess(1, 5, 64);   // PID 1: 优先级1, 执行时间5, 内存64
    manager.createProcess(2, 3, 128);  // PID 2: 优先级2, 执行时间3, 内存128
    manager.createProcess(3, 8, 32);   // PID 3: 优先级3, 执行时间8, 内存32
    manager.createProcess(1, 4, 256);  // PID 4: 优先级1, 执行时间4, 内存256

    // 执行一次调度让第一个进程运行
    manager.schedule();

    // 挂起一个进程用于演示
    manager.suspendProcess(3);

    cout << "初始化完成！系统已准备好4个演示进程。" << endl;
    manager.displayStatus();
    do {
        manager.displayMenu();
        cin >> choice;

        switch (choice) {
        case 1: {
            int priority, time, memory;
            cout << "输入优先级: ";
            cin >> priority;
            cout << "输入执行时间: ";
            cin >> time;
            cout << "输入内存需求: ";
            cin >> memory;
            manager.createProcess(priority, time, memory);
            break;
        }
        case 2: {
            int pid;
            cout << "输入要撤销的进程PID: ";
            cin >> pid;
            manager.terminateProcess(pid);
            break;
        }
        case 3:
            manager.timeSliceExpired();
            break;
        case 4: {
            int pid;
            cout << "输入要挂起的进程PID: ";
            cin >> pid;
            manager.suspendProcess(pid);
            break;
        }
        case 5: {
            int pid;
            cout << "输入要激活的进程PID: ";
            cin >> pid;
            manager.activateProcess(pid);
            break;
        }
        case 6:
            manager.executeTimeUnit();
            break;
        case 7:
            manager.displayStatus();
            break;
        case 8:
            cout << "退出系统" << endl;
            break;
        default:
            cout << "无效选择!" << endl;
        }
    } while (choice != 8);

    return 0;
}
