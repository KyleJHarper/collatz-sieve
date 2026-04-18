#pragma once

#include <cxxabi.h>
#include <string>


namespace ABIHelpers {
    // Demangle names.
    inline std::string demangle(const char* name) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        std::string result = (status == 0 && demangled) ? demangled : name;
        free(demangled);
        return result;
    }
}
