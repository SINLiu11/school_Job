#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <random>
#include <chrono>
#include <functional>
#include <iomanip>
#include <ctime>

// 定义一个简单的"信息块"结构，包含数据和是否为结束标志
struct OutputBlock {
    std::string data;
    int requesterId;      // 添加请求者ID，用于区分不同进程
    bool isEndMarker;     // true 表示这是该批次的结束标志

    OutputBlock(const std::string& d, int id, bool end = false)
        : data(d), requesterId(id), isEndMarker(end) {
    }
};

class SpoolingManager {
private:
    std::queue<OutputBlock> outputBuffer; // 模拟磁盘输入井的缓冲区
    std::mutex bufferMutex;               // 保护缓冲区的互斥锁
    std::condition_variable bufferCV;     // 用于通知缓冲区状态变化的条件变量
    bool shouldStop;                      // 控制程序结束的标志
    int completedBlocks;                  // 记录已完成的信息块数量

public:
    SpoolingManager() : shouldStop(false), completedBlocks(0) {}

    // 生产者接口：将数据块放入缓冲区
    void enqueueOutput(const OutputBlock& block) {
        std::lock_guard<std::mutex> lock(bufferMutex);
        outputBuffer.push(block);
        bufferCV.notify_one(); // 通知消费者有新数据
    }

    // 消费者接口：从缓冲区取出并处理数据块
    void processOutput() {
        while (true) {
            std::unique_lock<std::mutex> lock(bufferMutex);

            // 等待直到缓冲区非空或程序应停止
            bufferCV.wait(lock, [this] { return !outputBuffer.empty() || shouldStop; });

            // 如果程序应停止且缓冲区为空，则退出
            if (shouldStop && outputBuffer.empty()) {
                break;
            }

            // 取出一个数据块
            OutputBlock currentBlock = outputBuffer.front();
            outputBuffer.pop();
            lock.unlock(); // 尽快释放锁，避免阻塞生产者

            // 模拟"实际输出"到打印机/CRT
            simulateOutput(currentBlock);

            // 如果遇到结束标志，说明一个完整的"信息块"已处理完毕
            if (currentBlock.isEndMarker) {
                {
                    std::lock_guard<std::mutex> lock(bufferMutex);
                    completedBlocks++;
                }
                std::cout << "\n[SPOOLING] >>> 进程 " << currentBlock.requesterId
                    << " 的完整信息块已输出完毕 (共 " << completedBlocks << " 个块) <<<\n"
                    << std::endl;
            }
        }
    }

    // 模拟输出操作（可以替换为真实的 I/O 调用）
    void simulateOutput(const OutputBlock& block) {
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_s(&now_tm, &now_time_t);

        char time_str[20];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &now_tm);

        // 格式化输出
        std::cout << "[" << time_str << "] [SPOOLING] ";
        std::cout << "进程 " << std::setw(2) << block.requesterId << " | ";
        std::cout << "输出: " << std::setw(20) << block.data;

        if (block.isEndMarker) {
            std::cout << " [结束标志]";
        }

        std::cout << std::endl;

        // 模拟输出设备的速度较慢，增加延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    // 设置停止标志
    void stop() {
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            shouldStop = true;
        }
        bufferCV.notify_all(); // 唤醒所有等待的线程，让它们检查 shouldStop
    }

    int getCompletedBlocks() const {
        return completedBlocks;
    }
};

class OutputRequester {
private:
    SpoolingManager& spoolingMgr; // 引用共享的 SpoolingManager
    int requesterId;              // 请求者的ID，用于区分不同的"进程"
    std::mt19937 rng;             // 随机数生成器

public:
    OutputRequester(SpoolingManager& mgr, int id)
        : spoolingMgr(mgr), requesterId(id), rng(std::random_device{}()) {
    }

    // 模拟一个"进程"的输出请求
    void operator()() {
        std::uniform_int_distribution<> itemCountDist(4, 10); // 每个进程输出4-10项数据
        std::uniform_int_distribution<> delayDist(200, 1000); // 每项数据之间随机延迟

        int itemCount = itemCountDist(rng);

        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_s(&now_tm, &now_time_t);

        char time_str[20];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &now_tm);

        std::cout << "\n[" << time_str << "] [进程 " << std::setw(2) << requesterId
            << "] 开始输出，共 " << itemCount << " 项数据。" << std::endl;

        for (int i = 0; i < itemCount; ++i) {
            // 生成随机数据
            std::string data = "数据项_" + std::to_string(i + 1);
            OutputBlock block(data, requesterId);

            // 将数据项放入缓冲区
            spoolingMgr.enqueueOutput(block);

            // 格式化输出
            std::cout << "[" << time_str << "] [进程 " << std::setw(2) << requesterId
                << "] 已提交: " << data << std::endl;

            // 模拟进程间随机的输出间隔
            std::this_thread::sleep_for(std::chrono::milliseconds(delayDist(rng)));
        }

        // 提交结束标志
        OutputBlock endBlock("END_MARKER", requesterId, true);
        spoolingMgr.enqueueOutput(endBlock);

        std::cout << "[" << time_str << "] [进程 " << std::setw(2) << requesterId
            << "] 已提交结束标志。" << std::endl << std::endl;
    }
};

int main() {
    std::cout << "==============================================" << std::endl;
    std::cout << "        SPOOLING 技术模拟实验 (4组示例)" << std::endl;
    std::cout << "==============================================" << std::endl << std::endl;

    SpoolingManager spoolingMgr;

    // 创建四个"输出进程"（即 OutputRequester 实例）
    OutputRequester requester1(spoolingMgr, 1);
    OutputRequester requester2(spoolingMgr, 2);
    OutputRequester requester3(spoolingMgr, 3);
    OutputRequester requester4(spoolingMgr, 4);

    // 创建线程来并发执行这四个"输出进程"
    std::thread t1(requester1);
    std::thread t2(requester2);
    std::thread t3(requester3);
    std::thread t4(requester4);

    // 创建线程来运行 SpoolingManager 的输出处理循环
    std::thread spoolingThread([&spoolingMgr]() {
        spoolingMgr.processOutput();
        });

    // 等待四个输出进程完成
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    std::cout << "\n[主程序] 所有请求进程已完成，等待SPOOLING进程处理剩余数据..." << std::endl;

    // 让 SpoolingManager 处理完剩余的所有数据
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 通知 SpoolingManager 停止
    spoolingMgr.stop();

    // 等待 SpoolingManager 线程结束
    spoolingThread.join();

    std::cout << "\n==============================================" << std::endl;
    std::cout << " 实验完成! 共处理 " << spoolingMgr.getCompletedBlocks() << " 个完整信息块" << std::endl;
    std::cout << "==============================================" << std::endl;

    return 0;
}
