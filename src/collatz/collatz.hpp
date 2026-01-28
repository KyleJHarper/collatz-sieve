#pragma once

#include <gmp.h>
#include <iostream>
#include <gmpxx.h>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <array>
#include <string>
#include <stdint.h>
#include "concepts.hpp"
#include "gmp_helpers.hpp"
#include "count_trailing_helpers.hpp"



//
// Custom Exceptions
//
class CollatzSequenceOverflow : public std::runtime_error {
    public:
    explicit CollatzSequenceOverflow(const std::string& msg) : std::runtime_error(msg) {}
};



//
// Sequences are not size_t in length.  Use a common type that's smaller.
//
typedef uint32_t seq_size_t;



//
// Constants and helpers for our sequences.
//
namespace CollatzConstants {
    // GMP will sometimes alloc() if you operate on a non-GMP (e.g.: ui) value.
    static const mpz_class MPZ_ONE = 1;
    static const mpz_class MPZ_TWO = 2;
    static const mpz_class MPZ_THREE = 3;

    // GMP float-style values used in a lot of calculations.
    static const mpf_class MPF_ONE = 1;
    static const mpf_class MPF_TWO = 2;
    static const mpf_class MPF_THREE = 3;
    static const mpf_class MPF_HALF = 0.5;

    // Let's lock in what "odd" and "even" mean.
    constexpr bool ODD = true;
    constexpr bool EVEN = false;

    // Trying to perform 3X+1 on any value higher than this would overflow a 64-bit unsigned integer.
    constexpr std::array<uint128_t, 5> MAX_3XP1 = {
        ((uint128_t(1) <<  8) - 1 - 1) / 3,   // 8 bit
        ((uint128_t(1) << 16) - 1 - 1) / 3,   // 16 bit
        ((uint128_t(1) << 32) - 1 - 1) / 3,   // 32 bit
        ((uint128_t(1) << 64) - 1 - 1) / 3,   // 64 bit
        ((uint128_t(1) << 127) + ((uint128_t(1) << 127) - 1) - 1) / 3  // 128 bit
        // Requires a little juggling to avoid overflow.  Yay PEMDAS!
    };
    //
    // Now a helper for it.
    template<BuiltinIntegral T>
    inline constexpr uint128_t get_max_3xp1() {
        switch (sizeof(T) * 8) {
            case   8: return MAX_3XP1[0];
            case  16: return MAX_3XP1[1];
            case  32: return MAX_3XP1[2];
            case  64: return MAX_3XP1[3];
            case 128: return MAX_3XP1[4];
            default: throw std::logic_error("Bit size not supported: " + std::to_string(sizeof(T) * 8));
        }
    }

