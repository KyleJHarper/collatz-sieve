#pragma once

#include "abi_helpers.hpp"
#include "concepts.hpp"
#include <type_traits>
#include <typeinfo>



//
// The equality checks for types often need error messages, pointer checking, etc.  This class wraps it up.
//
class EqualityHelper {
    private:
    std::string* _err_ptr = nullptr;
    std::string _category = "Uncategorized";



    public:
    EqualityHelper(std::string* err = nullptr) {
        _err_ptr = err;
    }



    //
    // Set Category
    // Specify a category name to help disinguish where you're calling from.
    //
    void set_category(const std::string& category) {
        _category = category;
    }



    //
    // Early-exit helper.  Emits a message and returns false for caller's flow-control.
    //
    [[nodiscard]] bool fail(const std::string& msg) {
        if (_err_ptr) {
            if (! _err_ptr->empty()) {
                *_err_ptr += " -> ";
            }
            *_err_ptr += "[" + _category + "] " + msg;
        }
        return false;
    };



    //
    // Pointers Null Agree
    // Compares two pointers to ensure they either both are null or both aren't.
    //
    template<typename T>
    [[nodiscard]] bool pointers_null_agree(const T* a, const T* b) {
        if (a == nullptr && b != nullptr) {
            return fail("Pointer null agreement: a==nullptr, b!=nullptr");
        }
        if (a != nullptr && b == nullptr) {
            return fail("Pointer null agreement: a!=nullptr, b==nullptr");
        }
        return true;
    };



    //
    // Integrals Equal
    // Compares two integrals of type T and returns true if they match, otherwise errors with name in the message.
    //
    template<AnySupportedIntegral T>
    [[nodiscard]] bool integrals_equal(const T& a, const T& b) {
        if constexpr (BuiltinIntegral<T>) {
            if (a != b) {
                return fail("Integral mismatch:  a==" + to_string_any(a) + ", b==" + to_string_any(b));
            }
        } else {
            if (mpz_cmp(a.get_mpz_t(), b.get_mpz_t()) != 0) {
                return fail("Integral mismatch:  a==" + to_string_any(a) + ", b==" + to_string_any(b));
            }
        }
        return true;
    }



    //
    // Booleans Equal
    // Compares two booleans and returns true if they match, otherwise errors with name in the message.
    //
    [[nodiscard]] bool booleans_equal(const bool a, const bool b) {
        if (a != b) {
            return fail("Boolean parity mismatch: a==" + to_string_any(a) + ", b==" + to_string_any(b));
        }
        return true;
    }



    //
    // Strings Equal
    // Compares two strings and returns true if they match, otherwise errors with name in the message.
    //
    [[nodiscard]] bool strings_equal(const std::string& a, const std::string& b) {
        if (a != b) {
            return fail("String mismatch:  a==" + a + ", b==" + b);
        }
        return true;
    }



    //
    // Pointers Equal
    // Compare two pointers and return true if they point to the same address.
    //
    template<typename T>
    [[nodiscard]] bool same_address(const T* a, const T* b) {
        if (a != b) {
            return fail("Pointer mismatch: pointers don't have the same address.");
        }
        return true;
    }



    //
    // Equal
    // Generic equality comparison.  Will attempt to deduce type T to perform the correct function from above.
    //
    // When type is mpz_class, we will use mpz_cmp() to avoid a temporary allocation.  You're welcome.
    // When type is pointer, we will compare address location (nullptr==nullptr is true)
    //
    template<typename T>
    [[nodiscard]] bool equal(const T& a, const T& b) {
        if constexpr(std::same_as<std::string, T>) {
            return strings_equal(a, b);
        } else if constexpr(std::same_as<bool, T>) {
            return booleans_equal(a, b);
        } else if constexpr(AnySupportedIntegral<T>) {
            return integrals_equal(a, b);
        } else if constexpr(std::is_pointer_v<T>) {
            if (! pointers_null_agree(a, b)) { return false; }
            return same_address(a, b);
        } else {
            return fail("No known equality operator is available for type: " + ABIHelpers::demangle(typeid(T).name()));
        }
    }



    //
    // Unequal
    // Simply applies this for convenience:  !equal(a, b)
    //
    template<typename T>
    [[nodiscard]] bool unequal(const T& a, const T& b) {
        return !equal(a, b);
    }

};
