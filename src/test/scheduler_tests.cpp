<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2012-2022 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <random.h>
#include <scheduler.h>
#include <util/time.h>

#include <boost/test/unit_test.hpp>

#include <functional>
#include <mutex>
#include <thread>
#include <vector>

BOOST_AUTO_TEST_SUITE(scheduler_tests)

<<<<<<< HEAD
static void microTask(CScheduler& s, std::mutex& mutex, int& counter, int delta, std::chrono::system_clock::time_point rescheduleTime)
=======
static void microTask(CScheduler& s, std::mutex& mutex, int& counter, int delta, std::chrono::steady_clock::time_point rescheduleTime)
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        counter += delta;
    }
<<<<<<< HEAD
    std::chrono::system_clock::time_point noTime = std::chrono::system_clock::time_point::min();
=======
    auto noTime = std::chrono::steady_clock::time_point::min();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    if (rescheduleTime != noTime) {
        CScheduler::Function f = std::bind(&microTask, std::ref(s), std::ref(mutex), std::ref(counter), -delta + 1, noTime);
        s.schedule(f, rescheduleTime);
    }
}

BOOST_AUTO_TEST_CASE(manythreads)
{
    // Stress test: hundreds of microsecond-scheduled tasks,
    // serviced by 10 threads.
    //
    // So... ten shared counters, which if all the tasks execute
    // properly will sum to the number of tasks done.
    // Each task adds or subtracts a random amount from one of the
    // counters, and then schedules another task 0-1000
    // microseconds in the future to subtract or add from
    // the counter -random_amount+1, so in the end the shared
    // counters should sum to the number of initial tasks performed.
    CScheduler microTasks;

    std::mutex counterMutex[10];
    int counter[10] = { 0 };
<<<<<<< HEAD
    FastRandomContext rng{/* fDeterministic */ true};
=======
    FastRandomContext rng{/*fDeterministic=*/true};
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    auto zeroToNine = [](FastRandomContext& rc) -> int { return rc.randrange(10); }; // [0, 9]
    auto randomMsec = [](FastRandomContext& rc) -> int { return -11 + (int)rc.randrange(1012); }; // [-11, 1000]
    auto randomDelta = [](FastRandomContext& rc) -> int { return -1000 + (int)rc.randrange(2001); }; // [-1000, 1000]

<<<<<<< HEAD
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point now = start;
    std::chrono::system_clock::time_point first, last;
=======
    auto start = std::chrono::steady_clock::now();
    auto now = start;
    std::chrono::steady_clock::time_point first, last;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    size_t nTasks = microTasks.getQueueInfo(first, last);
    BOOST_CHECK(nTasks == 0);

    for (int i = 0; i < 100; ++i) {
<<<<<<< HEAD
        std::chrono::system_clock::time_point t = now + std::chrono::microseconds(randomMsec(rng));
        std::chrono::system_clock::time_point tReschedule = now + std::chrono::microseconds(500 + randomMsec(rng));
=======
        auto t = now + std::chrono::microseconds(randomMsec(rng));
        auto tReschedule = now + std::chrono::microseconds(500 + randomMsec(rng));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        int whichCounter = zeroToNine(rng);
        CScheduler::Function f = std::bind(&microTask, std::ref(microTasks),
                                             std::ref(counterMutex[whichCounter]), std::ref(counter[whichCounter]),
                                             randomDelta(rng), tReschedule);
        microTasks.schedule(f, t);
    }
    nTasks = microTasks.getQueueInfo(first, last);
    BOOST_CHECK(nTasks == 100);
    BOOST_CHECK(first < last);
    BOOST_CHECK(last > now);

    // As soon as these are created they will start running and servicing the queue
    std::vector<std::thread> microThreads;
<<<<<<< HEAD
=======
    microThreads.reserve(10);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    for (int i = 0; i < 5; i++)
        microThreads.emplace_back(std::bind(&CScheduler::serviceQueue, &microTasks));

    UninterruptibleSleep(std::chrono::microseconds{600});
<<<<<<< HEAD
    now = std::chrono::system_clock::now();
=======
    now = std::chrono::steady_clock::now();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    // More threads and more tasks:
    for (int i = 0; i < 5; i++)
        microThreads.emplace_back(std::bind(&CScheduler::serviceQueue, &microTasks));
    for (int i = 0; i < 100; i++) {
<<<<<<< HEAD
        std::chrono::system_clock::time_point t = now + std::chrono::microseconds(randomMsec(rng));
        std::chrono::system_clock::time_point tReschedule = now + std::chrono::microseconds(500 + randomMsec(rng));
=======
        auto t = now + std::chrono::microseconds(randomMsec(rng));
        auto tReschedule = now + std::chrono::microseconds(500 + randomMsec(rng));
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        int whichCounter = zeroToNine(rng);
        CScheduler::Function f = std::bind(&microTask, std::ref(microTasks),
                                             std::ref(counterMutex[whichCounter]), std::ref(counter[whichCounter]),
                                             randomDelta(rng), tReschedule);
        microTasks.schedule(f, t);
    }

    // Drain the task queue then exit threads
    microTasks.StopWhenDrained();
    // wait until all the threads are done
    for (auto& thread: microThreads) {
        if (thread.joinable()) thread.join();
    }

    int counterSum = 0;
    for (int i = 0; i < 10; i++) {
        BOOST_CHECK(counter[i] != 0);
        counterSum += counter[i];
    }
    BOOST_CHECK_EQUAL(counterSum, 200);
}

