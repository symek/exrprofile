//
// Created by symek on 3/30/25.
//
// ThreadPool.h
#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace exrprofile {


    class ThreadPool {
    public:
        explicit ThreadPool(size_t thread_count) : stop_flag(false) {
            for (size_t i = 0; i < thread_count; ++i) {
                workers.emplace_back([this]() {
                    while (true) {
                        std::function<void()> task;

                        {   // lock scope
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this]() {
                                return stop_flag || !tasks.empty();
                            });

                            if (stop_flag && tasks.empty())
                                return;

                            task = std::move(tasks.front());
                            tasks.pop();
                        }

                        task(); // run the task
                    }
                });
            }
        }

        ~ThreadPool() {
            {
                std::scoped_lock lock(queue_mutex);
                stop_flag = true;
            }
            condition.notify_all();
            for (std::thread &t: workers) {
                if (t.joinable()) t.join();
            }
        }

        template<typename Func>
        void enqueue(Func &&f) {
            {
                std::scoped_lock lock(queue_mutex);
                tasks.emplace(std::forward<Func>(f));
            }
            condition.notify_one();
        }

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;

        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop_flag;
    };
} // end of namespace exrprofile