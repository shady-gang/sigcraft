#ifndef SIGCRAFT_THREADPOOL_H
#define SIGCRAFT_THREADPOOL_H

#include <thread>
#include <functional>
#include <semaphore>
#include <future>
#include <cassert>

struct ThreadPool {
    using Task = std::function<void(void)>;
    rustex::mutex<std::vector<Task>> tasks;
    std::counting_semaphore<65536> sem { 0 };
    std::vector<std::future<void>> threads;

    ThreadPool(unsigned size) {
        assert(size > 0);
        for (unsigned i = 0; i < size; i++) {
            threads.emplace_back(std::async(std::launch::async, [&]() {
                while (true) {
                    auto pop_task = [&]() -> std::optional<Task> {
                        auto tasks_guard = tasks.lock_mut();
                        if (tasks_guard->empty()) {
                            return std::nullopt;
                        } else {
                            auto task = tasks_guard->back();
                            tasks_guard->pop_back();
                            return task;
                        }
                    };

                    sem.acquire();
                    auto task = pop_task();
                    // if the semaphore let us through, either a task is available
                    if (task)
                        (*task)();
                        // or one isn't and this is the end
                    else
                        break;
                }
            }));
        }
    }

    ThreadPool(ThreadPool&) = delete;

    ~ThreadPool() {
        sem.release(threads.size());
    }

    void schedule(Task&& t) {
        auto tasks_guard = tasks.lock_mut();
        tasks_guard->push_back(std::move(t));
        sem.release(1);
    }
};

#endif