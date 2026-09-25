"""Tabulate the E1 heuristic sweep (tests/cpp/test_heuristic_sweep.hpp).

Usage::

    python scripts/heuristics_e1_summary.py e1.csv [more.csv ...] > e1_tables.md

Reads the rows the sweep appends to ``RCSPP_SWEEP_OUT`` and prints markdown tables. Every
configuration is scored against the exact bidirectional run on the same dataset and model:

* ``found`` -- the configuration's best reduced cost as a fraction of the optimum (1 is the
  optimum, 0 is no improving column);
* ``cols`` -- improving columns returned;
* ``x_exact`` -- how many times faster than the exact bidirectional solve (above 1 is faster);
* ``x_fwd`` -- the same against the exact forward solve, where it finished.
"""

from __future__ import annotations

import sys

import pandas as pd

EXACT_KEYS = {"quota": "inf", "join_budget": "inf", "max_pairs": "inf", "timeout_s": "inf"}


def load(paths: list[str]) -> pd.DataFrame:
    frames = [pd.read_csv(path, dtype=str, keep_default_na=False) for path in paths]
    df = pd.concat(frames, ignore_index=True)
    for col in [
        "seconds",
        "extended",
        "forward_labels",
        "backward_labels",
        "dominance_checks",
        "join_pairs",
        "joined_paths",
        "columns",
        "negative_columns",
        "top10",
        "invalid",
    ]:
        df[col] = pd.to_numeric(df[col])
    df["best"] = pd.to_numeric(df["best"], errors="coerce")
    # Repeated runs of a configuration (the timing repetitions) merge into one row: the median of
    # every numeric column, the first of the rest, and how many samples there were. The work
    # counts and the columns are deterministic except under a timeout. A timeout run's budget is
    # a fraction of that repetition's exact time, so it is keyed by the fraction (in `extra`), not
    # by the absolute budget.
    key = [
        "dataset",
        "model",
        "algorithm",
        "quota",
        "join_budget",
        "max_pairs",
        "join_after_stop",
        "extra",
    ]
    df.loc[df["extra"].str.startswith("timeout"), "timeout_s"] = "fraction"
    key.append("timeout_s")
    numeric = [c for c in df.columns if pd.api.types.is_numeric_dtype(df[c])]
    rest = [c for c in df.columns if c not in numeric and c not in key]
    grouped = df.groupby(key, sort=False)
    merged = grouped[numeric].median()
    merged = merged.join(grouped[rest].first())
    merged["samples"] = grouped.size()
    return merged.reset_index()


def is_plain(df: pd.DataFrame, algorithm: str) -> pd.Series:
    mask = (df["algorithm"] == algorithm) & (df["extra"] == "")
    for col, value in EXACT_KEYS.items():
        mask &= df[col] == value
    return mask


def score(df: pd.DataFrame) -> pd.DataFrame:
    """Adds found / x_exact / x_fwd, against the plain-model references of each dataset."""
    exact = df[is_plain(df, "bidirectional") & ~df["model"].str.contains("relax")]
    exact = exact.set_index(["dataset", "model"])
    forward = df[is_plain(df, "simple") & ~df["model"].str.contains("relax")]
    forward = forward.set_index(["dataset", "model"])

    def reference_model(model: str) -> str:
        return model.split("+")[0]

    found, x_exact, x_fwd, fwd_ext = [], [], [], []
    for _, row in df.iterrows():
        key = (row["dataset"], reference_model(row["model"]))
        ref = exact.loc[key] if key in exact.index else None
        fwd = forward.loc[key] if key in forward.index else None
        if ref is None or not (ref["best"] < 0):
            found.append(float("nan"))
        elif pd.isna(row["best"]) or row["best"] >= 0:
            found.append(0.0)
        else:
            found.append(row["best"] / ref["best"])
        x_exact.append(ref["seconds"] / row["seconds"] if ref is not None else float("nan"))
        if fwd is not None and fwd["status"] == "complete":
            x_fwd.append(fwd["seconds"] / row["seconds"])
            fwd_ext.append(fwd["extended"])
        else:
            x_fwd.append(float("nan"))
            fwd_ext.append(fwd["extended"] if fwd is not None else float("nan"))
    df = df.copy()
    df["found"] = found
    df["x_exact"] = x_exact
    df["x_fwd"] = x_fwd
    df["fwd_extended"] = fwd_ext
    return df


def band(extended: float) -> str:
    if pd.isna(extended):
        return "?"
    if extended < 13_000:
        return "<13k"
    if extended < 100_000:
        return "13k-100k"
    if extended < 1_000_000:
        return "100k-1M"
    return ">1M"


def label(row: pd.Series) -> str:
    parts = [row["algorithm"]]
    if row["quota"] != "inf":
        parts.append(f"q{row['quota']}")
    if row["join_budget"] != "inf":
        parts.append(f"jb{row['join_budget']}")
    if row["max_pairs"] != "inf":
        parts.append(f"mp{row['max_pairs']}")
    if row["extra"]:
        parts.append(row["extra"])
    if row["timeout_s"] != "inf" and "timeout" not in row["extra"]:
        parts.append(f"t{row['timeout_s']}")
    if row["timeout_s"] != "inf" and row["algorithm"] == "bidirectional":
        parts.append("join" if row["join_after_stop"] == "1" else "nojoin")
    return " ".join(parts)


