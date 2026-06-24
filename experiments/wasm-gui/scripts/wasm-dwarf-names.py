#!/usr/bin/env python3
# Build a wasm function-INDEX -> C-name map by symbolizing each function's first instruction's
# FILE OFFSET against the module's own DWARF (llvm-symbolizer). This is the offline symbolizer for
# fpcast'd modules where lld emits no `name` section: run the SAME DWARF-carrying .wasm in V8 --prof
# (so func indices match), then map v8prof-top.py's wasm-function[N] -> real name here. ~ addr2line.
import sys, subprocess, json
SYM = "/home/linuxbrew/.linuxbrew/Cellar/llvm@21/21.1.8/bin/llvm-symbolizer"
def readu(b,o):
    r=s=0
    while True:
        x=b[o]; o+=1; r|=(x&0x7f)<<s
        if not x&0x80: return r,o
        s+=7
def first_instr_offsets(path):
    f=open(path,'rb').read(); o=8
    imp_funcs=0
    starts=[]  # (func_index, file_offset_of_first_instruction)
    while o<len(f):
        sid=f[o]; o+=1; sz,o2=readu(f,o); end=o2+sz
        if sid==2:  # import section -> count imported funcs (they consume low indices)
            cnt,p=readu(f,o2)
            for _ in range(cnt):
                ml,p=readu(f,p); p+=ml
                nl,p=readu(f,p); p+=nl
                kind=f[p]; p+=1
                if kind==0: imp_funcs+=1; _,p=readu(f,p)
                elif kind==1: p+=1; mx=f[p]; p+=1; _,p=readu(f,p); 
                elif kind==2: fl=f[p]; p+=1; _,p=readu(f,p); 
                elif kind==3: p+=1; p+=1
                # NB: table/mem/global decoding approximate; we only need func count which precedes them typically
        elif sid==10:  # code
            cnt,p=readu(f,o2)
            for i in range(cnt):
                bsz,p=readu(f,p); body=p; bend=p+bsz
                # body: locals: vec( (count:u32, valtype:byte) )
                nl2,q=readu(f,body)
                for _ in range(nl2):
                    _,q=readu(f,q); q+=1  # local count + valtype
                # q now at first instruction
                starts.append((imp_funcs+i, q))
                p=bend
        o=end
    return starts
def main():
    path=sys.argv[1]
    want=set(int(x) for x in sys.argv[2:]) if len(sys.argv)>2 else None
    starts=first_instr_offsets(path)
    sel=[(idx,off) for idx,off in starts if (want is None or idx in want)]
    inp="\n".join(hex(off) for _,off in sel)
    r=subprocess.run([SYM,f"--obj={path}"],input=inp,capture_output=True,text=True)
    blocks=r.stdout.split("\n\n")
    out={}
    for (idx,off),blk in zip(sel,blocks):
        lines=[l for l in blk.splitlines() if l.strip()]
        name=lines[0] if lines else "??"
        src=lines[1] if len(lines)>1 else ""
        out[idx]=(name,src)
        if want is not None:
            print(f"function[{idx}] = {name}   ({src})")
    if want is None:
        json.dump({str(k):v for k,v in out.items()}, open(sys.argv[1]+".names.json","w"))
        named=sum(1 for n,_ in out.values() if n not in("??",""))
        print(f"resolved {named}/{len(out)} function names -> {path}.names.json")
if __name__=="__main__": main()
