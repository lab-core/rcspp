import numpy as np

def add_gaussian_noise(vector: dict[int, float], mean: float = 0.0, std: float = 1.0) -> dict[int, float]:
    return {key: value + np.random.normal(mean, std) for key, value in vector.items()}