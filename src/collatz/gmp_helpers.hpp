#pragma once

#include <cstddef>
#include <stdint.h>
#include <gmpxx.h>



/**
* @namespace GMPHelpers
* @brief Collection of methods for managing GMP data types, such as `mpz_class` and `mpf_class`.
*/
namespace GMPHelpers {

    /**
    * @brief Try to calculate the true (heap) size of an `mpz_class` object.
    * @param obj Reference to the object.
    * @return Best attempt at the total bytes used by the object.
    */
    inline size_t gmp_deep_sizeof(const mpz_class& obj) {
        return sizeof(mpz_class) + (mpz_size(obj.get_mpz_t()) * sizeof(mp_limb_t));
    }



    /**
    * @brief Try to calculate the true (heap) size of an `mpf_class` object.
    * @param obj Reference to the object.
    * @return Best attempt at the total bytes used by the object.
    */
    inline size_t gmp_deep_sizeof(const mpf_class& obj) {
        return sizeof(mpf_class) + (mpf_size(obj.get_mpf_t()) * sizeof(mp_limb_t));
    }



    /**
    * @brief Retrieve xactly 64 bits from an `mpz_t`.
    * @param obj Reference to the `mpz_t` object.
    * @return A `uint64_t` with the bits.
    */
    inline uint64_t mpz_get_ui64(const mpz_t& obj) {
        uint64_t result = 0;
        mpz_export(&result, nullptr, -1, sizeof(result), 0, 0, obj);
        return result;
    }

}
