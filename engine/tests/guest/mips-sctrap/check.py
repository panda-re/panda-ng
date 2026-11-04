"""Compare the guest's own view (console.txt "T" lines) with the trap log (trap.log).

  python3 check.py <out dir>

Prints one verdict line per check, then the unpaired entries by number.
"""
import collections
import re
import sys

O32 = {4001: "exit", 4011: "execve", 4119: "sigreturn", 4193: "rt_sigreturn", 4246: "exit_group",
       4088: "reboot"}
N64 = {5058: "exit", 5057: "execve", 5211: "rt_sigreturn", 5205: "exit_group", 5164: "reboot"}
N32 = {6058: "exit", 6057: "execve", 6211: "rt_sigreturn", 6205: "exit_group", 6164: "reboot"}
NORET = {**O32, **N64, **N32}
ABI = {"o32": 5, "n32": 11, "n64": 12}
GETPID = {"o32": 4020, "n64": 5038, "n32": 6038}
READ = {"o32": 4003, "n64": 5000, "n32": 6000}
CLONE = {"o32": 4120, "n64": 5055, "n32": 6055}
CLONE3 = {"o32": 4435, "n64": 5435, "n32": 6435}
WAIT4 = {"o32": 4114, "n64": 5059, "n32": 6059}


def kv(line):
    return dict(re.findall(r"([\w@]+)=(\S+)", line))


