#ifndef ROBUST_FILE_TRANSFER_WINDOW_HPP
#define ROBUST_FILE_TRANSFER_WINDOW_HPP
// ------------------------------------------------------------------------
#include "common.hpp"
#include <vector>
// ------------------------------------------------------------------------
namespace rft
{
   struct Window {
      explicit Window(uint16_t maxSize) : maxSize(maxSize)
      {
         chunks.resize(maxSize);
         sequenceNumbers.resize(maxSize, false);
      }

      std::vector<std::vector<char>> chunks;
      uint8_t id = 0;
      uint16_t maxSize;
      uint16_t currentSize = 1;
      std::vector<bool> sequenceNumbers;

      void store_chunk(std::vector<char>& chunk, const uint16_t sequenceNumber)
      {
         chunks[sequenceNumber] = std::move(chunk);
         sequenceNumbers[sequenceNumber] = true;
      }
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_WINDOW_HPP
// ------------------------------------------------------------------------
