#pragma once
#include <zstd.h>
#include <streambuf>
#include <istream>
#include <vector>
#include <stdexcept>


//
// A set of classes to help with decompressing data using Zstd
//

//
// ZSTD IStreamBuf
// A subclass of a streambuf which will try to attempt to leverage the C-style ZSTD bits to create a decompressed stream.
//
/**
* @class zstd_istreambuf
* @brief A `streambuf` subclass which wraps the C-style ZSTD bits to decompress a stream.
*/
class zstd_istreambuf : public std::streambuf {
    private:
    /// @brief The source stream to read compressed data from.
    std::streambuf* _src;
    /// @brief The ZSTD decompression object.
    ZSTD_DStream* _zstd_dstream;
    /// @brief Basic input buffer to feed to ZSTD.
    std::vector<char> _input_buffer;
    /// @brief Basic output buffer to write decompressed data to.
    std::vector<char> _output_buffer;
    /// @brief Tracker for input position while reading.
    size_t _input_position = 0;
    /// @brief Sentinel value for input while reading.
    size_t _input_size = 0;



    /**
    * @brief Wrapper for ZSTD_isError(), which checks an error code and throws the error name if found.
    * @param code The code to check with `ZSTD_getErrorName`.
    */
    static void check(size_t code) {
        if (ZSTD_isError(code)) {
            throw std::runtime_error(ZSTD_getErrorName(code));
        }
    }



    protected:
    /**
    * @brief Underflow check which will read in as much data as possible and decompress it.
    * @return Integer indicating underflow/EOF status.
    */
    int underflow() override {
        // Check get area.
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        // Make the output buffer using private members.
        ZSTD_outBuffer out{_output_buffer.data(), _output_buffer.size(), 0};

        // Process data as long as possible.
        while (out.pos == 0) {
            // Check if it's at the end of the input.
            if (_input_position == _input_size) {
                _input_size = _src->sgetn(_input_buffer.data(), _input_buffer.size());
                _input_position = 0;
                // Definitely done.  Return.
                if (_input_size == 0) {
                    return EOF;
                }
            }

            // Setup the Zstd input buffer to read input members.
            ZSTD_inBuffer in{_input_buffer.data(), _input_size, _input_position};

            // Decompress and check the output.
            size_t ret = ZSTD_decompressStream(_zstd_dstream, &out, &in);
            check(ret);
            _input_position = in.pos;

            // If code is zero and pos is zero, it's done with a frame, but might have more.  Check.
            if (ret == 0 && out.pos == 0) {
                // See if it can refill input before delaring EOF.
                if (_input_position == _input_size) {
                    _input_size = _src->sgetn(_input_buffer.data(), _input_buffer.size());
                    _input_position = 0;
                    if (_input_size == 0) {
                        // Okay, it's really-really done.  Quit.
                        return EOF;
                    }
                    // There was more work to do.  Continue to process more.
                    continue;
                }
            }
        }

        // Set the get area and positions.
        setg(_output_buffer.data(), _output_buffer.data(), _output_buffer.data() + out.pos);
        return traits_type::to_int_type(*gptr());
    }



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Disallow copying and moving.
    zstd_istreambuf(const zstd_istreambuf&) = delete;



    /// @brief Disallow copying and moving.
    zstd_istreambuf& operator=(const zstd_istreambuf&) = delete;



    /// @brief Constructor connects caller's `src` to read into the ZSTD decompressor.
    zstd_istreambuf(std::streambuf* src) {
        _src = src;
        _input_buffer.resize(ZSTD_DStreamInSize());
        _output_buffer.resize(ZSTD_DStreamOutSize());
        _zstd_dstream = ZSTD_createDStream();
        if (!_zstd_dstream) {
            throw std::runtime_error("ZSTD_createDStream failed");
        }
        check(ZSTD_initDStream(_zstd_dstream));
        setg(_output_buffer.data(), _output_buffer.data(), _output_buffer.data());
    }



    /// @brief Destructor simply needs to free() up the ZSTD stream.
    ~zstd_istreambuf() override {
        ZSTD_freeDStream(_zstd_dstream);
    }

    /// @}
};




/**
* @class zstd_istream
* @brief Creates an istream subclass and connects it to a `zstd_istreambuf` so it may be passed around like a normal istream.
*
* This class largely just wraps the `zstd_istreambuf` to make reading ZSTD data easier.
*/
class zstd_istream : public std::istream {
    private:
    /// @brief The actual streambuf to send data in/out of, connected to this objects `rdbuf`.
    zstd_istreambuf _buffer;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Disallow copying and moving.
    zstd_istream(const zstd_istream&) = delete;



    /// @brief Disallow copying and moving.
    zstd_istream& operator=(const zstd_istream&) = delete;



    /**
    * @brief Constructor takes an underlying istream to connect with a `zstd_istreambuf`.
    * @param underlying The source istream where ZSTD should read compressed data from.
    */
    zstd_istream(std::istream& underlying) : std::istream(nullptr), _buffer(underlying.rdbuf()) {
        rdbuf(&_buffer);
    }
};
