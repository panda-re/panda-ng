//! PANDA event UUIDs, derived in Rust from the shared vector file
//! (tests/static/event-uuid-vectors.tsv), which tests/static/events_registry.py also checks
//! against the engine's C (events.c) and Python's uuid5.
//!
//! An event's UUID is UUIDv5(namespace, name + NUL + hashed signature). The hashed signature is
//! the part before `|`, with whitespace and empty items dropped, as `name:type` joined by `,`,
//! with bool spelled u8 and an array's length without leading zeros.
//! The crate has no dependencies, so SHA-1 is written out here. This is only what the test needs:
//! the bindings have no events API yet.
//!
//!     cd rust/panda-bindings && cargo test --test event_uuid_vectors
//!
//! No arch feature is needed.

use std::path::PathBuf;

fn sha1(data: &[u8]) -> [u8; 20] {
    let mut h: [u32; 5] = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0];
    let mut msg = data.to_vec();
    let bits = (data.len() as u64).wrapping_mul(8);
    msg.push(0x80);
    while msg.len() % 64 != 56 {
        msg.push(0);
    }
    msg.extend_from_slice(&bits.to_be_bytes());
    for block in msg.chunks(64) {
        let mut w = [0u32; 80];
        for i in 0..16 {
            w[i] = u32::from_be_bytes([block[4 * i], block[4 * i + 1], block[4 * i + 2], block[4 * i + 3]]);
        }
        for i in 16..80 {
            w[i] = (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]).rotate_left(1);
        }
        let (mut a, mut b, mut c, mut d, mut e) = (h[0], h[1], h[2], h[3], h[4]);
        for (i, wi) in w.iter().enumerate() {
            let (f, k) = match i {
                0..=19 => ((b & c) | (!b & d), 0x5A827999),
                20..=39 => (b ^ c ^ d, 0x6ED9EBA1),
                40..=59 => ((b & c) | (b & d) | (c & d), 0x8F1BBCDC),
                _ => (b ^ c ^ d, 0xCA62C1D6),
            };
            let t = a.rotate_left(5).wrapping_add(f).wrapping_add(e).wrapping_add(k).wrapping_add(*wi);
            e = d;
            d = c;
            c = b.rotate_left(30);
            b = a;
            a = t;
        }
        for (x, y) in h.iter_mut().zip([a, b, c, d, e]) {
            *x = x.wrapping_add(y);
        }
    }
    let mut out = [0u8; 20];
    for (i, x) in h.iter().enumerate() {
        out[4 * i..4 * i + 4].copy_from_slice(&x.to_be_bytes());
    }
    out
}

fn parse_uuid(s: &str) -> [u8; 16] {
    let hex: Vec<u8> = s.bytes().filter(|&c| c != b'-').collect();
    assert_eq!(hex.len(), 32, "bad uuid {s:?}");
    let mut out = [0u8; 16];
    for i in 0..16 {
        out[i] = u8::from_str_radix(std::str::from_utf8(&hex[2 * i..2 * i + 2]).unwrap(), 16).unwrap();
    }
    out
}

/// A type as the hash spells it: whitespace dropped, bool as u8, an array's length without
/// leading zeros (docs/design/event-signature-grammar.md).
fn identity_type(ty: &str) -> String {
    let t: String = ty.chars().filter(|c| !c.is_whitespace()).collect();
    if t == "bool" {
        return "u8".into();
    }
    if !t.starts_with("ptr<") && t.ends_with(']') {
        if let Some((elem, n)) = t[..t.len() - 1].split_once('[') {
            let n: u64 = n.parse().expect("array length");
            return format!("{elem}[{n}]");
        }
    }
    t
}

/// The hashed part of a signature: fields before `|`, canonical.
fn hashed_signature(sig: &str) -> String {
    let before = sig.split('|').next().unwrap_or("");
    before
        .split(',')
        .map(str::trim)
        .filter(|item| !item.is_empty())
        .map(|item| {
            let (name, ty) = item.split_once(':').expect("field without a type");
            format!("{}:{}", name.trim(), identity_type(ty))
        })
        .collect::<Vec<_>>()
        .join(",")
}

fn event_uuid(ns: &[u8; 16], name: &str, hashed: &str) -> [u8; 16] {
    let mut data = ns.to_vec();
    data.extend_from_slice(name.as_bytes());
    data.push(0);
    data.extend_from_slice(hashed.as_bytes());
    let digest = sha1(&data);
    let mut u = [0u8; 16];
    u.copy_from_slice(&digest[..16]);
    u[6] = (u[6] & 0x0f) | 0x50; // version 5
    u[8] = (u[8] & 0x3f) | 0x80; // RFC 4122 variant
    u
}

#[test]
fn sha1_known_answers() {
    let hex = |d: [u8; 20]| d.iter().map(|b| format!("{b:02x}")).collect::<String>();
    assert_eq!(hex(sha1(b"")), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    assert_eq!(hex(sha1(b"abc")), "a9993e364706816aba3e25717850c26c9cd0d89d");
    assert_eq!(
        hex(sha1(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
        "84983e441c3bd26ebaae4aa1f95129e5e54670f1"
    );
}

#[test]
fn event_uuid_vectors() {
    let path: PathBuf =
        [env!("CARGO_MANIFEST_DIR"), "..", "..", "tests", "static", "event-uuid-vectors.tsv"].iter().collect();
    let text = std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("{}: {e}", path.display()));
    let mut rows = 0;
    for line in text.lines().filter(|l| !l.is_empty() && !l.starts_with('#')) {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 5, "{line:?}");
        let (ns, name, sig, hashed, want) = (cols[0], cols[1], cols[2], cols[3], cols[4]);
        assert_eq!(hashed_signature(sig), hashed, "{name} {sig:?}: hashed part");
        assert_eq!(event_uuid(&parse_uuid(ns), name, hashed), parse_uuid(want), "{name} {sig:?}: uuid");
        rows += 1;
    }
    assert!(rows >= 10, "only {rows} vectors in {}", path.display());
}
