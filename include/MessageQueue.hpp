#ifndef ROBUST_FILE_TRANSFER_MESSAGEQUEUE_HPP
#define ROBUST_FILE_TRANSFER_MESSAGEQUEUE_HPP
// ------------------------------------------------------------------------
#include "Message.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <plog/Log.h>
// ------------------------------------------------------------------------
namespace rft
{
   template<typename Message>
   struct MessageQueue {
      MessageQueue() = default;
      MessageQueue(const MessageQueue<Message>& other) = delete;
      MessageQueue(const MessageQueue<Message>&& other) = delete;
      ~MessageQueue() { clear(); }

      const Message& front()
      {
         std::unique_lock lock(mux_queue);
         return deque.front();
      }

      const Message& back()
      {
         std::unique_lock lock(mux_queue);
         return deque.back();
      }

      Message pop_front()
      {
         std::unique_lock lock(mux_queue);
         Message msg = std::move(deque.front());
         deque.pop_front();
         return msg;
      }

      Message pop_back()
      {
         std::unique_lock lock(mux_queue);
         Message msg = std::move(deque.back());
         deque.pop_back();
         return msg;
      }

      void push_front(const Message& msg)
      {
         std::unique_lock lock_1(mux_queue);
         deque.emplace_front(std::move(msg));

         std::unique_lock lock_2(mux_cv);
         cv.notify_one();
      }

      void push_back(const Message& msg)
      {
         std::unique_lock lock_1(mux_queue);
         deque.emplace_back(std::move(msg));

         std::unique_lock lock_2(mux_cv);
         cv.notify_one();
      }

      bool empty()
      {
         std::unique_lock lock(mux_queue);
         return deque.empty();
      }

      size_t count()
      {
         std::unique_lock lock(mux_queue);
         return deque.size();
      }

      void clear()
      {
         std::unique_lock lock(mux_queue);
         return deque.clear();
      }

      void wait()
      {
         std::unique_lock lock(mux_cv);
         cv.wait_for(lock, seconds(10));
      }

      void wake()
      {
         std::unique_lock lock(mux_cv);
         cv.notify_one();
      }

    private:
      std::mutex mux_queue;
      std::deque<Message> deque;
      std::condition_variable cv;
      std::mutex mux_cv;
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_MESSAGEQUEUE_HPP
// ------------------------------------------------------------------------
