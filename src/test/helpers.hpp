#pragma once
#include "../collatz/concepts.hpp"
#include <iostream>
#include <sstream>



inline void preamble(std::string& name) {
    std::cout << "=============================" << std::endl;
    std::cout << name << std::endl;
    std::cout << "=============================" << std::endl;
}



template<AnySupportedIntegral T>
inline void announce_run_all(std::string extra = "") {
    std::string data_type = "";
    if constexpr (std::is_same_v<T, uint64_t>) {
        data_type = "uint64_t";
    } else if constexpr (std::is_same_v<T, uint128_t>) {
        data_type = "uint128_t";
    } else if constexpr (std::is_same_v<T, mpz_class>) {
        data_type = "mpz_class";
    }
    std::cout << "Performing tests with " << data_type << "." << extra << std::endl;
}



inline void start_test(std::string& name) {
    std::cout << "  - " << name << "() ..." << std::flush;
}
inline void start_test(const char* name) {
    std::string s_name = name;
    start_test(s_name);
}



inline void end_test() {
    std::cout << " passed." << std::endl;
}



inline void done(std::string& name) {
    std::cout << "All " << name << " tests passed." << std::endl;
}



template<typename ObjType>
inline bool cloneable(const ObjType& src) {
    // Make a stream and error string.
    std::stringstream stream;
    std::string err;

    // Serialize to it.
    if (! src.serialize(stream, &err)) {
        std::cout << "Failed to seralize for cloning.  Error is: " << err << std::endl;
        return false;
    }

    // Make a blank object.
    ObjType copy;

    // Deserialize into it.
    if (! copy.deserialize(stream, &err)) {
        std::cout << "Failed to deseralize for cloning.  Error is: " << err << std::endl;
        return false;
    }

    // Confirm equality.
    if (! copy.equal(src, &err)) {
        std::cout << "Cloned object is not equal to source.  Error is: " << err << std::endl;
        return false;
    }

    return true;
}



template<typename ObjType>
inline bool equality(const ObjType& first, const ObjType& second, const ObjType& different) {
    std::string err;

    // An object should always be equal to itself.
    if (! first.equal(first, &err)) {
        std::cout << "First object is not equal to itself.  Error is: " << err << std::endl;
        return false;
    }

    // Second object should equal the first.
    if (! first.equal(second, &err)) {
        std::cout << "First object is not equal to second.  Error is: " << err << std::endl;
        return false;
    }

    // Static member should exist and work.
    if (! ObjType::st_equal(first, second, &err)) {
        std::cout << "First object is not equal to second (static method).  Error is: " << err << std::endl;
        return false;
    }

    // A different object shouldn't match.
    if (first.equal(different, &err)) {
        std::cout << "First object is equal to different, yet shouldn't be.";
        return false;
    }

    return true;
}