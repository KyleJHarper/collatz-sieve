#pragma once

#include <iostream>
#include <string>


/**
* @class ProgressReport
* @brief Writes single or multi-line output to an ANSI-compliant terminal and erases it, assuming no wrapping.
* @note This is not thread safe and writes to stdout by default.
*/
class ProgressReport {
    private:
    /// @brief Output stream to write to.  Defaults to `std::cout` via the constructor.
    std::ostream& _out;



    /// @brief Number of lines from the previous write.
    size_t _line_count = 0;



    /// @brief Counts the number of lines in text.
    static size_t count_lines(const std::string& text) {
        // If it's empty, return 0.
        if (text.empty()) {
            return 0;
        }

        // There's at least one characters, which means at leats one line.  Add more if newline is seen.
        size_t count = 1;
        for (char c : text) {
            if (c == '\n') {
                count++;
            }
        }

        return count;
    }



    public:
    /// @brief Basic constructor.  Just needs a place to write to.
    explicit ProgressReport(std::ostream& out = std::cout) : _out(out) {}



    /// @brief Basic destructor.  Could call clear() here, but don't for now.
    ~ProgressReport() {}



    /// @brief Erases all lines written by `report()`.  Safe to call repeatedly.
    void clear() {
        for (size_t i = 0; i < _line_count; i++) {
            // Clear the current line.
            _out << "\033[2K";

            // Move the cursor up a line if needed.
            if (i + 1 < _line_count) {
                _out << "\033[1A";
            }
        }

        // Return to column 0.  Flush that change (and others from the loop above).  Then set line count to zero.
        _out << "\r";
        _out.flush();
        _line_count = 0;
    }



    /// @brief Clears previously written text and then emits `text`.  Single or multi line is valid.
    void report(const std::string& text) {
        clear();
        _out << text;
        _out.flush();
        _line_count = count_lines(text);
    }

};
