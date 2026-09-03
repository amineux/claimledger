#!/usr/bin/env python3
"""Fallback verifier for the ClaimLedger citation journal.

Implements the same posting rules as cobol/src/*.cbl and
claimledger::verify_journal:

  citing paper  DR  intellectual debt     amount_cents
  cited  paper  CR  intellectual capital  amount_cents

Exits 0 iff the journal is a closed double-entry book (totals match, no
self-posts, positive amounts) and, when a trial_balance.csv is supplied,
agrees with it on the grand totals.
"""

from __future__ import annotations

import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path


def read_journal_csv(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def read_journal_dat(path: Path) -> list[dict]:
    rows = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        if len(raw) < 96:
            raw = raw.ljust(96)
        rows.append(
            {
                "je_id": raw[0:8],
                "date": raw[8:16],
                "debit_account": raw[16:32].strip(),
                "credit_account": raw[32:48].strip(),
                "amount_cents": raw[48:58].strip(),
                "memo": raw[58:96].rstrip(),
            }
        )
    return rows


def verify(rows: list[dict]) -> tuple[bool, str, dict[str, list[int]], int]:
    totals = defaultdict(lambda: [0, 0])  # debit, credit
    grand = 0
    for i, row in enumerate(rows, start=1):
        debit = (row.get("debit_account") or row.get("debit") or "").strip()
        credit = (row.get("credit_account") or row.get("credit") or "").strip()
        try:
            amt = int(row.get("amount_cents") or row.get("amount") or "0")
        except ValueError:
            return False, f"line {i}: bad amount", totals, 0
        if not debit or not credit:
            return False, f"line {i}: empty account", totals, 0
        if debit == credit:
            return False, f"line {i}: self-entry {debit}", totals, 0
        if amt <= 0:
            return False, f"line {i}: non-positive amount", totals, 0
        totals[debit][0] += amt
        totals[credit][1] += amt
        grand += amt
    return True, f"BALANCED  entries={len(rows)}  cents={grand}", totals, grand


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--journal", type=Path, default=Path("out/ledger/journal.csv"))
    ap.add_argument("--dat", type=Path, default=None)
    ap.add_argument("--trial", type=Path, default=None)
    args = ap.parse_args()

    if args.dat and args.dat.exists():
        rows = read_journal_dat(args.dat)
        source = args.dat
    elif args.journal.exists():
        rows = read_journal_csv(args.journal)
        source = args.journal
    else:
        print(f"verify_ledger: no journal at {args.journal}", file=sys.stderr)
        return 2

    ok, msg, totals, grand = verify(rows)
    print(f"{source}: {msg}")
    if not ok:
        return 1

    trial_path = args.trial
    if trial_path is None:
        guess = args.journal.parent / "trial_balance.csv"
        trial_path = guess if guess.exists() else None
    if trial_path and trial_path.exists():
        with trial_path.open(newline="", encoding="utf-8") as f:
            trows = list(csv.DictReader(f))
        total = next((r for r in trows if r.get("account") == "TOTAL"), None)
        if total:
            td = int(total["debit_cents"])
            tc = int(total["credit_cents"])
            if td != grand or tc != grand:
                print(f"trial balance totals mismatch: csv={td}/{tc} journal={grand}")
                return 1
            print(f"{trial_path}: totals agree ({td})")

    suppliers = sorted(totals.items(), key=lambda kv: kv[1][1] - kv[1][0], reverse=True)
    print("top net creditors (idea suppliers):")
    for acct, (d, c) in suppliers[:8]:
        print(f"  {acct:16s}  dr={d:6d}  cr={c:6d}  net={c - d:6d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
