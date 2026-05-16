#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/stream_helper.hpp"
#include <sstream>
#include <random>




template<AnySupportedIntegral T>
void test_stream_helper_basic_construction() {
    start_test(__func__);

    // Default construction isn't available.

    // An error pointer is not required.
    StreamHelper sh(nullptr, nullptr);

    // It is, however, allowed, and should return messages.
    std::stringstream stream;
    std::string err;
    StreamHelper sh2(&stream, &stream, &err);
    if (sh.fail("test message") == true) {
        assert(false);
    }

    // Encode a value and decode it using the same stream.
    uint8_t value = 42;
    if (! sh2.serialize_integral(value)) {
        std::cout << "Failed to serialize integral.  Error string is: " << err << std::endl;
        assert(false);
    }
    uint8_t deserialized_value = 0;
    if (! sh2.deserialize_integral(deserialized_value)) {
        std::cout << "Failed to deserialize integral.  Error string is: " << err << std::endl;
        assert(false);
    }
    assert(value == deserialized_value);

    // Do it again with type T.
    T T_value = 42;
    if (! sh2.serialize_integral(T_value)) {
        std::cout << "Failed to serialize integral.  Error string is: " << err << std::endl;
        assert(false);
    }
    T T_deserialized_value = 0;
    if (! sh2.deserialize_integral(T_deserialized_value)) {
        std::cout << "Failed to deserialize integral.  Error string is: " << err << std::endl;
        assert(false);
    }
    assert(T_value == T_deserialized_value);

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_set_category() {
    start_test(__func__);

    std::string err;
    std::string category = "Test Category";
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);
    sh.set_category(category);
    if (sh.fail("msg") == true) {
        assert(false);
    }
    assert(err.find(category) != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_fail() {
    start_test(__func__);

    std::string err;
    std::string category = "Main Category";
    std::string sub_category = "Sub Category";
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);
    sh.set_category(category);

    // Basic failure in the main thread.
    if (sh.fail("from main thread") == true) {
        assert(false);
    }
    assert(err.find(category) != std::string::npos);

    // Send the same error to a sub-section and it should append meaningfully.
    auto sub = [](std::string* err = nullptr){
        std::string category = "Sub Category";
        std::stringstream stream;
        StreamHelper sh(&stream, &stream, err);
        sh.set_category(category);
        return sh.fail("from sub thread");
    };
    assert(sub(&err) == false);
    assert(err.find(sub_category) != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_serialize_integral() {
    start_test(__func__);

    std::string err;
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);

    // Smaller (1-byte) type.
    uint8_t value_8 = 42;
    uint8_t out_value_8 = 0;
    stream.str("");
    stream.clear();
    stream.seekg(0);
    if (! sh.serialize_integral(value_8)) {
        std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    // Read manually.
    stream.read(reinterpret_cast<char*>(&out_value_8), sizeof(value_8));
    assert(value_8 == out_value_8);

    // Larger (4-byte) type.
    uint32_t value_32 = 42;
    uint32_t out_value_32 = 0;
    stream.str("");
    stream.clear();
    stream.seekg(0);
    if (! sh.serialize_integral(value_32)) {
        std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    // Read manually.
    stream.read(reinterpret_cast<char*>(&out_value_32), sizeof(value_32));
    assert(value_32 == out_value_32);

    // Negative values should be okay.
    int32_t signed_value_32 = -42;
    int32_t out_signed_value_32 = 0;
    stream.str("");
    stream.clear();
    stream.seekg(0);
    if (! sh.serialize_integral(signed_value_32)) {
        std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    // Read manually.
    stream.read(reinterpret_cast<char*>(&out_signed_value_32), sizeof(signed_value_32));
    assert(signed_value_32 == out_signed_value_32);

    // Type T
    if constexpr(FixedWidthIntegral<T>) {
        // Positive Value
        T T_value = 42;
        T T_out_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read manually.
        stream.read(reinterpret_cast<char*>(&T_out_value), sizeof(T));
        assert(T_value == T_out_value);

        // Negative Value
        T T_signed_value = -42;
        T T_out_signed_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_signed_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read manually.
        stream.read(reinterpret_cast<char*>(&T_out_signed_value), sizeof(T));
        assert(T_signed_value == T_out_signed_value);
    } else if constexpr (GMPIntegral<T>) {
        // Similar to Fixed Width, but writes sign (+/-) and data length metadata which needs read.
        uint8_t sign = 0;
        uint64_t data_size = 0;
        std::vector<uint8_t> mpz_data;

        // Start with a small value in a single limb.
        T T_value = 42;
        T T_out_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read manually.
        // Step 1: First byte is the sign.
        stream.read(reinterpret_cast<char*>(&sign), sizeof(sign));
        // Step 2: Next 8 bytes is the data length of mpz limbs data.
        stream.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
        // Step 3: Raw limb data exported by mpz_export.  Requires mpz_import().
        mpz_data.resize(data_size);
        stream.read(reinterpret_cast<char*>(mpz_data.data()), data_size);
        mpz_import(
            T_out_value.get_mpz_t(),
            data_size,
            -1,               // Least significant word first.
            sizeof(uint8_t),  // Using 1 byte at a time.
            -1,               // Least significant byte first.
            0,
            mpz_data.data()
        );
        assert(T_value == T_out_value);

        // Now a negative value.
        T T_signed_value = -42;
        T T_out_signed_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_signed_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read manually.
        // Step 1: First byte is the sign.
        stream.read(reinterpret_cast<char*>(&sign), sizeof(sign));
        // Step 2: Next 8 bytes is the data length of mpz limbs data.
        stream.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
        // Step 3: Raw limb data exported by mpz_export.  Requires mpz_import().
        mpz_data.resize(data_size);
        stream.read(reinterpret_cast<char*>(mpz_data.data()), data_size);
        mpz_import(
            T_out_signed_value.get_mpz_t(),
            data_size,
            -1,               // Least significant word first.
            sizeof(uint8_t),  // Using 1 byte at a time.
            -1,               // Least significant byte first.
            0,
            mpz_data.data()
        );
        if (sign > 0) {
            T_out_signed_value = -T_out_signed_value;
        }
        assert(T_signed_value == T_out_signed_value);

        // Do it again with a value requiring multiple limbs.
        T T_value_big = 42;
        T_value_big <<= 255;
        T_value_big += 32342352352;
        T T_out_value_big = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_value_big)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read manually.
        // Step 1: First byte is the sign.
        stream.read(reinterpret_cast<char*>(&sign), sizeof(sign));
        // Step 2: Next 8 bytes is the data length of mpz limbs data.
        stream.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
        // Step 3: Raw limb data exported by mpz_export.  Requires mpz_import().
        mpz_data.resize(data_size);
        stream.read(reinterpret_cast<char*>(mpz_data.data()), data_size);
        mpz_import(
            T_out_value_big.get_mpz_t(),
            data_size,
            -1,               // Least significant word first.
            sizeof(uint8_t),  // Using 1 byte at a time.
            -1,               // Least significant byte first.
            0,
            mpz_data.data()
        );
        assert(T_value_big == T_out_value_big);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_deserialize_integral() {
    start_test(__func__);

    std::string err;
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);

    // Smaller (1-byte) type.
    uint8_t value_8 = 42;
    uint8_t out_value_8 = 0;
    stream.str("");
    stream.clear();
    stream.seekg(0);
    if (! sh.serialize_integral(value_8)) {
        std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    // Read from helper.
    if (! sh.deserialize_integral(out_value_8)) {
        std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    assert(value_8 == out_value_8);

    // Larger (4-byte) type.
    uint32_t value_32 = 42;
    uint32_t out_value_32 = 0;
    stream.str("");
    stream.clear();
    stream.seekg(0);
    if (! sh.serialize_integral(value_32)) {
        std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    // Read from helper.
    if (! sh.deserialize_integral(out_value_32)) {
        std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    assert(value_32 == out_value_32);

    // Negative values should be okay.
    int32_t signed_value_32 = -42;
    int32_t out_signed_value_32 = 0;
    stream.str("");
    stream.clear();
    stream.seekg(0);
    if (! sh.serialize_integral(signed_value_32)) {
        std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    // Read from helper.
    if (! sh.deserialize_integral(out_signed_value_32)) {
        std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
        assert(false);
    }
    assert(signed_value_32 == out_signed_value_32);

    // Type T
    if constexpr(FixedWidthIntegral<T>) {
        // Positive Value
        T T_value = 42;
        T T_out_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read from helper.
        if (! sh.deserialize_integral(T_out_value)) {
            std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        assert(T_value == T_out_value);

        // Negative Value
        T T_signed_value = -42;
        T T_out_signed_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_signed_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read from helper.
        if (! sh.deserialize_integral(T_out_signed_value)) {
            std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        assert(T_signed_value == T_out_signed_value);
    } else if constexpr (GMPIntegral<T>) {
        // Start with a small value in a single limb.
        T T_value = 42;
        T T_out_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read from helper.
        if (! sh.deserialize_integral(T_out_value)) {
            std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        assert(T_value == T_out_value);

        // Now a negative value.
        T T_signed_value = -42;
        T T_out_signed_value = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_signed_value)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read from helper.
        if (! sh.deserialize_integral(T_out_signed_value)) {
            std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        assert(T_signed_value == T_out_signed_value);

        // Do it again with a value requiring multiple limbs.
        T T_value_big = 42;
        T_value_big <<= 255;
        T_value_big += 32342352352;
        T T_out_value_big = 0;
        stream.str("");
        stream.clear();
        stream.seekg(0);
        if (! sh.serialize_integral(T_value_big)) {
            std::cout << "Failed to serialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        // Read from helper.
        if (! sh.deserialize_integral(T_out_value_big)) {
            std::cout << "Failed to deserialize integral.  Err: " << err << std::endl;
            assert(false);
        }
        assert(T_value_big == T_out_value_big);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_write_bytes() {
    start_test(__func__);

    std::string err;
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);

    std::vector<char> vec;
    std::vector<char> out;
    vec.push_back('a');
    vec.push_back('b');
    vec.push_back('c');
    vec.push_back('d');
    vec.push_back('e');
    vec.push_back('f');
    vec.push_back('g');
    if (! sh.write_bytes(vec.data(), vec.size())) {
        std::cout << "Failed to write data.  Err: " << err << std::endl;
        assert(false);
    }
    // Read using manual techniques, not "read_bytes()".
    out.resize(vec.size());
    stream.read(reinterpret_cast<char*>(out.data()), vec.size());
    for (size_t i = 0; i < vec.size(); i++) {
        assert(vec.at(i) == out.at(i));
    }

    // Rewind and read partial.
    size_t partial_size = 3;
    stream.seekg(0);
    out.clear();
    out.resize(partial_size);
    stream.read(reinterpret_cast<char*>(out.data()), partial_size);
    for (size_t i = 0; i < partial_size; i++) {
        assert(vec.at(i) == out.at(i));
    }

    // Read the remainder.
    size_t remainder_size = vec.size() - partial_size;
    out.clear();
    out.resize(remainder_size);
    stream.read(reinterpret_cast<char*>(out.data()), remainder_size);
    for (size_t i = 0; i < remainder_size; i++) {
        assert(vec.at(i + partial_size) == out.at(i));
    }

    // Test with type T.  Only stable with fixed-width integrals.  GMP is heap-allocated.
    if constexpr(FixedWidthIntegral<T>) {
        std::vector<T> T_vec;
        std::vector<T> T_out;
        T_vec.push_back(T(1));
        T_vec.push_back(T(2));
        T_vec.push_back(T(3));
        T_vec.push_back(T(4));
        T_vec.push_back(T(5));
        T_vec.push_back(T(6));
        T_vec.push_back(T(7));
        T_vec.push_back(T(8));
        T_vec.push_back(T(9));
        T_vec.push_back(T(10));
        if (! sh.write_bytes(T_vec.data(), T_vec.size() * sizeof(T))) {
            std::cout << "Failed to write data.  Err: " << err << std::endl;
            assert(false);
        }
        // Read using manual techniques, not "read_bytes()".
        T_out.resize(T_vec.size());
        stream.read(reinterpret_cast<char*>(T_out.data()), T_vec.size() * sizeof(T));
        for (size_t i = 0; i < T_vec.size(); i++) {
            assert(T_vec.at(i) == T_out.at(i));
        }
    }

    // Random data.
    // Clear the whole stream.
    stream.str("");
    stream.clear();
    stream.seekg(0);
    static thread_local std::mt19937_64 rng(123);
    size_t random_bytes_size = 4096;
    uint8_t* random_data = static_cast<uint8_t*>(std::malloc(random_bytes_size));
    size_t i = 0;
    while (i + sizeof(uint64_t) <= random_bytes_size) {
        uint64_t value = rng();
        std::memcpy(random_data + i, &value, sizeof(value));
        i += sizeof(value);
    }
    if (i < random_bytes_size) {
        uint64_t tail = rng();
        std::memcpy(random_data + i, &tail, random_bytes_size - i);
    }
    // Write it
    if (! sh.write_bytes(random_data, random_bytes_size)) {
        std::cout << "Failed to write data.  Err: " << err << std::endl;
        assert(false);
    }
    assert(stream.str().size() == random_bytes_size);
    // Read it.
    uint8_t* out_random_data = static_cast<uint8_t*>(std::malloc(random_bytes_size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(out_random_data), random_bytes_size);
    assert(stream.gcount() == static_cast<std::streamsize>(random_bytes_size));
    assert(std::memcmp(random_data, out_random_data, random_bytes_size) == 0);

    std::free(random_data);
    std::free(out_random_data);

    // Writing large values should fail for safety.
    size_t too_many_bytes = StreamHelper::READ_WRITE_LIMIT + 1;
    if (sh.write_bytes(vec.data(), too_many_bytes)) {
        std::cout << "Writing too many bytes worked, but shouldn't have." << std::endl;
        assert(false);
    }
    assert(err.find("Unable to write more than") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_read_bytes() {
    start_test(__func__);

    std::string err;
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);

    //
    // The write_bytes method has already been tested.  Use it for convenience here.
    //

    std::vector<char> vec;
    std::vector<char> out;
    vec.push_back('a');
    vec.push_back('b');
    vec.push_back('c');
    vec.push_back('d');
    vec.push_back('e');
    vec.push_back('f');
    vec.push_back('g');
    if (! sh.write_bytes(vec.data(), vec.size())) {
        std::cout << "Failed to write data.  Err: " << err << std::endl;
        assert(false);
    }
    // Read using read_bytes().
    out.resize(vec.size());
    if (! sh.read_bytes(out.data(), vec.size())) {
        std::cout << "Failed to read data.  Err: " << err << std::endl;
        assert(false);
    }
    for (size_t i = 0; i < vec.size(); i++) {
        assert(vec.at(i) == out.at(i));
    }

    // Rewind and read partial.
    size_t partial_size = 3;
    stream.seekg(0);
    out.clear();
    out.resize(partial_size);
    if (! sh.read_bytes(out.data(), partial_size)) {
        std::cout << "Failed to read data.  Err: " << err << std::endl;
        assert(false);
    }
    for (size_t i = 0; i < partial_size; i++) {
        assert(vec.at(i) == out.at(i));
    }

    // Read the remainder.
    size_t remainder_size = vec.size() - partial_size;
    out.clear();
    out.resize(remainder_size);
    if (! sh.read_bytes(out.data(), remainder_size)) {
        std::cout << "Failed to read data.  Err: " << err << std::endl;
        assert(false);
    }
    for (size_t i = 0; i < remainder_size; i++) {
        assert(vec.at(i + partial_size) == out.at(i));
    }

    // Test with type T.  Only stable with fixed-width integrals.  GMP is heap-allocated.
    if constexpr(FixedWidthIntegral<T>) {
        std::vector<T> T_vec;
        std::vector<T> T_out;
        T_vec.push_back(T(1));
        T_vec.push_back(T(2));
        T_vec.push_back(T(3));
        T_vec.push_back(T(4));
        T_vec.push_back(T(5));
        T_vec.push_back(T(6));
        T_vec.push_back(T(7));
        T_vec.push_back(T(8));
        T_vec.push_back(T(9));
        T_vec.push_back(T(10));
        if (! sh.write_bytes(T_vec.data(), T_vec.size() * sizeof(T))) {
            std::cout << "Failed to write data.  Err: " << err << std::endl;
            assert(false);
        }
        // Read using read_bytes().
        T_out.resize(T_vec.size());
        if (! sh.read_bytes(T_out.data(), T_vec.size() * sizeof(T))) {
            std::cout << "Failed to read data.  Err: " << err << std::endl;
            assert(false);
        }
        for (size_t i = 0; i < T_vec.size(); i++) {
            assert(T_vec.at(i) == T_out.at(i));
        }
    }

    // Random data.
    // Clear the whole stream.
    stream.str("");
    stream.clear();
    stream.seekg(0);
    static thread_local std::mt19937_64 rng(123);
    size_t random_bytes_size = 4096;
    uint8_t* random_data = static_cast<uint8_t*>(std::malloc(random_bytes_size));
    size_t i = 0;
    while (i + sizeof(uint64_t) <= random_bytes_size) {
        uint64_t value = rng();
        std::memcpy(random_data + i, &value, sizeof(value));
        i += sizeof(value);
    }
    if (i < random_bytes_size) {
        uint64_t tail = rng();
        std::memcpy(random_data + i, &tail, random_bytes_size - i);
    }
    // Write it
    if (! sh.write_bytes(random_data, random_bytes_size)) {
        std::cout << "Failed to write data.  Err: " << err << std::endl;
        assert(false);
    }
    assert(stream.str().size() == random_bytes_size);
    // Read it.
    uint8_t* out_random_data = static_cast<uint8_t*>(std::malloc(random_bytes_size));
    stream.seekg(0);
    if (! sh.read_bytes(out_random_data, random_bytes_size)) {
        std::cout << "Failed to read data.  Err: " << err << std::endl;
        assert(false);
    }
    assert(stream.gcount() == static_cast<std::streamsize>(random_bytes_size));
    assert(std::memcmp(random_data, out_random_data, random_bytes_size) == 0);

    std::free(random_data);
    std::free(out_random_data);

    // Reading large values should fail for safety.
    size_t too_many_bytes = StreamHelper::READ_WRITE_LIMIT + 1;
    if (sh.read_bytes(vec.data(), too_many_bytes)) {
        std::cout << "Reading too many bytes worked, but shouldn't have." << std::endl;
        assert(false);
    }
    assert(err.find("Unable to read more than") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_serialize_bool() {
    start_test(__func__);

    std::string err;
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);

    bool b_true = true;
    bool b_out = false;
    if (! sh.serialize_bool(b_true)) {
        std::cout << "Failed to serialize bool.  Err: " << err << std::endl;
        assert(false);
    }
    // Manually read.  Bools are stored as whole bytes (uint8_t) in case some platforms would emit bits or weirdness.
    uint8_t buffer = 0;
    assert(b_true != b_out);
    stream.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));
    b_out = buffer > 0;
    assert(b_true == b_out);

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_deserialize_bool() {
    start_test(__func__);

    std::string err;
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);

    bool b_true = true;
    bool b_out = false;
    if (! sh.serialize_bool(b_true)) {
        std::cout << "Failed to serialize bool.  Err: " << err << std::endl;
        assert(false);
    }
    // Read with helper.
    if (! sh.deserialize_bool(b_out)) {
        std::cout << "Failed to deserialize bool.  Err: " << err << std::endl;
        assert(false);
    }
    assert(b_true == b_out);

    end_test();
}



template<AnySupportedIntegral T>
void test_stream_helper_() {
    start_test(__func__);

    std::string err;
    std::stringstream stream;
    StreamHelper sh(&stream, &stream, &err);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_stream_helper_basic_construction<T>();
    test_stream_helper_set_category<T>();
    test_stream_helper_fail<T>();
    test_stream_helper_serialize_integral<T>();
    test_stream_helper_deserialize_integral<T>();
    test_stream_helper_write_bytes<T>();
    test_stream_helper_read_bytes<T>();
    test_stream_helper_serialize_bool<T>();
    test_stream_helper_deserialize_bool<T>();
}



int main() {
    std::string name = "StreamHelper";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
