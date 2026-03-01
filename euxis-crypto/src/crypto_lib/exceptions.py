# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Crypto Library Exception Types - Structured Error Handling.

This module defines specific error types for different crypto operation categories
following the Result<T, E> pattern for safe error handling.
"""

from dataclasses import dataclass
from enum import Enum
from typing import Any


class CryptoErrorCategory(Enum):
    """Categories of crypto errors for systematic handling."""

    KEY_MANAGEMENT = "key_management"
    ENCRYPTION = "encryption"
    DECRYPTION = "decryption"
    HASHING = "hashing"
    VALIDATION = "validation"
    PARSING = "parsing"
    ALGORITHM = "algorithm"


@dataclass(frozen=True)
class ErrorContext:
    """Immutable context information for crypto errors."""

    operation: str
    category: CryptoErrorCategory
    details: dict[str, Any]
    recoverable: bool = False

    def with_detail(self, key: str, value: Any) -> "ErrorContext":
        """Return new context with additional detail."""
        new_details = self.details.copy()
        new_details[key] = value
        return ErrorContext(
            operation=self.operation,
            category=self.category,
            details=new_details,
            recoverable=self.recoverable
        )


@dataclass(frozen=True)
class Result[T, E]:
    """Result<T, E> type for safe error handling without exceptions."""

    _value: T | None = None
    _error: E | None = None
    _is_success: bool = False

    @classmethod
    def success(cls, value: T) -> "Result[T, E]":
        """Create successful result."""
        return cls(_value=value, _is_success=True)

    @classmethod
    def error(cls, error: E) -> "Result[T, E]":
        """Create error result."""
        return cls(_error=error, _is_success=False)

    @property
    def is_success(self) -> bool:
        """Check if result is successful."""
        return self._is_success

    @property
    def is_error(self) -> bool:
        """Check if result is error."""
        return not self._is_success

    def unwrap(self) -> T:
        """Get success value or raise exception."""
        if not self._is_success:
            msg = f"Called unwrap() on error result: {self._error}"
            raise ValueError(msg)
        return self._value

    def unwrap_or(self, default: T) -> T:
        """Get success value or return default."""
        return self._value if self._is_success else default

    def unwrap_error(self) -> E:
        """Get error value or raise exception."""
        if self._is_success:
            msg = f"Called unwrap_error() on success result: {self._value}"
            raise ValueError(msg)
        return self._error


# Type alias for common crypto result pattern
type CryptoResult[T] = Result[T, "CryptoError"]


class CryptoError(Exception):
    """Base exception for all crypto operations with structured context."""

    def __init__(self, message: str, context: ErrorContext | None = None) -> None:
        super().__init__(message)
        self.message = message
        self.context = context or ErrorContext(
            operation="unknown",
            category=CryptoErrorCategory.ALGORITHM,
            details={},
            recoverable=False
        )

    def __str__(self) -> str:
        if self.context.details:
            details_str = ", ".join(f"{k}={v}" for k, v in self.context.details.items())
            return f"{self.message} (operation={self.context.operation}, {details_str})"
        return f"{self.message} (operation={self.context.operation})"


class InvalidKeyError(CryptoError):
    """Key-related errors with specific context preservation."""

    def __init__(
        self,
        message: str,
        key_type: str | None = None,
        expected_size: int | None = None,
        actual_size: int | None = None,
    ) -> None:
        details = {}
        if key_type:
            details["key_type"] = key_type
        if expected_size is not None:
            details["expected_size"] = expected_size
        if actual_size is not None:
            details["actual_size"] = actual_size

        context = ErrorContext(
            operation="key_validation",
            category=CryptoErrorCategory.KEY_MANAGEMENT,
            details=details,
            recoverable=False  # Key errors are typically not recoverable
        )
        super().__init__(message, context)


class EncryptionError(CryptoError):
    """Encryption-specific errors with operation context."""

    def __init__(
        self,
        message: str,
        algorithm: str | None = None,
        data_size: int | None = None,
    ) -> None:
        details = {}
        if algorithm:
            details["algorithm"] = algorithm
        if data_size is not None:
            details["data_size"] = data_size

        context = ErrorContext(
            operation="encryption",
            category=CryptoErrorCategory.ENCRYPTION,
            details=details,
            recoverable=False
        )
        super().__init__(message, context)


class DecryptionError(CryptoError):
    """Decryption-specific errors with failure context."""

    def __init__(
        self,
        message: str,
        algorithm: str | None = None,
        corruption_detected: bool = False,
    ) -> None:
        details = {}
        if algorithm:
            details["algorithm"] = algorithm
        details["corruption_detected"] = corruption_detected

        context = ErrorContext(
            operation="decryption",
            category=CryptoErrorCategory.DECRYPTION,
            details=details,
            recoverable=False
        )
        super().__init__(message, context)


class HashingError(CryptoError):
    """Hashing operation errors with algorithm context."""

    def __init__(
        self,
        message: str,
        algorithm: str | None = None,
        input_size: int | None = None,
    ) -> None:
        details = {}
        if algorithm:
            details["algorithm"] = algorithm
        if input_size is not None:
            details["input_size"] = input_size

        context = ErrorContext(
            operation="hashing",
            category=CryptoErrorCategory.HASHING,
            details=details,
            recoverable=True  # Hashing errors might be recoverable with different algorithm
        )
        super().__init__(message, context)


class ValidationError(CryptoError):
    """Validation errors for crypto data integrity."""

    def __init__(
        self,
        message: str,
        validation_type: str | None = None,
        expected: str | None = None,
        actual: str | None = None,
    ) -> None:
        details = {}
        if validation_type:
            details["validation_type"] = validation_type
        if expected:
            details["expected"] = expected
        if actual:
            details["actual"] = actual

        context = ErrorContext(
            operation="validation",
            category=CryptoErrorCategory.VALIDATION,
            details=details,
            recoverable=False
        )
        super().__init__(message, context)


class ParsingError(CryptoError):
    """Errors parsing crypto data formats."""

    def __init__(
        self,
        message: str,
        format_type: str | None = None,
        position: int | None = None,
    ) -> None:
        details = {}
        if format_type:
            details["format_type"] = format_type
        if position is not None:
            details["position"] = position

        context = ErrorContext(
            operation="parsing",
            category=CryptoErrorCategory.PARSING,
            details=details,
            recoverable=True  # Parsing errors might be recoverable with format correction
        )
        super().__init__(message, context)


class AlgorithmError(CryptoError):
    """Algorithm-specific or unsupported algorithm errors."""

    def __init__(
        self,
        message: str,
        algorithm: str | None = None,
        supported_algorithms: list | None = None,
    ) -> None:
        details = {}
        if algorithm:
            details["algorithm"] = algorithm
        if supported_algorithms:
            details["supported_algorithms"] = supported_algorithms

        context = ErrorContext(
            operation="algorithm_selection",
            category=CryptoErrorCategory.ALGORITHM,
            details=details,
            recoverable=True  # Algorithm errors are recoverable by choosing different algorithm
        )
        super().__init__(message, context)


# Legacy aliases for backward compatibility
UnsupportedAlgorithmError = AlgorithmError
KeyDerivationError = InvalidKeyError
