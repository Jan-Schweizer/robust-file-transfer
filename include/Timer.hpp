#ifndef ROBUST_FILE_TRANSFER_TIMER_HPP
#define ROBUST_FILE_TRANSFER_TIMER_HPP
// ------------------------------------------------------------------------
#include "common.hpp"
// ------------------------------------------------------------------------
namespace rft
{
   class Timer
   {
      boost::asio::steady_timer t;

    public:
      explicit Timer(boost::asio::io_context& io_context) : t(io_context) {}

      template<typename Timeunit>
      void setTimeout(Timeunit timeout, std::function<void(const boost::system::error_code& error_code)> callback)
      {
         t.expires_after(timeunit(timeout));
         t.async_wait(callback);
      }

      bool isExpired()
      {
         return t.expiry() <= steady_timer::clock_type::now();
      }

      void cancel()
      {
         t.cancel();
      }
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_TIMER_HPP
