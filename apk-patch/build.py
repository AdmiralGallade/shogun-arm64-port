#!/usr/bin/env python3
"""
Shogun v1.2.4 -- repackage + billing-harden + sign.

Produces an APK that:
  * declares the native library under lib/armeabi-v7a/ (the binary really is
    ARMv7-A; the original 'armeabi' folder was mislabelled and modern ABI
    lists no longer contain bare 'armeabi'),
  * has the dead Google Play Billing v1 path neutralised in native code,
  * is signed with a fresh key using JAR v1 + APK Signature Scheme v2.

No JDK / Android SDK required: zip alignment, v1 signing and v2 signing are
implemented here directly.
"""
import struct, zlib, zipfile, hashlib, base64, os, sys, shutil

from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, padding
import datetime

# Supply your own copy of the game: pass its path as argv[1], or set $SHOGUN_APK.
_HERE = os.path.dirname(os.path.abspath(__file__))
SRC = (sys.argv[1] if len(sys.argv) > 1 else
       os.environ.get("SHOGUN_APK",
                      os.path.join(_HERE, os.pardir, "Shogun-1.2.4.apk")))
OUTDIR = os.environ.get("SHOGUN_OUT", os.path.join(_HERE, "out"))
os.makedirs(OUTDIR, exist_ok=True)
OUT = os.path.join(OUTDIR, "Shogun-1.2.4-patched-armeabi-v7a-signed.apk")
KEYOUT = os.path.join(OUTDIR, "shogun-resign-key.pem")

OLD_LIB = "lib/armeabi/libHAL.Android.so"
NEW_LIB = "lib/armeabi-v7a/libHAL.Android.so"

# ---------------------------------------------------------------- ELF helpers
class Elf:
    def __init__(self, blob):
        self.d = bytearray(blob)
        u32 = lambda o: struct.unpack_from('<I', self.d, o)[0]
        u16 = lambda o: struct.unpack_from('<H', self.d, o)[0]
        phoff, phent, phnum = u32(28), u16(42), u16(44)
        self.loads = []
        for i in range(phnum):
            o = phoff + i * phent
            if u32(o) == 1:
                self.loads.append((u32(o + 8), u32(o + 4), u32(o + 16)))
        shoff, shnum, shent, shstr = u32(32), u16(48), u16(46), u16(50)
        sh = lambda i: dict(nameoff=u32(shoff + i * shent),
                            off=u32(shoff + i * shent + 16),
                            size=u32(shoff + i * shent + 20))
        base = sh(shstr)['off']
        self.S = {}
        for i in range(shnum):
            s = sh(i)
            e = self.d.index(b'\x00', base + s['nameoff'])
            self.S[bytes(self.d[base + s['nameoff']:e]).decode()] = s
        sy, st = self.S['.symtab'], self.S['.strtab']['off']
        self.sym = {}
        for o in range(sy['off'], sy['off'] + sy['size'], 16):
            nmo = u32(o)
            if nmo == 0 or (self.d[o + 12] & 0xf) != 2:
                continue
            e = self.d.index(b'\x00', st + nmo)
            self.sym[bytes(self.d[st + nmo:e]).decode('utf-8', 'replace')] = (u32(o + 4), u32(o + 8))

    def v2f(self, va):
        for vaddr, off, fsz in self.loads:
            if vaddr <= va < vaddr + fsz:
                return off + (va - vaddr)
        raise ValueError("vaddr 0x%x not mapped" % va)


# Return-0 stubs.  Thumb: movs r0,#0 ; bx lr      ARM: mov r0,#0 ; bx lr
STUB0_THUMB = bytes.fromhex('00207047')
STUB0_ARM   = bytes.fromhex('000000e31eff2fe1')

# Functions that reach out to the (long dead) Play Billing v1 service through
# JNI.  Forcing them to report "no store available" takes the game down the
# code path it already uses on any device without Market installed.
PATCHES = [
    ("HAL_CanMakePurchases", "store never reported as available"),
    ("HAL_PurchaseItem",     "purchase attempt becomes a no-op"),
]


def patch_library(blob):
    elf = Elf(blob)
    report = []
    for name, why in PATCHES:
        if name not in elf.sym:
            report.append((name, None, None, "SYMBOL ABSENT - skipped"))
            continue
        val, size = elf.sym[name]
        thumb = val & 1
        off = elf.v2f(val & ~1)
        stub = STUB0_THUMB if thumb else STUB0_ARM
        if size < len(stub):
            report.append((name, off, size, "TOO SMALL - skipped"))
            continue
        before = bytes(elf.d[off:off + len(stub)])
        elf.d[off:off + len(stub)] = stub
        report.append((name, off, size,
                       "%s  %s -> %s   (%s)" % ("THUMB" if thumb else "ARM",
                                                before.hex(), stub.hex(), why)))
    return bytes(elf.d), report


