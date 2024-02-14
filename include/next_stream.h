/*
    Network Next. Copyright © 2017 - 2024 Network Next, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following 
    conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
       and the following disclaimer in the documentation and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote 
       products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
    OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef NEXT_STREAM_H
#define NEXT_STREAM_H

#include "next.h"
#include "next_bitpacker.h"

namespace next
{
    /**
        Functionality common to all stream classes.
     */

    class BaseStream
    {
    public:

        /**
            Base stream constructor.
         */

        explicit BaseStream() : m_context( NULL ) {}

        /**
            Set a context on the stream.
         */

        void SetContext( void * context )
        {
            m_context = context;
        }

        /**
            Get the context pointer set on the stream.
            @returns The context pointer. May be NULL.
         */

        void * GetContext() const
        {
            return m_context;
        }

    private:

        void * m_context;                           ///< The context pointer set on the stream. May be NULL.
    };

    /**
        Stream class for writing bitpacked data.
        This class is a wrapper around the bit writer class. Its purpose is to provide unified interface for reading and writing.
        You can determine if you are writing to a stream by calling Stream::IsWriting inside your templated serialize method.
        This is evaluated at compile time, letting the compiler generate optimized serialize functions without the hassle of maintaining separate read and write functions.
        IMPORTANT: Generally, you don't call methods on this class directly. Use the serialize_* macros instead. See test/shared.h for some examples.
     */

    class WriteStream : public BaseStream
    {
    public:

        enum { IsWriting = 1 };
        enum { IsReading = 0 };

        /**
            Write stream constructor.
            @param buffer The buffer to write to.
            @param bytes The number of bytes in the buffer. Must be a multiple of four.
            @param allocator The allocator to use for stream allocations. This lets you dynamically allocate memory as you read and write packets.
         */

        WriteStream( uint8_t * buffer, int bytes ) : m_writer( buffer, bytes ) {}

        /**
            Serialize an integer (write).
            @param value The integer value in [min,max].
            @param min The minimum value.
            @param max The maximum value.
            @returns Always returns true. All checking is performed by debug asserts only on write.
         */

        bool SerializeInteger( int32_t value, int32_t min, int32_t max )
        {
            next_assert( min < max );
            next_assert( value >= min );
            next_assert( value <= max );
            const int bits = bits_required( min, max );
            uint32_t unsigned_value = value - min;
            m_writer.WriteBits( unsigned_value, bits );
            return true;
        }

        /**
            Serialize a number of bits (write).
            @param value The unsigned integer value to serialize. Must be in range [0,(1<<bits)-1].
            @param bits The number of bits to write in [1,32].
            @returns Always returns true. All checking is performed by debug asserts on write.
         */

        bool SerializeBits( uint32_t value, int bits )
        {
            next_assert( bits > 0 );
            next_assert( bits <= 32 );
            m_writer.WriteBits( value, bits );
            return true;
        }

        /**
            Serialize an array of bytes (write).
            @param data Array of bytes to be written.
            @param bytes The number of bytes to write.
            @returns Always returns true. All checking is performed by debug asserts on write.
         */

        bool SerializeBytes( const uint8_t * data, int bytes )
        {
            next_assert( data );
            next_assert( bytes >= 0 );
            SerializeAlign();
            m_writer.WriteBytes( data, bytes );
            return true;
        }

        /**
            Serialize an align (write).
            @returns Always returns true. All checking is performed by debug asserts on write.
         */

        bool SerializeAlign()
        {
            m_writer.WriteAlign();
            return true;
        }

        /**
            If we were to write an align right now, how many bits would be required?
            @returns The number of zero pad bits required to achieve byte alignment in [0,7].
         */

        int GetAlignBits() const
        {
            return m_writer.GetAlignBits();
        }

        /**
            Flush the stream to memory after you finish writing.
            Always call this after you finish writing and before you call WriteStream::GetData, or you'll potentially truncate the last dword of data you wrote.
         */

        void Flush()
        {
            m_writer.FlushBits();
        }

        /**
            Get a pointer to the data written by the stream.
            IMPORTANT: Call WriteStream::Flush before you call this function!
            @returns A pointer to the data written by the stream
         */

        const uint8_t * GetData() const
        {
            return m_writer.GetData();
        }

        /**
            How many bytes have been written so far?
            @returns Number of bytes written. This is effectively the packet size.
         */

        int GetBytesProcessed() const
        {
            return m_writer.GetBytesWritten();
        }

        /**
            Get number of bits written so far.
            @returns Number of bits written.
         */

        int GetBitsProcessed() const
        {
            return m_writer.GetBitsWritten();
        }

    private:

        BitWriter m_writer;                 ///< The bit writer used for all bitpacked write operations.
    };

    /**
        Stream class for reading bitpacked data.
        This class is a wrapper around the bit reader class. Its purpose is to provide unified interface for reading and writing.
        You can determine if you are reading from a stream by calling Stream::IsReading inside your templated serialize method.
        This is evaluated at compile time, letting the compiler generate optimized serialize functions without the hassle of maintaining separate read and write functions.
        IMPORTANT: Generally, you don't call methods on this class directly. Use the serialize_* macros instead. See test/shared.h for some examples.
     */

    class ReadStream : public BaseStream
    {
    public:

        enum { IsWriting = 0 };
        enum { IsReading = 1 };

        /**
            Read stream constructor.
            @param buffer The buffer to read from.
            @param bytes The number of bytes in the buffer. May be a non-multiple of four, however if it is, the underlying buffer allocated should be large enough to read the any remainder bytes as a dword.
            @param allocator The allocator to use for stream allocations. This lets you dynamically allocate memory as you read and write packets.
         */

        ReadStream( const uint8_t * buffer, int bytes ) : BaseStream(), m_reader( buffer, bytes ) {}

        /**
            Serialize an integer (read).
            @param value The integer value read is stored here. It is guaranteed to be in [min,max] if this function succeeds.
            @param min The minimum allowed value.
            @param max The maximum allowed value.
            @returns Returns true if the serialize succeeded and the value is in the correct range. False otherwise.
         */

        bool SerializeInteger( int32_t & value, int32_t min, int32_t max )
        {
            next_assert( min < max );
            const int bits = bits_required( min, max );
            if ( m_reader.WouldReadPastEnd( bits ) )
                return false;
            uint32_t unsigned_value = m_reader.ReadBits( bits );
            value = (int32_t) unsigned_value + min;
            return true;
        }

        /**
            Serialize a number of bits (read).
            @param value The integer value read is stored here. Will be in range [0,(1<<bits)-1].
            @param bits The number of bits to read in [1,32].
            @returns Returns true if the serialize read succeeded, false otherwise.
         */

        bool SerializeBits( uint32_t & value, int bits )
        {
            next_assert( bits > 0 );
            next_assert( bits <= 32 );
            if ( m_reader.WouldReadPastEnd( bits ) )
                return false;
            uint32_t read_value = m_reader.ReadBits( bits );
            value = read_value;
            return true;
        }

        /**
            Serialize an array of bytes (read).
            @param data Array of bytes to read.
            @param bytes The number of bytes to read.
            @returns Returns true if the serialize read succeeded. False otherwise.
         */

        bool SerializeBytes( uint8_t * data, int bytes )
        {
            if ( !SerializeAlign() )
                return false;
            if ( m_reader.WouldReadPastEnd( bytes * 8 ) )
                return false;
            m_reader.ReadBytes( data, bytes );
            return true;
        }

        /**
            Serialize an align (read).
            @returns Returns true if the serialize read succeeded. False otherwise.
         */

        bool SerializeAlign()
        {
            const int alignBits = m_reader.GetAlignBits();
            if ( m_reader.WouldReadPastEnd( alignBits ) )
                return false;
            if ( !m_reader.ReadAlign() )
                return false;
            return true;
        }

        /**
            If we were to read an align right now, how many bits would we need to read?
            @returns The number of zero pad bits required to achieve byte alignment in [0,7].
         */

        int GetAlignBits() const
        {
            return m_reader.GetAlignBits();
        }

        /**
            Get number of bits read so far.
            @returns Number of bits read.
         */

        int GetBitsProcessed() const
        {
            return m_reader.GetBitsRead();
        }

        /**
            How many bytes have been read so far?
            @returns Number of bytes read. Effectively this is the number of bits read, rounded up to the next byte where necessary.
         */

        int GetBytesProcessed() const
        {
            return ( m_reader.GetBitsRead() + 7 ) / 8;
        }

    private:

        BitReader m_reader;             ///< The bit reader used for all bitpacked read operations.
    };

    /**
        Serialize integer value (read/write).
        This is a helper macro to make writing unified serialize functions easier.
        Serialize macros returns false on error so we don't need to use exceptions for error handling on read. This is an important safety measure because packet data comes from the network and may be malicious.
        IMPORTANT: This macro must be called inside a templated serialize function with template \<typename Stream\>. The serialize method must have a bool return value.
        @param stream The stream object. May be a read or write stream.
        @param value The integer value to serialize in [min,max].
        @param min The minimum value.
        @param max The maximum value.
     */

    #define serialize_int( stream, value, min, max )                    \
        do                                                              \
        {                                                               \
            next_assert( min < max );                               \
            int32_t int32_value = 0;                                    \
            if ( Stream::IsWriting )                                    \
            {                                                           \
                next_assert( int64_t(value) >= int64_t(min) );      \
                next_assert( int64_t(value) <= int64_t(max) );      \
                int32_value = (int32_t) value;                          \
            }                                                           \
            if ( !stream.SerializeInteger( int32_value, min, max ) )    \
            {                                                           \
                return false;                                           \
            }                                                           \
            if ( Stream::IsReading )                                    \
            {                                                           \
                value = int32_value;                                    \
                if ( int64_t(value) < int64_t(min) ||                   \
                     int64_t(value) > int64_t(max) )                    \
                {                                                       \
                    return false;                                       \
                }                                                       \
            }                                                           \
        } while (0)

    /**
        Serialize bits to the stream (read/write).
        This is a helper macro to make writing unified serialize functions easier.
        Serialize macros returns false on error so we don't need to use exceptions for error handling on read. This is an important safety measure because packet data comes from the network and may be malicious.
        IMPORTANT: This macro must be called inside a templated serialize function with template \<typename Stream\>. The serialize method must have a bool return value.
        @param stream The stream object. May be a read or write stream.
        @param value The unsigned integer value to serialize.
        @param bits The number of bits to serialize in [1,32].
     */

    #define serialize_bits( stream, value, bits )                       \
        do                                                              \
        {                                                               \
            next_assert( bits > 0 );                                \
            next_assert( bits <= 32 );                              \
            uint32_t uint32_value = 0;                                  \
            if ( Stream::IsWriting )                                    \
            {                                                           \
                uint32_value = (uint32_t) value;                        \
            }                                                           \
            if ( !stream.SerializeBits( uint32_value, bits ) )          \
            {                                                           \
                return false;                                           \
            }                                                           \
            if ( Stream::IsReading )                                    \
            {                                                           \
                value = uint32_value;                                   \
            }                                                           \
        } while (0)

    /**
        Serialize a boolean value to the stream (read/write).
        This is a helper macro to make writing unified serialize functions easier.
        Serialize macros returns false on error so we don't need to use exceptions for error handling on read. This is an important safety measure because packet data comes from the network and may be malicious.
        IMPORTANT: This macro must be called inside a templated serialize function with template \<typename Stream\>. The serialize method must have a bool return value.
        @param stream The stream object. May be a read or write stream.
        @param value The boolean value to serialize.
     */

    #define serialize_bool( stream, value )                             \
        do                                                              \
        {                                                               \
            uint32_t uint32_bool_value = 0;                             \
            if ( Stream::IsWriting )                                    \
            {                                                           \
                uint32_bool_value = value ? 1 : 0;                      \
            }                                                           \
            serialize_bits( stream, uint32_bool_value, 1 );             \
            if ( Stream::IsReading )                                    \
            {                                                           \
                value = uint32_bool_value ? true : false;               \
            }                                                           \
        } while (0)
}

#endif // #ifndef NEXT_STREAM_H