BOOST_AUTO_TEST_CASE(wait_until_past)
{
    std::condition_variable condvar;
    Mutex mtx;
    WAIT_LOCK(mtx, lock);

<<<<<<< HEAD
    const auto no_wait= [&](const std::chrono::seconds& d) {
        return condvar.wait_until(lock, std::chrono::system_clock::now() - d);
=======
    const auto no_wait = [&](const std::chrono::seconds& d) {
        return condvar.wait_until(lock, std::chrono::steady_clock::now() - d);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    };

    BOOST_CHECK(std::cv_status::timeout == no_wait(std::chrono::seconds{1}));
    BOOST_CHECK(std::cv_status::timeout == no_wait(std::chrono::minutes{1}));
    BOOST_CHECK(std::cv_status::timeout == no_wait(std::chrono::hours{1}));
    BOOST_CHECK(std::cv_status::timeout == no_wait(std::chrono::hours{10}));
    BOOST_CHECK(std::cv_status::timeout == no_wait(std::chrono::hours{100}));
    BOOST_CHECK(std::cv_status::timeout == no_wait(std::chrono::hours{1000}));
}

BOOST_AUTO_TEST_CASE(singlethreadedscheduler_ordered)
{
    CScheduler scheduler;

    // each queue should be well ordered with respect to itself but not other queues
    SingleThreadedSchedulerClient queue1(scheduler);
    SingleThreadedSchedulerClient queue2(scheduler);

    // create more threads than queues
    // if the queues only permit execution of one task at once then
    // the extra threads should effectively be doing nothing
    // if they don't we'll get out of order behaviour
    std::vector<std::thread> threads;
<<<<<<< HEAD
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(std::bind(&CScheduler::serviceQueue, &scheduler));
=======
    threads.reserve(5);
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&] { scheduler.serviceQueue(); });
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    }

    // these are not atomic, if SinglethreadedSchedulerClient prevents
    // parallel execution at the queue level no synchronization should be required here
    int counter1 = 0;
    int counter2 = 0;

    // just simply count up on each queue - if execution is properly ordered then
    // the callbacks should run in exactly the order in which they were enqueued
    for (int i = 0; i < 100; ++i) {
        queue1.AddToProcessQueue([i, &counter1]() {
            bool expectation = i == counter1++;
            assert(expectation);
        });

        queue2.AddToProcessQueue([i, &counter2]() {
            bool expectation = i == counter2++;
            assert(expectation);
        });
    }

    // finish up
    scheduler.StopWhenDrained();
    for (auto& thread: threads) {
        if (thread.joinable()) thread.join();
    }

    BOOST_CHECK_EQUAL(counter1, 100);
    BOOST_CHECK_EQUAL(counter2, 100);
}

BOOST_AUTO_TEST_CASE(mockforward)
{
    CScheduler scheduler;

    int counter{0};
    CScheduler::Function dummy = [&counter]{counter++;};

    // schedule jobs for 2, 5 & 8 minutes into the future

    scheduler.scheduleFromNow(dummy, std::chrono::minutes{2});
    scheduler.scheduleFromNow(dummy, std::chrono::minutes{5});
    scheduler.scheduleFromNow(dummy, std::chrono::minutes{8});

    // check taskQueue
<<<<<<< HEAD
    std::chrono::system_clock::time_point first, last;
=======
    std::chrono::steady_clock::time_point first, last;
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    size_t num_tasks = scheduler.getQueueInfo(first, last);
    BOOST_CHECK_EQUAL(num_tasks, 3ul);

    std::thread scheduler_thread([&]() { scheduler.serviceQueue(); });

    // bump the scheduler forward 5 minutes
    scheduler.MockForward(std::chrono::minutes{5});

    // ensure scheduler has chance to process all tasks queued for before 1 ms from now.
    scheduler.scheduleFromNow([&scheduler] { scheduler.stop(); }, std::chrono::milliseconds{1});
    scheduler_thread.join();

    // check that the queue only has one job remaining
    num_tasks = scheduler.getQueueInfo(first, last);
    BOOST_CHECK_EQUAL(num_tasks, 1ul);

    // check that the dummy function actually ran
    BOOST_CHECK_EQUAL(counter, 2);

    // check that the time of the remaining job has been updated
<<<<<<< HEAD
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
=======
    auto now = std::chrono::steady_clock::now();
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
    int delta = std::chrono::duration_cast<std::chrono::seconds>(first - now).count();
    // should be between 2 & 3 minutes from now
    BOOST_CHECK(delta > 2*60 && delta < 3*60);
}

BOOST_AUTO_TEST_SUITE_END()
