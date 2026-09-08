#!/usr/bin/env python3
"""Independent verification of the rebuilt APK -- parses the finished file
from scratch and re-derives every claim rather than trusting the builder."""
import struct, zipfile, hashlib, base64, io, os
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

_HERE = os.path.dirname(os.path.abspath(__file__))
_OUT = os.environ.get("SHOGUN_OUT", os.path.join(_HERE, "out"))
APK = os.environ.get("SHOGUN_PATCHED_APK",
                     os.path.join(_OUT, "Shogun-1.2.4-patched-armeabi-v7a-signed.apk"))
ORIG = os.environ.get("SHOGUN_APK",
                      os.path.join(_HERE, os.pardir, "Shogun-1.2.4.apk"))
CHUNK = 1048576
MAGIC = b"APK Sig Block 42"
ok = lambda m: print("   [PASS] " + m)
bad = lambda m: (print("   [FAIL] " + m), FAILS.append(m))
FAILS = []

d = open(APK, 'rb').read()
print("=" * 74); print("A. ZIP STRUCTURE"); print("=" * 74)
z = zipfile.ZipFile(APK)
names = z.namelist()
print("   entries: %d" % len(names))
if z.testzip() is None: ok("every entry's CRC checks out")
else: bad("CRC mismatch in %s" % z.testzip())
if "lib/armeabi-v7a/libHAL.Android.so" in names: ok("native lib present under lib/armeabi-v7a/")
else: bad("native lib missing from armeabi-v7a")
if "lib/armeabi/libHAL.Android.so" not in names: ok("obsolete lib/armeabi/ path removed")
else: bad("old armeabi path still present")

info = z.getinfo("res/raw/data.mp3")
if info.compress_type == zipfile.ZIP_STORED: ok("data.mp3 still STORED (mmap-able via openRawResourceFd)")
else: bad("data.mp3 got compressed - openRawResourceFd would fail")
hdr = info.header_offset
nlen, elen = struct.unpack_from('<HH', d, hdr + 26)
data_at = hdr + 30 + nlen + elen
print("   data.mp3 payload begins at byte %d" % data_at)
if data_at % 4 == 0: ok("data.mp3 is 4-byte aligned")
else: bad("data.mp3 misaligned (%d %% 4 = %d)" % (data_at, data_at % 4))

print(); print("=" * 74); print("B. ASSET + LIBRARY INTEGRITY vs ORIGINAL"); print("=" * 74)
zo = zipfile.ZipFile(ORIG)
a, b = zo.read("res/raw/data.mp3"), z.read("res/raw/data.mp3")
if a == b: ok("data.mp3 byte-identical to original (%d bytes)" % len(b))
else: bad("data.mp3 differs from original")
if zo.read("AndroidManifest.xml") == z.read("AndroidManifest.xml"): ok("AndroidManifest.xml untouched")
else: bad("manifest changed unexpectedly")
if zo.read("classes.dex") == z.read("classes.dex"): ok("classes.dex untouched")
else: bad("dex changed unexpectedly")
so_old, so_new = zo.read("lib/armeabi/libHAL.Android.so"), z.read("lib/armeabi-v7a/libHAL.Android.so")
if len(so_old) == len(so_new): ok("library size unchanged (%d bytes)" % len(so_new))
else: bad("library size changed")
diff = [i for i in range(len(so_old)) if so_old[i] != so_new[i]]
print("   bytes changed in library: %d  at offsets %s" % (len(diff), [hex(x) for x in diff]))
if len(diff) == 8: ok("exactly 8 bytes changed (two 4-byte stubs)")
else: bad("unexpected number of modified bytes")

print(); print("=" * 74); print("C. THE PATCH, DISASSEMBLED FROM THE SHIPPED FILE"); print("=" * 74)
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
for name, off in [("HAL_CanMakePurchases", 0x5e23c), ("HAL_PurchaseItem", 0x5e158)]:
    ins = list(md.disasm(so_new[off:off + 4], off))
    txt = " ; ".join("%s %s" % (i.mnemonic, i.op_str) for i in ins)
    print("   %-22s @0x%x -> %s" % (name, off, txt))
    if txt == "movs r0, #0 ; bx lr": ok("%s returns 0 (billing reported unavailable)" % name)
    else: bad("%s not patched as intended" % name)

print(); print("=" * 74); print("D. JAR v1 SIGNATURE"); print("=" * 74)
man = z.read("META-INF/MANIFEST.MF").decode()
secs = {}
for blk in man.split("\r\n\r\n"):
    if blk.startswith("Name: "):
        n = blk.split("\r\n")[0][6:]
        dg = [l for l in blk.split("\r\n") if l.startswith("SHA-256-Digest:")][0].split(": ")[1]
        secs[n] = dg
