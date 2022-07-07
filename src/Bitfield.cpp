// ------------------------------------------------------------------------
#include "Bitfield.hpp"
#include <cstring>
// ------------------------------------------------------------------------
namespace rft
{
   Bitfield::Bitfield(uint16_t size) : size(size)
   {
      bitfield.resize((size + (size - 1)) / 8);
   }
   // ------------------------------------------------------------------------
   Bitfield::BitReference Bitfield::operator[](uint16_t idx)
   {
      uint16_t byte = idx / 8;
      uint16_t offset = idx % 8;
      return {*this, byte, offset};
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
   Bitfield::BitReference::BitReference(Bitfield& bitfield, uint16_t byte, uint16_t offset)
       : bitfield(bitfield), byte(byte), offset(offset) {}
   // ------------------------------------------------------------------------
   Bitfield::BitReference::operator bool() const
   {
      unsigned char mask = bitfield.bitfield[byte];
      mask &= 1UL << (8 - offset - 1);
      return static_cast<bool>(mask >> (8 - offset - 1));
   }
   // ------------------------------------------------------------------------
   Bitfield::BitReference& Bitfield::BitReference::operator=(const bool b)
   {
      unsigned char mask;
      if (b) {
         mask = 1UL << (8 - offset - 1);
         bitfield.bitfield[byte] |= mask;
         return *this;
      } else {
         mask = ~(1UL << (8 - offset - 1));
         bitfield.bitfield[byte] &= mask;
         return *this;
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
