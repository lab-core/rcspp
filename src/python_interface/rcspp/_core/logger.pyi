from enum import IntEnum

class LogLevel(IntEnum):
    Trace = 0
    Debug = 1
    Info = 2
    Warn = 3
    Error = 4
    Fatal = 5
    Off = 6

def init(level: LogLevel) -> None: ...
def set_level(level: LogLevel) -> None: ...
def get_level() -> LogLevel: ...