    // Precomputed maximum initial values for a given bit size.  The next value would overflow during its sequence.
    constexpr std::array<uint64_t, 65> MAX_INITIAL_VALUE_BY_64BIT = {
        0,  // 0
        1,  // 1
        2,  // 2
        2,  // 3
        2,  // 4
        6,  // 5
        14,  // 6
        14,  // 7
        26,  // 8
        26,  // 9
        26,  // 10
        26,  // 11
        26,  // 12
        26,  // 13
        446,  // 14
        446,  // 15
        702,  // 16
        702,  // 17
        1818,  // 18
        1818,  // 19
        1818,  // 20
        4254,  // 21
        4254,  // 22
        9662,  // 23
        9662,  // 24
        20894,  // 25
        26622,  // 26
        60974,  // 27
        60974,  // 28
        60974,  // 29
        77670,  // 30
        113382,  // 31
        159486,  // 32
        159486,  // 33
        159486,  // 34
        665214,  // 35
        1042430,  // 36
        1212414,  // 37
        2684646,  // 38
        3041126,  // 39
        4637978,  // 40
        5656190,  // 41
        6416622,  // 42
        6631674,  // 43
        6631674,  // 44
        6631674,  // 45
        19638398,  // 46
        19638398,  // 47
        19638398,  // 48
        80049390,  // 49
        80049390,  // 50
        120080894,  // 51
        210964382,  // 52
        319804830,  // 53
        319804830,  // 54
        319804830,  // 55
        319804830,  // 56
        319804830,  // 57
        319804830,  // 58
        319804830,  // 59
        319804830,  // 60
        1410123942,  // 61
        1410123942,  // 62
        8528817510,  // 63
        12327829502,  // 64
    };
    constexpr std::array<uint128_t, 129> MAX_INITIAL_VALUE_BY_128BIT = {
        0,  // 0
        1,  // 1
        2,  // 2
        2,  // 3
        2,  // 4
        6,  // 5
        14,  // 6
        14,  // 7
        26,  // 8
        26,  // 9
        26,  // 10
        26,  // 11
        26,  // 12
        26,  // 13
        446,  // 14
        446,  // 15
        702,  // 16
        702,  // 17
        1818,  // 18
        1818,  // 19
        1818,  // 20
        4254,  // 21
        4254,  // 22
        9662,  // 23
        9662,  // 24
        20894,  // 25
        26622,  // 26
        60974,  // 27
        60974,  // 28
        60974,  // 29
        77670,  // 30
        113382,  // 31
        159486,  // 32
        159486,  // 33
        159486,  // 34
        665214,  // 35
        1042430,  // 36
        1212414,  // 37
        2684646,  // 38
        3041126,  // 39
        4637978,  // 40
        5656190,  // 41
        6416622,  // 42
        6631674,  // 43
        6631674,  // 44
        6631674,  // 45
        19638398,  // 46
        19638398,  // 47
        19638398,  // 48
        80049390,  // 49
        80049390,  // 50
        120080894,  // 51
        210964382,  // 52
        319804830,  // 53
        319804830,  // 54
        319804830,  // 55
        319804830,  // 56
        319804830,  // 57
        319804830,  // 58
        319804830,  // 59
        319804830,  // 60
        1410123942,  // 61
        1410123942,  // 62
        8528817510,  // 63
        12327829502,  // 64
        "23035537406"_u128,  // 65
        "45871962270"_u128,  // 66
        "59152641054"_u128,  // 67
        "70141259774"_u128,  // 68
        "77566362558"_u128,  // 69
        "110243094270"_u128,  // 70
        "272025660542"_u128,  // 71
        "272025660542"_u128,  // 72
        "272025660542"_u128,  // 73
        "272025660542"_u128,  // 74
        "446559217278"_u128,  // 75
        "567839862630"_u128,  // 76
        "871673828442"_u128,  // 77
        "871673828442"_u128,  // 78
        "2674309547646"_u128,  // 79
        "3716509988198"_u128,  // 80
        "3716509988198"_u128,  // 81
        "3716509988198"_u128,  // 82
        "3716509988198"_u128,  // 83
        "3716509988198"_u128,  // 84
        "3716509988198"_u128,  // 85
        "3716509988198"_u128,  // 86
        "3716509988198"_u128,  // 87
        "64848224337146"_u128,  // 88
        "64848224337146"_u128,  // 89
        "64848224337146"_u128,  // 90
        "116050121715710"_u128,  // 91
        "201321227677934"_u128,  // 92
        "394491988532894"_u128,  // 93
        "406738920960666"_u128,  // 94
        "613450176662510"_u128,  // 95
        "1254251874774374"_u128,  // 96
        "1254251874774374"_u128,  // 97
        "1254251874774374"_u128,  // 98
        "1254251874774374"_u128,  // 99
        "1254251874774374"_u128,  // 100
        "1254251874774374"_u128,  // 101
        "8562235014026654"_u128,  // 102
        "8562235014026654"_u128,  // 103
        "8562235014026654"_u128,  // 104
        "10709980568908646"_u128,  // 105
        "10709980568908646"_u128,  // 106
        "10709980568908646"_u128,  // 107
        "10709980568908646"_u128,  // 108

        // The following are placeholders until I can compute them.
        // This is the highest value I've computed without hitting the next 2^k max IV.
        "26453686357881703"_u128,  // 109
        "26453686357881703"_u128,  // 110
        "26453686357881703"_u128,  // 111
        "26453686357881703"_u128,  // 112
        "26453686357881703"_u128,  // 113
        "26453686357881703"_u128,  // 114
        "26453686357881703"_u128,  // 115
        "26453686357881703"_u128,  // 116
        "26453686357881703"_u128,  // 117
        "26453686357881703"_u128,  // 118
        "26453686357881703"_u128,  // 119
        "26453686357881703"_u128,  // 120
        "26453686357881703"_u128,  // 121
        "26453686357881703"_u128,  // 122
        "26453686357881703"_u128,  // 123
        "26453686357881703"_u128,  // 124
        "26453686357881703"_u128,  // 125
        "26453686357881703"_u128,  // 126
        "26453686357881703"_u128,  // 127
        "26453686357881703"_u128,  // 128
    };

