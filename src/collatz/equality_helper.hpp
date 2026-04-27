#pragma once

#include "abi_helpers.hpp"
#include "concepts.hpp"
#include <type_traits>
#include <typeinfo>



/**
* @class EqualityHelper
* @brief Compares integrals, booleans, and other data types, and reports differences to `_err_ptr`, optionally.
*
* This class exists to help compare two pieces of data and expects them to be identical.  It will treat inequality as an error and
* write it to the `_err_ptr` if specified.  A category can be set too, in the event equality is being tested in several areas of
* code or objects.
*/
class EqualityHelper {
    private:
    /// @brief The error pointer to write errors to.  Optional.
    std::string* _err_ptr = nullptr;
    /// @brief The category assigned (prepended) when an error (inequality) is found.
    std::string _category = "Uncategorized";



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Constructor simply connects caller's error string to this member, if any.
    EqualityHelper(std::string* err = nullptr) {
        _err_ptr = err;
    }

    /// @}



    /**
    * @brief Specify a category name to help disinguish where you're calling from.
    * @param category The category to set.
    */
    void set_category(const std::string& category) {
        _category = category;
    }



    /**
    * @brief Early-exit helper.  Emits a message and returns false for caller's flow-control.
    * @param msg The message to add to the error output, if specified.
    * @return Always false.  Do not discard.
    */
    [[nodiscard]] bool fail(const std::string& msg) {
        if (_err_ptr) {
            if (! _err_ptr->empty()) {
                *_err_ptr += " -> ";
            }
            *_err_ptr += "[" + _category + "] " + msg;
        }
        return false;
    };



    /**
    * @brief Compares two pointers to ensure they either both are null or both aren't.
    * @param a First pointer to compare.
    * @param b Second pointer to compare.
    * @tparam T Any data type.
    * @return A boolean indicating success or failure.  Do not discard.
    */
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



    /**
    * @brief Compares two integrals of type `T`.
    * @param a First integral to compare.
    * @param b Second integral to compare.
    * @tparam T Any supported integral (see concepts.hpp).
    * @return A boolean indicating success or failure.  Do not discard.
    */
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



    /**
    * @brief Compares two booleans.
    * @param a First boolean to compare.
    * @param b Second boolean to compare.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool booleans_equal(const bool a, const bool b) {
        if (a != b) {
            return fail("Boolean parity mismatch: a==" + to_string_any(a) + ", b==" + to_string_any(b));
        }
        return true;
    }



    /**
    * @brief Compares two strings.
    * @param a First string to compare.
    * @param b Second string to compare.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool strings_equal(const std::string& a, const std::string& b) {
        if (a != b) {
            return fail("String mismatch:  a==" + a + ", b==" + b);
        }
        return true;
    }



    /**
    * @brief Compares two pointers to see if they point to the same address.
    * @param a First pointer to compare.
    * @param b Second pointer to compare.
    * @tparam T Any type.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    template<typename T>
    [[nodiscard]] bool same_address(const T* a, const T* b) {
        if (a != b) {
            return fail("Pointer mismatch: pointers don't have the same address.");
        }
        return true;
    }



    /**
    * @brief Generic equality comparison.  Will attempt to deduce type T to perform the correct function from above.
    * @note When type is a pointer, address comparison is performed with `same_address()`.
    * @param a First item to compare.
    * @param b Second item to compare.
    * @tparam T Any type.
    * @return A boolean indicating success or failure.  Do not discard.
    */
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



    /// @brief Helper which simply performs: `!equal(a, b)`.  See it for details.
    template<typename T>
    [[nodiscard]] bool unequal(const T& a, const T& b) {
        return !equal(a, b);
    }

};
