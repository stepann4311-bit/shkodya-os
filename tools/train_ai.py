#!/usr/bin/env python3
"""Train the Ai Chat model that ships inside the Shkodya OS image.

    python3 tools/train_ai.py --out ai/ai_model.bin

Two bag-of-words retrievers are trained: one over the code Q&A corpus, one over
the small-talk corpus. Each entry becomes an int8 weight vector over a shared
vocabulary, so scoring a query is a dot product and an argmax.

The vocabulary is explicit rather than hashed. Hashing into a fixed bucket count
was the first design and it had to go: with ~900 distinct corpus words in 1024
buckets, an off-topic query token lands in an occupied bucket about 58% of the
time, and a single bucket hit already scores 127*127 - indistinguishable from a
real one-word match. A vocabulary has no collisions at all, and at this corpus
size it is also *smaller* than the hash table was.

kernel.c re-implements this tokeniser by hand (ai_lookup, ai_encode); the two
must agree exactly. That is why the tokeniser is deliberately primitive:
lowercase byte-wise, split on anything that is not [a-z0-9] or a CP866 Cyrillic
byte, drop stop words, drop tokens shorter than two bytes, no stemming.

Text is stored as CP866, not UTF-8: that is the encoding the kernel's 8x16 font
and the Russian keyboard layout already use (kbd_map_ru maps 'q' to 0xA9).
"""

import argparse
import os
import struct
import sys

MAGIC = b"AIM1"
HEADER_SIZE = 64
MIN_TOKEN = 2
QUESTION_WEIGHT = 2.0          # question tokens count double against answer tokens
CHAT_WEIGHT = 1.0


# --------------------------------------------------------------------------- #
# text -> cp866 bytes
# --------------------------------------------------------------------------- #

def to_cp866(text):
    """Encode to the kernel's single-byte Cyrillic code page.

    Characters the code page cannot represent become spaces, so a stray em dash
    in the corpus cannot corrupt a token boundary.
    """
    out = []
    for ch in text:
        try:
            out.append(ch.encode("cp866"))
        except UnicodeEncodeError:
            out.append(b" ")
    return b"".join(out)


def lower_byte(b):
    """CP866 + ASCII lowercasing, byte-wise. Mirrors ai_lower() in kernel.c.

    CP866 splits Cyrillic uppercase into two runs: 0x80..0x8F (А..П) and
    0x90..0x9F (Р..Я), mapping to 0xA0..0xAF and 0xE0..0xEF respectively.
    """
    if 0x80 <= b <= 0x8F:
        return b + 0x20
    if 0x90 <= b <= 0x9F:
        return b + 0x50
    if 0x41 <= b <= 0x5A:                    # 'A'..'Z'
        return b + 0x20
    return b


def is_word_byte(b):
    """Letters and digits that belong to a token. Mirrors kernel.c."""
    if 0x30 <= b <= 0x39:                    # digits
        return True
    if 0x61 <= b <= 0x7A:                    # lowercase a-z (already lowered)
        return True
    if 0xA0 <= b <= 0xAF or 0xE0 <= b <= 0xEF:   # а-п, р-я
        return True
    return False


# Words with no retrieval value. Without this, a query like "how do i bake
# bread" matches the code corpus on "how"/"do" alone. The list is emitted into
# ai/stopwords.inc so the kernel cannot drift away from it.
STOPWORDS = [
    "a", "about", "all", "also", "am", "an", "and", "any", "are", "as", "at",
    "be", "been", "being", "but", "by", "can", "could", "did", "do", "does",
    "doing", "down", "each", "every", "for", "from", "had", "has", "have",
    "he", "her", "here", "his", "how", "i", "if", "in", "into", "is", "it",
    "its", "just", "may", "me", "might", "more", "most", "must", "my", "no",
    "not", "of", "off", "on", "once", "only", "or", "our", "out", "over",
    "she", "should", "so", "some", "than", "that", "the", "their", "them",
    "then", "there", "these", "they", "this", "those", "to", "under", "up",
    "us", "very", "was", "we", "were", "what", "when", "where", "which",
    "who", "whose", "why", "will", "with", "would", "you", "your",
    # Russian, matching the small-talk corpus
    "а", "бы", "в", "вы", "где", "да", "для", "до", "его", "ее", "если",
    "есть", "же", "за", "и", "из", "или", "их", "к", "как", "ко", "мы",
    "на", "над", "нам", "не", "ни", "но", "о", "он", "она", "они", "от",
    "по", "под", "при", "с", "со", "то", "ты", "у", "уже", "что", "это",
    "я",
]
STOPWORD_BYTES = {to_cp866(w) for w in STOPWORDS}


