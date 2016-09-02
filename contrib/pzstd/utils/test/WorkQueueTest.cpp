/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include "utils/Buffer.h"
#include "utils/WorkQueue.h"

#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

using namespace pzstd;

namespace {
struct Popper {
  WorkQueue<int>* queue;
  int* results;
  std::mutex* mutex;

  void operator()() {
    int result;
    while (queue->pop(result)) {
      std::lock_guard<std::mutex> lock(*mutex);
      results[result] = result;
    }
  }
};
}

TEST(WorkQueue, SingleThreaded) {
  WorkQueue<int> queue;
  int result;

  queue.push(5);
  EXPECT_TRUE(queue.pop(result));
  EXPECT_EQ(5, result);

  queue.push(1);
  queue.push(2);
  EXPECT_TRUE(queue.pop(result));
  EXPECT_EQ(1, result);
  EXPECT_TRUE(queue.pop(result));
  EXPECT_EQ(2, result);

  queue.push(1);
  queue.push(2);
  queue.finish();
  EXPECT_TRUE(queue.pop(result));
  EXPECT_EQ(1, result);
  EXPECT_TRUE(queue.pop(result));
  EXPECT_EQ(2, result);
  EXPECT_FALSE(queue.pop(result));

  queue.waitUntilFinished();
}

TEST(WorkQueue, SPSC) {
  WorkQueue<int> queue;
  const int max = 100;

  for (int i = 0; i < 10; ++i) {
    queue.push(i);
  }

  std::thread thread([ &queue, max ] {
    int result;
    for (int i = 0;; ++i) {
      if (!queue.pop(result)) {
        EXPECT_EQ(i, max);
        break;
      }
      EXPECT_EQ(i, result);
    }
  });

  std::this_thread::yield();
  for (int i = 10; i < max; ++i) {
    queue.push(i);
  }
  queue.finish();

  thread.join();
}

TEST(WorkQueue, SPMC) {
  WorkQueue<int> queue;
  std::vector<int> results(10000, -1);
  std::mutex mutex;
  std::vector<std::thread> threads;
  for (int i = 0; i < 100; ++i) {
    threads.emplace_back(Popper{&queue, results.data(), &mutex});
  }

  for (int i = 0; i < 10000; ++i) {
    queue.push(i);
  }
  queue.finish();

  for (auto& thread : threads) {
    thread.join();
  }

  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(i, results[i]);
  }
}

TEST(WorkQueue, MPMC) {
  WorkQueue<int> queue;
  std::vector<int> results(10000, -1);
  std::mutex mutex;
  std::vector<std::thread> popperThreads;
  for (int i = 0; i < 100; ++i) {
    popperThreads.emplace_back(Popper{&queue, results.data(), &mutex});
  }

  std::vector<std::thread> pusherThreads;
  for (int i = 0; i < 10; ++i) {
    auto min = i * 1000;
    auto max = (i + 1) * 1000;
    pusherThreads.emplace_back(
        [ &queue, min, max ] {
          for (int i = min; i < max; ++i) {
            queue.push(i);
          }
        });
  }

  for (auto& thread : pusherThreads) {
    thread.join();
  }
  queue.finish();

  for (auto& thread : popperThreads) {
    thread.join();
  }

  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(i, results[i]);
  }
}

TEST(WorkQueue, BoundedSizeWorks) {
  WorkQueue<int> queue(1);
  int result;
  queue.push(5);
  queue.pop(result);
  queue.push(5);
  queue.pop(result);
  queue.push(5);
  queue.finish();
  queue.pop(result);
  EXPECT_EQ(5, result);
}

TEST(WorkQueue, BoundedSizePushAfterFinish) {
  WorkQueue<int> queue(1);
  int result;
  queue.push(5);
  std::thread pusher([&queue] {
    queue.push(6);
  });
  // Dirtily try and make sure that pusher has run.
  std::this_thread::sleep_for(std::chrono::seconds(1));
  queue.finish();
  EXPECT_TRUE(queue.pop(result));
  EXPECT_EQ(5, result);
  EXPECT_FALSE(queue.pop(result));

  pusher.join();
}

TEST(WorkQueue, BoundedSizeMPMC) {
  WorkQueue<int> queue(100);
  std::vector<int> results(10000, -1);
  std::mutex mutex;
  std::vector<std::thread> popperThreads;
  for (int i = 0; i < 10; ++i) {
    popperThreads.emplace_back(Popper{&queue, results.data(), &mutex});
  }

  std::vector<std::thread> pusherThreads;
  for (int i = 0; i < 100; ++i) {
    auto min = i * 100;
    auto max = (i + 1) * 100;
    pusherThreads.emplace_back(
        [ &queue, min, max ] {
          for (int i = min; i < max; ++i) {
            queue.push(i);
          }
        });
  }

  for (auto& thread : pusherThreads) {
    thread.join();
  }
  queue.finish();

  for (auto& thread : popperThreads) {
    thread.join();
  }

  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(i, results[i]);
  }
}

TEST(BufferWorkQueue, SizeCalculatedCorrectly) {
  {
    BufferWorkQueue queue;
    queue.finish();
    EXPECT_EQ(0, queue.size());
  }
  {
    BufferWorkQueue queue;
    queue.push(Buffer(10));
    queue.finish();
    EXPECT_EQ(10, queue.size());
  }
  {
    BufferWorkQueue queue;
    queue.push(Buffer(10));
    queue.push(Buffer(5));
    queue.finish();
    EXPECT_EQ(15, queue.size());
  }
  {
    BufferWorkQueue queue;
    queue.push(Buffer(10));
    queue.push(Buffer(5));
    queue.finish();
    Buffer buffer;
    queue.pop(buffer);
    EXPECT_EQ(5, queue.size());
  }
}
