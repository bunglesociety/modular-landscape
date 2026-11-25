# Modular Landscape 42 (from 41): C5 (the retuned exports), so the keyboard sounds to its top (AR_HI 72).
# Modular Landscape 41 (from 40): Alexis's retuned exports (pitch shifted, speed kept) as a third source: more
# notes of their own (C2, A2, A3), more takes; a stroke <= 1.85 s, the loop's second <= 1.0 s (the flash).
# Modular Landscape 40 (from 39): each zone's exact period (Q8, its own samples) for the wheel's comb (AR_PER).
# Modular Landscape 39 (from 38): played once, a note ends with its own stroke, faded over 0.6 s (AR_OS_END).
# Modular Landscape 38 (from 37): the loop over two strokes, the end's fade 0.3 s.
# Modular Landscape 37 (from 36, its sound kept): every stroke can hold (a crossfaded loop, TENUE / PEDALE).
# Modular Landscape 36 (from 35, Alexis: it has a soul; now playable): each stroke only its own note, starting at
# once, one bow stroke, 1.1 .. 2.4 s.
# Modular Landscape 35: a new instrument from Alexis's own bowed guitar takes (24 Nov 2026), "notes v2" (D minor,
# standard tuning) and "notes medium aigu" (open G). Not made to sound like 29 (other recording, other instrument):
# the notes are made to sound together. Light treatment only: the stroke cut where it starts, a 35 Hz high-pass,
# no low-pass, no timbre matching, no attack lifting; one sample rate for all (22.05 kHz: the bow's air kept in the
# bass too); strokes as long as played (up to 3 s, so LENGTH has room); per note the stroke that is long, clean and
# gentle, then the one whose brightness and attack sit with its neighbours; the levels evened across the keyboard.
#   python3 tools/archet/v42/build_archet42.py PIECE.wav [PIECE2.wav ...]
import soundfile as sf, numpy as np, librosa, json, sys, os
from scipy.signal import butter, sosfiltfilt
here=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(here,'..','..'))
from sampleio import ima_encode, IMA_STEP, IMA_IDX
SR=48000
def ima_decode(b,n):
    p,i,o=0,0,[]
    for k in range(n):
        c=(b[k>>1]>>(4*(k&1)))&15; s=IMA_STEP[i]; vd=s>>3
        if c&4: vd+=s
        if c&2: vd+=s>>1
        if c&1: vd+=s>>2
        p=max(-32768,min(32767,p-vd if c&8 else p+vd)); i=max(0,min(88,i+IMA_IDX[c&7])); o.append(p)
    return np.array(o,float)
def hnr(seg,root,sr):
    from scipy.fft import next_fast_len; N=next_fast_len(len(seg),real=True)   # (35: a prime length made each FFT crawl)
    S=np.abs(np.fft.rfft(seg*np.hanning(len(seg)),N))**2; f=np.fft.rfftfreq(N,1/sr)
    f0=440*2**((root-69)/12); harm=np.zeros_like(S,bool)
    for k in range(1,int(min(sr/2,12000)/f0)): harm|=np.abs(f-k*f0)<f0*0.06
    band=f<12000; return 10*np.log10(S[harm&band].sum()/S[~harm&band].sum())
# the held notes of every piece
import pickle
CACHE=os.path.join(here,'cands.pkl')
if os.path.exists(CACHE): cands,Y=pickle.load(open(CACHE,'rb'))
else:
    cands={}
    for src in sys.argv[1:]:
        y,sr=sf.read(src); y=y.mean(1) if y.ndim>1 else y; assert sr==SR
        x=librosa.resample(y,orig_sr=sr,target_sr=22050).astype(np.float32)
        f0,vf,_=librosa.pyin(x,fmin=60,fmax=1000,sr=22050,frame_length=2048,hop_length=256)
        hop=256/22050; m=np.where(vf,69+12*np.log2(np.nan_to_num(f0,nan=1)/440),np.nan); i=0
        while i<len(m):
            if np.isnan(m[i]): i+=1; continue
            j=i; ref=m[i]; vals=[]
            while j<len(m) and not np.isnan(m[j]) and abs(m[j]-round(ref))<0.45: vals.append(m[j]); j+=1
            if (j-i)*hop>=1.0:
                a,b=i*hop,j*hop; md=float(np.median(vals)); seg=y[int((a+.2)*SR):int(min(b,a+2.2)*SR)]
                if len(seg)>SR*.6:
                    cands.setdefault(int(round(md)),[]).append(dict(y=y,t=a,len=b-a,midi=md,hnr=hnr(seg,md,SR),src=os.path.basename(src)))
            i=max(j,i+1)
    Y={}
    for L in cands.values():
        for d in L: Y[d['src']]=d['y']; d['y']=d['src']
    pickle.dump((cands,Y),open(CACHE,'wb'))
