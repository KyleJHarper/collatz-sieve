#include <cstdint>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <gmpxx.h>
#include <gmp.h>
#include <ctime>
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"
#include "collatz/collatz.hpp"
#include "collatz/binary_tree_math.hpp"
#include "collatz/collatz_affine_map.hpp"
#include <CLI/CLI.hpp>



inline int bit_width_uint128(__uint128_t n) {
    if (n == 0) return 0;

    uint64_t high = uint64_t(n >> 64);
    if (high != 0) {
        return 128 - __builtin_clzll(high);
    } else {
        uint64_t low = uint64_t(n);
        return 64 - __builtin_clzll(low);
    }
}



template<AnySupportedIntegral T>
void test_1(T max_value, T report_every) {
    logger->info("Test 1 -- Fully Nieve");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            steps++;
            if (n % 2 == 0) {
                // Even
                n = n / 2;
            } else {
                // Odd
                n = (3 * n) + 1;
            }
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_1_1(T max_value, T report_every) {
    if constexpr(FixedWidthIntegral<T>) {
        logger->info(("Test 1.1 -- Not applicable to non-GMP types."));
        return;
    }
    logger->info("Test 1.1 -- NOT CARRIED FORWARD!  Fully Nieve using GMP C API instead of class operators");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            steps++;
            if (n % 2 == 0) {
                // Even
                if constexpr(FixedWidthIntegral<T>) {
                    n = n / 2;
                } else {
                    mpz_fdiv_q_2exp(n.get_mpz_t(), n.get_mpz_t(), 1);
                }
            } else {
                // Odd
                if constexpr(FixedWidthIntegral<T>) {
                    n = (3 * n) + 1;
                } else {
                    mpz_mul_ui(n.get_mpz_t(), n.get_mpz_t(), 3);
                    mpz_add_ui(n.get_mpz_t(), n.get_mpz_t(), 1);
                }
            }
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_2(T max_value, T report_every) {
    logger->info("Test 2 -- Switch Comparison to Bit Check");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            steps++;
            if ((n & 1) == 0) {
                // Even
                n = n / 2;
            } else {
                // Odd
                n = (3 * n) + 1;
            }
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_3(T max_value, T report_every) {
    logger->info("Test 3 -- Switch 3x to Shift-and-Add");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            steps++;
            if ((n & 1) == 0) {
                // Even
                n = n / 2;
            } else {
                // Odd
                n = (n << 1) + n + 1;
            }
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_4(T max_value, T report_every) {
    logger->info("Test 4 -- Switch x/2 to Right Shift");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            steps++;
            if ((n & 1) == 0) {
                // Even
                n >>= 1;
            } else {
                // Odd
                n = (n << 1) + n + 1;
            }
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_5(T max_value, T report_every) {
    logger->info("Test 5 -- Apply x/2 After Odd Automatically");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            steps++;
            if ((n & 1) == 0) {
                // Even
                n >>= 1;
            } else {
                // Odd
                n = ((n << 1) + n + 1) >> 1;
                steps++;
            }
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_6(T max_value, T report_every) {
    logger->info("Test 6 -- Remove Else Clause for if/then Block");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            if ((n & 1) == 1) {
                // Odd
                n = (n << 1) + n + 1;
                steps++;
            }
            // Always Even At This Point
            n >>= 1;
            steps++;
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_7(T max_value, T report_every) {
    logger->info("Test 7 -- Shift x/2 By Count of Trailing Zeros");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    size_t right_shifts;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            if ((n & 1) == 1) {
                // Odd
                n = (n << 1) + n + 1;
                steps++;
            }
            // Always Even At This Point
            right_shifts = Bit::count_trailing_zeros(n);
            n >>= right_shifts;
            steps += right_shifts;
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_8(T max_value, T report_every) {
    logger->info("Test 8 -- Apply 3x+1 According to Count of Trailing Ones with a Loop");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    size_t right_shifts;
    size_t trailing_ones;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            // The number of trailing ones indicates how many times we'll do (3x+1)/2, followed by an even at the end.
            if ((n & 1) == 1) {
                trailing_ones = Bit::count_trailing_ones(n);
                if (trailing_ones > 4) {
                    for (size_t i = 1; i <= trailing_ones; i++) {
                        // Combine it as: (3x + 1) / 2.
                        n = ((n << 1) + n + 1) >> 1;
                    }
                    steps += (2 * trailing_ones);
                } else {
                    // It's not worth the loop.  Just do the 3x+1.
                    n = (n << 1) + n + 1;
                    steps++;
                }
            }
            // Always Even At This Point
            right_shifts = Bit::count_trailing_zeros(n);
            n >>= right_shifts;
            steps += right_shifts;
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_9(T max_value, T report_every) {
    logger->info("Test 9 -- Use an Affine Map for (3x+1)/2 Based on Trailing Ones Count");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    size_t right_shifts;
    size_t trailing_ones;
    CollatzAffineMap<T> af_map;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1) {
            if ((n & 1) == 1) {
                af_map.reset();
                static thread_local T tls_out;
                trailing_ones = Bit::count_trailing_ones(n);
                steps += (2 * trailing_ones);
                for (size_t i = 0; i < trailing_ones; i++) {
                    af_map.apply_F();
                }
                af_map.calculate(n, tls_out);
                n = tls_out;
            }
            // Always Even At This Point
            right_shifts = Bit::count_trailing_zeros(n);
            n >>= right_shifts;
            steps += right_shifts;
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_10(T max_value, T report_every) {
    logger->info("Test 10 -- Stop at the AST (for comparison; doesn't go to 1)");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T total_steps = 0;
    T next_report = report_every;
    size_t right_shifts;
    size_t trailing_ones;
    CollatzAffineMap<T> af_map;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        T n = i;
        T steps = 0;
        while (n > 1 && n >= i) {
            if ((n & 1) == 1) {
                af_map.reset();
                static thread_local T tls_out;
                trailing_ones = Bit::count_trailing_ones(n);
                steps += (2 * trailing_ones);
                for (size_t i = 0; i < trailing_ones; i++) {
                    af_map.apply_F();
                }
                af_map.calculate(n, tls_out);
                n = tls_out;
            }
            // Always Even At This Point
            right_shifts = Bit::count_trailing_zeros(n);
            n >>= right_shifts;
            steps += right_shifts;
        }
        total_steps += steps;
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
}



template<AnySupportedIntegral T>
void test_uint64_t_ideal(T max_value, T report_every) {
    logger->info("Test uint64_t ideal -- Ideal for uint64_t");
    if constexpr(FixedWidthIntegral<T> && sizeof(T) == 8) {
        logger->info("  - Test 1 Verdict: Naive baseline.");
        logger->info("  - Test 2 Verdict: Use n % 2 instead of n & 1.");
        logger->info("  - Test 3 Verdict: Perform shift and add.");
        logger->info("  - Test 4 Verdict: Right shift instead of x/2.");
        logger->info("  - Test 5 Verdict: Do not shift automatically after 3x + 1.");
        logger->info("  - Test 6 Verdict: Remove else-clause after odd check.");
        logger->info("  - Test 7 Verdict: Right shift by count of trailing zeros (ctz).");
        logger->info("  - Test 8 Verdict: Do not apply (3x + 1) / 2 by count of trailing ones (cto).");
        logger->info("  - Test 9 Verdict: Do not use an affine map by count of trailing ones (cto).");
        logger->info("  - Test 10 Verdict: n/a.  Only provided as reference.");
        std::chrono::time_point start = std::chrono::high_resolution_clock::now();
        T total_steps = 0;
        T next_report = report_every;
        size_t right_shifts;
        for (T i = 1; i <= max_value; i++) {
            if (report_every > 0 && i >= next_report) {
                logger->info("  Progress {}/{} {}%"
                    , to_string_any(i)
                    , to_string_any(max_value)
                    , to_string_any(T(100 * i / max_value))
                );
                next_report += report_every;
            }
            T n = i;
            T steps = 0;
            while (n > 1) {
                if (n % 2 == 1) {
                    // Odd
                    n = (n << 1) + n + 1;
                    steps++;
                }
                // Always Even At This Point
                right_shifts = Bit::count_trailing_zeros(n);
                n >>= right_shifts;
                steps += right_shifts;
            }
            total_steps += steps;
        }
        std::chrono::time_point end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
    } else {
        logger->info("  Not applicable.");
    }
}



template<AnySupportedIntegral T>
void test_uint128_t_ideal(T max_value, T report_every) {
    logger->info("Test uint128_t ideal -- Ideal for uint128_t");
    if constexpr(FixedWidthIntegral<T> && sizeof(T) == 16) {
        logger->info("  - Test 1 Verdict: Naive baseline.");
        logger->info("  - Test 2 Verdict: Negligible.  Use n % 2 instead of n & 1.");
        logger->info("  - Test 3 Verdict: Negligible.  Perform shift and add.");
        logger->info("  - Test 4 Verdict: Negligible.  Right shift instead of x/2.");
        logger->info("  - Test 5 Verdict: Negligible.  Do not shift automatically after 3x + 1.");
        logger->info("  - Test 6 Verdict: Negligible.  Remove else-clause after odd check.");
        logger->info("  - Test 7 Verdict: Right shift by count of trailing zeros (ctz).");
        logger->info("  - Test 8 Verdict: Do not apply (3x + 1) / 2 by count of trailing ones (cto).");
        logger->info("  - Test 9 Verdict: Do not use an affine map by count of trailing ones (cto).");
        logger->info("  - Test 10 Verdict: n/a.  Only provided as reference.");
        std::chrono::time_point start = std::chrono::high_resolution_clock::now();
        T total_steps = 0;
        T next_report = report_every;
        size_t right_shifts;
        for (T i = 1; i <= max_value; i++) {
            if (report_every > 0 && i >= next_report) {
                logger->info("  Progress {}/{} {}%"
                    , to_string_any(i)
                    , to_string_any(max_value)
                    , to_string_any(T(100 * i / max_value))
                );
                next_report += report_every;
            }
            T n = i;
            T steps = 0;
            while (n > 1) {
                if (n % 2 == 1) {
                    // Odd
                    n = (n << 1) + n + 1;
                    steps++;
                }
                // Always Even At This Point
                right_shifts = Bit::count_trailing_zeros(n);
                n >>= right_shifts;
                steps += right_shifts;
            }
            total_steps += steps;
        }
        std::chrono::time_point end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
    } else {
        logger->info("  Not applicable.");
    }
}



template<AnySupportedIntegral T>
void test_mpz_class_ideal(T max_value, T report_every) {
    logger->info("Test mpz_class ideal -- Ideal for mpz_class");
    if constexpr(GMPIntegral<T>) {
        logger->info("  - Test 1 Verdict: Naive baseline.");
        logger->info("  - Test 2 Verdict: Use n & 1 instead of n % 2.");
        logger->info("  - Test 3 Verdict: Do not shift and add.");
        logger->info("  - Test 4 Verdict: Right shift instead of x/2.");
        logger->info("  - Test 5 Verdict: Negligible.  Do not shift automatically after 3x + 1.");
        logger->info("  - Test 6 Verdict: Negligible.  Remove else-clause after odd check.");
        logger->info("  - Test 7 Verdict: Right shift by count of trailing zeros (ctz).");
        logger->info("  - Test 8 Verdict: Negligible.  Use affine map in test 9 instead.");
        logger->info("  - Test 9 Verdict: Use an affine map by count of trailing ones (cto).");
        logger->info("  - Test 10 Verdict: n/a.  Only provided as reference.");
        std::chrono::time_point start = std::chrono::high_resolution_clock::now();
        T total_steps = 0;
        T next_report = report_every;
        size_t right_shifts;
        size_t trailing_ones;
        CollatzAffineMap<T> af_map;
        for (T i = 1; i <= max_value; i++) {
            if (report_every > 0 && i >= next_report) {
                logger->info("  Progress {}/{} {}%"
                    , to_string_any(i)
                    , to_string_any(max_value)
                    , to_string_any(T(100 * i / max_value))
                );
                next_report += report_every;
            }
            T n = i;
            T steps = 0;
            while (n > 1) {
                if ((n & 1) == 1) {
                    // Odd
                    af_map.reset();
                    static thread_local T tls_out;
                    trailing_ones = Bit::count_trailing_ones(n);
                    steps += (2 * trailing_ones);
                    for (size_t i = 0; i < trailing_ones; i++) {
                        af_map.apply_F();
                    }
                    af_map.calculate(n, tls_out);
                    n = tls_out;
                }
                // Always Even At This Point
                right_shifts = Bit::count_trailing_zeros(n);
                n >>= right_shifts;
                steps += right_shifts;
            }
            total_steps += steps;
        }
        std::chrono::time_point end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        logger->info("Done in {}ms.  Took {} steps.", duration.count(), to_string_any(total_steps));
    } else {
        logger->info("  Not applicable.");
    }
}



template<AnySupportedIntegral T>
void test_st_verify(T max_value, T report_every) {
    logger->info("Test Collatz<T>::st_verify -- See if st_verify matches the peak performance above (slightly faster without step tracking).");
    std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    T next_report = report_every;
    for (T i = 1; i <= max_value; i++) {
        if (report_every > 0 && i >= next_report) {
            logger->info("  Progress {}/{} {}%"
                , to_string_any(i)
                , to_string_any(max_value)
                , to_string_any(T(100 * i / max_value))
            );
            next_report += report_every;
        }
        if (Collatz<T>::st_verify(i) == false) {
            throw std::logic_error("Couldn't get true from st_verify.  How?");
        }
    }
    std::chrono::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    logger->info("Done in {}ms.  Took {} steps.", duration.count(), "n/a");
}



template<AnySupportedIntegral T>
void run_all(size_t bits, size_t progress_every_x_percent) {
    T max_value = T(1) << bits;
    T report_every = progress_every_x_percent == 0 ? T(0) : (max_value * progress_every_x_percent) / 100;
    test_1<T>(max_value, report_every);
    test_1_1<T>(max_value, report_every);
    test_2<T>(max_value, report_every);
    test_3<T>(max_value, report_every);
    test_4<T>(max_value, report_every);
    test_5<T>(max_value, report_every);
    test_6<T>(max_value, report_every);
    test_7<T>(max_value, report_every);
    test_8<T>(max_value, report_every);
    test_9<T>(max_value, report_every);
    test_10<T>(max_value, report_every);
    test_uint64_t_ideal<T>(max_value, report_every);
    test_uint128_t_ideal<T>(max_value, report_every);
    test_mpz_class_ideal<T>(max_value, report_every);
    test_st_verify<T>(max_value, report_every);
}



int main(int argc, char **argv) {
    // Initialize logger.
    init_logger("log/collatz_compression.log");



    // Process options.
    size_t bits;
    bool force_128bit = false;
    bool force_mpz = false;
    size_t progress_every_x_percent;
    bool verbose;
    CLI::App options("Benchmarks various forms of compression to speed up Collatz testing.");
    options.add_option("-b,--bits", bits, "Number bits for testing.  AKA 2^bits.")->default_val(16);
    options.add_flag("-i,--int128", force_128bit, "Use 128-bit integer instead of native 64-bit integral type.");
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class instead of native 64-bit integral type.");
    options.add_option("-p,--progress", progress_every_x_percent, "Report progress every X%.  Zero disables.")->default_val(5);
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Bits: {}", bits);
    logger->debug("  Force int128: {}", force_128bit);
    logger->debug("  Force MPZ: {}", force_mpz);
    logger->debug("  Progress Every: {}%", progress_every_x_percent);
    logger->debug("  Verbose: {}", verbose);

    // Set data type.
    // -- Respect Forced Data Type
    std::string data_type = "uint64_t";
    if (force_128bit) {
        logger->info("Upgrading to uint128_t because user specified option.");
        data_type = "uint128_t";
    }
    if (force_mpz) {
        logger->info("Upgrading to mpz_class because user specified option.");
        data_type = "mpz_class";
    }
    // -- Upgrade If Needed.
    if (data_type == "uint64_t") {
        uint64_t max_initial_value = CollatzConstants::get_max_initial_value_by_bit<uint64_t>(64);
        size_t max_bits = BinaryTreeMath<uint64_t>::st_max_full_level_at_node_value(max_initial_value);
        if (bits > max_bits) {
            logger->info("Bits (-b) is over {} and you didn't specify -i or -m.  Auto upgrading from uint64_t to uint128_t.", max_bits);
            data_type = "uint128_t";
        }
    }
    if (data_type == "uint128_t" && bits > 127) {
        uint128_t max_initial_value = CollatzConstants::get_max_initial_value_by_bit<uint128_t>(128);
        size_t max_bits = BinaryTreeMath<uint128_t>::st_max_full_level_at_node_value(max_initial_value);
        if (bits > max_bits) {
            logger->info("Bits (-b) is over {} and you didn't specify -m.  Auto upgrading from uint128_t to mpz_class.", max_bits);
            data_type = "mpz_class";
        }
    }


    //
    // Run
    //
    if (data_type == "uint64_t") {
        run_all<uint64_t>(bits, progress_every_x_percent);
    }
    if (data_type == "uint128_t") {
        run_all<uint128_t>(bits, progress_every_x_percent);
    }
    if (data_type == "mpz_class") {
        run_all<mpz_class>(bits, progress_every_x_percent);
    }



    // Go home.
    return 0;
}
