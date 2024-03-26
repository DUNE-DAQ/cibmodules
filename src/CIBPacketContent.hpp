/*
 * CIBPacketContent.hpp
 *
 *  Created on: Mar 25, 2024
 *      Author: Nuno Barros
 */

#ifndef CIBMODULES_SRC_CIBPACKETCONTENT_HPP_
#define CIBMODULES_SRC_CIBPACKETCONTENT_HPP_

#include <cstdint>

namespace dunedaq {
    namespace cibmodules{
        namespace content {

          // NFB: Note that order denotes bit order in the word:
          //      Upper fields are in the lsb and lower field is in the msb
          /// Internal buffer definition
          ///
          //static uint8_t format_version = 0x2;

          typedef struct buffer_t {
              int       handle;
              size_t    len;
          } buffer_t;

          ///
          /// -- TCP header
          ///

          typedef struct tcp_header_t {
              typedef uint16_t pkt_size_t;
              typedef uint8_t   seq_size_t;
              typedef uint8_t   ver_size_t;
              uint16_t packet_size         : 16; // Size of the data content in bytes
              uint8_t  sequence_id         : 8; // packet order...rotates every 256
              uint8_t  format_version : 8;
              static size_t const size_bytes = sizeof(uint32_t);
              static size_t const n_bits_size = 16;
              static size_t const n_bits_sequence_id = 8;
              static size_t const n_bits_version = 8;
          } tcp_header_t;

          // NFB: Careful with unions. Setting one member and then accessing another
          // is undefined behavior in C++.
          // However, I have tested that they work on gcc on the PTB

          typedef union tcp_header {
              tcp_header_t word;
              uint32_t     value;
          } tcp_header;

          namespace word {
            enum word_type {t_trigger=0x0,t_fback=0x1,t_chksum=0x4,t_ts=0x3};
            ///
            /// -- payload
            ///
            // -- The body of a word is composed of 12 bytes
            typedef uint16_t trigger_code_t;
            ///
            /// N. Barros - May 7, 2018 : Changed the structure of the parsing
            ///
            /// Now all structures cast into 16 bytes, but declare different
            /// bit fields to interpret each frame in the context of its respective type
            ///
            /// Also, to move into a format where the full timestamp is stored, the full timestamp
            /// is now placed in the 64 lsb, instead of the msb. Does complies with the memory alignment
            /// and simplifies the parsing of the structures
            ///

            typedef struct word_t {
              typedef uint64_t ts_size_t;
              typedef uint64_t pad_size_t;
              typedef uint64_t word_type_t;
              ts_size_t timestamp;
              pad_size_t payload : 62;
              word_type_t word_type : 2;
              static size_t const size_bytes = 4*sizeof(uint32_t);
              static size_t const size_u32 = 4*sizeof(uint32_t)/sizeof(uint32_t);
              static size_t const n_bits_timestamp  = 64;
              static size_t const n_bits_payload = 61;
              static size_t const n_bits_type = 3;
            } word_t;

            typedef union word{
                word_t frame;
                uint8_t *get_bytes() {return reinterpret_cast<uint8_t*>(&frame);}
            } word;

            /// -- Several different structures that can be used to reinterpret the payload depending on
            /// the word type. All these structures map into the full 16 bytes of the CTB words
            ///

            typedef struct feedback_t {
                typedef uint64_t  ts_size_t;
                typedef uint16_t  code_size_t;
                typedef uint16_t  source_size_t;
                typedef uint32_t  wtype_size_t;
                typedef uint32_t  pad_size_t;
                ts_size_t     timestamp;
                code_size_t   code      : 16;
                source_size_t source    : 16;
                pad_size_t    padding   : 29;
                wtype_size_t  word_type : 3;
                static size_t const size_bytes = 2*sizeof(uint64_t);
                static size_t const size_u32 = size_bytes/sizeof(uint32_t);
                static size_t const n_bits_timestamp  = 64;
                static size_t const n_bits_payload = 32;
                static size_t const n_bits_type     = 3;
            } feedback_t;


             typedef struct timestamp_t {
               typedef uint64_t ts_size_t;
               typedef uint64_t pad_size_t;
               typedef uint64_t wtype_size_t;
               ts_size_t    timestamp;
               pad_size_t   padding   : 61;
               wtype_size_t word_type : 3;
               static size_t const size_bytes = 2*sizeof(uint64_t);
               static size_t const size_u32 = size_bytes/sizeof(uint32_t);
               static size_t const n_bits_timestamp = 64;
               static size_t const n_bits_unused    = 61;
               static size_t const n_bits_type      = 3;
             } timestamp_t;

             typedef struct trigger_t {
               typedef uint64_t ts_size_t;
               typedef uint64_t mask_size_t;
               typedef uint64_t wtype_size_t;
               ts_size_t timestamp ;
               mask_size_t  trigger_word : 62 ;
               wtype_size_t word_type : 2 ;
               static size_t const size_bytes = 2*sizeof(uint64_t);
               static size_t const size_u32 = size_bytes/sizeof(uint32_t);
               static size_t const n_bits_timestamp = 64;
               static size_t const n_bits_tmask     = 61;
               static size_t const n_bits_type      = 3;

               bool IsTrigger() const { return word_type == word_type::t_trigger; }
               bool IsLLT() const { return word_type == word_type::t_lt ; }
               bool IsTrigger( const unsigned int i ) const {
                 if ( IsHLT() ) return trigger_word & ( 0x1 << i ) ;
                 if ( IsLLT() ) return i == 0 ? false : trigger_word & ( 0x1 << (i-1) ) ;
                 return false ;
               }
             } trigger_t;

          } // -- namespace word
        } // -- namespace content
       } // -- namespace cibmodules
      } // -- namespace dunedaq

#endif /* CIBMODULES_SRC_CIBPACKETCONTENT_HPP_ */
