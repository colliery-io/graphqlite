"""
Ollama client for LLM inference.

Provides a simple interface to Ollama's REST API for chat completions.
"""

import json
import httpx
from dataclasses import dataclass
from typing import Generator


@dataclass
class Message:
    """A chat message."""
    role: str  # "system", "user", or "assistant"
    content: str


class OllamaClient:
    """
    Client for Ollama's REST API.

    Default endpoint: http://localhost:11434
    """

    def __init__(
        self,
        model: str = "qwen3:8b",
        base_url: str = "http://localhost:11434",
        timeout: float = 120.0,
    ):
        """
        Initialize Ollama client.

        Args:
            model: Model name (default: qwen3:8b)
            base_url: Ollama server URL
            timeout: Request timeout in seconds
        """
        self.model = model
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout
        self._client = httpx.Client(timeout=timeout)

    def chat(
        self,
        messages: list[Message],
        temperature: float = 0.7,
        stream: bool = False,
    ) -> str:
        """
        Send a chat completion request.

        Args:
            messages: List of Message objects
            temperature: Sampling temperature (0.0-1.0)
            stream: Whether to stream the response

        Returns:
            Assistant's response text
        """
        # For Qwen3, append /no_think to disable extended thinking
        formatted_messages = []
        for i, m in enumerate(messages):
            content = m.content
            # Add /no_think to last user message for Qwen3
            if i == len(messages) - 1 and m.role == "user" and "qwen" in self.model.lower():
                content = content + " /no_think"
            formatted_messages.append({"role": m.role, "content": content})

        payload = {
            "model": self.model,
            "messages": formatted_messages,
            "stream": stream,
            "options": {
                "temperature": temperature,
            },
        }

        response = self._client.post(
            f"{self.base_url}/api/chat",
            json=payload,
        )
        response.raise_for_status()

        if stream:
            return self._handle_stream(response)
        else:
            data = response.json()
            return data["message"]["content"]

    def _handle_stream(self, response: httpx.Response) -> str:
        """Handle streaming response."""
        full_response = []
        for line in response.iter_lines():
            if line:
                data = json.loads(line)
                if "message" in data:
                    full_response.append(data["message"].get("content", ""))
        return "".join(full_response)

    def chat_stream(
        self,
        messages: list[Message],
        temperature: float = 0.7,
    ) -> Generator[str, None, None]:
        """
        Stream chat completion tokens.

        Args:
            messages: List of Message objects
            temperature: Sampling temperature

        Yields:
            Response tokens as they arrive
        """
        # For Qwen3, append /no_think to disable extended thinking
        formatted_messages = []
        for i, m in enumerate(messages):
            content = m.content
            if i == len(messages) - 1 and m.role == "user" and "qwen" in self.model.lower():
                content = content + " /no_think"
            formatted_messages.append({"role": m.role, "content": content})

        payload = {
            "model": self.model,
            "messages": formatted_messages,
            "stream": True,
            "options": {
                "temperature": temperature,
            },
        }

        with self._client.stream(
            "POST",
            f"{self.base_url}/api/chat",
            json=payload,
        ) as response:
            response.raise_for_status()
            for line in response.iter_lines():
                if line:
                    data = json.loads(line)
                    if "message" in data:
                        content = data["message"].get("content", "")
                        if content:
                            yield content

    def generate(
        self,
        prompt: str,
        system: str | None = None,
        temperature: float = 0.7,
    ) -> str:
        """
        Simple generate endpoint (non-chat).

        Args:
            prompt: The prompt text
            system: Optional system prompt
            temperature: Sampling temperature

        Returns:
            Generated text
        """
        payload = {
            "model": self.model,
            "prompt": prompt,
            "stream": False,
            "options": {
                "temperature": temperature,
            },
        }
        if system:
            payload["system"] = system

        response = self._client.post(
            f"{self.base_url}/api/generate",
            json=payload,
        )
        response.raise_for_status()
        return response.json()["response"]

    def is_available(self) -> bool:
        """Check if Ollama server is available."""
        try:
            response = self._client.get(f"{self.base_url}/api/tags")
            return response.status_code == 200
        except Exception:
            return False

    def list_models(self) -> list[str]:
        """List available models."""
        try:
            response = self._client.get(f"{self.base_url}/api/tags")
            response.raise_for_status()
            data = response.json()
            return [m["name"] for m in data.get("models", [])]
        except Exception:
            return []

    def close(self):
        """Close the HTTP client."""
        self._client.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
