---
title: Theory
---

# Theory: how and why RCSPP works

The rest of this documentation explains *how to call* the library. This section explains *what problem it
solves, why the methods work, and where each idea lives in the code*. No background in column generation or
labeling algorithms is assumed, and every page works through small numeric examples that you can check by hand.

## The short version

The **resource-constrained shortest path problem** (RCSPP) asks for the cheapest path from a source to a sink in
a directed graph, where every arc also consumes **resources** (time, load, battery, working hours…) that must stay
within limits along the path.

It can be solved on its own, but it is most often used as the **pricing problem** in **column generation**. Many
planning problems (vehicle routing, crew scheduling, rostering, network routing…) come down to choosing a
combination of paths. There are far too many possible paths to list, so column generation starts from a few and
repeatedly asks for a new path that would improve the current solution. That question is an RCSPP.

`rcspp` solves it by building partial paths, called **labels**, and discarding those that can never lead to
anything better than another label (**dominance**).

## Reading order

| # | Page | Main question |
|---|---|---|
| 1 | [Master problem, pricing and duals](master-and-pricing.md) | Where does the RCSPP come from, and why are arcs given `Row`s? |
| 2 | [Resources and labels](resources-and-labels.md) | How does the library represent time, load, cost…? |
| 3 | [Dominance and elementarity](dominance-and-elementarity.md) | Why is it safe to discard labels? What about cycles? |
| 4 | [Algorithms and preprocessing](algorithms-and-preprocessing.md) | Exact vs heuristic search; removing arcs before the search |
| 5 | [From LP to integer solutions](lp-to-integer.md) | Branch-and-price, and what the examples do instead |
| 6 | The Python layer *(coming next)* | How the pieces map onto `rcspp`'s Python API |

The pages use vehicle routing as their running example because it is easy to picture. The library itself is not
specific to routing.

:::{toctree}
:maxdepth: 1
:hidden:

master-and-pricing
resources-and-labels
dominance-and-elementarity
algorithms-and-preprocessing
lp-to-integer
:::
