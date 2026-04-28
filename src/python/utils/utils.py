import numpy as np

def add_gaussian_noise(vector: dict[int, float], mean: float = 0.0, std: float = 1.0) -> dict[int, float]:
    return {key: value + np.random.normal(mean, std) for key, value in vector.items()}

def multiplicative_gaussian_noise(vector: dict[int, float], mean: float = 1, std: float = 0.1) -> list[dict[int, float]]:
    return {key: value * np.random.normal(mean, std) for key, value in vector.items()}

def dict_l1_norm(vector: dict[int, float]):
    l = [abs(i) for i in vector.values()]
    return sum(l)

def dict_dot_product(vector1:dict[int, float], vector2:dict[int, float]) -> float:
    if vector1.keys() != vector2.keys():
            raise ValueError("Dicts must have the same keys")
    prod = 0
    for i in vector1:
        prod += vector1[i]*vector2[i]
    return prod

def dict_addition(vector1:dict[int, float], vector2:dict[int, float]) -> dict:
    if vector1.keys() != vector2.keys():
            raise ValueError("Dicts must have the same keys")
    s = {}
    for i in vector1:
        s[i] = vector1[i]+vector2[i]
    return s

def dict_scalar_mult(vector:dict[int, float], multiplier:float) -> dict:
    s = {}
    for i in vector:
        s[i] = multiplier*vector[i]
    return s