    //
    // Max Check
    template<AnySupportedIntegral T>
    inline constexpr size_t get_max_initial_value_max_bits() {
        if constexpr(NativeIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_64BIT.size() - 1;
        } else if constexpr(ExtendedIntegral<T> || GMPIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_128BIT.size() - 1;
        }
    }
    //
    // Lookup
    template<AnySupportedIntegral T>
    inline constexpr T get_max_initial_value_by_bit(size_t bit_size) {
        // Safety Check
        if (bit_size > get_max_initial_value_max_bits<T>()) {
            throw std::out_of_range("Max initial value for bit size " + std::to_string(bit_size) + " not found.");
        }

        // Pick the right type.
        if constexpr(NativeIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_64BIT[bit_size];
        } else if constexpr(ExtendedIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_128BIT[bit_size];
        } else if constexpr(GMPIntegral<T>) {
            return uint128_to_mpz(MAX_INITIAL_VALUE_BY_128BIT[bit_size]);
        }
        throw std::logic_error("Unknown type for bit required.");
    }


    //
    // Lots of powers of 3.  All of them.  Nom nom nom.
    //
    constexpr size_t POW3_64BIT_ELEMENT_COUNT = 40;
    constexpr uint64_t POW3_64BIT[POW3_64BIT_ELEMENT_COUNT] = {
        1ULL
        , 3ULL
        , 9ULL
        , 27ULL
        , 81ULL
        , 243ULL
        , 729ULL
        , 2187ULL
        , 6561ULL
        , 19683ULL
        , 59049ULL
        , 177147ULL
        , 531441ULL
        , 1594323ULL
        , 4782969ULL
        , 14348907ULL
        , 43046721ULL
        , 129140163ULL
        , 387420489ULL
        , 1162261467ULL
        , 3486784401ULL
        , 10460353203ULL
        , 31381059609ULL
        , 94143178827ULL
        , 282429536481ULL
        , 847288609443ULL
        , 2541865828329ULL
        , 7625597484987ULL
        , 22876792454961ULL
        , 68630377364883ULL
        , 205891132094649ULL
        , 617673396283947ULL
        , 1853020188851841ULL
        , 5559060566555523ULL
        , 16677181699666569ULL
        , 50031545098999707ULL
        , 150094635296999121ULL
        , 450283905890997363ULL
        , 1350851717672992089ULL
        , 4052555153018976267ULL
    };
    // 128-bit now.
    constexpr size_t POW3_128BIT_ELEMENT_COUNT = 80;
    constexpr __uint128_t POW3_128BIT[POW3_128BIT_ELEMENT_COUNT] = {
        "1"_u128,
        "3"_u128,
        "9"_u128,
        "27"_u128,
        "81"_u128,
        "243"_u128,
        "729"_u128,
        "2187"_u128,
        "6561"_u128,
        "19683"_u128,
        "59049"_u128,
        "177147"_u128,
        "531441"_u128,
        "1594323"_u128,
        "4782969"_u128,
        "14348907"_u128,
        "43046721"_u128,
        "129140163"_u128,
        "387420489"_u128,
        "1162261467"_u128,
        "3486784401"_u128,
        "10460353203"_u128,
        "31381059609"_u128,
        "94143178827"_u128,
        "282429536481"_u128,
        "847288609443"_u128,
        "2541865828329"_u128,
        "7625597484987"_u128,
        "22876792454961"_u128,
        "68630377364883"_u128,
        "205891132094649"_u128,
        "617673396283947"_u128,
        "1853020188851841"_u128,
        "5559060566555523"_u128,
        "16677181699666569"_u128,
        "50031545098999707"_u128,
        "150094635296999121"_u128,
        "450283905890997363"_u128,
        "1350851717672992089"_u128,
        "4052555153018976267"_u128,
        "12157665459056928801"_u128,
        "36472996377170786403"_u128,
        "109418989131512359209"_u128,
        "328256967394537077627"_u128,
        "984770902183611232881"_u128,
        "2954312706550833698643"_u128,
        "8862938119652501095929"_u128,
        "26588814358957503287787"_u128,
        "79766443076872509863361"_u128,
        "239299329230617529590083"_u128,
        "717897987691852588770249"_u128,
        "2153693963075557766310747"_u128,
        "6461081889226673298932241"_u128,
        "19383245667680019896796723"_u128,
        "58149737003040059690390169"_u128,
        "174449211009120179071170507"_u128,
        "523347633027360537213511521"_u128,
        "1570042899082081611640534563"_u128,
        "4710128697246244834921603689"_u128,
        "14130386091738734504764811067"_u128,
        "42391158275216203514294433201"_u128,
        "127173474825648610542883299603"_u128,
        "381520424476945831628649898809"_u128,
        "1144561273430837494885949696427"_u128,
        "3433683820292512484657849089281"_u128,
        "10301051460877537453973547267843"_u128,
        "30903154382632612361920641803529"_u128,
        "92709463147897837085761925410587"_u128,
        "278128389443693511257285776231761"_u128,
        "834385168331080533771857328695283"_u128,
        "2503155504993241601315571986085849"_u128,
        "7509466514979724803946715958257547"_u128,
        "22528399544939174411840147874772641"_u128,
        "67585198634817523235520443624317923"_u128,
        "202755595904452569706561330872953769"_u128,
        "608266787713357709119683992618861307"_u128,
        "1824800363140073127359051977856583921"_u128,
        "5474401089420219382077155933569751763"_u128,
        "16423203268260658146231467800709255289"_u128,
        "49269609804781974438694403402127765867"_u128
    };
    // MPZ now.
    constexpr size_t POW3_MPZ_ELEMENT_COUNT = 80;
    static mpz_class POW3_MPZ[POW3_MPZ_ELEMENT_COUNT] = {
        "1"_mpz,
        "3"_mpz,
        "9"_mpz,
        "27"_mpz,
        "81"_mpz,
        "243"_mpz,
        "729"_mpz,
        "2187"_mpz,
        "6561"_mpz,
        "19683"_mpz,
        "59049"_mpz,
        "177147"_mpz,
        "531441"_mpz,
        "1594323"_mpz,
        "4782969"_mpz,
        "14348907"_mpz,
        "43046721"_mpz,
        "129140163"_mpz,
        "387420489"_mpz,
        "1162261467"_mpz,
        "3486784401"_mpz,
        "10460353203"_mpz,
        "31381059609"_mpz,
        "94143178827"_mpz,
        "282429536481"_mpz,
        "847288609443"_mpz,
        "2541865828329"_mpz,
        "7625597484987"_mpz,
        "22876792454961"_mpz,
        "68630377364883"_mpz,
        "205891132094649"_mpz,
        "617673396283947"_mpz,
        "1853020188851841"_mpz,
        "5559060566555523"_mpz,
        "16677181699666569"_mpz,
        "50031545098999707"_mpz,
        "150094635296999121"_mpz,
        "450283905890997363"_mpz,
        "1350851717672992089"_mpz,
        "4052555153018976267"_mpz,
        "12157665459056928801"_mpz,
        "36472996377170786403"_mpz,
        "109418989131512359209"_mpz,
        "328256967394537077627"_mpz,
        "984770902183611232881"_mpz,
        "2954312706550833698643"_mpz,
        "8862938119652501095929"_mpz,
        "26588814358957503287787"_mpz,
        "79766443076872509863361"_mpz,
        "239299329230617529590083"_mpz,
        "717897987691852588770249"_mpz,
        "2153693963075557766310747"_mpz,
        "6461081889226673298932241"_mpz,
        "19383245667680019896796723"_mpz,
        "58149737003040059690390169"_mpz,
        "174449211009120179071170507"_mpz,
        "523347633027360537213511521"_mpz,
        "1570042899082081611640534563"_mpz,
        "4710128697246244834921603689"_mpz,
        "14130386091738734504764811067"_mpz,
        "42391158275216203514294433201"_mpz,
        "127173474825648610542883299603"_mpz,
        "381520424476945831628649898809"_mpz,
        "1144561273430837494885949696427"_mpz,
        "3433683820292512484657849089281"_mpz,
        "10301051460877537453973547267843"_mpz,
        "30903154382632612361920641803529"_mpz,
        "92709463147897837085761925410587"_mpz,
        "278128389443693511257285776231761"_mpz,
        "834385168331080533771857328695283"_mpz,
        "2503155504993241601315571986085849"_mpz,
        "7509466514979724803946715958257547"_mpz,
        "22528399544939174411840147874772641"_mpz,
        "67585198634817523235520443624317923"_mpz,
        "202755595904452569706561330872953769"_mpz,
        "608266787713357709119683992618861307"_mpz,
        "1824800363140073127359051977856583921"_mpz,
        "5474401089420219382077155933569751763"_mpz,
        "16423203268260658146231467800709255289"_mpz,
        "49269609804781974438694403402127765867"_mpz
    };
}