def tokenize(raw):
    """bytes -> list of token bytes, with stop words removed."""
    tokens = []
    cur = bytearray()

    def flush():
        if len(cur) >= MIN_TOKEN and bytes(cur) not in STOPWORD_BYTES:
            tokens.append(bytes(cur))

    for b in raw:
        lb = lower_byte(b)
        if is_word_byte(lb):
            cur.append(lb)
        elif cur:
            flush()
            cur = bytearray()
    if cur:
        flush()
    return tokens


def emit_stopwords(path):
    """Write the stop-word table as a C array of CP866 byte strings.

    Emitting the list means the kernel's copy is generated rather than typed
    twice, so a query is tokenised identically on both sides by construction.
    Every byte outside [A-Za-z] is octal-escaped, which stays unambiguous
    regardless of what follows it in the literal.
    """
    def literal(word):
        out = []
        for b in word:
            if (0x41 <= b <= 0x5A) or (0x61 <= b <= 0x7A):
                out.append(chr(b))
            else:
                out.append("\\%03o" % b)
        return '"%s"' % "".join(out)

    lines = [
        "/* Generated by tools/train_ai.py - do not edit by hand.",
        " *",
        " * Stop words dropped by both the trainer and the kernel. Kept as CP866",
        " * byte strings so the two tokenisers are identical by construction.",
        " */",
        "static const char *const ai_stopwords[] = {",
    ]
    for word in sorted(STOPWORD_BYTES):
        lines.append("    %s," % literal(word))
    lines.append("};")
    lines.append("#define AI_STOPWORD_COUNT %d" % len(STOPWORD_BYTES))
    with open(path, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    return len(STOPWORD_BYTES)


# --------------------------------------------------------------------------- #
# corpora
# --------------------------------------------------------------------------- #

def read_pairs(path, sep="\t"):
    """Read `left<TAB>right` lines, skipping blanks and # comments."""
    pairs = []
    with open(path, "r", encoding="utf-8") as fh:
        for lineno, line in enumerate(fh, 1):
            line = line.rstrip("\n").rstrip("\r")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if sep not in line:
                sys.exit("error: %s:%d has no %r separator" % (path, lineno, sep))
            left, right = line.split(sep, 1)
            left, right = left.strip(), right.strip()
            if not left or not right:
                sys.exit("error: %s:%d has an empty side" % (path, lineno))
            pairs.append((left, right))
    if not pairs:
        sys.exit("error: %s produced no pairs" % path)
    return pairs


# Sublinear term frequency, as an integer table, so the kernel can apply the
# identical weights: TFW[tf] = round(1000 * (1 + ln(tf))).
TFW = [0, 1000, 1693, 2099, 2386, 2609, 2792, 2946, 3079, 3194, 3297]


def build_vocab(qa, chat):
    """Sorted list of every token that appears in either corpus."""
    seen = set()
    for left, right in list(qa) + list(chat):
        seen.update(tokenize(to_cp866(left)))
        seen.update(tokenize(to_cp866(right)))
    return sorted(seen)


def vectorise(groups, index_of, n_vocab):
    """groups: list of (token_list, weight) -> max-normalised int8 vector.

    Max-normalised, not L2-normalised: dividing by the L2 norm and then by the
    resulting peak cancels the norm out, so the result is just `w / max|w| * 127`.
    Writing it that way is what lets the kernel reproduce it with integers only -
    there is no FPU in the image to call on.
    """
    acc = [0] * n_vocab
    for tokens, weight in groups:
        counts = {}
        for tok in tokens:
            counts[tok] = counts.get(tok, 0) + 1
        for tok, tf in counts.items():
            j = index_of.get(tok)
            if j is None:
                continue                     # query-only word: no weight anywhere
            acc[j] += int(weight * TFW[min(tf, len(TFW) - 1)])

    peak = max(acc) if acc else 0
    if peak <= 0:
        return [0] * n_vocab
    return [int(round(v / peak * 127.0)) for v in acc]


# --------------------------------------------------------------------------- #
# model layout
# --------------------------------------------------------------------------- #
#  header (64 B): 4s magic + 15 u32:
#     0 magic  4 n_qa  8 n_chat  12 n_vocab
#     16 qa_index_off   20 qa_w_off   24 qa_pool_off   28 qa_pool_len
#     32 chat_index_off 36 chat_w_off 40 chat_pool_off 44 chat_pool_len
#     48 vocab_index_off 52 vocab_pool_off 56 vocab_pool_len 60 total_size
#
#  qa_index    : n_qa * u32 q_off, q_len, a_off, a_len   (offsets into qa_pool)
#  qa_w        : n_qa * n_vocab int8
#  qa_pool     : CP866 bytes referenced by qa_index
#  chat_index  : same shape as qa_index
#  chat_w      : n_chat * n_vocab int8
#  chat_pool   : CP866 bytes referenced by chat_index
#  vocab_index : n_vocab * u32 off, u32 len   (offsets into vocab_pool)
#  vocab_pool  : CP866 token bytes, sorted, so the kernel can binary search
#
# Every section starts 16-byte aligned and offsets are absolute file offsets, so
# the kernel validates the whole thing with a bounds check on the total size.

def align16(n):
    return (n + 15) & ~15


def build_blob(qa, chat, vocab, index_of):
    n_qa, n_chat, n_vocab = len(qa), len(chat), len(vocab)

    def pool_of(entries):
        pool = bytearray()
        idx = bytearray()
        for left, right in entries:
            lb, rb = to_cp866(left), to_cp866(right)
            lo = len(pool); pool += lb
            ro = len(pool); pool += rb
            idx += struct.pack("<IIII", lo, len(lb), ro, len(rb))
        return bytes(idx), bytes(pool)

    qa_index, qa_pool = pool_of(qa)
    chat_index, chat_pool = pool_of(chat)

    def weights(entries, qw, aw):
        out = bytearray()
        for left, right in entries:
            vec = vectorise([(tokenize(to_cp866(left)), qw),
                             (tokenize(to_cp866(right)), aw)],
                            index_of, n_vocab)
            out += struct.pack("<%db" % n_vocab, *vec)
        return bytes(out)

    qa_w = weights(qa, QUESTION_WEIGHT, 1.0)
    chat_w = weights(chat, CHAT_WEIGHT, 0.4)

    vocab_pool = bytearray()
    vocab_index = bytearray()
    for tok in vocab:
        off = len(vocab_pool)
        vocab_pool += tok
        vocab_index += struct.pack("<II", off, len(tok))

    sections = (("qa_index", qa_index), ("qa_w", qa_w), ("qa_pool", qa_pool),
                ("chat_index", chat_index), ("chat_w", chat_w),
                ("chat_pool", chat_pool), ("vocab_index", vocab_index),
                ("vocab_pool", bytes(vocab_pool)))

    offset = HEADER_SIZE
    body = bytearray()
    layout = {}
    for name, payload in sections:
        pad = align16(offset) - offset
        body += b"\0" * pad
        offset = align16(offset)
        layout[name] = offset
        body += payload
        offset += len(payload)

    total = offset
    header = struct.pack(
        "<4s" + "I" * 15,
        MAGIC, n_qa, n_chat, n_vocab,
        layout["qa_index"], layout["qa_w"],
        layout["qa_pool"], len(qa_pool),
        layout["chat_index"], layout["chat_w"],
        layout["chat_pool"], len(chat_pool),
        layout["vocab_index"], layout["vocab_pool"], len(vocab_pool),
        total,
    )
    assert len(header) == HEADER_SIZE, len(header)
    blob = header + bytes(body)
    assert len(blob) == total, (len(blob), total)
    return blob, layout, total


# --------------------------------------------------------------------------- #
# evaluation
# --------------------------------------------------------------------------- #

def unpack(vec, n):
    return list(struct.unpack("<%db" % n, vec))


def load_matrix(blob, offset, count, n_vocab):
    return [unpack(blob[offset + i * n_vocab: offset + (i + 1) * n_vocab], n_vocab)
            for i in range(count)]


def evaluate(pairs, matrix, weight, index_of, n_vocab):
    """Self-check: does each held-out left side retrieve its own entry?

    Reports the number of *distinct vocabulary items* the query and the winning
    row have in common, because that, not the dot product, is what tells a real
    match from noise. A one-word overlap is a coincidence even when it scores
    127*127, so the kernel uses this count as its acceptance gate.
    """
    hits, correct, overlap_ok, overlap_bad = 0, [], [], []
    for i, (left, _) in enumerate(pairs):
        qvec = vectorise([(tokenize(to_cp866(left)), weight)], index_of, n_vocab)
        best_j, best_s, best_ov = -1, -10 ** 9, 0
        runner_ov = 0
        for j, row in enumerate(matrix):
            s = sum(a * b for a, b in zip(qvec, row))
            ov = sum(1 for a, b in zip(qvec, row) if a and b)
            if s > best_s:
                runner_ov = best_ov
                best_j, best_s, best_ov = j, s, ov
            elif ov > runner_ov:
                runner_ov = ov
        correct.append(best_s)
        (overlap_ok if best_j == i else overlap_bad).append(best_ov)
        if best_j == i:
            hits += 1
    return hits, correct, overlap_ok, overlap_bad


def report(name, hits, correct, ok, bad):
    n = len(correct)
    print("  %-5s self-check %d/%d (%.1f%%)  score %d..%d  overlap correct %d..%d  wrong max %d"
          % (name, hits, n, 100.0 * hits / n,
             min(correct), max(correct),
             min(ok) if ok else 0, max(ok) if ok else 0,
             max(bad) if bad else 0))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--qa", default="ai/corpus_qa.txt")
    ap.add_argument("--chat", default="ai/corpus_chat.txt")
    ap.add_argument("--out", default="ai/ai_model.bin")
    args = ap.parse_args()

    for path in (args.qa, args.chat):
        if not os.path.isfile(path):
            sys.exit("error: %s not found" % path)

    qa = read_pairs(args.qa)
    chat = read_pairs(args.chat)
    vocab = build_vocab(qa, chat)
    index_of = {tok: i for i, tok in enumerate(vocab)}
    n_vocab = len(vocab)

    n_stops = emit_stopwords(os.path.join(os.path.dirname(args.out), "stopwords.inc"))
    blob, layout, total = build_blob(qa, chat, vocab, index_of)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as fh:
        fh.write(blob)

    print("trained %s" % args.out)
    print("  qa entries : %d" % len(qa))
    print("  chat pairs : %d" % len(chat))
    print("  vocabulary : %d tokens" % n_vocab)
    print("  stop words : %d (-> ai/stopwords.inc)" % n_stops)
    print("  file size  : %d bytes (%.1f KiB)" % (total, total / 1024.0))

    qa_res = evaluate(qa, load_matrix(blob, layout["qa_w"], len(qa), n_vocab),
                      QUESTION_WEIGHT, index_of, n_vocab)
    chat_res = evaluate(chat, load_matrix(blob, layout["chat_w"], len(chat), n_vocab),
                        CHAT_WEIGHT, index_of, n_vocab)
    report("qa", *qa_res)
    report("chat", *chat_res)

    # Off-topic probe: words that appear nowhere in the corpus must produce an
    # empty query vector, and therefore no score at all.
    probe = tokenize(to_cp866("how do i bake bread"))
    known = [t for t in probe if t in index_of]
    print("  probe      : 'how do i bake bread' -> %d tokens, %d in vocabulary"
          % (len(probe), len(known)))
    print("  thresholds : qa overlap>=%d  chat overlap>=%d"
          % (min(qa_res[2]) if qa_res[2] else 1, min(chat_res[2]) if chat_res[2] else 1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
