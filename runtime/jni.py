"""
jni.py -- the JNI bridge, in both directions  (milestone M3)

The library does not only receive JNI calls, it makes them: Java hands it a
JNIEnv*, and emulated code dereferences that to reach a table of function
pointers.  So we build a fake JNIEnv inside guest memory whose 233 slots are
all distinct trap addresses, and service them from a mock JVM here.

The mock models net.int13.HalActivity, whose real interface was read out of
classes.dex, so the calls the engine makes land on methods that actually
existed in the shipped app.
"""
import struct

JNI_DATA = 0x7C000000          # vtable, env struct, string arena
JNI_TRAP = 0x7D000000          # 233 four-byte slots
NSLOTS = 233


def _names():
    n, i = {}, 4
    for x in ["GetVersion", "DefineClass", "FindClass", "FromReflectedMethod",
              "FromReflectedField", "ToReflectedMethod", "GetSuperclass",
              "IsAssignableFrom", "ToReflectedField", "Throw", "ThrowNew",
              "ExceptionOccurred", "ExceptionDescribe", "ExceptionClear",
              "FatalError", "PushLocalFrame", "PopLocalFrame", "NewGlobalRef",
              "DeleteGlobalRef", "DeleteLocalRef", "IsSameObject", "NewLocalRef",
              "EnsureLocalCapacity", "AllocObject", "NewObject", "NewObjectV",
              "NewObjectA", "GetObjectClass", "IsInstanceOf", "GetMethodID"]:
        n[i] = x; i += 1
    for base in ["Call%sMethod", "CallNonvirtual%sMethod"]:
        for ty in ["Object", "Boolean", "Byte", "Char", "Short", "Int", "Long",
                   "Float", "Double", "Void"]:
            for v in ["", "V", "A"]:
                n[i] = (base % ty) + v; i += 1
    n[i] = "GetFieldID"; i += 1
    for pre in ["Get", "Set"]:
        for ty in ["Object", "Boolean", "Byte", "Char", "Short", "Int", "Long",
                   "Float", "Double"]:
            n[i] = "%s%sField" % (pre, ty); i += 1
    n[i] = "GetStaticMethodID"; i += 1
    for ty in ["Object", "Boolean", "Byte", "Char", "Short", "Int", "Long",
               "Float", "Double", "Void"]:
        for v in ["", "V", "A"]:
            n[i] = "CallStatic%sMethod%s" % (ty, v); i += 1
    n[i] = "GetStaticFieldID"; i += 1
    for pre in ["Get", "Set"]:
        for ty in ["Object", "Boolean", "Byte", "Char", "Short", "Int", "Long",
                   "Float", "Double"]:
            n[i] = "%sStatic%sField" % (pre, ty); i += 1
    for x in ["NewString", "GetStringLength", "GetStringChars",
              "ReleaseStringChars", "NewStringUTF", "GetStringUTFLength",
              "GetStringUTFChars", "ReleaseStringUTFChars", "GetArrayLength",
              "NewObjectArray", "GetObjectArrayElement", "SetObjectArrayElement"]:
        n[i] = x; i += 1
    for pre, suf in [("New", "Array"), ("Get", "ArrayElements"),
                     ("Release", "ArrayElements"), ("Get", "ArrayRegion"),
                     ("Set", "ArrayRegion")]:
        for ty in ["Boolean", "Byte", "Char", "Short", "Int", "Long", "Float",
                   "Double"]:
            n[i] = "%s%s%s" % (pre, ty, suf); i += 1
    for x in ["RegisterNatives", "UnregisterNatives", "MonitorEnter",
              "MonitorExit", "GetJavaVM", "GetStringRegion", "GetStringUTFRegion",
              "GetPrimitiveArrayCritical", "ReleasePrimitiveArrayCritical",
              "GetStringCritical", "ReleaseStringCritical", "NewWeakGlobalRef",
              "DeleteWeakGlobalRef", "ExceptionCheck", "NewDirectByteBuffer",
              "GetDirectBufferAddress", "GetDirectBufferCapacity",
              "GetObjectRefType"]:
        n[i] = x; i += 1
    return n


