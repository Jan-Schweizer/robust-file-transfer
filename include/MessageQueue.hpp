#ifndef ROBUST_FILE_TRANSFER_MESSAGEQUEUE_HPP
#define ROBUST_FILE_TRANSFER_MESSAGEQUEUE_HPP
// ------------------------------------------------------------------------
#include "Message.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
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
         std::unique_lock lock(muxQueue);
         return deque.front();
      }

      const Message& back()
      {
         std::unique_lock lock(muxQueue);
         return deque.back();
      }

      Message pop_front()
      {
         std::unique_lock lock(muxQueue);
         Message msg = std::move(deque.front());
         deque.pop_front();
         return msg;
      }

      Message pop_back()
      {
         std::unique_lock lock(muxQueue);
         Message msg = std::move(deque.back());
         deque.pop_back();
         return msg;
      }

      void push_front(const Message& msg)
      {
         std::unique_lock lock_1(muxQueue);
         deque.emplace_front(std::move(msg));

         std::unique_lock lock_2(muxCv);
         cv.notify_one();
      }

      void push_back(const Message& msg)
      {
         std::unique_lock lock_1(muxQueue);
         deque.emplace_back(std::move(msg));

         std::unique_lock lock_2(muxCv);
         cv.notify_one();
      }

      bool empty()
      {
         std::unique_lock lock(muxQueue);
         return deque.empty();
      }

      size_t count()
      {
         std::unique_lock lock(muxQueue);
         return deque.size();
      }

      void clear()
      {
         std::unique_lock lock(muxQueue);
         return deque.clear();
      }

      void wait()
      {
         while (empty()) {
            std::unique_lock lock(muxCv);
            cv.wait(lock);
         }
      }

    private:
      std::mutex muxQueue;
      std::deque<Message> deque;
      std::condition_variable cv;
      std::mutex muxCv;
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_MESSAGEQUEUE_HPP
// ------------------------------------------------------------------------
