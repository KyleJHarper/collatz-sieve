#pragma once
#include <chrono>
#include <stdexcept>
#include <string>
#include <fstream>
#include <thread>
#include "concepts.hpp"



/**
* @class Progress
* @brief Writes data from a pointer location to a file on a regular schedule, keeping a progress.
*/
class Progress {
    private:
    /// @brief String path written to.
    std::string _file_path;
    /// @brief Background thread which sleeps and writes.
    std::thread _monitor_thread;
    /// @brief Stopping flag.
    std::atomic<bool> _stop{false};
    /// @brief Sleep duration before emitting another progress report.
    size_t _sleep_duration_ms = 100;

    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor.
    Progress() {}



    /// @brief Constructor taking a file path to write to.
    Progress(const std::string& file_path) {
        _file_path = file_path;
    }



    /// @brief Destructor joins the monitoring thread and closes out handles.
    ~Progress() {
        join();
    }



    /// @brief Disable copy and move operations.
    Progress(const Progress&) = delete;
    /// @brief Disable copy and move operations.
    Progress& operator=(const Progress&) = delete;

    /// @}



    /**
    * @brief Writes a line containing a timestamp prefix and the callers value in `x`.
    * @tparam AnyPrintable Any data type that is printable via a standard `<<` operator.
    * @param x Reference to the data to write.
    */
    template<typename AnyPrintable>
    void writeline(const  AnyPrintable& x) {
        std::ofstream fh(_file_path, std::ios::app);
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = *std::localtime(&t);
        if (!fh) {
           throw std::runtime_error("Could not open file: " + _file_path);
        }
        if constexpr(std::is_pointer_v<AnyPrintable>) {
            fh << "[" << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "]  " << to_string_any(*x) << std::endl;
        } else {
            fh << "[" << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "]  " << to_string_any(x) << std::endl;
        }
    }



    /**
    * @brief Begins monitoring the data referenced by `x` in a new thread.
    * @tparam AnyPrintable Any data type that is printable via a standard `<<` operator.
    * @param x The reference to connect with the new monitoring thread.
    * @param delay_seconds How often to tell the monitoring thread to read `x` and write it out.
    */
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



    /**
    * @brief The subtask running inside `_monitor_thread` after `monitor()` is called.  Should probably be private.
    * @tparam AnyPrintable Any data type that is printable via a standard `<<` operator.
    */
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



    /// @brief Stops the monitoring thread by flagging it and calling join, if possible.
    void join() {
        _stop = true;
        if (_monitor_thread.joinable()) {
            _monitor_thread.join();
        }
    }

};
