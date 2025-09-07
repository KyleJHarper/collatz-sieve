#pragma once
#include <chrono>
#include <stdexcept>
#include <string>
#include <fstream>
#include <thread>
#include "concepts.hpp"



//
// Just a simple helper to write progress to a file so I can resuem workloads.
//

class Progress {
    private:
    std::string _file_path;
    std::thread _monitor_thread;
    std::atomic<bool> _stop{false};
    size_t _sleep_duration_ms = 100;

    public:
    Progress() {}
    Progress(const std::string& file_path) {
        _file_path = file_path;
    }
    ~Progress() {
        join();
    }
    Progress(const Progress&) = delete;
    Progress& operator=(const Progress&) = delete;


    template<typename AnyPrintable>
    void writeline(const  AnyPrintable& x) {
        std::ofstream fh(_file_path, std::ios::app);
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = *std::localtime(&t);
        if (!fh) {
           throw std::runtime_error("Could not open file: " + _file_path);
        }
        fh << "[" << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "]  " << to_string_any(x) << std::endl;
    }

    template<typename AnyPrintable>
    void monitor(AnyPrintable& x, size_t delay_seconds = 5) {
        _stop = false;
        _monitor_thread = std::thread(
            &Progress::monitor_subtask<AnyPrintable>
            , this
            , std::ref(x)
            , delay_seconds
        );
    }

    template<typename AnyPrintable>
    void monitor_subtask(AnyPrintable& x, size_t delay_seconds) {
        float waited = delay_seconds + 1;
        while (!_stop) {
            if (waited >= (delay_seconds * 1000)) {
                writeline(x);
                waited = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(_sleep_duration_ms));
            waited += _sleep_duration_ms;
        }
    }

    void join() {
        _stop = true;
        if (_monitor_thread.joinable()) {
            _monitor_thread.join();
        }
    }
};
