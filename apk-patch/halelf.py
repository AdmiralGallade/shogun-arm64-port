import struct
from capstone import *
class HAL:
    def __init__(self,path):
        self.d=open(path,'rb').read(); d=self.d
        self.u32=lambda o:struct.unpack_from('<I',d,o)[0]
        self.u16=lambda o:struct.unpack_from('<H',d,o)[0]
        u32,u16=self.u32,self.u16
        # program headers -> vaddr map
        phoff=u32(28);phent=u16(42);phnum=u16(44)
        self.loads=[]
        for i in range(phnum):
            o=phoff+i*phent
            if u32(o)==1: self.loads.append((u32(o+8),u32(o+4),u32(o+16)))  # vaddr,off,filesz
        # sections
        shoff=u32(32);shnum=u16(48);shent=u16(46);shstr=u16(50)
        def sh(i):
            o=shoff+i*shent
            return dict(nameoff=u32(o),off=u32(o+16),size=u32(o+20),addr=u32(o+12))
        base=sh(shstr)['off']; self.S={}
        for i in range(shnum):
            s=sh(i);e=d.index(b'\x00',base+s['nameoff']);self.S[d[base+s['nameoff']:e].decode()]=s
        # symtab
        sy=self.S['.symtab']; st=self.S['.strtab']['off']
        self.sym={}
        for o in range(sy['off'],sy['off']+sy['size'],16):
            nmo=u32(o)
            if nmo==0: continue
            e=d.index(b'\x00',st+nmo); n=d[st+nmo:e].decode('utf-8','replace')
            if (d[o+12]&0xf)==2: self.sym[n]=(u32(o+4),u32(o+8))   # value,size
    def v2f(self,va):
        for vaddr,off,fsz in self.loads:
            if vaddr<=va<vaddr+fsz: return off+(va-vaddr)
        return None
    def dis(self,name,limit=40):
        if name not in self.sym: return None
        val,size=self.sym[name]
        thumb=val&1; addr=val&~1
        off=self.v2f(addr)
        md=Cs(CS_ARCH_ARM, CS_MODE_THUMB if thumb else CS_MODE_ARM)
        md.detail=False
        code=self.d[off:off+size]
        out=[]
        for i,ins in enumerate(md.disasm(code,addr)):
            if i>=limit: out.append("      ... (%d bytes total)"%size); break
            out.append("   %08x  %-12s %s %s"%(ins.address,ins.bytes.hex(),ins.mnemonic,ins.op_str))
        return dict(va=addr,thumb=bool(thumb),size=size,off=off,text="\n".join(out))
