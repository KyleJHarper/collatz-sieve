#pragma once
#include <zstd.h>
#include <streambuf>
#include <vector>
#include <stdexcept>
#include <ostream>
#include <iostream>
#include <thread>


//
// A set of classes to help with compressing data using Zstd
//



//
// ZSTD OStreamBuf
// A subclass of a streambuf which will try to attempt to leverage the C-style ZSTD bits to create a compressed stream.
//
class zstd_ostreambuf : public std::streambuf {
    private:
    std::streambuf* _dest;
    ZSTD_CStream* _zstd_cstream;
    std::vector<char> _input_buffer;
    std::vector<char> _output_buffer;
    bool _finalized = false;
    ZSTD_EndDirective _zstd_end_directive = ZSTD_e_continue;



    //
    // Flush Input
    // Reads all pending input and asks Zstd to compress it to the CStream.
    //
    int flush_input() {
        // Calculate how many bytes we need to read and build an in-buffer locally for it.
        size_t size = pptr() - pbase();
        ZSTD_inBuffer in{pbase(), size, 0};

        // Loop through the in-buffer until it's exhausted.
        while (in.pos < in.size) {
            // Setup an out-buffer to write to our private member.
            ZSTD_outBuffer out{_output_buffer.data(), _output_buffer.size(), 0};
            // Run the data through stream and store the compressed bits.
            check(ZSTD_compressStream2(_zstd_cstream, &out, &in, _zstd_end_directive));
            // Put the buffered (compressed) data into the destination streambuf.
            if (_dest->sputn(_output_buffer.data(), out.pos) != std::streamsize(out.pos)) {
                return -1;
            }
        }

        // Reset the put area now that we're done.
        setp(_input_buffer.data(), _input_buffer.data() + _input_buffer.size());
        return 0;
    }



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
    // Overflow
    // Overrides the overflow method and calls flush_input().
    //
    int overflow(int ch) override {
        if (_finalized) { return EOF; }

        if (pptr() == epptr()) {
            if (flush_input() == -1) {
                return EOF;
            }
        }

        if (ch != EOF) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }

        return traits_type::not_eof(ch);
    }



    //
    // Sync
    // Overrides the sync method which in our case is just flush_input().
    int sync() override {
        if (_finalized) { return 0; }
        return flush_input();
    }



    public:
    // Disallow copying
    zstd_ostreambuf(const zstd_ostreambuf&) = delete;
    zstd_ostreambuf& operator=(const zstd_ostreambuf&) = delete;



    //
    // Constructor
    // Only accepts a destination stream buffer and a compression level.
    //
    zstd_ostreambuf(std::streambuf* dest, int level = ZSTD_CLEVEL_DEFAULT) {
        // Setup the stream buffers and destination.  Give the input buffer a big size to aid with threaded compression.
        _dest = dest;
        _input_buffer.resize(1 << 24);  // 16MB
        _output_buffer.resize(ZSTD_CStreamOutSize());
        _zstd_cstream = ZSTD_createCCtx();
        if (!_zstd_cstream) {
            throw std::runtime_error("ZSTD_createCCtx failed");
        }

        // Initialize the compression stream.  Check for error codes.
        int thread_count = std::thread::hardware_concurrency();
        if (thread_count < 1) {
            thread_count = 1;
        }
        thread_count = std::min(16, thread_count);
        check(ZSTD_CCtx_setParameter(_zstd_cstream, ZSTD_c_compressionLevel, level));
        check(ZSTD_CCtx_setParameter(_zstd_cstream, ZSTD_c_nbWorkers, thread_count));
        // When compression is > 19, long range scanning is required.  We also change the end diretive to flush immediately so we
        // can engage more CPU threads.  The trade-off is around 10% worse compression at the top end (level = 22).
        if (level > 19) {
            check(ZSTD_CCtx_setParameter(_zstd_cstream, ZSTD_c_enableLongDistanceMatching, 1));
            _zstd_end_directive = ZSTD_e_flush;
        }
        check(ZSTD_CCtx_reset(_zstd_cstream, ZSTD_reset_session_only));

        // Set the write points.
        setp(_input_buffer.data(), _input_buffer.data() + _input_buffer.size());
    }



    //
    // Destructor
    // Required to flush any remaining data and free() the Zstd memory bits.
    //
    ~zstd_ostreambuf() override {
        // Free up Zstd allocated resources.
        ZSTD_freeCStream(_zstd_cstream);
    }



    //
    // Finalize
    // Zstd requires us to (obviously) flush any remaining data to it for work, but also check ZSTD_e_end when in MT mode until it
    // reports 0.  There's some frame-work happening (checksum maybe?) that I didn't fully dig into, but this method is mandatory
    // before calling any destruction, implicit or otherwise.
    //
    void finalize() {
        if (_finalized) { return; }

        // Ensure any remaining data in the put area is sent to Zstd.
        if (pptr() > pbase()) {
            if (flush_input() == -1) {
                throw std::runtime_error("Final flush_input (from inside finalize()) failed");
            }
        }

        // Loop ZSTD_e_end until the frame is fully flushed (returns 0).
        ZSTD_inBuffer input = { nullptr, 0, 0 };
        size_t finished;
        do {
            ZSTD_outBuffer output = { _output_buffer.data(), _output_buffer.size(), 0 };
            finished = ZSTD_compressStream2(_zstd_cstream, &output, &input, ZSTD_e_end);
            if (ZSTD_isError(finished)) {
                throw std::runtime_error(ZSTD_getErrorName(finished));
            }
            if (output.pos > 0) {
                if (_dest->sputn(_output_buffer.data(), output.pos) != std::streamsize(output.pos)) {
                    throw std::runtime_error("Write failure during finalize()");
                }
            }
        } while (finished > 0);

        // Do not allow this or any other stream-touching methods to operate again.
        _finalized = true;
    }
};




//
// Zstd OStream
// Simple helper to take an ostream (e.g. a file ostream) and connect it to a zstd_ostreambuf so it can be passed around as if it
// were a normal ostream.
//
class zstd_ostream : public std::ostream {
    private:
    zstd_ostreambuf _buffer;



    public:
    // Disallow copying
    zstd_ostream(const zstd_ostream&) = delete;
    zstd_ostream& operator=(const zstd_ostream&) = delete;



    //
    // Constructor
    //
    zstd_ostream(std::ostream& underlying, int level) : std::ostream(nullptr), _buffer(underlying.rdbuf(), level) {
        rdbuf(&_buffer);
    }



    //
    // Expose finalize for the streambuf.
    //
    void finalize() {
        _buffer.finalize();
    }
};
