# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Tests for async/await concurrency patterns used in voice pipeline."""

import asyncio
import time

import pytest


class TestAsyncConcurrencyPatterns:
    """Test async/await concurrency patterns used in voice pipeline."""

    @pytest.mark.asyncio
    async def test_concurrent_async_tasks_with_cancellation(self):
        """Test cancellation of concurrent async tasks."""
        results = []

        async def slow_task(task_id, delay) -> None:
            try:
                await asyncio.sleep(delay)
                results.append(f"task_{task_id}_completed")
            except asyncio.CancelledError:
                results.append(f"task_{task_id}_cancelled")
                raise

        tasks = []
        for i in range(3):
            task = asyncio.create_task(slow_task(i, 2.0))
            tasks.append(task)

        await asyncio.sleep(0.1)

        for task in tasks:
            task.cancel()

        with pytest.raises(asyncio.CancelledError):
            await asyncio.gather(*tasks)

        cancelled_count = sum(1 for r in results if "cancelled" in r)
        assert cancelled_count == 3

    @pytest.mark.asyncio
    async def test_async_queue_backpressure(self):
        """Test async queue backpressure handling."""
        queue = asyncio.Queue(maxsize=2)

        async def producer() -> str:
            for i in range(10):
                try:
                    await asyncio.wait_for(queue.put(f"item_{i}"), timeout=0.1)
                except TimeoutError:
                    return f"backpressure_at_{i}"
            return "all_produced"

        async def consumer():
            items = []
            for _ in range(5):
                item = await queue.get()
                items.append(item)
                await asyncio.sleep(0.05)
            return items

        producer_result, consumer_result = await asyncio.gather(producer(), consumer())

        assert "backpressure_at_" in producer_result
        assert len(consumer_result) == 5
        assert all("item_" in item for item in consumer_result)

    @pytest.mark.asyncio
    async def test_concurrent_voice_pipeline_stages(self):
        """Test concurrent execution of voice pipeline stages."""

        async def whisper_transcribe(audio_data) -> str:
            await asyncio.sleep(0.2)
            return f"transcribed: {audio_data}"

        async def llm_process(text) -> str:
            await asyncio.sleep(0.3)
            return f"response: {text}"

        async def tts_synthesize(response) -> str:
            await asyncio.sleep(0.1)
            return f"audio: {response}"

        requests = [f"audio_data_{i}" for i in range(3)]

        async def process_request(audio_data):
            transcribed = await whisper_transcribe(audio_data)
            response = await llm_process(transcribed)
            return await tts_synthesize(response)

        start_time = time.time()
        results = await asyncio.gather(*[process_request(req) for req in requests])
        elapsed = time.time() - start_time

        assert len(results) == 3
        for i, result in enumerate(results):
            expected = f"audio: response: transcribed: audio_data_{i}"
            assert result == expected

        assert elapsed < 1.0
