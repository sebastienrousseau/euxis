# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Tests for concurrent message bus operations."""

import threading
import time
from queue import Empty, Full, Queue


class TestMessageBusConcurrency:
    """Test concurrent message bus operations."""

    def setup_method(self):
        self.message_queue = Queue(maxsize=100)
        self.published_messages = []
        self.consumed_messages = []

    def test_producer_consumer_pattern(self):
        """Test producer-consumer pattern with backpressure."""

        def producer(producer_id) -> None:
            for i in range(20):
                message = f"producer_{producer_id}_msg_{i}"
                try:
                    self.message_queue.put(message, timeout=0.1)
                    self.published_messages.append(message)
                except Full:
                    break

        def consumer() -> None:
            while True:
                try:
                    message = self.message_queue.get(timeout=0.5)
                    self.consumed_messages.append(message)
                    time.sleep(0.01)
                except Empty:
                    break

        producer_thread = threading.Thread(target=producer, args=(1,))
        consumer_thread = threading.Thread(target=consumer)

        producer_thread.start()
        consumer_thread.start()

        producer_thread.join()
        consumer_thread.join()

        assert len(self.consumed_messages) > 0
        assert len(self.consumed_messages) <= len(self.published_messages)

        for consumed in self.consumed_messages:
            assert consumed in self.published_messages

    def test_multiple_publishers_ordering(self):
        """Test message ordering with multiple publishers."""

        def publisher(publisher_id) -> None:
            for i in range(5):
                message = f"pub_{publisher_id}_msg_{i}"
                self.message_queue.put(message)

        threads = []
        for pub_id in range(3):
            thread = threading.Thread(target=publisher, args=(pub_id,))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        messages = []
        while not self.message_queue.empty():
            messages.append(self.message_queue.get())

        assert len(messages) == 15

        for pub_id in range(3):
            pub_messages = [m for m in messages if f"pub_{pub_id}_" in m]
            for i, msg in enumerate(pub_messages):
                expected = f"pub_{pub_id}_msg_{i}"
                assert msg == expected
