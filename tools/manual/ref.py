import re,os,json,glob
T=os.environ['TEMP']+'/manual'
notes={'Comp':['comp'],'DS-Mini':['ds-mini'],'DS':['ds'],'Delay':['delay'],'EQ':['eq','phase2_1-D'],'Exciter':['exciter'],'FIR':['fir','phase2_1-A'],'Gate':['gate'],'Kicker':['kicker','kicker-sub'],'Limit':['limit'],'MultiComp':['multicomp','phase2_1-U'],'Room':['room','BUILD_PROMPT_PHASE3_NEW_PLUGINS'],'Sature':['sature'],'Verb':['verb'],'TS':['ts'],'Chorus':['chorus']}
dirs={'Comp':'comp','DS-Mini':'dsmini','DS':'ds','Delay':'delay','EQ':'eq','Exciter':'exciter','FIR':'fir','Gate':'gate','Kicker':'kicker','Limit':'limit','MultiComp':'multicomp','Room':'room','Sature':'sature','Verb':'verb','TS':'ts','Chorus':'chorus'}
strip=lambda s: re.sub(r'^(band|tap) (n|\d+) ','',s.lower())
def fb(k):
    out=[]
    for l in open(f"{T}/params/Fancy {k}.txt",encoding='utf-8',errors='ignore'):
        mt=re.match(r'#\s*\d+\s+"([^"]+)"\s+(.*?)\s*$',l)
        if mt:
            full=mt.group(1); n=re.sub(r'^(Tap|Band) \d+ ','',full)
            d=re.sub(r'\s+\[[^\]]*\]$','',mt.group(2))
            if not n.startswith('MIDI CC') and n not in [o[0] for o in out]: out.append((n,d,full!=n))
    return out
def rows(note):
    byname,byid={},{}
    for l in open(f"docs/{note}.md",encoding='utf-8',errors='replace'):
        c=[x.strip() for x in l.strip().strip('|').split('|')]
        if len(c)>=5 and c[0].startswith('`'):
            byname.setdefault(strip(c[1]),c)
            byid.setdefault(re.sub(r'^b?N|N','',c[0].strip('`')).lower(),c)
    return byname,byid
rx=re.compile(r'"([A-Za-z0-9_]+)"\s*(?:,\s*\d+\s*\})?\s*,\s*(?:prefix\s*\+\s*|tapPrefix\s*\+\s*|bandName\s*\+\s*)?"([^"]{2,60})"')
allref={}
for k,ns in notes.items():
    byname,byid={},{}
    for n in ns:
        a,b=rows(n); byname.update(a); byid.update(b)
    src="".join(open(f,encoding='utf-8',errors='ignore').read() for f in glob.glob(f"plugins/{dirs[k]}/src/*Processor.cpp"))
    n2id={strip(n):i.lower() for i,n in rx.findall(src)}
    items=[]; miss=[]
    for n,d,rep in fb(k):
        row=byname.get(strip(n)) or byid.get(n2id.get(strip(n),'#'))
        if not row: miss.append(n)
        items.append({'name':n,'default':d,'repeat':rep,'range':row[2] if row else None,'unit':row[4] if row and len(row)>4 else ''})
    allref[k]=items
    print(k,len(items),'missing',miss)
json.dump(allref,open(f"{T}/ref.json",'w',encoding='utf-8'),indent=1,ensure_ascii=False)
