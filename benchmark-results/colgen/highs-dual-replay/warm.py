"""Warm-start replay: solve the artificial columns alone, add the priced columns, solve again (as the layer does).

  warm.py LP.mps [scale=<artificial cost>] [option=value ...]
scale replaces the artificial columns' cost (to sweep the threshold).
"""
import sys, highspy, numpy as np
path = sys.argv[1]
src = highspy.Highs(); src.setOptionValue("output_flag", False); src.readModel(path)
lp = src.getLp()
cost = np.array(lp.col_cost_)
art_cost = cost.max()
art = np.flatnonzero(cost == art_cost)
opts = {}
new_cost = None
cold = False
for arg in sys.argv[2:]:
    k, v = arg.split("=")
    if k == "cost": new_cost = float(v); continue
    if k == "cold": cold = True; continue
    try: v = int(v)
    except ValueError:
        try: v = float(v)
        except ValueError: v = {"true": True, "false": False}.get(v, v)
    opts[k] = v
if new_cost is not None: cost[art] = new_cost
A = lp.a_matrix_
starts, idx, val = np.array(A.start_), np.array(A.index_), np.array(A.value_)
def cols(ids):
    s, i, v = [0], [], []
    for j in ids:
        i += list(idx[starts[j]:starts[j+1]]); v += list(val[starts[j]:starts[j+1]]); s.append(len(i))
    return np.array(s[:-1], np.int32), np.array(i, np.int32), np.array(v)
h = highspy.Highs()
h.setOptionValue("output_flag", "verbose" in opts)
opts.pop("verbose", None)
for k, v in opts.items(): h.setOptionValue(k, v)
h.addRows(lp.num_row_, lp.row_lower_, lp.row_upper_, 0, [], [], [])
s, i, v = cols(art)
h.addCols(len(art), cost[art], np.array(lp.col_lower_)[art], np.array(lp.col_upper_)[art], len(v), s, i, v)
h.run()
duals = np.array(h.getSolution().row_dual)
print("first", h.getModelStatus(), "max|dual|", np.abs(duals).max())
rest = np.setdiff1d(np.arange(lp.num_col_), art)
s, i, v = cols(rest)
h.addCols(len(rest), cost[rest], np.array(lp.col_lower_)[rest], np.array(lp.col_upper_)[rest], len(v), s, i, v)
h.run()
if cold and h.getModelStatus() != highspy.HighsModelStatus.kOptimal:
    h.clearSolver(); h.run(); print("cold retry")
d2 = np.array(h.getSolution().row_dual)
print("duals", d2[:4], "max|dual|", np.abs(d2).max())
print("second", h.getModelStatus(), h.getInfo().objective_function_value, "art cost", cost[art][0], "n art", len(art))
