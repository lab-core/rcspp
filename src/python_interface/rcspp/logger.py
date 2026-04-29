#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import importlib

_ext = importlib.import_module("rcsppy")

# Importer LogLevel depuis le sous-module C++
LogLevel = _ext.logger.LogLevel

# Créer une classe Logger wrapper en Python
class Logger:
    """Wrapper Python pour le Logger C++"""
    
    @staticmethod
    def init(level=LogLevel.Info, to_console=True, file_path=""):
        """Initialiser le logger"""
        _ext.logger.init(level, to_console, file_path)
    
    @staticmethod
    def set_level(level):
        """Changer le niveau de log"""
        _ext.logger.set_level(level)
    
    @staticmethod
    def level():
        """Obtenir le niveau de log actuel"""
        return _ext.logger.level()
    
    @staticmethod
    def is_level_active(level):
        """Vérifier si un niveau est actif"""
        return _ext.logger.is_level_active(level)

__all__ = ["Logger", "LogLevel"]