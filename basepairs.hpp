#ifndef INCL_BASEPAIRS
#define INCL_BASEPAIRS

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

#define ALPH_SIZE 4

//typedef unsigned char Base;

//Based on github.com/dnbaker/bonsai/blob/master/bonsai/include/util.h
using i8  = std::int8_t;  using u8  = std::uint8_t;
using i16 = std::int16_t; using u16 = std::uint16_t;
using i32 = std::int32_t; using u32 = std::uint32_t;
using i64 = std::int64_t; using u64 = std::uint64_t;

const char BASE_CHARS[] {'A', 'C', 'G', 'T', 'N'};
const u8 BASE_BYTES[] 
     {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //0-15  ga
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //16-31 rb
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //32-47 ag
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //48-63 e!
      4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, //64-79 (A,C,G)
      4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, //80-95 (T)
      4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, //96-111 (a,c,g)
      4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};//112-127 (t)

std::string reverse_complement(const std::string &seq);

std::vector<u8> seq_to_bases(const std::string &seq);
    
void parse_fasta(std::ifstream &fasta_in, 
                 std::string &fwd_bases, 
                 std::string &rev_bases,
                 bool terminate=false);

#endif