# ---------------------------------------------------------------- zip writing
def dos_time(dt):
    y, mo, d, h, mi, s = dt
    return ((h << 11) | (mi << 5) | (s // 2), ((y - 1980) << 9) | (mo << 5) | d)


class ZipBuilder:
    """Minimal ZIP writer with 4-byte alignment for STORED entries."""
    def __init__(self):
        self.buf = bytearray()
        self.central = []

    def add(self, name, data, method, date_time):
        nb = name.encode()
        if method == zipfile.ZIP_DEFLATED:
            co = zlib.compressobj(9, zlib.DEFLATED, -15)
            payload = co.compress(data) + co.flush()
        else:
            payload = data
        crc = zlib.crc32(data) & 0xffffffff
        t, d = dos_time(date_time)
        extra = b''
        if method == zipfile.ZIP_STORED:
            # pad the extra field so the payload starts on a 4-byte boundary
            head = 30 + len(nb)
            pad = (-(len(self.buf) + head)) % 4
            extra = b'\x00' * pad
        off = len(self.buf)
        self.buf += struct.pack('<IHHHHHIIIHH', 0x04034b50, 20, 0, method, t, d,
                                crc, len(payload), len(data), len(nb), len(extra))
        self.buf += nb + extra + payload
        self.central.append(
            struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 20, 20, 0, method, t, d,
                        crc, len(payload), len(data), len(nb), 0, 0, 0, 0, 0, off) + nb)

    def finish(self):
        cd_off = len(self.buf)
        cd = b''.join(self.central)
        eocd = struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, len(self.central),
                           len(self.central), len(cd), cd_off, 0)
        return bytes(self.buf), cd, eocd, cd_off


# ---------------------------------------------------------------- v1 signing
def b64(x):
    return base64.b64encode(x).decode()


def v1_sign(entries, cert, key):
    """entries: list of (name, data). Returns [(name, bytes), ...] to prepend."""
    man = "Manifest-Version: 1.0\r\nCreated-By: 1.0 (Android)\r\n\r\n"
    sections = {}
    for name, data in entries:
        sec = "Name: %s\r\nSHA-256-Digest: %s\r\n\r\n" % (
            name, b64(hashlib.sha256(data).digest()))
        sections[name] = sec
        man += sec
    manb = man.encode()

    sf = ("Signature-Version: 1.0\r\nCreated-By: 1.0 (Android)\r\n"
          "SHA-256-Digest-Manifest: %s\r\n\r\n" % b64(hashlib.sha256(manb).digest()))
    for name, _ in entries:
        sf += "Name: %s\r\nSHA-256-Digest: %s\r\n\r\n" % (
            name, b64(hashlib.sha256(sections[name].encode()).digest()))
    sfb = sf.encode()

    from cryptography.hazmat.primitives.serialization import pkcs7
    p7 = (pkcs7.PKCS7SignatureBuilder()
          .set_data(sfb)
          .add_signer(cert, key, hashes.SHA256())
          .sign(serialization.Encoding.DER,
                [pkcs7.PKCS7Options.DetachedSignature,
                 pkcs7.PKCS7Options.NoAttributes,
                 pkcs7.PKCS7Options.Binary]))
    return [("META-INF/MANIFEST.MF", manb),
            ("META-INF/CERT.SF", sfb),
            ("META-INF/CERT.RSA", p7)]


# ---------------------------------------------------------------- v2 signing
CHUNK = 1048576
SIG_ALGO_RSA_PKCS1_SHA256 = 0x0103
MAGIC = b"APK Sig Block 42"


def lp(b):
    return struct.pack('<I', len(b)) + b


def seq(items):
    return b''.join(lp(i) for i in items)


def chunk_digest(sections):
    chunks = []
    for s in sections:
        for i in range(0, len(s), CHUNK):
            chunks.append(s[i:i + CHUNK])
    digests = b''
    for c in chunks:
        h = hashlib.sha256()
        h.update(b'\xa5' + struct.pack('<I', len(c)) + c)
        digests += h.digest()
    top = hashlib.sha256()
    top.update(b'\x5a' + struct.pack('<I', len(chunks)) + digests)
    return top.digest()


