#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/zstd_compress.hpp"
#include "../collatz/zstd_decompress.hpp"
#include <random>
#include <sstream>




template<AnySupportedIntegral T>
void test_zstandard_round_trip() {
    start_test(__func__);

    // Hoist important bits.
    static thread_local std::mt19937_64 rng(123);
    std::stringstream compressed_stream(std::ios::in | std::ios::out | std::ios::binary);

    // Test Different Compressions
    size_t random_bytes_size = size_t(1) << 20; // 1MB
    size_t max_compression_level = 22;
    for (size_t compression_level = 0; compression_level < max_compression_level; compression_level++) {
        // Alloc space.
        uint8_t* random_data = static_cast<uint8_t*>(std::malloc(random_bytes_size));
        uint8_t* decompressed_random_data = static_cast<uint8_t*>(std::malloc(random_bytes_size));

        // Remake the streams.  We call finalize() which means it isn't reusable.
        zstd_ostream z_out(compressed_stream, compression_level);
        zstd_istream z_in(compressed_stream);

        // Write random data.
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

        // Compress to z_out.
        z_out.write(reinterpret_cast<char*>(random_data), random_bytes_size);
        z_out.finalize(); // Required by zstd.

        // Rewind for reading.
        compressed_stream.seekg(0);
        compressed_stream.seekp(0);
        compressed_stream.clear();

        // Decompress into a new buffer.
        z_in.read(reinterpret_cast<char*>(decompressed_random_data), random_bytes_size);

        // Compare
        assert(std::memcmp(random_data, decompressed_random_data, random_bytes_size) == 0);

        // Rewind for writing.
        compressed_stream.seekg(0);
        compressed_stream.seekp(0);
        compressed_stream.clear();

        // Free
        std::free(random_data);
        std::free(decompressed_random_data);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    // There's not a lot to test, frankly.  The class inherits from a stream and then leans on the ZSTD code to perform basic ops.
    // This suite will simply perform a round-trip on some random data.
    test_zstandard_round_trip<T>();
}



int main() {
    std::string name = "ZStandard";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
