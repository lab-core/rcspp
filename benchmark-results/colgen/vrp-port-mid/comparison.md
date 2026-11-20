# The Python VRP example against the pure layer, mid tier

Same setup as `../vrp-port-small/comparison.md` (2026-11-20, laptop `leo-laptop` on power, `../rcspp-dev` at
`ee8b0272`, `COLGEN_INIT=single`), 3 rounds, the two configurations alternated inside each round. The laptop was in use
(a design chat, other programs): other processes used 28–85 % of a core on R201_50 and C201, once 196 %.

```powershell
$env:COLGEN_INIT = "single"
.\run_matrix.ps1 --output vrp-port-mid --instances "C101_50,R201_50,RC101_50,C201" `
    --configs "example-mip,layer-purepython-gurobipy-exact" --repeats 3
```

LP time = master + pricing + rest, the median of 3 [min–max]; the example's integer solve (18 ms to 1.1 s) left out.

| instance | LP (both) | iterations old / new | columns old / new | LP s old | LP s new | new − old | rule |
|---|---|---|---|---|---|---|---|
| C101_50 | 363.2468 | 42 / 43 | 1,690 / 1,702 | 0.279 [0.267–0.286] | 0.214 [0.201–0.215] | −0.065 (−23 %) | pass |
| R201_50 | 756.3386 | 35 / 31 | 12,821 / 11,918 | 6.515 [6.305–8.955] | 5.016 [4.998–5.805] | −1.50 (−23 %) | pass |
| RC101_50 | 828.3619 | 28 / 28 | 649 / 715 | 0.140 [0.135–0.146] | 0.107 [0.103–0.125] | −0.033 (−24 %) | pass |
| C201 | 591.5566 | 158–184 / 235 | 35,339–35,556 / 45,755 | 29.52 [27.54–44.64] | 18.74 [18.41–23.84] | −10.8 (−37 %) | pass |

- LP equal to 1e-12 relative everywhere.
- The example is not deterministic on C201 (184, 158, 158 iterations; `F-59` (3)); everything else repeats exactly.
- Median parts (s), master / pricing / rest, old against new: R201_50 1.048 / 4.801 / 0.700 against
  0.266 / 4.766 / 0.012; C201 21.54 / 5.59 / 2.39 against 8.90 / 9.78 / 0.07. Pricing alike on R201_50; on C201 the
  layer prices more (235 iterations against 158–184) but its gurobipy master is 2.4 times faster than python-mip's
  and its rest almost nothing.
- Peak memory lower with the layer (C201 182 MB against 306 MB).
- The rule of `D-148` passes on all four, now above its 0.25 s floor on R201_50 and C201.