missing = [n for n in names if not n.startswith("META-INF/") and n not in secs]
if not missing: ok("manifest covers all %d non-signature entries" % len(secs))
else: bad("manifest missing entries: %s" % missing)
mism = [n for n, dg in secs.items()
        if base64.b64encode(hashlib.sha256(z.read(n)).digest()).decode() != dg]
if not mism: ok("every SHA-256-Digest in MANIFEST.MF matches the entry")
else: bad("digest mismatch: %s" % mism)
sf = z.read("META-INF/CERT.SF").decode()
want = [l for l in sf.split("\r\n") if l.startswith("SHA-256-Digest-Manifest:")][0].split(": ")[1]
got = base64.b64encode(hashlib.sha256(z.read("META-INF/MANIFEST.MF")).digest()).decode()
if want == got: ok("CERT.SF manifest digest matches MANIFEST.MF")
else: bad("CERT.SF manifest digest wrong")

print(); print("=" * 74); print("E. APK SIGNATURE SCHEME v2 (verified as Android would)"); print("=" * 74)
eocd = d.rfind(b'PK\x05\x06')
cd_off, cd_size = struct.unpack_from('<I', d, eocd + 16)[0], struct.unpack_from('<I', d, eocd + 12)[0]
if d[cd_off - 16:cd_off] == MAGIC: ok("APK Signing Block magic found immediately before central directory")
else: bad("no signing block magic")
size_end = struct.unpack_from('<Q', d, cd_off - 24)[0]
blk_start = cd_off - 8 - size_end
size_start = struct.unpack_from('<Q', d, blk_start)[0]
if size_start == size_end: ok("signing block size fields agree (%d bytes)" % size_end)
else: bad("signing block size fields disagree")

p, found = blk_start + 8, None
while p < cd_off - 24:
    plen = struct.unpack_from('<Q', d, p)[0]
    pid = struct.unpack_from('<I', d, p + 8)[0]
    if pid == 0x7109871a: found = d[p + 12:p + 8 + plen]
    p += 8 + plen
if found: ok("v2 signature pair (id 0x7109871a) present")
else: bad("no v2 pair")

def rd(buf, o):
    n = struct.unpack_from('<I', buf, o)[0]
    return buf[o + 4:o + 4 + n], o + 4 + n
signers, _ = rd(found, 0)
signer, _ = rd(signers, 0)
signed_data, o = rd(signer, 0)
signatures, o = rd(signer, o)
pubkey, o = rd(signer, o)
digests, q = rd(signed_data, 0)
certs, q = rd(signed_data, q)
dg_entry, _ = rd(digests, 0)
algo = struct.unpack_from('<I', dg_entry, 0)[0]
claimed = dg_entry[8:]
cert_der, _ = rd(certs, 0)

eocd_d = bytearray(d[eocd:])
struct.pack_into('<I', eocd_d, 16, blk_start)
chunks = []
for sec in (d[:blk_start], d[cd_off:cd_off + cd_size], bytes(eocd_d)):
    for i in range(0, len(sec), CHUNK):
        chunks.append(sec[i:i + CHUNK])
cat = b''.join(hashlib.sha256(b'\xa5' + struct.pack('<I', len(c)) + c).digest() for c in chunks)
computed = hashlib.sha256(b'\x5a' + struct.pack('<I', len(chunks)) + cat).digest()
print("   %d chunks, algo 0x%04x (RSASSA-PKCS1-v1_5 SHA-256)" % (len(chunks), algo))
if computed == claimed: ok("recomputed content digest matches the signed digest")
else: bad("content digest MISMATCH - APK would be rejected")

cert = x509.load_der_x509_certificate(cert_der)
sig_entry, _ = rd(signatures, 0)
try:
    cert.public_key().verify(sig_entry[8:], signed_data, padding.PKCS1v15(), hashes.SHA256())
    ok("RSA signature over signed-data verifies against the embedded certificate")
except Exception as e:
    bad("signature verification failed: %s" % e)
if cert.public_key().public_bytes(serialization.Encoding.DER,
        serialization.PublicFormat.SubjectPublicKeyInfo) == pubkey:
    ok("embedded public key matches the certificate")
else:
    bad("public key / certificate mismatch")
print("   certificate subject: %s" % cert.subject.rfc4514_string())

print(); print("=" * 74)
print("RESULT: %s" % ("ALL CHECKS PASSED" if not FAILS else "%d FAILURE(S): %s" % (len(FAILS), FAILS)))
print("        output: %s (%d bytes)" % (os.path.basename(APK), len(d)))
print("=" * 74)