def main():
    out = sys.argv[1]
    ev = []
    for line in open(f"{out}/trap.log"):
        d = kv(line)
        d["kind"] = line[0]
        for k in ("id", "abi", "num", "tid", "restart", "resumed", "injected", "inject_ret", "ret"):
            if k in d:
                d[k] = int(d[k])
        ev.append(d)
    T = [l.split()[1:] for l in open(f"{out}/console.txt", errors="replace") if l.startswith("T ")]
    ok = True
    used = set()

    def verdict(name, good, detail):
        nonlocal ok
        ok &= bool(good)
        print(f"{'PASS' if good else 'FAIL'} {name}: {detail}")

    ent = {e["id"]: e for e in ev if e["kind"] == "E" and not e["resumed"]}
    rets = collections.defaultdict(list)
    for e in ev:
        if e["kind"] == "R":
            rets[e["id"]].append(e)
    # every reported return is the same task as its entry (kernel's own task pointer -> pid)
    mism = [(i, ent[i]["tid"], r["tid"]) for i, rs in rets.items() for r in rs
            if i in ent and ent[i]["tid"] != r["tid"]]
    verdict("pairing-task", not mism, f"{sum(len(r) for r in rets.values())} returns, "
            f"{len(mism)} on another task than their entry {mism[:5]}")
    dup = [i for i, rs in rets.items() if len([r for r in rs if not r["restart"] and not r["resumed"]]) > 1]
    verdict("one-return", not dup, f"{len(dup)} calls with more than one final return {dup[:5]}")

    for t in T:
        if t[0] not in ABI:
            print("guest:", " ".join(t))
            continue
        abi, name, f = t[0], t[1], kv(" ".join(t[2:]))
        a = ABI[abi]
        mine = [e for e in ev if e.get("abi") == a]
        if name == "basic":
            tid = int(f["tid"])
            e = [x for x in mine if x["kind"] == "E" and x["tid"] == tid and x["num"] == GETPID[abi]
                 and x["a"].startswith("11111111")]
            r = [x for x in rets.get(e[0]["id"], [])] if e else []
            verdict(f"{abi} args", e and e[0]["a"] == f["args"] and r and r[0]["ret"] == int(f["ret"]),
                    f"guest {f['args']} ret {f['ret']}; trap {e[0]['a'] if e else None} "
                    f"ret {r[0]['ret'] if r else None} pc {e[0]['pc'] if e else None}")
        elif name == "abi":
            verdict(f"{abi} abi-by-number", True, f"guest: o32 getpid(4020)={f['o32']} "
                    f"n64 getpid(5038)={f['n64']} n32 getpid(6038)={f['n32']}; trap ABIs: " +
                    ", ".join(f"{n}->abi {x['abi']}" for n in (4020, 5038, 6038)
                              for x in ev[::-1] if x["kind"] == "E" and x["num"] == n
                              and x["tid"] == int(f["tid"]))[:200])
        elif name == "count":
            tid, n = int(f["tid"]), int(f["n"])
            e = [x for x in ev if x["kind"] == "E" and x["tid"] == tid and x["a"].startswith("c0de,")]
            r = [x for x in e if any(not y["restart"] for y in rets.get(x["id"], []))]
            verdict(f"{abi} count", len(e) == n and len(r) == n, f"guest {n}; trap {len(e)} entries, "
                    f"{len(r)} returns")
        elif name == "thread":
            tid, fd, ret = int(f["tid"]), int(f["fd"]), int(f["ret"])
            e = [x for x in mine if x["kind"] == "E" and x["tid"] == tid and x["num"] == READ[abi]
                 and int(x["a"].split(",")[0], 16) == fd]
            r = [y for x in e for y in rets.get(x["id"], [])]
            verdict(f"{abi} thread {f['i']}", len(e) == 1 and len(r) == 1 and r[0]["ret"] == ret,
                    f"tid {tid} read(fd {fd}) guest ret {ret}; trap entry pc "
                    f"{e[0]['pc'] if e else None} ksp {e[0]['ksp'] if e else None} usp "
                    f"{e[0]['usp'] if e else None}, returns {[(y['ret'], y['tid']) for y in r]}")
        elif name in ("vfork", "clone3"):
            tid, child = int(f["tid"]), int(f["child"])
            num = CLONE[abi] if name == "vfork" else CLONE3[abi]
            e = [x for x in mine if x["kind"] == "E" and x["tid"] == tid and x["num"] == num]
            e = e[-1:] if e else []
            r = [y for x in e for y in rets.get(x["id"], [])]
            marker = "c41d" if name == "vfork" else "c103"
            ce = [x for x in mine if x["kind"] == "E" and x["a"].startswith(marker + ",")]
            verdict(f"{abi} {name}", e and len(r) == 1 and r[0]["ret"] == child and r[0]["tid"] == tid
                    and ce and ce[0]["tid"] == child,
                    f"parent {tid} child {child}: trap entry ksp {e[0]['ksp'] if e else None} usp "
                    f"{e[0]['usp'] if e else None}; returns {[(y['ret'], y['tid'], y['ksp']) for y in r]}; "
                    f"child's getpid entry tid {ce[0]['tid'] if ce else None} ksp "
                    f"{ce[0]['ksp'] if ce else None} usp {ce[0]['usp'] if ce else None}")
        elif name in ("sig", "stopcont"):
            if name == "stopcont":
                f["restart"] = "nohandler"
            tid, fd, ret = int(f["tid"]), int(f["fd"]), int(f["ret"])
            e = [x for x in mine if x["kind"] == "E" and x["tid"] == tid and x["num"] == READ[abi]
                 and int(x["a"].split(",")[0], 16) == fd and x["id"] not in used]
            e = e[:2] if f["restart"] in ("1", "nohandler") else e[:1]
            used.update(x["id"] for x in e)
            seq = [(x["id"], [(y["ret"], y["restart"]) for y in rets.get(x["id"], [])]) for x in e]
            if f["restart"] in ("1", "nohandler"):
                good = len(e) == 2 and seq[0][1] and seq[0][1][0][1] == 1 and seq[1][1] == [(ret, 0)]
            else:
                good = len(e) == 1 and seq[0][1] == [(ret, 0)]
            verdict(f"{abi} {name} restart={f['restart']}", good,
                    f"guest read ret {ret}; trap (entry id, [(ret, restart)]): {seq}")
        elif name == "asid":
            tid = int(f["tid"])
            e = [x for x in mine if x["kind"] == "E" and x["tid"] == tid and x["num"] == WAIT4[abi]]
            e = e[-1:]
            r = [y for x in e for y in rets.get(x["id"], [])]
            verdict(f"{abi} asid-rollover", e and len(r) == 1 and r[0]["ret"] == int(f["child"]),
                    f"wait4 across 400 forks: entry asid {e[0]['asid'] if e else None} ksp "
                    f"{e[0]['ksp'] if e else None}; return {[(y['ret'], y['asid'], y['ksp']) for y in r]}")
        elif name in ("inject", "injret", "injpw", "fault", "faultret", "skip", "setret"):
            tid = int(f["tid"])
            gp = [x for x in ev if x.get("num") == GETPID[abi] and x.get("tid") == tid]
            if name == "inject":
                h = [x for x in gp if x["kind"] == "E" and x["a"].startswith("1a1ec700")]
                good = int(f["ret"]) == int(f["pid"]) and any(x["injected"] and x["inject_ret"] == int(f["ppid"]) for x in h)
                verdict(f"{abi} inject", good, f"guest ret {f['ret']} pid {f['pid']} ppid {f['ppid']}; trap "
                        + str([(x["resumed"], x["injected"], x["inject_ret"]) for x in h]))
            elif name == "injret":
                h = [x for x in gp if x["kind"] == "R" and ent.get(x["id"], {}).get("a", "").startswith("1a1ec701")]
                good = (int(f["ret"]) == int(f["pid"]) and f["a0"] == "1a1ec701" and f["a1"] == "b0b0"
                        and f["a2"] == "c0c0" and any(x["injected"] and x["inject_ret"] == int(f["ppid"]) for x in h))
                verdict(f"{abi} inject-at-return", good, f"guest ret {f['ret']} a0-a2 {f['a0']},{f['a1']},{f['a2']}; "
                        f"trap returns {[(x['ret'], x['resumed'], x['injected'], x['inject_ret']) for x in h]}")
            elif name == "injpw":
                h = [x for x in gp if x["kind"] == "E" and x["a"].startswith("1a1ec702")]
                good = (int(f["ret"]) == int(f["pid"]) and f["file@4096"] == "INJECTED"
                        and (abi != "o32" or (f["e"] == "dead0001" and f["f"] == "dead0002")))
                verdict(f"{abi} inject-pwrite64", good, f"guest ret {f['ret']} file@4096={f['file@4096']} "
                        f"stack/a4-a5 after {f['e']},{f['f']}; trap {[(x['resumed'], x['injected'], x['inject_ret']) for x in h]}")
            elif name in ("fault", "faultret"):
                m = "fa017000" if name == "fault" else "fa017001"
                h = [x for x in gp if ent.get(x["id"], {}).get("a", "").startswith(m)]
                good = f["resident_before"] == "0" and f["after"] == "1" and int(f["ret"]) == int(f["pid"])
                verdict(f"{abi} {name}", good, f"guest mincore before {f['resident_before']} after {f['after']} "
                        f"ret {f['ret']}; trap {[(x['kind'], x['resumed']) for x in h]}")
            elif name == "skip":
                verdict(f"{abi} skip", f["ret_err"] == "-13" and f["ret_ok"] == "77", f"guest {f}")
            elif name == "setret":
                verdict(f"{abi} set-return", f["ret"] == "-2", f"guest {f}")

    unp = collections.Counter()
    for i, e in ent.items():
        if not any(not r["restart"] for r in rets.get(i, [])):
            unp[(e["abi"], e["num"])] += 1
    print("unpaired entries (abi, num): count:",
          ", ".join(f"{a}/{n}{'(' + NORET[n] + ')' if n in NORET else ''}: {c}" for (a, n), c in sorted(unp.items())))
    print("OVERALL", "PASS" if ok else "FAIL")


if __name__ == "__main__":
    main()
