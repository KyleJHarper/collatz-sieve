#pragma once

#include "abi.hpp"
#include "bit.hpp"
#include "concepts.hpp"
#include "endianness.hpp"
#include <ostream>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>



/**
* @class StreamHelper
* @brief Assists with serializing data, deserializing data, and providing feedback when failures occur without throwing.
* @note This class only writes in little-endian (it'll `bwsap` for you), and expects data from sources in little-endian.
*/
class StreamHelper {
    private:
    /// @brief Pointer to the input stream from reading data.
    std::istream* _in_ptr = nullptr;
    /// @brief Pointer to the output stream when writing data.
    std::ostream* _out_ptr = nullptr;
    /// @brief Pointer to an error string, where problems can be stored if found.
    std::string* _err_ptr = nullptr;
    /// @brief A category to append to each message sent to `_err_ptr`.
    std::string _category = "Uncategorized";



    /// @brief Private helper to use '_out_ptr` and apply a null check
    std::ostream& get_out() {
        if (_out_ptr == nullptr) {
            throw std::logic_error("Out stream is not defined (nullptr).  Cannot write to it.");
        }
        return *_out_ptr;
    }



    /// @brief Private helper to use `_in_ptr` and apply a null check.
    //
    // Same for in.
    std::istream& get_in() {
        if (_in_ptr == nullptr) {
            throw std::logic_error("In stream is not defined (nullptr).  Cannot read from it.");
        }
        return *_in_ptr;
    }



    public:
    /// *@brief A cap on data reading to prevent misuse or bugs eating RAM.  ~1GB.
    static constexpr uint64_t READ_WRITE_LIMIT = 1ULL << 30;



    /// @name Lifecycle Management
    /// @{

    /**
    * @brief Basic constructor.
    * @param in Pointer to an input stream for read operations.  Send `nullptr` if not needed.
    * @param out Pointer to an output stream for read operations.  Send `nullptr` if not needed.
    * @param err Pointer to a string where errors can be written.  If `nullptr`, ignored.
    */
    StreamHelper(std::istream* in, std::ostream* out, std::string* err = nullptr) {
        _in_ptr = in;
        _out_ptr = out;
        _err_ptr = err;
    }

    /// @}



    /// @brief Specify a category name to help disinguish where you're calling from.
    void set_category(const std::string& category) {
        _category = category;
    }



    /**
    * @brief Early-exit helper.  Emits a message and returns false for caller's flow-control.
    * @note This method does not throw.  Do not discard.
    * @param msg Details about the problem.  Will be prepended by the `_category`.  Ignored if `_err_ptr = nullptr`.
    * @return False, always.
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



    /// @name Integral Serialization
    /// @{

    /**
    * @brief Serialize any supported integral of type `T`.
    *
    * Will serialize any integral of type `T`.  When type is fixed-width, the caller is expected to deserialize using the same type
    * to ensure the correct number of bytes are read, and therefore no size data is emitted to the ostream.  When the type is
    * variable length (mpz_class), this will emit sign, data size, and (if non-zero) limb data.
    *
    * @note This method does not throw.  Do not discard.
    * @warning When using fixed-width, caller must use an explicit-width integral.  Things like size_t and int/uint are not portable.
    * @param value The value to serialize.
    * @tparam T Any supported integral (see concepts.hpp).
    * @return True if successful, false otherwise.
    */
    template<AnySupportedIntegral T>
    [[nodiscard]] bool serialize_integral(const T& value) {
        std::ostream& out = get_out();
        if constexpr(FixedWidthIntegral<T>) {
            T final_value = value;
            // Flip endianness if needed.
            if constexpr(sizeof(T) > 1 && Endian::is_big_endian()) {
                final_value = Bit::byteswap(final_value);
            }
            out.write(reinterpret_cast<const char*>(&final_value), sizeof(final_value));
            if (! out) {
                return fail("Unable to write integral to out in serialize_integral");
            }
        } else if constexpr (GMPIntegral<T>) {
            // Sign: 0 = zero, 1 = positive, -1 = negative
            int8_t sign = mpz_sgn(value.get_mpz_t());
            if (! serialize_integral(sign)) {
                return fail("MPZ sign serialize failed");
            }

            // When zero, just skip anything else.
            if (sign == 0) {
                return true;
            }

            // Export limbs (little-endian, 1 byte per limb chunk)
            uint64_t size = 0;
            std::vector<uint8_t> buffer(mpz_sizeinbase(value.get_mpz_t(), 2) / 8 + 1);
            mpz_export(
                buffer.data(),
                &size,            // Track the count for writing later.
                -1,               // Least significant word first.
                sizeof(uint8_t),  // Using 1-byte at a time.
                -1,               // Least significant byte first
                0,
                value.get_mpz_t()
            );

            // Upgrade the count to uint64_t.  Then write it out for deserialization later.
            if (! serialize_integral(size)) {
                return fail("MPZ size serialize");
            }

            // Write the data now.
            if (! write_bytes(buffer.data(), size)) {
                return fail("Unable to write mpz_class data payload in serialize_integral");
            }
        } else {
            return fail("Unsupported type sent to seralize_integral(): " + ABI::demangle<T>());
        }

        // All good.
        return true;
    }



