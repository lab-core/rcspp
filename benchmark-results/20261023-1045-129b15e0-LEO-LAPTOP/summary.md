# rcspp-vrp-benchmark

- date: 2026-10-23T10:46:08
- commit: 129b15e0 (uncommitted changes)
- args: 1
- machine: LEO-LAPTOP
- cpu: 11th Gen Intel(R) Core(TM) i7-1165G7 @ 2.80GHz (4 cores, 8 threads)
- memory: 12 GB
- os: Microsoft Windows 11 Home 10.0.26200
- power: plugged in or desktop
- build: Release, Visual Studio 18 2026, MSVC 19.51.36260.0
- gurobi: C:/gurobi1301/win64/lib/gurobi130.lib
- route relaxation: None

```text
Instance    LP Cost     Gap%       Simple      Pushing      Pulling ConstructiveTabu         Tabu      BucketS      BucketP 
----------------------------------------------------------------------------------------------------------------------------
C101         828.94 0.20% FAIL  00:00:01.14  00:00:01.03  00:00:00.97      00:00:00.89  00:00:01.25  00:00:01.09  00:00:01.09 
R101        1636.39   -0.08%  00:00:00.33  00:00:00.27  00:00:00.26      00:00:00.25  00:00:00.38  00:00:00.32  00:00:00.31 
RC101       1572.74   -2.91%  00:00:00.84  00:00:00.75  00:00:00.68      00:00:00.45  00:00:01.17  00:00:01.01  00:00:01.00 
----------------------------------------------------------------------------------------------------------------------------
Total             -        -  00:00:02.30  00:00:02.04  00:00:01.91      00:00:01.59  00:00:02.80  00:00:02.41  00:00:02.40 
```
