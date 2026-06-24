#!/usr/bin/env python3
# Build wasm function-INDEX -> C-name from a `wasm-ld -Map` file (DWARF-independent: uses the linker's
# own symbol table). The map lists CODE symbols in code order with file offsets; pairing them with the
# code section's function-body offsets (same binary) yields index->name. fpcast-emu preserves original
# indices (appends thunks), so this map names deployed wasm-function[N] for N < n_original.
import sys, re, json
def readu(b,o):
    r=s=0
    while True:
        x=b[o]; o+=1; r|=(x&0x7f)<<s
        if not x&0x80: return r,o
        s+=7
def code_func_offsets(path):
    """Return (imported_func_count, [(index, entry_file_offset)]) where entry_file_offset is the
    file offset of each function's SIZE-LEB (i.e. the start of its code entry)."""
    f=open(path,'rb').read(); o=8; imp=0; ents=[]
    while o<len(f):
        sid=f[o]; o+=1; sz,o2=readu(f,o)
        if sid==2:
            cnt,p=readu(f,o2)
            for _ in range(cnt):
                ml,p=readu(f,p); p+=ml; nl,p=readu(f,p); p+=nl; k=f[p]; p+=1
                if k==0: imp+=1; _,p=readu(f,p)
                elif k==1: p+=1; lim=f[p]; p+=1; _,p=readu(f,p); 
                elif k==2: lim=f[p]; p+=1; _,p=readu(f,p); 
                elif k==3: p+=2
        elif sid==10:
            cnt,p=readu(f,o2)
            for i in range(cnt):
                ents.append((imp+i, p))     # p = start of size-LEB for this entry
                bsz,p=readu(f,p); p+=bsz
        o=o2+sz
    return imp, ents
def map_symbols(mappath):
    """Parse CODE-section function symbols -> {file_offset: name}. The map's CODE symbol lines look like
       '       -    15138       4b    <path>:(_start)' followed by an indented '   _start' alias line.
    We take the primary line (has Off + Size + ':(name)')."""
    syms={}
    in_code=False
    for ln in open(mappath, errors='replace'):
        m=re.match(r'\s*(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+(\S.*)$', ln)
        if not m: continue
        addr,off,size,rest=m.group(1),m.group(2),m.group(3),m.group(4)
        if rest.strip()=='CODE': in_code=True; continue
        if rest.strip() in ('DATA','CUSTOM','DATACOUNT'): in_code=False
        if not in_code: continue
        mm=re.search(r':\(([^)]+)\)\s*$', rest)
        if mm:
            syms[int(off,16)]=mm.group(1)
    return syms
def main():
    wasm, mapf = sys.argv[1], sys.argv[2]
    imp, ents = code_func_offsets(wasm)
    syms = map_symbols(mapf)
    # the map's function 'Off' is the entry offset (size-LEB). ents entry offset == same. join by offset.
    idx2name={}
    for idx, off in ents:
        if off in syms: idx2name[idx]=syms[off]
    json.dump(idx2name, open(wasm+".mapnames.json","w"))
    print(f"matched {len(idx2name)}/{len(ents)} defined funcs (imp={imp}) -> {wasm}.mapnames.json")
    if len(sys.argv)>3:
        for q in sys.argv[3:]:
            print(f"  function[{q}] = {idx2name.get(int(q),'<thunk/unmatched>')}")
if __name__=="__main__": main()
