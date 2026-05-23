#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <cstdlib>

using namespace std;
using namespace std::chrono;

void linearTask(const vector<int>& arr, long long& sum, int& minVal) {
    sum = 0;
    minVal = 2147483647;
    for (int i = 0; i < arr.size(); i++) {
        if (arr[i] % 2 == 0) {
            sum += arr[i];
            if (arr[i] < minVal) {
                minVal = arr[i];
            }
        }
    }
}

void mutexWorker(int start, int end, const vector<int>& arr, long long& globalSum, int& globalMin, mutex& mtx) {
    long long localSum = 0;
    int localMin = 2147483647;
    for (int i = start; i < end; i++) {
        if (arr[i] % 2 == 0) {
            localSum += arr[i];
            if (arr[i] < localMin) {
                localMin = arr[i];
            }
        }
    }
    
    mtx.lock();
    globalSum += localSum;
    if (localMin < globalMin) {
        globalMin = localMin;
    }
    mtx.unlock();
}

void mutexTask(const vector<int>& arr, long long& sum, int& minVal, int threadsCount) {
    sum = 0;
    minVal = 2147483647;
    mutex mtx;
    vector<thread> threads;
    int chunkSize = arr.size() / threadsCount;

    for (int i = 0; i < threadsCount; i++) {
        int start = i * chunkSize;
        int end = (i == threadsCount - 1) ? arr.size() : start + chunkSize;
        threads.push_back(thread(mutexWorker, start, end, cref(arr), ref(sum), ref(minVal), ref(mtx)));
    }
    for (int i = 0; i < threadsCount; i++) {
        threads[i].join();
    }
}

void casWorker(int start, int end, const vector<int>& arr, atomic<long long>& globalSum, atomic<int>& globalMin) {
    long long localSum = 0;
    int localMin = 2147483647;
    for (int i = start; i < end; i++) {
        if (arr[i] % 2 == 0) {
            localSum += arr[i];
            if (arr[i] < localMin) {
                localMin = arr[i];
            }
        }
    }

    long long currentSum = globalSum.load();
    while (!globalSum.compare_exchange_weak(currentSum, currentSum + localSum)) {}

    int currentMin = globalMin.load();
    while (localMin < currentMin && !globalMin.compare_exchange_weak(currentMin, localMin)) {}
}

void casTask(const vector<int>& arr, long long& sum, int& minVal, int threadsCount) {
    atomic<long long> atomicSum(0);
    atomic<int> atomicMin(2147483647);
    vector<thread> threads;
    int chunkSize = arr.size() / threadsCount;

    for (int i = 0; i < threadsCount; i++) {
        int start = i * chunkSize;
        int end = (i == threadsCount - 1) ? arr.size() : start + chunkSize;
        threads.push_back(thread(casWorker, start, end, cref(arr), ref(atomicSum), ref(atomicMin)));
    }
    for (int i = 0; i < threadsCount; i++) {
        threads[i].join();
    }
    sum = atomicSum.load();
    minVal = atomicMin.load();
}

int main() {
    int logicalCores = thread::hardware_concurrency();
    int physicalCores = logicalCores > 1 ? logicalCores / 2 : 1;

    vector<int> sizes = {10000, 1000000, 10000000, 100000000};
    vector<int> threadCounts = {
        physicalCores / 2 > 0 ? physicalCores / 2 : 1,
        physicalCores,
        logicalCores,
        logicalCores * 2,
        logicalCores * 4,
        logicalCores * 8,
        logicalCores * 16,
        logicalCores * 32
    };

    for (int size : sizes) {
        cout << "Array size: " << size << "\n";
        vector<int> arr(size);
        for (int i = 0; i < size; i++) {
            arr[i] = (rand() % 10000) + 1;
        }

        long long sum = 0;
        int minVal = 0;

        auto start = high_resolution_clock::now();
        linearTask(arr, sum, minVal);
        auto end = high_resolution_clock::now();
        double time = duration_cast<nanoseconds>(end - start).count() * 1e-9;
        cout << "Linear\t\tTime: " << fixed << setprecision(6) << time << " s\tSum: " << sum << "\tMin: " << minVal << "\n";

        for (int tc : threadCounts) {
            start = high_resolution_clock::now();
            mutexTask(arr, sum, minVal, tc);
            end = high_resolution_clock::now();
            time = duration_cast<nanoseconds>(end - start).count() * 1e-9;
            cout << "Mutex (" << tc << ")\tTime: " << fixed << setprecision(6) << time << " s\tSum: " << sum << "\n";
        }

        for (int tc : threadCounts) {
            start = high_resolution_clock::now();
            casTask(arr, sum, minVal, tc);
            end = high_resolution_clock::now();
            time = duration_cast<nanoseconds>(end - start).count() * 1e-9;
            cout << "CAS   (" << tc << ")\tTime: " << fixed << setprecision(6) << time << " s\tSum: " << sum << "\n";
        }
        cout << "---------------------------------------------------\n";
    }
    return 0;
}