// ------------------------------------------------------------------------
#include "Bitfield.hpp"
#include <cstring>
// ------------------------------------------------------------------------
namespace rft
{
   Bitfield::Bitfield(uint16_t size) : size(size)
   {
      bitfield.resize((size + (8 - 1)) / 8, 0);
   }
   // ------------------------------------------------------------------------
   Bitfield::BitReference Bitfield::operator[](uint16_t idx)
   {
      uint16_t byte = idx / 8;
      uint16_t offset = idx % 8;
      return {bitfield[byte], offset};
   }
   // ------------------------------------------------------------------------
   bool Bitfield::operator[](uint16_t idx) const
   {
      uint16_t byte = idx / 8;
      uint16_t offset = idx % 8;
      unsigned char mask = bitfield[byte];
      mask &= 1UL << (8 - offset - 1);
      return static_cast<bool>(mask >> (8 - offset - 1));
   }
   // ------------------------------------------------------------------------
   void Bitfield::from(std::vector<bool>& sequenceNumbers)
   {
      for (size_t i = 0; i < size; ++i) {
         this->operator[](i) = sequenceNumbers[i];
      }
   }
   // ------------------------------------------------------------------------
   void Bitfield::from(unsigned char* payload)
   {
      std::memcpy(bitfield.data(), payload, bitfield.size());
   }
   // ------------------------------------------------------------------------
   Bitfield::BitReference::BitReference(unsigned char& byte, uint16_t offset) : byte(byte), offset(offset)
   {}
   // ------------------------------------------------------------------------
   Bitfield::BitReference::operator bool() const
   {
      unsigned char mask = byte;
      mask &= 1UL << (8 - offset - 1);
      return static_cast<bool>(mask >> (8 - offset - 1));
   }
   // ------------------------------------------------------------------------
   Bitfield::BitReference& Bitfield::BitReference::operator=(const bool b)
   {
      unsigned char mask;
      if (b) {
         mask = 1UL << (8 - offset - 1);
         byte |= mask;
         return *this;
      } else {
         mask = ~(1UL << (8 - offset - 1));
         byte &= mask;
         return *this;
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