def md(df: pd.DataFrame, floatfmt: str = ".3g") -> str:
    """A markdown table, without depending on ``tabulate``."""

    def cell(value: object) -> str:
        if isinstance(value, float):
            if pd.isna(value):
                return ""
            if value.is_integer() and abs(value) >= 1:
                return f"{int(value):,}".replace(",", " ")
            return format(value, floatfmt)
        return str(value)

    header = "| " + " | ".join(str(c) for c in df.columns) + " |"
    rule = "|" + "|".join("---" for _ in df.columns) + "|"
    body = ["| " + " | ".join(cell(v) for v in row) + " |" for row in df.itertuples(index=False)]
    return "\n".join([header, rule, *body])


def references(df: pd.DataFrame) -> str:
    rows = []
    for (dataset, model), group in df.groupby(["dataset", "model"], sort=False):
        if "relax" in model:
            continue
        ex = group[is_plain(group, "bidirectional")]
        fw = group[is_plain(group, "simple")]
        if ex.empty:
            continue
        ex = ex.iloc[0]
        fw = fw.iloc[0] if not fw.empty else None
        rows.append(
            {
                "dataset": dataset,
                "model": model,
                "optimum": ex["best"],
                "bidir s": ex["seconds"],
                "bidir cols": ex["negative_columns"],
                "fwd s": fw["seconds"] if fw is not None else float("nan"),
                "fwd status": fw["status"] if fw is not None else "",
                "fwd extended": fw["extended"] if fw is not None else float("nan"),
                "band": band(fw["extended"]) if fw is not None else "?",
            }
        )
    return md(pd.DataFrame(rows))


def pivot(df: pd.DataFrame, mask: pd.Series, title: str) -> str:
    sub = df[mask].copy()
    if sub.empty:
        return f"### {title}\n\n(no rows)\n"
    sub["config"] = sub.apply(label, axis=1)
    out = [f"### {title}\n"]
    for metric in ["found", "negative_columns", "x_exact"]:
        table = sub.pivot_table(
            index="config", columns="dataset", values=metric, aggfunc="first", sort=False
        )
        table.insert(0, "median", table.median(axis=1))
        out.append(f"**{metric}**\n")
        out.append(md(table.reset_index()))
        out.append("")
    return "\n".join(out)


def main(paths: list[str]) -> None:
    df = score(load(paths))
    df["band"] = df["fwd_extended"].map(band)

    invalid = df[df["invalid"] > 0]
    print("# E1 heuristic sweep\n")
    print(
        f"{len(df)} runs, {df['dataset'].nunique()} datasets, models: "
        f"{', '.join(sorted(df['model'].unique()))}\n"
    )
    print(f"Runs with an invalid column: **{len(invalid)}**\n")
    if not invalid.empty:
        print(md(invalid[["dataset", "model", "algorithm", "extra", "invalid", "first_issue"]]))

    for model in sorted(m for m in df["model"].unique() if "relax" not in m):
        sub = df[df["model"].str.split("+").str[0] == model]
        plain = sub[sub["model"] == model]
        print(f"\n## Model `{model}`\n")
        print("### References\n")
        print(references(sub))
        print()
        truncated = (
            plain["quota"].ne("inf")
            & plain["extra"].eq("")
            & plain["join_budget"].eq("inf")
            & plain["timeout_s"].eq("inf")
        )
        print(pivot(plain, truncated, "Truncated labeling"))
        budgets = plain["algorithm"].eq("bidirectional") & (
            plain["join_budget"].ne("inf") | plain["max_pairs"].ne("inf")
        )
        print(pivot(plain, budgets, "Join budgets"))
        timeouts = plain["extra"].str.startswith("timeout")
        print(pivot(plain, timeouts, "Early stops"))
        dives = plain["algorithm"].isin(
            ["greedy", "tabu", "improving-tabu", "diversification-greedy"]
        )
        print(pivot(plain, dives, "Dives and diversification"))
        relaxed = sub["model"].str.contains("relax")
        print(
            pivot(
                sub.assign(algorithm=sub["algorithm"] + " [" + sub["model"] + "]"),
                relaxed,
                "Relaxed dominance",
            )
        )

        probes = plain[plain["extra"] == "setup-probe"]
        if not probes.empty:
            table = probes.pivot_table(
                index="dataset", columns="algorithm", values="seconds", aggfunc="first"
            )
            table["bidir setup overhead s"] = table["bidirectional"] - table["simple"]
            q1 = plain[truncated & plain["algorithm"].eq("bidirectional") & plain["quota"].eq("1")]
            table = table.join(q1.set_index("dataset")["seconds"].rename("bidir q1 s"))
            table["overhead share of q1"] = table["bidir setup overhead s"] / table["bidir q1 s"]
            print("### Setup cost (Q8)\n")
            print(md(table.reset_index()))
            print()


if __name__ == "__main__":
    main(sys.argv[1:])