NAMES = _names()


def sig_args(sig):
    """Split a JNI method signature into single-letter argument kinds."""
    out, i = [], sig.index('(') + 1
    while i < len(sig) and sig[i] != ')':
        c = sig[i]
        if c == 'L':
            i = sig.index(';', i) + 1
            out.append('L')
        elif c == '[':
            i += 1
            while i < len(sig) and sig[i] == '[':
                i += 1
            if sig[i] == 'L':
                i = sig.index(';', i) + 1
            else:
                i += 1
            out.append('L')
        else:
            out.append(c)
            i += 1
    return out


class JVM:
    def __init__(self, g, verbose=True):
        self.g = g
        self.verbose = verbose
        self.obj = {}
        self.next_h = 0x2000
        self.unimpl = {}
        self.trace = []
        self.java_calls = []
        self.pending_release = {}
        self.crit_buf = {}
        self.audio = {'samples': 512, 'hz': 22050}
        self._build()
        self._register()

    # ------------------------------------------------------------ plumbing
    def _build(self):
        g = self.g
        g.uc.mem_map(JNI_DATA, 0x20000)
        for i in range(NSLOTS):
            g.uc.mem_write(JNI_DATA + i * 4, struct.pack('<I', JNI_TRAP + i * 4))
        self.env = JNI_DATA + 0x1000                 # JNIEnv* -> *vtable
        g.uc.mem_write(self.env, struct.pack('<I', JNI_DATA))
        # JavaVM, for JNI_OnLoad: JNIInvokeInterface is 8 slots, and we park
        # its traps well past the JNIEnv table so one page serves both.
        vmv = JNI_DATA + 0x800
        for i in range(8):
            g.uc.mem_write(vmv + i * 4, struct.pack('<I', JNI_TRAP + (300 + i) * 4))
        self.javavm = JNI_DATA + 0x1008
        g.uc.mem_write(self.javavm, struct.pack('<I', vmv))
        self.arena = JNI_DATA + 0x2000
        self.arena_end = JNI_DATA + 0x20000
        g.add_trap_region(JNI_TRAP, 0x1000, self._dispatch)

    def alloc(self, n):
        a = self.arena
        self.arena = (self.arena + n + 7) & ~7
        if self.arena >= self.arena_end:
            raise RuntimeError("JNI arena exhausted")
        return a

    def handle(self, obj):
        h = self.next_h
        self.next_h += 8
        self.obj[h] = obj
        return h

    VM_NAMES = {3: 'DestroyJavaVM', 4: 'AttachCurrentThread',
                5: 'DetachCurrentThread', 6: 'GetEnv',
                7: 'AttachCurrentThreadAsDaemon'}

    def _dispatch(self, addr):
        slot = (addr - JNI_TRAP) // 4
        if slot >= 300:
            name = self.VM_NAMES.get(slot - 300, 'vm%d' % (slot - 300))
            self.trace.append('JavaVM.' + name)
            if name in ('GetEnv', 'AttachCurrentThread', 'AttachCurrentThreadAsDaemon'):
                self.g.write(self.a(1), struct.pack('<I', self.env))
                return self.g.ret(0)
            return self.g.ret(0)
        name = NAMES.get(slot, "slot%d" % slot)
        self.trace.append(name)
        fn = getattr(self, "jni_" + name, None)
        if fn is None:
            self.unimpl[name] = self.unimpl.get(name, 0) + 1
            if self.verbose:
                print("      [JNI UNIMPLEMENTED] slot %d  %s" % (slot, name))
            self.g.ret(0)
        else:
            fn()

    # JNI args: arg(0) is JNIEnv*, so real arguments start at 1
    def a(self, i):
        return self.g.arg(i)

    def s(self, i):
        return self.g.cstr(self.g.arg(i))

    # -------------------------------------------------------- mock Java side
    def _register(self):
        """net.int13.HalActivity, as read out of classes.dex."""
        self.activity = self.handle(('object', 'net/int13/HalActivity'))
        self.impl = {
            'canMakePurchase':  lambda *a: 0,          # billing is dead
            'geLang':           lambda *a: self.new_string('en'),
            'initAudio':        self._init_audio,
            'startAudio':       lambda *a: None,
            'pauseAudio':       lambda *a: None,
            'closeAudio':       lambda *a: None,
            'fillAudio':        lambda b: None,
            'initArchive':      lambda *a: self._note('initArchive'),
            'finish':           lambda *a: self._note('finish'),
            'finishActivity':   lambda *a: self._note('finishActivity'),
            'openURL':          lambda s: self._note('openURL'),
            'purchaseItem':     lambda s: self._note('purchaseItem'),
            'restoreItems':     lambda *a: self._note('restoreItems'),
        }

    def _init_audio(self, buffer_samples, hz):
        """HalActivity.initAudio(II)Z is (bufferSize, sampleRateHz) -- in that
        order.  Getting it backwards hands the mixer a buffer of the wrong
        length and it walks off the end of its stream table."""
        self.audio = {'samples': buffer_samples, 'hz': hz}
        self._note('initAudio')
        return 1

    def new_audio_buffer(self, bytes_per_sample=2):
        n = self.audio['samples'] * bytes_per_sample
        return self.new_byte_array(n), self.audio['samples']

    def _note(self, what):
        self.java_calls.append(what)
        return None

    def new_string(self, text):
        return self.handle(('string', text))

    # ------------------------------------------------------------- JNI impls
    def jni_GetVersion(self):           self.g.ret(0x00010006)
    def jni_ExceptionCheck(self):       self.g.ret(0)
    def jni_ExceptionOccurred(self):    self.g.ret(0)
    def jni_ExceptionClear(self):       self.g.ret(0)
    def jni_ExceptionDescribe(self):    self.g.ret(0)
    def jni_DeleteLocalRef(self):       self.g.ret(0)
    def jni_DeleteGlobalRef(self):      self.g.ret(0)
    def jni_EnsureLocalCapacity(self):  self.g.ret(0)
    def jni_PushLocalFrame(self):       self.g.ret(0)
    def jni_PopLocalFrame(self):        self.g.ret(0)
    def jni_MonitorEnter(self):         self.g.ret(0)
    def jni_MonitorExit(self):          self.g.ret(0)

    def jni_NewGlobalRef(self):
        self.g.ret(self.a(1))
    def jni_NewLocalRef(self):
        self.g.ret(self.a(1))
    def jni_NewWeakGlobalRef(self):
        self.g.ret(self.a(1))

    def jni_FindClass(self):
        name = self.s(1)
        self.g.ret(self.handle(('class', name)))

    def jni_GetObjectClass(self):
        o = self.obj.get(self.a(1))
        cls = o[1] if o and o[0] == 'object' else 'java/lang/Object'
        self.g.ret(self.handle(('class', cls)))

    def jni_IsInstanceOf(self):         self.g.ret(1)
    def jni_IsSameObject(self):         self.g.ret(1 if self.a(1) == self.a(2) else 0)

    def jni_GetMethodID(self):
        cls, name, sig = self.obj.get(self.a(1)), self.s(2), self.s(3)
        self.g.ret(self.handle(('method', cls, name, sig)))

    def jni_GetStaticMethodID(self):
        self.jni_GetMethodID()

    def jni_GetFieldID(self):
        cls, name, sig = self.obj.get(self.a(1)), self.s(2), self.s(3)
        self.g.ret(self.handle(('field', cls, name, sig)))

    def jni_GetStaticFieldID(self):
        self.jni_GetFieldID()

    def jni_GetIntField(self):
        obj, fid = self.obj.get(self.a(1)), self.obj.get(self.a(2))
        name = fid[2] if fid else '?'
        val = 0
        if obj and obj[0] == 'fd':
            val = obj[1]                    # java.io.FileDescriptor.descriptor
        self.trace.append("GetIntField:%s=%d" % (name, val))
        self.g.ret(val)

    # ---- strings
    def jni_NewStringUTF(self):
        self.g.ret(self.handle(('string', self.s(1) or '')))

    def jni_NewString(self):
        self.g.ret(self.handle(('string', '')))

    def jni_GetStringUTFChars(self):
        o = self.obj.get(self.a(1))
        text = (o[1] if o and o[0] == 'string' else '')
        b = text.encode('utf-8') + b'\0'
        p = self.alloc(len(b))
        self.g.write(p, b)
        if self.a(2):
            self.g.write(self.a(2), b'\x01\0\0\0')
        self.g.ret(p)

    def jni_ReleaseStringUTFChars(self):  self.g.ret(0)
    def jni_GetStringUTFLength(self):
        o = self.obj.get(self.a(1))
        self.g.ret(len((o[1] if o and o[0] == 'string' else '').encode('utf-8')))
    def jni_GetStringLength(self):        self.jni_GetStringUTFLength()

    # ---- arrays
    def jni_GetArrayLength(self):
        o = self.obj.get(self.a(1))
        self.g.ret(len(o[1]) if o and o[0] == 'array' else 0)

    def jni_GetPrimitiveArrayCritical(self):
        o = self.obj.get(self.a(1))
        if not o or o[0] != 'array':
            return self.g.ret(0)
        # One staging buffer per array, reused: the audio thread asks for the
        # same array every frame, and bump-allocating each time exhausts the
        # arena within a few seconds of playback.
        h = self.a(1)
        p = self.crit_buf.get(h)
        if p is None:
            p = self.crit_buf[h] = self.alloc(len(o[1]))
        self.g.write(p, bytes(o[1]))
        self.pending_release[p] = self.a(1)
        if self.a(2):
            self.g.write(self.a(2), b'\0\0\0\0')
        self.g.ret(p)

    def jni_ReleasePrimitiveArrayCritical(self):
        p = self.a(2)
        h = self.pending_release.pop(p, None)
        if h is not None:
            o = self.obj[h]
            o[1][:] = self.g.read(p, len(o[1]))     # copy back
        self.g.ret(0)

    def new_byte_array(self, n):
        return self.handle(('array', bytearray(n)))

    # ---- calls back into Java
    def _invoke(self, va_index, ret):
        mid = self.obj.get(self.a(2))
        if not mid or mid[0] != 'method':
            return self.g.ret(0)
        _, cls, name, sig = mid
        kinds = sig_args(sig)
        args, p = [], self.a(va_index)
        for k in kinds:
            if k in 'JD':
                p = (p + 7) & ~7
                lo, hi = (struct.unpack('<II', self.g.read(p, 8)))
                args.append(lo | (hi << 32)); p += 8
            else:
                args.append(struct.unpack('<I', self.g.read(p, 4))[0]); p += 4
        self.java_calls.append(name)
        if self.verbose:
            print("      [java] %s%s" % (name, sig))
        fn = self.impl.get(name)
        out = fn(*args) if fn else 0
        self.g.ret(out if isinstance(out, int) else 0)

    def _call_direct(self, ret):
        """Call<Type>Method: variadic args sit in r3 then the stack."""
        mid = self.obj.get(self.a(2))
        if not mid or mid[0] != 'method':
            return self.g.ret(0)
        _, cls, name, sig = mid
        kinds = sig_args(sig)
        args, i = [], 3
        for k in kinds:
            if k in 'JD':
                i = i + (i % 2)
                args.append(self.g.arg(i) | (self.g.arg(i + 1) << 32)); i += 2
            else:
                args.append(self.g.arg(i)); i += 1
        self.java_calls.append(name)
        if self.verbose:
            print("      [java] %s%s" % (name, sig))
        fn = self.impl.get(name)
        out = fn(*args) if fn else 0
        self.g.ret(out if isinstance(out, int) else 0)


# generate Call<Type>Method / ...V / ...A for every return type
def _mk(kind, variant):
    if variant == 'V':
        return lambda self: self._invoke(3, kind)
    return lambda self: self._call_direct(kind)


for _ty in ["Object", "Boolean", "Byte", "Char", "Short", "Int", "Long",
            "Float", "Double", "Void"]:
    for _v in ["", "V", "A"]:
        for _pre in ["Call%sMethod", "CallStatic%sMethod", "CallNonvirtual%sMethod"]:
            setattr(JVM, "jni_" + (_pre % _ty) + _v, _mk(_ty, _v))