//
// Collatz Metadata
//
// Metadata bloats the Collatz class, so it's offloaded here and disabled by default.  Collatz() retains a pointer to
// a CollatzMetadata object, keeping its size fixed regardless of how much metadata we create in the future.  The logic
// is: if a caller wants the full metadata, they're already willing to pay the cost of having metadata, so an extra 8
// bytes in a pointer and a 8-16 bytes for the CollatzMetadata object (alloc overhead) doesn't matter as much.
//
// This design decision favors callers who don't want Metadata: they get reduced memory footprint at the expense of
// callers who want metadata needing pointer dereferencing.
//
// Summary of the time-memory trade-off:
//   time  -> callers who don't want CollatzMetadata will calculated it each time a getter for metadata is called.
//   memory-> callers who do want CollatzMetadata have it on-demand, computed when Collatz().init() is called.
//
template<AnySupportedIntegral T>
class CollatzMetadata {
    private:
    // None, just make them public.



    public:
    // Set to zero!  GMP types are not default initialized to 0.
    T peak_value = 0;
    seq_size_t hwm_index = 0;
    seq_size_t step_count = 0;
    CollatzMetadata() {}



    //
    // Reset
    // Default-initialize manually.
    //
    void reset() {
        peak_value = 0;
        hwm_index = 0;
        step_count = 0;
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);
        if constexpr(GMPIntegral<T>) {
            total += gmp_deep_sizeof(peak_value);
        }
        return total;
    }
};




