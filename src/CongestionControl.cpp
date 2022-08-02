// ------------------------------------------------------------------------
#include "CongestionControl.hpp"
#include <cmath>
// ------------------------------------------------------------------------
namespace rft
{
   // ------------------------------------------------------------------------
   uint16_t CongestionControl::getNextWindowSize(uint32_t rrt)
   {
      rttCurrent = rrt;
      rttMax = std::max(rttMax, rttCurrent);

      uint16_t maxMBps = chrono::duration_cast<seconds>(timeunit(rttCurrent).count() * chrono::duration_cast<timeunit>(seconds(maxThroughput))).count();
      maxMBps = std::min(maxMBps, maxThroughput);
      uint16_t rwnd = maxMBps * 1024 * 1024 / CHUNK_SIZE;

      switch (phase) {
         case Phase::CC_NORMAL: {
            auto wwf = static_cast<uint32_t>(std::sqrt(rttMax / rttCurrent * cwnd));
            cwnd = cwnd + wwf / cwnd;
            break;
         }
         case Phase::CC_AVOIDANCE:
            cwnd = std::max(1U, static_cast<uint32_t>(BETA * cwnd));
            break;
      }

      return std::min(cwnd, rwnd);
   }
}// namespace rft
// ------------------------------------------------------------------------
