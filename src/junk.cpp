#include "collatz/collatz_affine_stride.hpp"
#include <iostream>


// int main(int argc, char** argv) {

//     if (argc < 2) {
//         std::cerr << "You must specify count to verify as arg1." << std::endl;
//         return 1;
//     }

//     uint64_t count = std::atoll(argv[1]);
//     for (uint64_t i = 2; i < count; i++) {
//         if (! Collatz<uint64_t>::st_verify_to_hwm(i)) {
//             throw std::runtime_error("What...?");
//         }
//     }

// }


int main() {
    uint16_t max_add = 0;
    using ChosenStride = AffineStride::Table<8>;
    for(size_t i = 0; i < ChosenStride::TABLE.size(); i++) {
        const AffineStride::Stride& stride = ChosenStride::TABLE[i];
        std::cout << "i=" << i << "  bits_required=" << static_cast<int>(stride.bits_required) << "  add=" << stride.add << std::endl;
        if (stride.add > max_add) {
            max_add = stride.add;
        }
    }
    std::cout << "The maximum add value was: " << max_add << std::endl;
}