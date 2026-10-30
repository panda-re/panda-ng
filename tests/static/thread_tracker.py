"""Standalone exact-thread return tracker tests; no OSI, engine or guest.
Run: panda-python tests/static/thread_tracker.py (CXX defaults to c++).
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

HARNESS = r'''
#include "plugins/syscalls2/thread_tracker.h"
#include <cassert>
#include <string>
using syscalls2::ThreadKey;
using Tracker = syscalls2::ThreadReturnTracker<std::string>;
static ThreadKey parent{1, 100, 10, 1};
static ThreadKey child{1, 200, 20, 1};
static ThreadKey sibling{1, 300, 30, 1};
int main() {
    Tracker t(4, 1);
    /* ASID is deliberately absent: same-mm siblings are distinct identities. */
    auto a = t.begin(parent, 4096, "a");
    auto b = t.begin(sibling, 4096, "b");
    assert(a && b && *a != *b);
    assert(!t.take(child, 4096));
    auto d = t.take(sibling, 4096);
    assert(d && d->payload == "b" && !d->child);
    assert(!t.take(sibling, 4096));
    assert(!t.take(parent, 4097));
    assert(t.take(parent, 4096)->payload == "a" && !t.size());

    /* Same exact task/retpc with multiple pending invocations is ambiguous. */
    a = t.begin(parent, 4096, "outer");
    b = t.begin(parent, 4096, "inner");
    assert(!t.take(parent, 4096) && t.size() == 2);
    assert(t.drop(*b));
    assert(t.take(parent, 4096)->payload == "outer");
    assert(!t.drop(*a));

    /* No birth means no child return, despite the same return PC. */
    a = t.begin(parent, 4096, "spawn", true);
    assert(!t.take(child, 4096));
    assert(t.failed_spawn(*a, parent));
    assert(!t.birth(*a, parent, child));
    assert(t.take(parent, 4096));

    /* CLONE_PARENT: explicit creator edge accepted; no real_parent exists. */
    a = t.begin(parent, 4096, "fork", true);
    assert(!t.birth(*a, sibling, child));
    assert(!t.birth(*a, parent, parent));
    assert(t.birth(*a, parent, child));
    assert(!t.birth(*a, parent, sibling));
    assert(!t.failed_spawn(*a, parent));
    d = t.take(child, 4096); /* vfork child-first */
    assert(d && d->child && d->origin == parent && d->invocation == *a);
    assert(!t.take(child, 4096));
    assert(t.take(parent, 4096) && !t.size());

    a = t.begin(parent, 4096, "parent-first", true);
    assert(t.birth(*a, parent, child));
    assert(t.take(parent, 4096));
    assert(t.take(child, 4096)->child && !t.size());
    assert(!t.birth(*a, parent, sibling));

    /* Reused pointer, distinct lifetime/format, and restored epochs never match. */
    a = t.begin(parent, 4096, "old");
    auto reused = parent; reused.lifetime++;
    auto other_format = parent; other_format.lifetime_format++;
    assert(!t.take(reused, 4096) && !t.take(other_format, 4096));
    t.reset(2);
    assert(!t.size() && !t.take(parent, 4096));
    assert(!t.begin(parent, 4096, "wrong epoch"));
    auto restored = parent; restored.epoch = 2;
    b = t.begin(restored, 4096, "new");
    assert(b && *b > *a && !t.drop(*a));
    assert(!t.birth(*b, restored, child));
    assert(t.take(restored, 4096)->payload == "new");

    t.reset(1);
    /* Death/exec preserve the independently observed opposite branch. */
    a = t.begin(parent, 4096, "death", true);
    assert(t.birth(*a, parent, child));
    t.death(parent);
    assert(!t.take(parent, 4096) && t.take(child, 4096));
    a = t.begin(parent, 4096, "exec", true);
    assert(t.birth(*a, parent, child));
    t.exec(child);
    assert(!t.take(child, 4096) && t.take(parent, 4096));
    a = t.begin(parent, 4096, "plain");
    assert(!t.birth(*a, parent, child));
    t.exec(parent);
    assert(!t.size());

    /* Child branch colliding with an existing exact invocation fails closed. */
    a = t.begin(parent, 4096, "fork", true);
    b = t.begin(child, 4096, "own");
    assert(t.birth(*a, parent, child));
    assert(!t.take(child, 4096));
    assert(t.drop(*b));
    assert(t.take(child, 4096)->child);
    t.drop(*a);

    /* Bounded storage never evicts; invalid/unknown identities never enter. */
    Tracker bounded(1, 1);
    a = bounded.begin(parent, 4096, "kept");
    assert(!bounded.begin(child, 4096, "overflow"));
    assert(bounded.take(parent, 4096)->payload == "kept");
    auto unknown = parent; unknown.task = 0;
    assert(!bounded.begin(unknown, 4096, "invalid"));
    unknown = parent; unknown.lifetime_format = 0;
    assert(!bounded.begin(unknown, 4096, "unknown lifetime"));
    unknown = parent; unknown.lifetime = 0; /* checked zero lifetime is valid */
    assert(bounded.begin(unknown, 4096, "zero"));
    d = bounded.take(unknown, 4096);
    bounded.reset(3);
    assert(d->payload == "zero"); /* delivery owns its payload */
    return 0;
}
'''


class ThreadTracker(unittest.TestCase):
    def test_exact_identity_tracker(self):
        with tempfile.TemporaryDirectory(prefix='thread-tracker-') as tmp:
            source = Path(tmp) / 'tracker.cpp'
            executable = Path(tmp) / 'tracker'
            source.write_text(HARNESS)
            command = shlex.split(os.environ.get('CXX', 'c++'))
            subprocess.run(command + ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                                      '-I', str(ROOT), str(source), '-o', str(executable)],
                           check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
