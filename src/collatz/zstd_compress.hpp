#pragma once
#include <zstd.h>
#include <streambuf>
#include <vector>
#include <stdexcept>
#include <ostream>
#include <iostream>
#include <thread>




/**
* @class zstd_ostreambuf
* @brief A `streambuf` subclass which wraps the C-style ZSTD bits to create a compressed stream.
*/
class zstd_ostreambuf : public std::streambuf {
    private:
    /// @brief The destination streambuf for compressed data from the `ZSTD_CStream`.
    std::streambuf* _dest;
    /// @brief The ZSTD compressor stream to send input to.
    ZSTD_CStream* _zstd_cstream;
    /// @brief Basic input buffer to feed to ZSTD.
    std::vector<char> _input_buffer;
    /// @brief Basic output buffer for ZSTD.
    std::vector<char> _output_buffer;
    /// @brief Internal flag for proper shutdown/destruction.
    bool _finalized = false;
    /// @brief Customizable end-directive to use for `ZSTD_compressStream2` calls.
    ZSTD_EndDirective _zstd_end_directive = ZSTD_e_continue;



    /**
    * @brief Reads all pending input and asks Zstd to compress it to the CStream.
    * @return 0 on success, -1 if writing to the destination streambuf fails.
    */
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

        // Reset the put area.
        setp(_input_buffer.data(), _input_buffer.data() + _input_buffer.size());
        return 0;
    }



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
    * @brief Flushes the buffer when full and inserts the incoming character if provided.
    * @return Integer indicating overflow/EOF status.
    */
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



    /**
    * @brief Overrides the sync method, which in this case is just flush_input().
    * @return Integer indicating status.
    */
    int sync() override {
        if (_finalized) { return 0; }
        return flush_input();
    }



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Disallow copying and moving.
    zstd_ostreambuf(const zstd_ostreambuf&) = delete;



    /// @brief Disallow copying and moving.
    zstd_ostreambuf& operator=(const zstd_ostreambuf&) = delete;



    /**
    * @brief Constructor which requires a destination stream.
    *
    * This implementation uses the `ZSTD_createCCtx` and takes a few liberties.
    *
    * First, it will attempt to use the number of threads as indicated by `std::hardware_concurrency()`.  If 0 or negative is
    * returned, it is set to 1.  If greater than 16 is returned, it is clamped to 16.
    *
    * Next, when compression level is over 19, long-distance matching is enabled.  This drastically improves the compression ratio
    * at the expensive of CPU-time (it's slower).
    *
    * Finally, when compression level is over 19, it also switches to the `ZSTD_e_flush` end-directive.  Normally, ZSTD waits for
    * a full window of data before waking up its compressor threads.  The flush directive forces compression immediately.  This
    * class uses an input buffer size of 16MB to help optimize compressor work.  The whole reason for switching to flush is to help
    * ZSTD engage more worker threads.  Without this, compression was observed to get choked down to a single thread and process
    * very slowly.
    *
    * \par Compliance
    *
    * The data emitted by this class should be a fully compliant ZSTD file.  Meaning, if written to disk, any other ZSTD compliant
    * program can read it, including the `zstd` CLI.
    *
    * @warning Caller must issue `finalize()` before destruction to ensure all buffers are flushed.
    * @param dest Any valid streambuf to write data to.
    * @param level The desired compression level.  It defaults to `ZSTD_CLEVEL_DEFAULT`.  Any valid ZSTD level is permitted by this
    * class.  When level exceeds 19, long-distance matching will be enabled: `ZSTD_c_enableLongDistanceMatching`.
    */
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
        // When compression is > 19, long range scanning is required.  Also change the end diretive to flush immediately so it can
        // engage more CPU threads.  The trade-off is around 10% worse compression at the top end (level = 22).
        if (level > 19) {
            check(ZSTD_CCtx_setParameter(_zstd_cstream, ZSTD_c_enableLongDistanceMatching, 1));
            _zstd_end_directive = ZSTD_e_flush;
        }
        check(ZSTD_CCtx_reset(_zstd_cstream, ZSTD_reset_session_only));

        // Set the write points.
        setp(_input_buffer.data(), _input_buffer.data() + _input_buffer.size());
    }



    /// @brief Destructor just needs to free() the Zstd memory bits.
    ~zstd_ostreambuf() override {
        ZSTD_freeCStream(_zstd_cstream);
    }

    /// @}



    /**
    * @brief Flush all remaining buffers and wait for ZSTD to say its finished.
    *
    * ZSTD needs all input flushed to it (obviously), but it also requires the caller to check `ZSTD_e_end` when in multi-threaded
    * mode until it reports 0.  There's some ZSTD frame bits (checksums maybe?) that isn't fully understood, but this cycling is
    * mandatory before calling any destruction, implicit or otherwise.
    *
    * @warning Caller must issue `finalize()` before destruction to ensure all buffers are flushed.
    */
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




/**
* @class zstd_ostream
* @brief Creates an ostream subclass and connects it to a `zstd_ostreambuf` so it may be passed around like a normal ostream.
*
* This class largely just wraps the `zstd_ostreambuf` to make writing ZSTD data easier.
*/
class zstd_ostream : public std::ostream {
    private:
    /// @brief The actual streambuf to send data in/out of, connected to this objects `rdbuf`.
    zstd_ostreambuf _buffer;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Disallow copying and moving.
    zstd_ostream(const zstd_ostream&) = delete;



    /// @brief Disallow copying and moving.
    zstd_ostream& operator=(const zstd_ostream&) = delete;



    /**
    * @brief Constructor takes an underlying ostream to connect with a `zstd_ostreambuf`.
    * @note Details about compression level and behavior are in the `zstd_ostreambuf` documentation.
    * @param underlying The destination ostream where ZSTD should send compressed data.
    * @param level The compression level to use.
    */
    zstd_ostream(std::ostream& underlying, int level) : std::ostream(nullptr), _buffer(underlying.rdbuf(), level) {
        rdbuf(&_buffer);
    }

    /// @}



    /// @brief Expose finalize for the streambuf.  Nothing more.
    void finalize() {
        _buffer.finalize();
    }

};