//
// Collatz
//
// Basic Collatz sequence object with minimal data to keep it tight.
// see: CollatzMetadata for details about time-memory trade-offs with _enable_metadata.
//
template<AnySupportedIntegral T>
class Collatz {
    private:
    // Memory packing and alignment matter!  Keep this class LIGHT unless the caller wants metadata.
    // All data must fit within one cache line.
    //                                           uint64_t | total | uint128_t | total | mpz_class | total
    T _initial_value;                         //        8 |     8 |        16 |    16 |        16 |    16
    bool _is_initialized : 1;                 //      1:1 |     9 |       1:1 |    17 |       1:1 |    17
    bool _track_sequence : 1;                 //      1:2 |     9 |       1:2 |    17 |       1:2 |    17
    bool _track_metadata : 1;                 //      1:3 |     9 |       1:3 |    17 |       1:3 |    17
    bool _sequence_overflow : 1;              //      1:4 |     9 |       1:4 |    17 |       1:4 |    17  (4 bits padding)
    // Alignment Padding                      //        7 |    16 |         7 |    24 |         7 |    24
    std::vector<T> _sequence;                 //       24 |    40 |        24 |    48 |        24 |    48
    CollatzMetadata<T>* _metadata = nullptr;  //        8 |    48 |         8 |    56 |         8 |    56
    // Struct Alignment Padding (u128 only)   //        0 |    48 |         8 |    64 |         0 |    56
    // Free Padding to Cacheline              //       16 |    64 |         0 |    64 |         8 |    64
    // -- Cache Line --



    public:
    // Reusable messages.
    static inline std::string E_NO_SEQUENCE_TRACKING = "You disabled sequence tracking when you created this object.";
    static inline std::string E_NO_METADATA_TRACKING = "You disabled metadata when you created this object.";


    //
    // Constructors.  Offload to init() so objects can be reused.
    //
    Collatz() {}
    Collatz(const T& initial_value, bool track_sequence = false, bool track_metadata = false) {
        init(initial_value, track_sequence, track_metadata);
    };



    //
    // Destructor
    //
    ~Collatz() {
        release_metadata();
    }