for L in cands.values():
    for d in L: d['y']=Y[d['y']] if isinstance(d['y'],str) else d['y']
print('pitch analysis done',flush=True)
TR=22050
def env(x,w): return np.sqrt(np.convolve(x*x,np.ones(w)/w,'same')+1e-12)
def aw(f):
    f2=f*f; return 12194**2*f2*f2/((f2+20.6**2)*np.sqrt((f2+107.7**2)*(f2+737.9**2))*(f2+12194**2))*1.2589
def loud(b):            # loudness of a body, dB (ITU-R BS.1770 K-weighting: how loud notes sound next to each other;
                        # 35's first try with A-weighting made the bass 17 dB hotter than the top)
    from scipy.signal import lfilter
    import math
    f0,G,Q=1681.974450955533,3.999843853973347,0.7071752369554196; K=math.tan(math.pi*f0/TR); Vh=10**(G/20); Vb=Vh**0.4996667741545416
    a0=1+K/Q+K*K; b1=[(Vh+Vb*K/Q+K*K)/a0,2*(K*K-Vh)/a0,(Vh-Vb*K/Q+K*K)/a0]; a1=[1,2*(K*K-1)/a0,(1-K/Q+K*K)/a0]
    f0,Q=38.13547087602444,0.5003270373238773; K=math.tan(math.pi*f0/TR)
    a2=[1,2*(K*K-1)/(1+K/Q+K*K),(1-K/Q+K*K)/(1+K/Q+K*K)]
    y=lfilter([1,-2,1],a2,lfilter(b1,a1,b)); return 10*np.log10(np.mean(y*y)+1e-12)
def feel(x):            # attack peak over body (dB), energy over 1.5 kHz (dB)
    b=x[int(.35*TR):int(1.1*TR)]; S=np.abs(np.fft.rfft(b))**2; f=np.fft.rfftfreq(len(b),1/TR)
    e=env(x,int(.01*TR)); return 20*np.log10(e[:int(.4*TR)].max()/np.median(e[int(.5*TR):int(1.3*TR)])), 10*np.log10(S[f>1500].sum()/S.sum())
HP=butter(2,35,'highpass',fs=TR,output='sos')
def pitch_ok(x,midi):   # (36: the whole stroke on its note: no note under it first, none after)
    f0,vf,_=librosa.pyin(x.astype(np.float32)/32768,fmin=60,fmax=1000,sr=TR,frame_length=2048,hop_length=256)
    m=69+12*np.log2(np.nan_to_num(f0,nan=1)/440); v=vf&(np.abs(m-midi)<3)
    if v.sum()<10: return False
    off=np.abs(m-midi)[vf]
    return np.mean(off<0.4)>=0.92 and np.all(np.abs(m[:8]-midi)[vf[:8]]<0.6)