    /**
    * @brief Deserialize any supported integral of type `T`.
    *
    * Will deserialize any integral of type `T`.  When type is fixed-width, the caller is expected to deserialize using the same
    * type to ensure the correct number of bytes are read, and therefore no size data is expected in the istream.  When the type is
    * variable length (mpz_class), this will expect a sign, data size, and (if non-zero) limb data.
    *
    * @note This method does not throw.  Do not discard.
    * @warning When using fixed-width, caller must use an explicit-width integral.  Things like size_t and int/uint are not portable.
    * @param rv Reference to the caller's memory to deserialize into.  Avoids GMP alloc().
    * @tparam T Any supported integral (see concepts.hpp).
    * @return True if successful, false otherwise.
    */
    template<AnySupportedIntegral T>
    [[nodiscard]] bool deserialize_integral(T& rv) {
        std::istream& in = get_in();
        if constexpr(FixedWidthIntegral<T>) {
            in.read(reinterpret_cast<char*>(&rv), sizeof(rv));
            if (! in) {
                return fail("Failed to read integral from deserialize_integral");
            }
            // Convert from little-endian if this system is big endian.
            if constexpr (sizeof(T) > 1 && Endian::is_big_endian()) {
                rv = Bit::byteswap(rv);
            }
        } else if constexpr (GMPIntegral<T>) {
            // First byte is the sign.
            int8_t sign;
            if (! deserialize_integral(sign)) {
                return fail("MPZ sign deserialize failed");
            }

            // When the sign is zero, there's no size or data to read.
            if (sign == 0) {
                return true;
            }

            // Read the size into a uint64_t from the next 8 bytes.
            uint64_t size;
            if (! deserialize_integral(size)) {
                return fail("MPZ size deserialize");
            }

            // Build a buffer, and read the next <size> bytes in from it for limb data.
            std::vector<uint8_t> buffer(size);
            if (! read_bytes(buffer.data(), size)) {
                return fail("Failed to read mpz_class data");
            }

            // Import the data to an mpz_class object.
            mpz_import(
                rv.get_mpz_t(),
                size,
                -1,               // Least significant word first.
                sizeof(uint8_t),  // Using 1 byte at a time.
                -1,               // Least significant byte first.
                0,
                buffer.data()
            );

            // Check the sign and flip it if necessary.
            if (sign < 0) {
                rv = -rv;
            }
        } else {
            return fail("Unsupported type sent to deseralize_integral(): " + ABI::demangle<T>());
        }

        // All good.
        return true;
    }

    /// @}



    /// @name Binary Serialization
    /// @{

    /**
    * @brief Generic helper to write bytes from any pointer location up to `size` bytes.
    * @note This method does not throw.  Do not discard.
    * @param data Any contiguous data writable by `reinterpret_cast<const char*>`.
    * @param size The number of bytes to write.
    * @return True if successful, false otherwise.
    */
    [[nodiscard]] bool write_bytes(const void* data, const uint64_t size) {
        if (size > READ_WRITE_LIMIT) {
            return fail("Unable to write more than " + std::to_string(READ_WRITE_LIMIT) + " bytes (" + std::to_string(size) + " requested)");
        }
        std::ostream& out = get_out();
        out.write(reinterpret_cast<const char*>(data), size);
        if (! out) {
            return fail("Failure in write_bytes() writing " + std::to_string(size) + " bytes");
        }
        return true;
    }



    /**
    * @brief Generic helper to read bytes from any pointer location up to `size` bytes.
    * @note This method does not throw.  Do not discard.
    * @param data Any contiguous data readable by `reinterpret_cast<const char*>`.
    * @param size The number of bytes to write.
    * @return True if successful, false otherwise.
    */
    [[nodiscard]] bool read_bytes(void* data, const uint64_t size) {
        if (size > READ_WRITE_LIMIT) {
            return fail("Unable to read more than " + std::to_string(READ_WRITE_LIMIT) + " bytes (" + std::to_string(size) + " requested)");
        }
        std::istream& in = get_in();
        in.read(reinterpret_cast<char*>(data), size);
        if (! in) {
            return fail("Failure in read_bytes() reading " + std::to_string(size) + " bytes");
        }
        return true;
    }

    /// @}



    /// @name Boolean Serialization
    /// @{

    /**
    * @brief Serialize boolean values.
    *
    * Serializes a boolean value, making sure to convert it to a `uint8_t` since bool types are not guaranteed to be a whole byte,
    * especially when bit-packing is involved.
    *
    * @note This method does not throw.  Do not discard.
    * @param value The value to serialize.
    * @return True if successful, false otherwise.
    */
    [[nodiscard]] bool serialize_bool(const bool& value) {
        uint8_t u8_value = value ? 1 : 0;
        if (! serialize_integral(u8_value)) {
            return fail("Unable to serialize bool");
        }
        return true;
    }



    /**
    * @brief Deserialize boolean values.
    *
    * Deserializes a boolean value, making sure to convert it from a `uint8_t` since bool types are not guaranteed to be a whole
    * byte, especially when bit-packing is involved.
    *
    * @note This method does not throw.  Do not discard.
    * @param rv Reference to caller's memory to update.
    * @return True if successful, false otherwise.
    */
    [[nodiscard]] bool deserialize_bool(bool& rv) {
        uint8_t u8_rv;
        if (! deserialize_integral(u8_rv)) {
            return fail("Unable to deserialize bool");
        }
        rv = u8_rv > 0;
        return true;
    }

    /// @}

};
