#!/usr/bin/env python3
# Minimal wasm name-section resolver: map a function index -> name (the just-in-time symbolizer).
import sys
def readu(b,o):
    r=s=0
    while True:
        x=b[o]; o+=1; r|=(x&0x7f)<<s
        if not x&0x80: return r,o
        s+=7
f=open(sys.argv[1],'rb').read()
assert f[:4]==b'\0asm'
o=8; names={}
while o<len(f):
    sid=f[o]; o+=1
    sz,o=readu(f,o); end=o+sz
    if sid==0:  # custom section
        nl,o2=readu(f,o); nm=f[o2:o2+nl]; o2+=nl
        if nm==b'name':
            p=o2
            while p<end:
                sub=f[p]; p+=1
                ssz,p=readu(f,p); se=p+ssz
                if sub==1:  # function names
                    cnt,p=readu(f,p)
                    for _ in range(cnt):
                        idx,p=readu(f,p); l,p=readu(f,p)
                        names[idx]=f[p:p+l].decode('utf8','replace'); p+=l
                p=se
    o=end
for q in sys.argv[2:]:
    i=int(q); print(f"function[{i}] = {names.get(i,'<no name / fpcast thunk>')}")