def stroke(d):
    # 36 (Alexis on 35: notes late, some doubled, some play the note under then mine, or the other way):
    # only the stretch where the pitch is the note (the pitch tracker's run), less 20 ms each side; the start where
    # the stroke reaches half its body (no swell to wait through: the engine's ATTACK makes it softer when wanted);
    # the end before any second bow stroke (a dip then a rise); 1.1 .. 2.4 s.
    a=d['t']+0.02; b=d['t']+d['len']-0.12
    seg=d['y'][int(a*SR):int(b*SR)]
    x=librosa.resample(seg,orig_sr=SR,target_sr=TR,res_type='soxr_vhq')*32768
    x=sosfiltfilt(HP,x)
    e=env(x,int(.01*TR)); body=np.median(e[int(.3*TR):int(min(1.0,len(x)/TR)*TR)])
    on=int(np.argmax(e[:int(.5*TR)]>body*0.5)); st=max(0,on-int(.006*TR))
    x=x[st:]
    # (a second stroke: the level falls under 0.3 x what the note held since 0.3 s, then comes back over 0.7 x;
    #  the attack's own peak not counted, a bass take bites 8..10 dB over its body)
    es=env(x,int(.04*TR)); i0=int(.3*TR); run=np.maximum.accumulate(np.concatenate([np.zeros(i0),es[i0:]])); cut=len(x)
    for i in range(i0,len(x)):
        if es[i]<0.3*run[i] and np.max(es[i:])>0.7*run[i]: cut=i; break
    x=x[:min(cut,int(2.4*TR))].copy()
    if len(x)<1.1*TR: return x[:0]
    e=env(x,int(.01*TR)); body=np.median(e[int(.3*TR):int(min(1.1,len(x)/TR-.2)*TR)]); tt=np.arange(len(x))/TR
    g=np.minimum(1,1.2*body/e); g=np.where(tt<0.5,g,1); g=np.where((tt>=0.4)&(tt<0.5),g+(1-g)*(tt-0.4)/0.1,g)
    sm=int(.02*TR); g=np.convolve(np.pad(g,sm,mode='edge'),np.ones(sm)/sm,'same')[sm:-sm]; x=x*np.minimum(g,1)
    fi=int(.005*TR); x[:fi]*=np.sin(np.linspace(0,np.pi/2,fi))**2
    fo=int(.15*TR); x[-fo:]*=0.5+0.5*np.cos(np.linspace(0,np.pi,fo))
    if not pitch_ok(x,d['midi']): return x[:0]
    return x