    //
    // Cout Friend
    // Treat the initial value as the <<() output.
    //
    friend std::ostream& operator<<(std::ostream &os, const Collatz<T>& m) {
        return os << m._initial_value;
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(const T& initial_value, bool track_sequence = false, bool track_metadata = false) {
        if (initial_value < 0) {
            throw std::runtime_error("You cannot create a Collatz sequence with a value lower than 0.");
        }

        // Reset if necessary.
        if(_is_initialized) { reset(); }

        // Establish or clear metadata object.  Reset() already cleared it, if it existed.
        if (_metadata == nullptr && track_metadata) { _metadata = new CollatzMetadata<T>(); }
        if (_metadata != nullptr && ! track_metadata) { release_metadata(); }
        _track_sequence = track_sequence;
        _track_metadata = track_metadata;
        _is_initialized = true;
        _initial_value = initial_value;

        // Process the sequence and store any related metadata, if needed.  Otherwise, leave.
        if (_track_sequence || _track_metadata) {
            try {
                for_each_sequence_step([&](const T& step) {
                    if (_track_sequence) { _sequence.push_back(step); }
                    if (_track_metadata) {
                        _metadata->step_count++;
                        if (step > _metadata->peak_value) {
                            _metadata->peak_value = step;
                        }
                        if (_metadata->hwm_index == 0 && step < _initial_value) {
                            _metadata->hwm_index = _metadata->step_count - 1;
                        }
                    }
                    return false;
                });
                // Decrement 1 to account for start position
                if (_track_metadata && _metadata->step_count > 0) {
                    _metadata->step_count--;
                }
            } catch(const CollatzSequenceOverflow& err) {
                _sequence_overflow = true;
                throw(err);
            }
            if (_track_sequence) {
                _sequence.shrink_to_fit();
            }
        }
    }



    //
    // Reset Object
    // Reset members to make this act like a new() object.
    //
    void reset() {
        _sequence.clear();
        _sequence.shrink_to_fit();
        _sequence_overflow = false;
        _track_sequence = false;
        _track_metadata = false;
        if (_metadata != nullptr) { _metadata->reset(); }
    }



    //
    // Release Metadata
    // Let callers decide when they're done with metadata.
    //
    void release_metadata() {
        if (_metadata == nullptr) { return; }
        delete _metadata;
        _metadata = nullptr;
        _track_metadata = false;
    }



    //
    // Getters
    //
    const T& get_initial_value() const { return _initial_value; }
    bool get_is_overflowed() const { return _sequence_overflow; }
    bool get_is_initialized() const { return _is_initialized; }
    bool get_track_sequence() const { return _track_sequence; }
    bool get_track_metadata() const { return _track_metadata; }
    const CollatzMetadata<T>* get_metadata() const { return _metadata; }
    //
    // Sequence and metadata accessors.
    const std::vector<T>& get_sequence() const {
        if(!_track_sequence) {
            throw std::logic_error(E_NO_SEQUENCE_TRACKING);
        }
        return _sequence;
    };
    const T& get_peak_value() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->peak_value;
    }
    size_t get_hwm_index() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return static_cast<size_t>(_metadata->hwm_index);
    }
    size_t get_step_count() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return static_cast<size_t>(_metadata->step_count);
    }



    //
    // Sequence String
    // Build a string version of all steps in a single, comma-separated string.
    //
    std::string get_sequence_string() {
        std::string rv;
        rv.append(to_string_any(_initial_value));

        for(size_t i=1; i<_sequence.size(); i++) {
            rv.append(", ");
            rv.append(to_string_any(_sequence[i]));
        }

        return rv;
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);

        if constexpr(BuiltinIntegral<T>) {
            total += sizeof(T) * _sequence.capacity();
        } else if constexpr(GMPIntegral<T>) {
            total += sizeof(mpz_class) * _sequence.capacity();
            for (const auto& val : _sequence) {
                total += gmp_deep_sizeof(val);
            }
        }

        if (_metadata != nullptr) {
            total += _metadata->deep_size();
        }

        return total;
    }



    //
    // For-Each Step
    // Run through the sequence with a callback each step.  Caller MUST return true or false to continue or stop.
    //
    template<typename Func>
    static void for_each_sequence_step(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_same_v<typename first_arg_type<Func>::type, const T&>, "Callback must be callable with a const T&");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        thread_local T current_step;
        current_step = initial_value;
        if constexpr(BuiltinIntegral<T>) {
            // Fixed integrals can use intrinsic arithmetic operators for "free", but can overflow.
            while (current_step >= 1) {
                bool stop = callback(current_step);
                if (stop || current_step == 1) { return; }
                if (current_step % 2 == 0) {
                    current_step /= 2;
                } else {
                    if (current_step > CollatzConstants::get_max_3xp1<T>()) {
                        throw CollatzSequenceOverflow("Overflow when building for_each_sequence_step().");
                    }
                    current_step *= 3;
                    current_step += 1;
                }
            }
        } else if constexpr(GMPIntegral<T>) {
            // GMP integers will alloc() with certain arithmetic operators, but can't overflow.
            while (current_step >= CollatzConstants::MPZ_ONE) {
                bool stop = callback(current_step);
                if (stop || current_step == 1) { return; }
                if (mpz_even_p(current_step.get_mpz_t())) {
                    mpz_tdiv_q_2exp(current_step.get_mpz_t(), current_step.get_mpz_t(), 1);  // current_step >> 1   ==>  current_step /= 2
                } else {
                    mpz_mul(current_step.get_mpz_t(), current_step.get_mpz_t(), CollatzConstants::MPZ_THREE.get_mpz_t());  // current_step *= 3
                    mpz_add(current_step.get_mpz_t(), current_step.get_mpz_t(), CollatzConstants::MPZ_ONE.get_mpz_t());    // current_step += 1
                }
            }
        }
    }
    //
    // Wrapper for the instance implementation.
    template<typename Func>
    void for_each_sequence_step(Func&& callback) const {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_same_v<typename first_arg_type<Func>::type, const T&>, "Callback must be callable with a const T&");

        // Sequence exists?  Use it directly.
        if (_sequence.size() > 0) {
            for (const T& current_step : _sequence) {
                bool stop = callback(current_step);
                if (stop) { return; }
            }
            return;
        }

        // Sequence didn't exist.  Calculate it on-the-fly via the static method.
        Collatz<T>::for_each_sequence_step(_initial_value, std::forward<Func>(callback));
    }



    //
    // Step Count
    // Returns the step count for any initial value.  Recall it's sequence size - 1.
    //
    static size_t st_get_step_count(const T& initial_value) {
        size_t steps = 0;
        for_each_sequence_step(initial_value, [&](const T& step) {
            steps++;
            return step < 0;  // We need to return false anyway, might as well get rid of a compiler warning.
        });
        return steps - 1;
    }
    //
    //
    // Step Count (Fast Variant)
    // Skips the for_each_sequence_step() iterator to minimize overhead.  See collatz_compression.cpp for details.
    //
    static size_t st_get_step_count_fast(const T& initial_value) {
        size_t right_shifts = 0;
        size_t steps = 0;

        if constexpr(BuiltinIntegral<T>) {
            // Native types are fast as-is.  Affine compression doesn't help, except bit-shifting CTZ.
            T tmp = initial_value;
            // Check for overflow here, once, instead of over and over.
            if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(std::numeric_limits<T>::digits)) {
                throw CollatzSequenceOverflow("Overflow when building st_get_step_count_fast().");
            }
            while (tmp > 1) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    tmp = (tmp << 1) + tmp + 1;
                    steps++;
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = count_trailing_zeros(tmp);
                tmp >>= right_shifts;
                steps += right_shifts;
            }
        } else if constexpr(GMPIntegral<T>) {
            // GMP types are significantly faster using an Affine map for CTZ and CTO, ergo it's worth the overhead.
            static thread_local T tmp;
            tmp = initial_value;
            size_t trailing_ones = 0;
            constexpr size_t limit = CollatzConstants::POW3_MPZ_ELEMENT_COUNT - 1;
            // See collatz_compression.cpp tests for details on how this works and why.
            while (tmp > 1) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    trailing_ones = count_trailing_ones(tmp);
                    steps += (2 * trailing_ones);
                    while (trailing_ones > limit) {
                        tmp = ((CollatzConstants::POW3_MPZ[limit] * (tmp + 1)) >> limit) - 1;
                        trailing_ones -= limit;
                    }
                    if (trailing_ones > 0) {
                        tmp = ((CollatzConstants::POW3_MPZ[trailing_ones] * (tmp + 1)) >> trailing_ones) - 1;
                    }
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = count_trailing_zeros(tmp);
                tmp >>= right_shifts;
                steps += right_shifts;
            }
        } else {
            throw std::runtime_error("Cannot determine data type for st_get_step_count_fast().");
        }
        return steps;
    }


    //
    // Get Peak
    // Finds the peak of a sequence.  Skips the for_each_sequence_step() iterator to minimize overhead.  See
    // collatz_compression.cpp for details.
    //
    // You may optionally stop at high-water mark (mainly for peak_by_bit program).
    //
    static inline void st_get_peak_fast(const T& initial_value, T& out_peak, bool stop_at_hwm = false) {
        size_t right_shifts = 0;
        out_peak = initial_value;

        if constexpr(BuiltinIntegral<T>) {
            // Native types are fast as-is.  Affine compression doesn't help, except bit-shifting CTZ.
            T tmp = initial_value;
            T bailout_value = (stop_at_hwm && initial_value > 1) ? (T)initial_value - 1 : T(1);
            // Check for overflow here, once, instead of over and over.
            // if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(std::numeric_limits<T>::digits)) {
            //     throw CollatzSequenceOverflow("Overflow when building st_get_step_count_fast().");
            // }
            while (tmp > bailout_value) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    if (tmp > CollatzConstants::get_max_3xp1<T>()) {
                        throw std::out_of_range("Cannot process initial_value " + to_string_any(initial_value) + " any further in st_get_peak_fast.");
                    }
                    tmp = (tmp << 1) + tmp + 1;
                    if (tmp > out_peak) {
                        out_peak = tmp;
                    }
                }
                // Always even at this point.  Shift zeros out.  Can't affect peak.
                right_shifts = count_trailing_zeros(tmp);
                tmp >>= right_shifts;
            }
        } else if constexpr(GMPIntegral<T>) {
            // GMP types are significantly faster using an Affine map for CTZ and CTO, ergo it's worth the overhead.
            static thread_local T tmp;
            tmp = initial_value;
            static thread_local T tmp_x_2;
            static thread_local T bailout_value;
            bailout_value = 1;
            if (stop_at_hwm && initial_value > 1) {
                bailout_value = initial_value;
                mpz_sub_ui(bailout_value.get_mpz_t(), bailout_value.get_mpz_t(), 1);
            }
            size_t trailing_ones = 0;
            constexpr size_t limit = CollatzConstants::POW3_MPZ_ELEMENT_COUNT - 1;
            // See collatz_compression.cpp tests for details on how this works and why.
            while (tmp > bailout_value) {
                // Handle odd.
                if (mpz_odd_p(tmp.get_mpz_t())) {
                    trailing_ones = count_trailing_ones(tmp);
                    while (trailing_ones > limit) {
                        // tmp = ((CollatzConstants::POW3_MPZ[limit] * (tmp + 1)) >> limit) - 1;
                        mpz_add_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        mpz_mul(tmp.get_mpz_t(), tmp.get_mpz_t(), CollatzConstants::POW3_MPZ[limit].get_mpz_t());
                        mpz_tdiv_q_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), limit);
                        mpz_sub_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        trailing_ones -= limit;
                    }
                    if (trailing_ones > 0) {
                        // tmp = ((CollatzConstants::POW3_MPZ[trailing_ones] * (tmp + 1)) >> trailing_ones) - 1;
                        mpz_add_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        mpz_mul(tmp.get_mpz_t(), tmp.get_mpz_t(), CollatzConstants::POW3_MPZ[trailing_ones].get_mpz_t());
                        mpz_tdiv_q_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), trailing_ones);
                        mpz_sub_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                    }
                    // Now check peak.  However!  We applied an accelerated F(x):  (3x + 1) / 2
                    // Peak was actually tmp * 2.
                    mpz_mul_2exp(tmp_x_2.get_mpz_t(), tmp.get_mpz_t(), 1);
                    if (mpz_cmp(tmp_x_2.get_mpz_t(), out_peak.get_mpz_t()) > 0) {
                        out_peak = tmp_x_2;
                    }
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = count_trailing_zeros(tmp);
                tmp >>= right_shifts;
            }
        } else {
            throw std::runtime_error("Cannot determine data type for st_get_step_count_fast().");
        }
    }



    //
    // Get FG String
    // Generate the F-G string for a sequence with some initial value.
    //
    static std::string st_get_fg_pattern_string(const T& initial_value, size_t max_chars = std::numeric_limits<size_t>::max()) {
        std::string result;
        size_t count = 0;
        bool skip = false;
        Collatz<T>::for_each_sequence_step(initial_value, [&](const T& step) {
            // When the previous was 'F' (odd), we need to skip the next, because F is a full Odd-Even pair.
            if (skip) {
                skip = false;
            } else {
                count++;
                if constexpr(BuiltinIntegral<T>) {
                    result += (step % 2 == 0 ? 'G' : 'F');
                } else if constexpr(GMPIntegral<T>) {
                    result += (mpz_even_p(step.get_mpz_t()) ? 'G' : 'F');
                }
                skip = result.back() == 'F';
            }
            return count >= max_chars;
        });
        return result;
    }
    //
    // Wrapper for the instance implementation.
    std::string get_fg_pattern_string(size_t max_chars = std::numeric_limits<size_t>::max()) const {
        return Collatz<T>::st_get_fg_pattern_string(_initial_value, max_chars);
    }



    //
    // Get OE String
    // Generate the odd-even string for the sequence, which is just an expansion of the F-G string.
    //
    static std::string st_get_oe_pattern_string(const T& initial_value, size_t max_chars = std::numeric_limits<size_t>::max()) {
        std::string fg_pattern = Collatz<T>::st_get_fg_pattern_string(initial_value, max_chars);
        return fg_to_oe(fg_pattern, max_chars);
    }
    //
    // Wrapper for the instance implementation.
    std::string get_oe_pattern_string(size_t max_chars = std::numeric_limits<size_t>::max()) {
        return Collatz<T>::st_get_oe_pattern_string(_initial_value, max_chars);
    }



    //
    // FG to OE
    // Convert an FG string to an OE string.
    //
    static std::string fg_to_oe(
        const std::string& fg_pattern
        , size_t max_oe_chars = std::numeric_limits<size_t>::max()
        , bool strip_last_e = true
    ) {
        std::string oe_string;
        for (const char& c : fg_pattern) {
            if (c == 'F') {
                oe_string += "OE";
            } else {
                oe_string += "E";
            }
            if (oe_string.size() > max_oe_chars) {
                break;
            }
        }

        // If we didn't hit the max_chars, remove the last E.  Why?  Because all sequences end in 1 (odd) and therefore get an "F" -> "OE"
        // But that last "E" is 1 going to 4 (1 * 3 + 1).
        // If the caller forbids this, skip it.
        if (strip_last_e && oe_string.size() < max_oe_chars && oe_string.size() > 0) {
            oe_string.pop_back();
        }

        // If we're over the max_chars requested, trim.
        if (oe_string.size() > max_oe_chars) {
            oe_string.resize(max_oe_chars);
        }

        return oe_string;
    }
};
