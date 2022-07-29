// ------------------------------------------------------------------------
#include "CongestionControl.hpp"
#include <cmath>
// ------------------------------------------------------------------------
namespace rft
{
   // ------------------------------------------------------------------------
   uint16_t CongestionControl::updateCWND(uint32_t rrt)
   {
      rttCurrent = rrt;
      rttMax = std::max(rttMax, rttCurrent);
      switch (phase) {
         case Phase::CC_NORMAL: {
            auto wwf = static_cast<uint32_t>(std::sqrt(rttMax / rttCurrent * cwnd));
            cwnd = std::min(cwnd + wwf / cwnd, maxWindowSize);
            break;
         }
         case Phase::CC_AVOIDANCE:
            cwnd = std::max(1U, static_cast<uint32_t>(beta * cwnd));
            break;
      }

      return cwnd;
   }
}// namespace rft
// ------------------------------------------------------------------------
