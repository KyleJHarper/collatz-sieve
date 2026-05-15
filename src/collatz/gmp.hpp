#pragma once

#include <cstddef>
#include <stdint.h>
#include <gmpxx.h>
#include <string_view>



/**
* @namespace GMP
* @brief Collection of methods for managing GMP data types, such as `mpz_class` and `mpf_class`.
*/
namespace GMP {

    /**
    * @brief Try to calculate the true (heap) size of an `mpz_class` object.
    * @param obj Reference to the object.
    * @return Best attempt at the total bytes used by the object.
    */
    inline size_t deep_sizeof(const mpz_class& obj) {
        return sizeof(mpz_class) + (mpz_size(obj.get_mpz_t()) * sizeof(mp_limb_t));
    }



    /**
    * @brief Try to calculate the true (heap) size of an `mpf_class` object.
    * @param obj Reference to the object.
    * @return Best attempt at the total bytes used by the object.
    */
    inline size_t deep_sizeof(const mpf_class& obj) {
        return sizeof(mpf_class) + (mpf_size(obj.get_mpf_t()) * sizeof(mp_limb_t));
    }



    /**
    * @brief Retrieve exactly 64 bits from an `mpz_t`.
    * @param obj Reference to the `mpz_t` object.
    * @return A `uint64_t` with the bits.
    */
    inline uint64_t mpz_get_ui64(const mpz_t obj) {
        uint64_t result = 0;
        mpz_export(&result, nullptr, -1, sizeof(result), 0, 0, obj);
        return result;
    }



    /**
    * @brief Convert a string view to an `mpz_class`.  Uses GMP's `mpz_set_string()` internally.
    * @param view The string view to parse.
    * @return The mpz_class representation of the string digits sent, if valid.
    */
    inline mpz_class parse_mpz(std::string_view view) {
        mpz_class value;
        if (mpz_set_str(value.get_mpz_t(), view.data(), 10) != 0) {
            throw std::invalid_argument("invalid digit in _mpz literal");
        }
        return value;
    }



    /**
    * @struct MpzEq
    * @brief An equality operator wrapped in a struct for Abseil-CPP to compare hash values with.
    */
    struct MpzEq {
        /// @brief Equality test.
        bool operator()(const mpz_class& a, const mpz_class& b) const {
            return a == b;
        }
    };

}



/**
* @brief Add capability to has `mpz_class` types with Abseil-CPP.  Required here for ADL.
* @tparam H Used internally by Absl.
* @param value The value to hash, which happens by combining the limb count and data to `h`.
*/
template <typename H>
H AbslHashValue(H h, const mpz_class& value) {
    // Get the limb count and add it to the hash.
    const mp_limb_t* limbs = mpz_limbs_read(value.get_mpz_t());
    size_t limb_count = mpz_size(value.get_mpz_t());
    h = H::combine(std::move(h), limb_count);

    // Now include the limbs themselves.
    for (size_t i = 0; i < limb_count; i++) {
        h = H::combine(std::move(h), limbs[i]);
    }
    return h;
}
