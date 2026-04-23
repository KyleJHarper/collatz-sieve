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
class zstd_istreambuf : public std::streambuf {
    private:
    std::streambuf* _src;
    ZSTD_DStream* _zstd_dstream;
    std::vector<char> _input_buffer;
    std::vector<char> _output_buffer;
    size_t _input_position = 0;
    size_t _input_size = 0;



    //
    // Check
    // Wrapper for ZSTD_isError(), which checks an error code and throws the error name if found.
    //
    static void check(size_t code) {
        if (ZSTD_isError(code))
            throw std::runtime_error(ZSTD_getErrorName(code));
    }



    protected:
    //
    // Underflow
    // Override the underflow detection.
    //
    int underflow() override {
        // Check get area.
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        // Make the output buffer using our members.
        ZSTD_outBuffer out{_output_buffer.data(), _output_buffer.size(), 0};

        // Process data as long as we can.
        while (out.pos == 0) {
            // Check if we're at the end of our input.
            if (_input_position == _input_size) {
                _input_size = _src->sgetn(_input_buffer.data(), _input_buffer.size());
                _input_position = 0;
                // Definitely done.  Return.
                if (_input_size == 0) {
                    return EOF;
                }
            }

            // Setup the Zstd input buffer to read out members.
            ZSTD_inBuffer in{_input_buffer.data(), _input_size, _input_position};

            // Decompress and check the output.
            size_t ret = ZSTD_decompressStream(_zstd_dstream, &out, &in);
            check(ret);
            _input_position = in.pos;

            // If code is zero and pos is zero, we're done with a frame, but might have more.  Check.
            if (ret == 0 && out.pos == 0) {
                // See if we can refill in put before delaring EOF.
                if (_input_position == _input_size) {
                    _input_size = _src->sgetn(_input_buffer.data(), _input_buffer.size());
                    _input_position = 0;
                    if (_input_size == 0) {
                        // Okay, we're really-really done.  We can quit.
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
    // Disallow copying
    zstd_istreambuf(const zstd_istreambuf&) = delete;
    zstd_istreambuf& operator=(const zstd_istreambuf&) = delete;



    //
    // Constructor
    //
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



    //
    // Destructor
    // Just frees the resource.
    //
    ~zstd_istreambuf() override {
        ZSTD_freeDStream(_zstd_dstream);
    }
};




//
// Zstd IStream
// Simple helper to take an istream (e.g. a file istream) and connect it to a zstd_istreambuf so it can be passed around as if it
// were a normal istream.
//
class zstd_istream : public std::istream {
    private:
    zstd_istreambuf _buffer;



    public:
    // Disallow copying
    zstd_istream(const zstd_istream&) = delete;
    zstd_istream& operator=(const zstd_istream&) = delete;



    //
    // Constructor
    //
    zstd_istream(std::istream& underlying) : std::istream(nullptr), _buffer(underlying.rdbuf()) {
        rdbuf(&_buffer);
    }
};
