#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/equality_helper.hpp"
#include <vector>





template<AnySupportedIntegral T>
void test_equality_helper_basic_construction() {
    start_test(__func__);

    // An error pointer is not required.
    EqualityHelper eq;

    // It is, however, allowed, and should return messages.
    std::string err;
    EqualityHelper eq2(&err);
    assert(eq2.integrals_equal(1, 2) == false);
    assert(err.find("[Uncategorized]") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_set_category() {
    start_test(__func__);

    std::string err;
    std::string category = "Test Category";
    EqualityHelper eq(&err);
    eq.set_category(category);
    assert(eq.integrals_equal(1, 2) == false);
    assert(err.find(category) != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_fail() {
    start_test(__func__);

    std::string err;
    std::string category = "Main Category";
    std::string sub_category = "Sub Category";
    EqualityHelper eq(&err);
    eq.set_category(category);

    // Basic failure in the main thread.
    assert(eq.integrals_equal(1, 2) == false);
    assert(err.find(category) != std::string::npos);

    // Send the same error to a sub-section and it should append meaningfully.
    auto sub = [](std::string* err = nullptr){
        std::string category = "Sub Category";
        EqualityHelper eq(err);
        eq.set_category(category);
        if (! eq.integrals_equal(1, 2)) {
            return eq.fail("These don't match from sub category");
        }
        return true;
    };
    assert(sub(&err) == false);
    assert(err.find(sub_category) != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_pointers_null_agree() {
    start_test(__func__);

    std::string err;
    EqualityHelper eq(&err);
    T value = 1;
    T* ptr_a = nullptr;
    T* ptr_b = nullptr;
    assert(eq.pointers_null_agree(ptr_a, ptr_b) == true);
    ptr_a = &value;
    assert(eq.pointers_null_agree(ptr_a, ptr_b) == false);
    assert(err.find("Pointer null agreement: a!=nullptr, b==nullptr") != std::string::npos);
    ptr_b = &value;
    assert(eq.pointers_null_agree(ptr_a, ptr_b) == true);
    ptr_a = nullptr;
    assert(eq.pointers_null_agree(ptr_a, ptr_b) == false);
    assert(err.find("Pointer null agreement: a==nullptr, b!=nullptr") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_integrals_equal() {
    start_test(__func__);

    T a = 1;
    T b = 1;
    std::string err;
    EqualityHelper eq(&err);
    assert(eq.integrals_equal(a, b) == true);
    b = 2;
    assert(eq.integrals_equal(a, b) == false);
    assert(err.find("Integral mismatch:  a==") != std::string::npos);
    assert(err.find(" b==") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_booleans_equal() {
    start_test(__func__);

    bool a = false;
    bool b = false;
    std::string err;
    EqualityHelper eq(&err);
    assert(eq.booleans_equal(a, b) == true);
    b = true;
    assert(eq.booleans_equal(a, b) == false);
    assert(err.find("Boolean parity mismatch:  a==") != std::string::npos);
    assert(err.find(" b==") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_strings_equal() {
    start_test(__func__);

    std::string a = "hello";
    std::string b = "hello";
    std::string err;
    EqualityHelper eq(&err);
    assert(eq.strings_equal(a, b) == true);
    b = "there";
    assert(eq.strings_equal(a, b) == false);
    assert(err.find("String mismatch:  a==") != std::string::npos);
    assert(err.find(" b==") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_same_address() {
    start_test(__func__);

    T value = 42;
    T* a = nullptr;
    T* b = nullptr;
    std::string err;
    EqualityHelper eq(&err);
    assert(eq.same_address(a, b) == true);
    a = &value;
    assert(eq.same_address(a, b) == false);
    assert(err.find("Pointer mismatch: pointers don't have the same address.") != std::string::npos);
    err.clear();
    a = nullptr;
    b = &value;
    assert(eq.same_address(a, b) == false);
    assert(err.find("Pointer mismatch: pointers don't have the same address.") != std::string::npos);
    err.clear();
    a = &value;
    assert(eq.same_address(a, b) == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_equality_helper_equal() {
    start_test(__func__);

    std::string err;
    EqualityHelper eq(&err);

    // Strings
    err.clear();
    std::string str_a = "hello";
    std::string str_b = "hello";
    assert(eq.equal(str_a, str_b) == true);
    str_b = "there";
    assert(eq.equal(str_a, str_b) == false);
    assert(err.find("String mismatch:  a==") != std::string::npos);
    assert(err.find(" b==") != std::string::npos);

    // Booleans
    err.clear();
    bool bool_a = false;
    bool bool_b = false;
    assert(eq.equal(bool_a, bool_b) == true);
    bool_b = true;
    assert(eq.equal(bool_a, bool_b) == false);
    assert(err.find("Boolean parity mismatch:  a==") != std::string::npos);
    assert(err.find(" b==") != std::string::npos);

    // Integrals
    err.clear();
    T T_a = 1;
    T T_b = 1;
    assert(eq.equal(T_a, T_b) == true);
    T_b = 2;
    assert(eq.equal(T_a, T_b) == false);
    assert(err.find("Integral mismatch:  a==") != std::string::npos);
    assert(err.find(" b==") != std::string::npos);

    // Pointers
    err.clear();
    T value = 1;
    T* ptr_a = nullptr;
    T* ptr_b = nullptr;
    assert(eq.equal(ptr_a, ptr_b) == true);
    ptr_a = &value;
    assert(eq.equal(ptr_a, ptr_b) == false);
    assert(err.find("Pointer null agreement: a!=nullptr, b==nullptr") != std::string::npos);
    ptr_b = &value;
    assert(eq.equal(ptr_a, ptr_b) == true);
    ptr_a = nullptr;
    assert(eq.equal(ptr_a, ptr_b) == false);
    assert(err.find("Pointer null agreement: a==nullptr, b!=nullptr") != std::string::npos);

    // Unknown types throw errors.
    err.clear();
    std::vector<T> vec_a;
    std::vector<T> vec_b;
    assert(eq.equal(vec_a, vec_b) == false);
    assert(err.find("No known equality operator is available for type:") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_equality_helper_basic_construction<T>();
    test_equality_helper_set_category<T>();
    test_equality_helper_fail<T>();
    test_equality_helper_pointers_null_agree<T>();
    test_equality_helper_integrals_equal<T>();
    test_equality_helper_booleans_equal<T>();
    test_equality_helper_strings_equal<T>();
    test_equality_helper_same_address<T>();
    test_equality_helper_equal<T>();
}



int main() {
    std::string name = "EqualityHelper";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
