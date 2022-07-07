#ifndef ROBUST_FILE_TRANSFER_BITFIELD_HPP
#define ROBUST_FILE_TRANSFER_BITFIELD_HPP
// ------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <vector>
// ------------------------------------------------------------------------
namespace rft
// ------------------------------------------------------------------------
{
   class Bitfield
   {
      // ------------------------------------------------------------------------
      friend class Client;
      friend class Server;

      std::vector<unsigned char> bitfield;
      /// Number of bits in the bitfield
      uint16_t size;

      class BitReference
      {
         friend class Bitfield;
         Bitfield& bitfield;
         uint16_t byte;
         uint16_t offset;

         BitReference(Bitfield& bitfield, uint16_t byte, uint16_t offset);

       public:
         explicit operator bool() const;
         BitReference& operator=(bool b);
      };

    public:
      explicit Bitfield(uint16_t size);

      void from(std::vector<bool>& sequenceNumbers);
      void from(unsigned char* payload);

      bool operator[](uint16_t idx) const;
      BitReference operator[](uint16_t idx);
   };
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_BITFIELD_HPP
