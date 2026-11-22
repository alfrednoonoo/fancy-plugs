import re,os,json
T=os.environ['TEMP']+'/manual'
mm={'Comp':'comp','DS-Mini':'ds-mini','DS':'ds','Delay':'delay','EQ':'eq','Exciter':'exciter','FIR':'fir','Gate':'gate','Kicker':'kicker','Limit':'limit','MultiComp':'multicomp','Room':'room','Sature':'sature','Verb':'verb','TS':'ts','Chorus':'chorus'}
r=json.load(open(f"{T}/ref.json",encoding='utf-8'))
out={}
for k,m in mm.items():
    tab={}
    for l in open(f"docs/{m}-manual.md",encoding='utf-8'):
        c=[x.strip() for x in l.strip().strip('|').split('|')]
        if l.startswith('|') and len(c)>=2 and not set(c[0])<=set('-: '):
            key=re.sub(r'\*|`','',c[0]).strip().lower()
            tab.setdefault(key,c[-1])
    miss=[]
    for i in r[k]:
        d=tab.get(i['name'].lower()) or tab.get(re.sub(r'^(resonance|gate|compressor|subtract|duck|mod|sc|up) ','',i['name'].lower()))
        i['desc']=d
        if not d: miss.append(i['name'])
    out[k]=miss
    print(k,len(r[k]),'need',len(miss),miss)
json.dump(r,open(f"{T}/ref2.json",'w',encoding='utf-8'),indent=1,ensure_ascii=False)
