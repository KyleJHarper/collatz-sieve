#pragma once

#include "concepts.hpp"
#include <ostream>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>



//
// Stream Helper
// Assist with serializing data, deserializing data, and providing feedback when failures occur without throwing.
//
// BIG FAT NOTE!
// We *only* write and read in little-endian.  The end.
//
class StreamHelper {
    private:
    std::istream* _in_ptr = nullptr;
    std::ostream* _out_ptr = nullptr;
    std::string* _err_ptr = nullptr;
    std::string _category = "Uncategorized";
    static constexpr uint64_t _READ_WRITE_LIMIT = 1ULL << 30;  // ~1GB



    //
    // Get Out
    // Private helper to use out and apply a null check
    //
    std::ostream& get_out() {
        if (_out_ptr == nullptr) {
            throw std::logic_error("Out stream is not defined (nullptr).  Cannot write to it.");
        }
        return *_out_ptr;
    }
    //
    // Same for in.
    std::istream& get_in() {
        if (_in_ptr == nullptr) {
            throw std::logic_error("In stream is not defined (nullptr).  Cannot read from it.");
        }
        return *_in_ptr;
    }



    public:
    StreamHelper(std::istream* in, std::ostream* out, std::string* err = nullptr) {
        _in_ptr = in;
        _out_ptr = out;
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
    // Write Bytes
    // Generic helper to write bytes from any pointer location up to `size` bytes.
    //
    [[nodiscard]] bool write_bytes(const void* data, const uint64_t size) {
        if (size > _READ_WRITE_LIMIT) {
            return fail("Unable to write more than " + std::to_string(_READ_WRITE_LIMIT) + " bytes (" + std::to_string(size) + " requested)");
        }
        std::ostream& out = get_out();
        out.write(reinterpret_cast<const char*>(data), size);
        if (! out) {
            return fail("Failure in write_bytes() writing " + std::to_string(size) + " bytes");
        }
        return true;
    }



    //
    // Read Bytes
    // Generic helper to read bytes from any pointer location up to `size` bytes.
    //
    [[nodiscard]] bool read_bytes(void* data, const uint64_t size) {
        if (size > _READ_WRITE_LIMIT) {
            return fail("Unable to read more than " + std::to_string(_READ_WRITE_LIMIT) + " bytes (" + std::to_string(size) + " requested)");
        }
        std::istream& in = get_in();
        in.read(reinterpret_cast<char*>(data), size);
        if (! in) {
            return fail("Failure in read_bytes() reading " + std::to_string(size) + " bytes");
        }
        return true;
    }



    //
    // Serialize Integral
    // Will serialize any integral of type T.  When type is fixed-width, the caller is expected to deserialize using the same type
    // to ensure the correct number of bytes are read, and therefore no size data is emitted to the ostream.  When the type is
    // variable length (mpz_class), we will emit sign and (if non-zero) chunk count and then limb data.
    //
    // Note, you must use a GuaranteedWidthIntegral.  Things like size_t and int/uint are not portable.
    //
    template<AnySupportedIntegral T>
    [[nodiscard]] bool serialize_integral(const T& value) {
        std::ostream& out = get_out();
        if constexpr(BuiltinIntegral<T>) {
            static_assert(GuaranteedWidthIntegral<T>);
            T final_value = value;
            // Flip endianness if needed.
            if constexpr(sizeof(T) > 1 && is_big_endian()) {
                final_value = byteswap(final_value);
            }
            out.write(reinterpret_cast<const char*>(&final_value), sizeof(final_value));
            if (! out) {
                return fail("Unable to write integral to out in serialize_integral");
            }
        } else {
            // Sign: 0 = zero, 1 = positive, -1 = negative
            int8_t sign = mpz_sgn(value.get_mpz_t());
            if (! serialize_integral(sign)) {
                return fail("MPZ sign serialize failed");
            }

            // When zero, we can just skip anything else.
            if (sign == 0) {
                return true;
            }

            // Export limbs (little-endian, 1 byte per limb chunk)
            uint64_t size = 0;
            std::vector<uint8_t> buffer(mpz_sizeinbase(value.get_mpz_t(), 2) / 8 + 1);
            mpz_export(
                buffer.data(),
                &size,           // Track the count for writing later.
                -1,               // Least significant word first.
                sizeof(uint8_t),  // We're using 1-byte at a time.
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
        }

        // All good.
        return true;
    }



    //
    // Deserialize Integral
    // Will deserialize any integral of type T from "in".  When the type of T is fixed-width, this function uses the sizeof(T) to
    // determine how many bytes to read.  When T is variable length (mpz_class), it will expect a 8 bytes (unit64_t) to indicate
    // how many bytes of limb data should be read next.
    //
    // We will write the result to a return value (rv) to avoid allocs on the GMP path.
    //
    // Note, you must use a GuaranteedWidthIntegral.  Things like size_t and int/uint are not portable.
    //
    template<AnySupportedIntegral T>
    [[nodiscard]] bool deserialize_integral(T& rv) {
        std::istream& in = get_in();
        if constexpr(BuiltinIntegral<T>) {
            static_assert(GuaranteedWidthIntegral<T>);
            in.read(reinterpret_cast<char*>(&rv), sizeof(rv));
            if (! in) {
                return fail("Failed to read integral from deserialize_integral");
            }
            // Convert from little-endian if this system is big endian.
            if constexpr (sizeof(T) > 1 && is_big_endian()) {
                rv = byteswap(rv);
            }
        } else {
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
        }

        // All good.
        return true;
    }



    //
    // Serialize Bool
    // AFAIK, bool types are not guaranteed to be 1 byte.  When serializing/deserializing them, enforce it by casting to uint8_t.
    // Lean on integral version for endian logic.
    //
    [[nodiscard]] bool serialize_bool(const bool& value) {
        uint8_t u8_value = value ? 1 : 0;
        if (! serialize_integral(u8_value)) {
            return fail("Unable to serialize bool");
        }
        return true;
    }



    //
    // Deserialize Bool
    // AFAIK, bool types are not guaranteed to be 1 byte.  When serializing/deserializing them, enforce it by casting to uint8_t.
    // We'll use a return value (rv) to match others.
    // Lean on integral version for endian logic.
    //
    [[nodiscard]] bool deserialize_bool(bool& rv) {
        uint8_t u8_rv;
        if (! deserialize_integral(u8_rv)) {
            return fail("Unable to deserialize bool");
        }
        rv = u8_rv == 0 ? false : true;
        return true;
    }



    //
    // ByteSwap
    // Perform a standard bswap on any builtin type.  Requires __builtin_bswapXX().
    //
    template<BuiltinIntegral T>
    static T byteswap(T value) {
        if constexpr (sizeof(T) == 1) {
            return value;
        } else if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(value);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(value);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(value);
        } else if constexpr (sizeof(T) == 16) {
            // 128-bit needs broken into two chunks.
            uint64_t high = static_cast<uint64_t>(value >> 64);
            uint64_t low  = static_cast<uint64_t>(value);
            high = __builtin_bswap64(high);
            low  = __builtin_bswap64(low);
            return (static_cast<T>(low) << 64) | high;
        } else {
            static_assert(sizeof(T) <= 16, "Unsupported size for byteswap");
            return value;
        }
    }
};
