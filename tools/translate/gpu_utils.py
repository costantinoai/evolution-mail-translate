#!/usr/bin/env python3
"""
gpu_utils.py
Shared GPU acceleration utilities for Argos Translate.

This module provides centralized GPU detection and configuration logic
to avoid code duplication across translation scripts.
"""

import os
import sys


def setup_gpu_acceleration(debug_log_func=None):
    """
    Configure GPU acceleration for argostranslate.
    Sets ARGOS_DEVICE_TYPE to 'cuda' if GPU is available, otherwise 'cpu'.

    Args:
        debug_log_func: Optional function for debug logging (e.g., debug_log from translate_runner)

    Returns:
        str: The device type that was configured ('cuda' or 'cpu')
    """
    # Helper for optional debug logging
    def log(msg):
        if debug_log_func:
            debug_log_func(msg)

    # Only set if not already configured by user
    if "ARGOS_DEVICE_TYPE" in os.environ:
        log(f"GPU config: Using user-defined ARGOS_DEVICE_TYPE={os.environ['ARGOS_DEVICE_TYPE']}")
        return os.environ["ARGOS_DEVICE_TYPE"]

    device_type = "cpu"  # Default fallback

    try:
        import torch
        if torch.cuda.is_available():
            device_type = "cuda"
            msg = "[translate] GPU acceleration enabled (CUDA available)"
            print(msg, file=sys.stderr)
            log("GPU config: GPU acceleration enabled (CUDA available)")
        else:
            msg = "[translate] Using CPU (no CUDA device found)"
            print(msg, file=sys.stderr)
            log("GPU config: Using CPU (no CUDA device found)")
    except ImportError:
        # PyTorch not installed, fall back to CPU
        msg = "[translate] Using CPU (PyTorch not available for GPU detection)"
        print(msg, file=sys.stderr)
        log("GPU config: Using CPU (PyTorch not available for GPU detection)")
    except Exception as e:
        # Any other error, fall back to CPU
        msg = f"[translate] Using CPU (error during GPU detection: {e})"
        print(msg, file=sys.stderr)
        log(f"GPU config: Using CPU (error during GPU detection: {e})")

    os.environ["ARGOS_DEVICE_TYPE"] = device_type
    log(f"GPU config: Set ARGOS_DEVICE_TYPE={device_type}")

    return device_type
