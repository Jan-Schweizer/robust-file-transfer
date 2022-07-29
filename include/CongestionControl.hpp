#ifndef ROBUST_FILE_TRANSFER_CONGESTIONCONTROL_HPP
#define ROBUST_FILE_TRANSFER_CONGESTIONCONTROL_HPP
// ------------------------------------------------------------------------
#include "Window.hpp"
#include "common.hpp"
// ------------------------------------------------------------------------
namespace rft
{
   class CongestionControl
   {
      friend class Server;

      enum class Phase
      {
         CC_NORMAL,
         CC_AVOIDANCE
      };

      explicit CongestionControl(uint16_t maxWindowSize) : maxWindowSize(maxWindowSize) {}

      Phase phase = Phase::CC_NORMAL;
      const double beta = 0.5;
      uint32_t rttMax = 0;
      uint32_t rttCurrent = 0;
      uint32_t maxWindowSize;
      uint16_t cwnd = 1;

      uint16_t updateCWND(uint32_t rrt);
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_CONGESTIONCONTROL_HPP
// ------------------------------------------------------------------------
