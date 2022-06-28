// ------------------------------------------------------------------------
#include "util.hpp"
#include "sha256.h"
#include <fstream>
// ------------------------------------------------------------------------
namespace rft
{
   // ------------------------------------------------------------------------
   std::string compute_SHA256(std::string& filename)
   {
      // each cycle processes about 1 MByte (divisible by 144 => improves Keccak/SHA3 performance)
      std::ifstream file(filename, std::ios::in | std::ios::binary);
      const size_t BufferSize = 144 * 7 * 1024;
      char* buffer = new char[BufferSize];
      SHA256 sha256;

      while (!file.eof()) {
         file.read(buffer, BufferSize);
         size_t numBytesRead = file.gcount();

         sha256.add(buffer, numBytesRead);
      }

      file.close();
      delete[] buffer;

      return sha256.getHash();
   }
   // ------------------------------------------------------------------------
   void hexdump(const void* data, size_t size)
   {
      char ascii[17];
      size_t i, j;
      ascii[16] = '\0';
      for (i = 0; i < size; ++i) {
         printf("%02X ", ((unsigned char*) data)[i]);
         if (((unsigned char*) data)[i] >= ' ' && ((unsigned char*) data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*) data)[i];
         } else {
            ascii[i % 16] = '.';
         }
         if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
               printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
               ascii[(i + 1) % 16] = '\0';
               if ((i + 1) % 16 <= 8) {
                  printf(" ");
               }
               for (j = (i + 1) % 16; j < 16; ++j) {
                  printf("   ");
               }
               printf("|  %s \n", ascii);
            }
         }
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