def v2_sign(body, cd, eocd, cd_off, cert, key):
    # For digest purposes the EOCD's central-directory offset points at the
    # start of the signing block, which is exactly where the body ends.
    eocd_d = bytearray(eocd)
    struct.pack_into('<I', eocd_d, 16, cd_off)
    digest = chunk_digest([body, cd, bytes(eocd_d)])

    cert_der = cert.public_bytes(serialization.Encoding.DER)
    pub_der = cert.public_key().public_bytes(
        serialization.Encoding.DER,
        serialization.PublicFormat.SubjectPublicKeyInfo)

    digests = seq([struct.pack('<I', SIG_ALGO_RSA_PKCS1_SHA256) + lp(digest)])
    certs = seq([cert_der])
    signed_data = lp(digests) + lp(certs) + lp(b'')

    sig = key.sign(signed_data, padding.PKCS1v15(), hashes.SHA256())
    signatures = seq([struct.pack('<I', SIG_ALGO_RSA_PKCS1_SHA256) + lp(sig)])

    signer = lp(signed_data) + lp(signatures) + lp(pub_der)
    v2_value = lp(seq([signer]))

    pair = struct.pack('<Q', 4 + len(v2_value)) + struct.pack('<I', 0x7109871a) + v2_value
    total = len(pair) + 8 + len(MAGIC)
    block = struct.pack('<Q', total) + pair + struct.pack('<Q', total) + MAGIC

    eocd_final = bytearray(eocd)
    struct.pack_into('<I', eocd_final, 16, cd_off + len(block))
    return body + block + cd + bytes(eocd_final), len(block)


# ---------------------------------------------------------------- main
def main():
    os.makedirs(OUTDIR, exist_ok=True)
    zin = zipfile.ZipFile(SRC)

    print("=" * 74)
    print("STEP 1  patch native library")
    print("=" * 74)
    entries = []
    patch_report = None
    for info in zin.infolist():
        if info.filename.startswith("META-INF/"):
            continue                      # old signature is being replaced
        data = zin.read(info.filename)
        name = info.filename
        if name == OLD_LIB:
            data, patch_report = patch_library(data)
            name = NEW_LIB
        entries.append((name, data, info.compress_type, info.date_time))

    for n, off, size, msg in patch_report:
        print("   %-24s @0x%-7s %s" % (n, ("%x" % off) if off else "-", msg))

    print()
    print("=" * 74)
    print("STEP 2  generate signing key")
    print("=" * 74)
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    now = datetime.datetime(2024, 1, 1, tzinfo=datetime.timezone.utc)
    subject = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, u"Shogun Personal Rebuild"),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, u"Personal Archive"),
    ])
    cert = (x509.CertificateBuilder()
            .subject_name(subject).issuer_name(subject)
            .public_key(key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now)
            .not_valid_after(now + datetime.timedelta(days=365 * 30))
            .sign(key, hashes.SHA256()))
    with open(KEYOUT, "wb") as f:
        f.write(key.private_bytes(serialization.Encoding.PEM,
                                  serialization.PrivateFormat.TraditionalOpenSSL,
                                  serialization.NoEncryption()))
        f.write(cert.public_bytes(serialization.Encoding.PEM))
    print("   RSA-2048, self-signed, valid 30 years")
    print("   key + cert -> %s" % KEYOUT)

    print()
    print("=" * 74)
    print("STEP 3  JAR v1 signature")
    print("=" * 74)
    sigfiles = v1_sign([(n, d) for n, d, _, _ in entries], cert, key)
    for n, d in sigfiles:
        print("   %-24s %d bytes" % (n, len(d)))

    print()
    print("=" * 74)
    print("STEP 4  assemble + align zip")
    print("=" * 74)
    zb = ZipBuilder()
    for n, d in sigfiles:
        zb.add(n, d, zipfile.ZIP_DEFLATED, (2024, 1, 1, 0, 0, 0))
    for n, d, m, dt in entries:
        zb.add(n, d, m, dt)
    body, cd, eocd, cd_off = zb.finish()
    print("   %d entries, body %d bytes, central dir %d bytes" % (len(zb.central), len(body), len(cd)))

    print()
    print("=" * 74)
    print("STEP 5  APK Signature Scheme v2")
    print("=" * 74)
    final, blocklen = v2_sign(body, cd, eocd, cd_off, cert, key)
    with open(OUT, "wb") as f:
        f.write(final)
    print("   signing block %d bytes inserted at offset %d" % (blocklen, cd_off))
    print("   wrote %s (%d bytes)" % (OUT, len(final)))
    zin.close()


if __name__ == "__main__":
    main()