def make_loop(x):
    # 37 (TENUE, pad 9; the PEDALE, pad 12): a loop in every stroke, played only when the part's LOOP is on (as
    # held, then REL). From 0.45 s (after the bite) to just before the end's fade; its end moved within 40 ms to
    # where the wave lines up best with the start's; its level evened (within +-6 dB, over 80 ms; from where the crossfade takes) so that the loop does
    # not pump; its last stretch (up to 0.2 s) crossfaded, equal power, into what comes before the start: the jump
    # from the end back to the start is seamless. Played once (LOOP off), the stroke is as 36's, but for this.
    n=len(x); ls=int(.45*TR); end=n-int(.02*TR)-int(TF*TR)-int(.01*TR)
    W=int(.02*TR); best,le=-2,end
    ref=x[ls-W:ls]
    for c in range(end-int(.04*TR),end):
        seg=x[c-W:c]; k=np.dot(seg,ref)/(np.linalg.norm(seg)*np.linalg.norm(ref)+1e-9)
        if k>best: best,le=k,c
    X=min(int(.3*TR),(le-ls)//2,ls-int(.1*TR)); a0=ls-X  # (the crossfade takes from before the start: evened too)
    # (38: Alexis on 37's TENUE: the bow's mechanics heard going round: evened harder, +-8 dB over 50 ms)
    e=env(x,int(.05*TR)); m=np.median(e[ls:le]); g=np.ones(n)
    g[a0:le]=np.clip(m/e[a0:le],10**(-8/20),10**(8/20))
    r=int(.05*TR); w=np.ones(n); w[a0-r:a0]=np.linspace(0,1,r); w[:a0-r]=0; w[le:le+r]=np.linspace(1,0,r)[:len(w[le:le+r])]; w[le+r:]=0
    sm=int(.05*TR); g=1+(np.convolve(np.pad(g,sm,mode='edge'),np.ones(sm)/sm,'same')[sm:-sm]-1)*w
    x*=g
    t=np.linspace(0,np.pi/2,X)
    x[le-X:le]=x[le-X:le]*np.cos(t)+x[ls-X:ls]*np.sin(t)
    return ls,le-1
# every clean held note, treated
pool={}
for n,L in cands.items():
    for d in L:
        if d['hnr']<13 or d['len']<1.2: continue
        x=stroke(d); 
        if len(x)<1.1*TR: continue
        d['x']=x; d['dur']=len(x)/TR; d['pk'],d['br']=feel(x); d['ld']=loud(x[int(.35*TR):int(1.1*TR)])
        pool.setdefault(n,[]).append(d)
print('pool',{n:len(v) for n,v in pool.items()},flush=True)
for n in sorted(pool): print('POOL',n,[(round(d['dur'],1),round(d['hnr']),round(d['pk'],1),round(d['br'],1)) for d in pool[n]])
ANCH=[n for n in [36,38,41,43,45,48,50,52,55,57,60,62,64,67,72] if n in pool]   # (41: C2, A2, A3 from the retuned exports; 42: C5, the keyboard to its top)
# 41: the notes 36..40 played kept their strokes (Alexis: it has a soul): those come from his own played pieces still; the
# retuned exports give the notes those lacked (C2, A2, A3), and second strokes for the loops
OWN={38,41,43,48,50,55,60,62,67}
def mine(n): return [d for d in pool[n] if 'pitch' not in d['src']] if n in OWN and any('pitch' not in d['src'] for d in pool[n]) else pool[n]
# first pass: long, clean, gentle
def score(d): return d['hnr']+5*min(d['dur'],2.0)-1.5*max(0,d['pk']-10)
first={n:max(mine(n),key=score) for n in ANCH}
# second pass: brightness and attack along a smooth line through the keyboard, among the good ones of each note
FIT=[n for n in ANCH if n<70]   # (42: the line and the picks as 41's: C5 added on top, kept out of them)
P=np.array(FIT,float); trend=np.polyfit(P,[first[n]['br'] for n in FIT],1); pkm=np.median([first[n]['pk'] for n in FIT])
pick=dict(first)
for it in range(3):     # (brightness along a line through the picks, refitted twice: one odd note does not bend it)
    trend=np.polyfit(P,[pick[n]['br'] for n in FIT],1)
    for n in ANCH:
        best=score(first[n]); good=[d for d in mine(n) if score(d)>=best-8]
        pick[n]=min(good,key=lambda d:abs(d['br']-np.polyval(trend,n))+0.5*max(0,d['pk']-pkm))
for n in ANCH: d=pick[n]; print('PICK',n,round(d['dur'],2),round(d['hnr'],1),round(d['pk'],1),round(d['br'],1),round(float(np.polyval(trend,n)),1),'pkm',round(pkm,1),flush=True)
# drop notes that stick out (shorter than 1.3 s next to longer neighbours, or far off the line)
ANCH=[n for n in ANCH if n>=70 or abs(pick[n]['br']-np.polyval(trend,n))<=5.0]
# levels: as loud as each other (A-weighted body), a gentle slope down to the top as heard on a bowed string
LT=np.median([pick[n]['ld'] for n in ANCH])
os.makedirs(os.path.join(here,'zones'),exist_ok=True); blob=bytearray(); zones=[]; R={}
for k,n in enumerate(ANCH):
    lo=0 if k==0 else R[ANCH[k-1]][1]+1; hi=127 if k==len(ANCH)-1 else n+(ANCH[k+1]-n-1)//2; R[n]=(lo,hi)
gain0=None
TF=0.3                  # (38: the end fades over 0.3 s, not 0.15: Alexis on 37, notes cut short at their end)
def body(d,n,a=.4):     # a stroke's middle (after its bite, before its fade), at note n's pitch, at the set level
    x=d['x'][int(a*TR):len(d['x'])-int(.16*TR)]*10**((LT-d['ld'])/20)
    if abs(d['midi']-pick[n]['midi'])>0.02:
        x=librosa.resample(x,orig_sr=TR*2**((pick[n]['midi']-d['midi'])/12),target_sr=TR,res_type='soxr_hq')
    return x
for n in ANCH:
    # 38: the loop runs over two strokes, so TENUE does not go round one bow's mechanics: the stroke chosen, then a
    # second take of that note (else one within 2 semitones, retuned), crossfaded over 0.25 s, then the end's fade
    d=pick[n]; x=d['x'][:min(len(d['x'])-int(.16*TR),int((1.5 if n>=70 else 1.85)*TR))]*10**((LT-d['ld'])/20)   # (all as loud as each other; 41: <= 1.85 s, the memory; 42: C5 1.5 s)
    os_end=0                                     # (39: played once, the note ends with its own stroke: archet_hdr AR_OS_END)
    others=[o for q in sorted(pool,key=lambda q:abs(q-n)) if abs(q-n)<=2 for o in pool[q] if o is not d and o['dur']>=1.1]
    if others:
        o=min(others,key=lambda o:(abs(o['midi']-d['midi'])>0.5)*3+abs(o['br']-d['br'])/3+max(0,o['pk']-pkm)/4-min(o['dur'],2.4)/2)
        b=body(o,n)[:int((0.6 if n>=70 else 1.0)*TR)]; eb=np.median(env(b,int(.05*TR))); ea=np.median(env(x[int(.4*TR):],int(.05*TR))); b=b*ea/eb
        XF=int(.25*TR); t=np.linspace(0,np.pi/2,XF)
        os_end=len(x)-XF//2
        x=np.concatenate([x[:-XF],x[-XF:]*np.cos(t)+b[:XF]*np.sin(t),b[XF:]])
        print(f"     {librosa.midi_to_note(n)}: then {o['src'][18:30]} at {o['t']:.1f} s (midi {o['midi']:.2f})",flush=True)
    fo=int(TF*TR); x=x.copy(); x[-fo:]*=0.5+0.5*np.cos(np.linspace(0,np.pi,fo))
    zx=np.concatenate([x,np.zeros(int(.02*TR))]); zones.append([n,zx,d,os_end])
pk=max(np.max(np.abs(z[1])) for z in zones); G=min(1,26000/pk)*10**(-0/20)
# overall level: the median body as 29's median body (the engine's mix and reverb are tuned for that loudness)
old=json.load(open(os.path.join(here,'..','archet_v29.json'))); ob=open(os.path.join(here,'..','archet_v29.bin'),'rb').read()
ref=np.median([np.sqrt(np.mean(ima_decode(ob[z[0]:],z[1])[int(.3*z[4]/65536*44100):int(1.2*z[4]/65536*44100)]**2)) for z in old['zones']])
cur=np.median([np.sqrt(np.mean(z[1][int(.3*TR):int(1.2*TR)]**2)) for z in zones]); G=min(ref/cur,30000/pk)
out=[]
for n,zx,d,os_end in zones:
    zx=zx*G; ls,le=make_loop(zx)
    s=np.clip(np.round(zx),-32768,32767).astype(int)
    nm=librosa.midi_to_note(n).replace('♯','#'); sf.write(os.path.join(here,'zones',nm+'.wav'),s/32768,TR,subtype='PCM_16')
    adp,(p,i)=ima_encode([int(v) for v in s],ls); off=len(blob); blob+=adp
    out.append([off,len(s),ls,le,int(round(TR/44100*65536)),int(round(d['midi']*16)),p,i,R[n][0],R[n][1],1,os_end,int(round(TR/(440*2**((d['midi']-69)/12))*256))])
    print(f"     loop {ls/TR:4.2f} .. {le/TR:4.2f} s")
    ee=env(s.astype(float),int(.01*TR)); att=np.argmax(ee>0.5*np.median(ee[int(.3*TR):int(1.0*TR)]))/TR*1000
    print(f"{nm:4} speaks in {att:3.0f} ms  {d['src'][18:30]:12} at {d['t']:6.1f} s  root {d['midi']:6.2f}  note/noise {d['hnr']:4.1f} dB  attack/body {d['pk']:4.1f} dB  bright {d['br']:6.1f} (line {np.polyval(trend,n):6.1f})  keys {R[n][0]:3}-{R[n][1]:3}  {len(s)/TR:4.2f} s")
drone=[]
for dd,src in zip(old.get('drone',[]),(40,43)):
    q=min(ANCH,key=lambda a:abs(a-src)); drone.append(out[ANCH.index(q)][:8]+dd[8:11]+out[ANCH.index(q)][11:13])
json.dump(dict(zones=out,drone=drone),open(os.path.join(here,'..','archet.json'),'w'))
open(os.path.join(here,'..','archet.bin'),'wb').write(blob)
print('ADPCM total',len(blob),'B (room: ~290000)')
