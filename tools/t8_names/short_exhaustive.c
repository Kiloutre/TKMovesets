/* Complete search of Kamui names of length 5..9 (the 5..12 branch of the hash), without any prefix.

   For 5 <= n <= 12 the hash uses three 4-byte words of the name:

     A = bytes 0..3            h1 = 5*(rol(5n ^ mix(n + A), 13) - S)
     E = bytes n-4..n-1        h2 = 5*(rol(h1 ^ mix(5n + E), 13) - S)
     M = bytes 0..3 (n < 8)    km = C2*rol(K - M1*M, 15)
         bytes 4..7 (n >= 8)   out = fmix(5*(rol(h2 ^ km, 13) - S))

   Every step is invertible, so any one word follows from the other two and the hash:

     n = 5, 6, 7   M is A. Enumerate A (4 bytes), derive E; E overlaps A in 8-n bytes, which must agree.
     n = 8         M is E (bytes 4..7). Enumerate those 4 bytes, derive A.
     n = 9         M = bytes 4..7, E = bytes 5..8. Enumerate bytes 4..8 (5 bytes), derive A.

   So every name of these lengths over the alphabet that has a given hash is found: the search is
   complete, and what limits it is only the number of chance matches (|alphabet|^n / 2^32 per hash:
   0.2, 15, 917, 57,800 and 3.6 million for n = 5..9).

   The filter: a character 4-gram model of whole names (costs in eighth-bits, read from a table written
   by short_exhaustive_run.py). A name is kept when its total cost is at most the cutoff. The enumerated
   bytes are pruned with an exact lower bound of that cost, so the list is complete WITHIN the cutoff:
   every name of that length with cost <= cutoff and the right hash is printed.

   For n = 9 one limit on the whole cost prunes almost nothing (992 million endings times every target),
   so two limits are used instead: one on the cost of the first four characters and one on the cost of
   everything after them. The second one is known while the endings are enumerated, up to an exact lower
   bound for the three characters whose left context is still unknown, and cuts the enumeration.

   usage: short_exhaustive lm.bin alphabet targets.txt n cutoff [start_cutoff ending_cutoff]
   targets: one hex hash per line. A negative cutoff means "no limit".
   prints "HASH<TAB>name<TAB>cost"; statistics as JSON on stderr. Every name is re-hashed with the reference function. */
#define main original_main
#include "kamui_search.c"
#undef main
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

static double now(void){
#ifdef _WIN32
    LARGE_INTEGER a,b;QueryPerformanceCounter(&a);QueryPerformanceFrequency(&b);return (double)a.QuadPart/b.QuadPart;
#else
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+1e-9*t.tv_nsec;
#endif
}
static uint32_t inv(uint32_t x){uint32_t y=x;for(int i=0;i<5;i++)y*=2-x*y;return y;}
static uint32_t unxor(uint32_t x,int k){uint32_t y=x;for(int j=k;j<32;j+=k)y^=x>>j;return y;}
static uint32_t IC1,IC2,I5,IM1;
static inline uint32_t unmix(uint32_t k){return ror(k*IC2,15)*IC1;}
static uint32_t before_final(uint32_t out){
    uint32_t r=unxor(out,16)*inv(FMIX2);
    uint32_t f=unxor(r,13)*inv(FMIX1);
    uint32_t t=unxor(f,16);
    return ror(t*inv(5)+MIX_SUB,13);
}

#define INF 1000000
static uint16_t*lm;static int NA,NS;static uint8_t code[256],allowed[256];static const char*abc;
static inline int c4(int a,int b,int c,int x){return lm[((((size_t)a*NS)+b)*NS+c)*NS+x];}   /* x: 0 = end, 1..NA = character */
static int namecost(const uint8_t*s,int n){
    int a=0,b=0,c=0,total=0;
    for(int i=0;i<n;i++){int x=code[s[i]];total+=c4(a,b,c,x);a=b;b=c;c=x;}
    return total+c4(a,b,c,0);
}
static uint32_t*T,*P;static int nt;
static unsigned long long enumerated,derived,alphabet_hits,emitted;
static int n,cutoff;

static void emit(const uint8_t*name,int j,int cost){
    if(kamui(name,n)!=T[j]){fprintf(stderr,"internal verification failed\n");exit(7);}
    printf("%08X\t%.*s\t%d\n",T[j],n,(const char*)name,cost);emitted++;
}

int main(int argc,char**argv){
    if(argc!=6&&argc!=8){fprintf(stderr,"usage: short_exhaustive lm.bin alphabet targets.txt n cutoff [start_cutoff ending_cutoff]\n");return 1;}
    IC1=inv(C1);IC2=inv(C2);I5=inv(5);IM1=inv(MIX_MUL1);
    abc=argv[2];NA=strlen(abc);NS=NA+1;if(NA<1||NA>63)return 3;
    for(int i=0;i<NA;i++){code[(uint8_t)abc[i]]=i+1;allowed[(uint8_t)abc[i]]=1;}
    size_t cells=(size_t)NS*NS*NS*NS;lm=malloc(cells*sizeof(uint16_t));if(!lm)return 4;
    FILE*f=fopen(argv[1],"rb");if(!f||fread(lm,sizeof(uint16_t),cells,f)!=cells)return 4;fclose(f);
    f=fopen(argv[3],"r");if(!f)return 4;int cap=0;unsigned v;
    while(fscanf(f,"%x",&v)==1){if(nt==cap){cap=cap?cap*2:1024;T=realloc(T,cap*sizeof(uint32_t));P=realloc(P,cap*sizeof(uint32_t));}T[nt]=v;P[nt++]=before_final(v);}
    fclose(f);
    n=atoi(argv[4]);cutoff=atoi(argv[5]);if(n<5||n>9)return 5;
    int bound=cutoff<0?INF:cutoff;
    /* n >= 8 only: separate limits for the cost of the first four characters and for the cost of everything after them */
    int cutA=argc==8?atoi(argv[6]):INF,cutW=argc==8?atoi(argv[7]):INF;
    if(cutA<0)cutA=INF;
    if(cutW<0)cutW=INF;
    uint32_t*buf=malloc((nt+16)*sizeof(uint32_t)),*fl=calloc(nt+16,sizeof(uint32_t));      /* fl: padded with zeros to a multiple of 16 */
    double t0=now();uint8_t name[16];
    const uint32_t n5=5u*(uint32_t)n;
    if(n<=7){
        int shared=8-n;uint32_t lowmask=shared==4?0xFFFFFFFFu:((1u<<(8*shared))-1);int sh=8*(4-shared);
        for(int a0=0;a0<NA;a0++){int x0=a0+1,k0=c4(0,0,0,x0);if(k0>bound)continue;
        for(int a1=0;a1<NA;a1++){int x1=a1+1,k1=k0+c4(0,0,x0,x1);if(k1>bound)continue;
        for(int a2=0;a2<NA;a2++){int x2=a2+1,k2=k1+c4(0,x0,x1,x2);if(k2>bound)continue;
        for(int a3=0;a3<NA;a3++){int x3=a3+1,k3=k2+c4(x0,x1,x2,x3);if(k3>bound)continue;
            name[0]=abc[a0];name[1]=abc[a1];name[2]=abc[a2];name[3]=abc[a3];
            uint32_t A=rd(name,0);enumerated++;
            uint32_t h1=5*(rol(n5^mix((uint32_t)n+A),13)-MIX_SUB);
            uint32_t km=C2*rol(0x318F97D9u-MIX_MUL1*A,15);
            uint32_t want=A>>sh;
            for(int j=0;j<nt;j++){uint32_t h2=P[j]^km;uint32_t X=ror(h2*I5+MIX_SUB,13);uint32_t E=unmix(X^h1)-n5;buf[j]=E;fl[j]=(E&lowmask)==want;}
            derived+=nt;
            for(int j0=0;j0<nt;j0+=16){
              uint32_t any=0;for(int q=0;q<16;q++)any|=fl[j0+q];
              if(!any)continue;
              for(int j=j0;j<j0+16&&j<nt;j++){
                uint32_t E=buf[j];
                if(!fl[j])continue;
                uint8_t e[4];memcpy(e,&E,4);int ok=1;
                for(int k=shared;k<4;k++)if(!allowed[e[k]]){ok=0;break;}
                if(!ok)continue;
                memcpy(name+n-4,e,4);alphabet_hits++;
                int cost=namecost(name,n);
                if(cost<=bound)emit(name,j,cost);
                name[3]=abc[a3];          /* restored: for n = 7 the copy rewrote byte 3 with the same value; keep it explicit */
              }
            }
        }}}}
    }else{
        /* lower bounds for a character whose left context is partly unknown */
        int*lb3=malloc(sizeof(int)*NS*NS*NS),*lb2=malloc(sizeof(int)*NS*NS),*lb1=malloc(sizeof(int)*NS);
        for(int x=1;x<NS;x++)for(int y=1;y<NS;y++)for(int z=1;z<NS;z++){int m=INF;for(int c=1;c<NS;c++){int k=c4(c,x,y,z);if(k<m)m=k;}lb3[(x*NS+y)*NS+z]=m;}
        for(int y=1;y<NS;y++)for(int z=1;z<NS;z++){int m=INF;for(int x=1;x<NS;x++){int k=lb3[(x*NS+y)*NS+z];if(k<m)m=k;}lb2[y*NS+z]=m;}
        for(int z=1;z<NS;z++){int m=INF;for(int y=1;y<NS;y++){int k=lb2[y*NS+z];if(k<m)m=k;}lb1[z]=m;}
        int minA=INF;
        for(int x0=1;x0<NS;x0++){int k0=c4(0,0,0,x0);for(int x1=1;x1<NS;x1++){int k1=k0+c4(0,0,x0,x1);for(int x2=1;x2<NS;x2++){int k2=k1+c4(0,x0,x1,x2);
            for(int x3=1;x3<NS;x3++){int k3=k2+c4(x0,x1,x2,x3);if(k3<minA)minA=k3;}}}}
        int last=n-1;      /* bytes 4..last are enumerated */
        /* k: lower bound of the whole cost; k - minA: lower bound of the cost after the first four characters */
        for(int a4=0;a4<NA;a4++){int x4=a4+1,k4=minA+lb1[x4];if(k4>bound||k4-minA>cutW)continue;
        for(int a5=0;a5<NA;a5++){int x5=a5+1,k5=k4+lb2[x4*NS+x5];if(k5>bound||k5-minA>cutW)continue;
        for(int a6=0;a6<NA;a6++){int x6=a6+1,k6=k5+lb3[(x4*NS+x5)*NS+x6];if(k6>bound||k6-minA>cutW)continue;
        for(int a7=0;a7<NA;a7++){int x7=a7+1,k7=k6+c4(x4,x5,x6,x7);if(k7>bound||k7-minA>cutW)continue;
        for(int a8=0;a8<(last==8?NA:1);a8++){
            int x8=a8+1,k8;
            if(last==8){k8=k7+c4(x5,x6,x7,x8)+c4(x6,x7,x8,0);}else{k8=k7+c4(x5,x6,x7,0);}
            if(k8>bound||k8-minA>cutW)continue;
            name[4]=abc[a4];name[5]=abc[a5];name[6]=abc[a6];name[7]=abc[a7];if(last==8)name[8]=abc[a8];
            enumerated++;
            uint32_t M=rd(name,4),E=rd(name,n-4);
            uint32_t km=C2*rol(0x318F97D9u-MIX_MUL1*M,15),ke=mix(n5+E);
            /* every byte of a name is in 0x30..0x7A: a branch-free test first, so that the loop vectorises and the rare survivors are found in blocks */
            for(int j=0;j<nt;j++){uint32_t h2=P[j]^km;uint32_t X=ror(h2*I5+MIX_SUB,13);uint32_t h1=X^ke;uint32_t Y=ror(h1*I5+MIX_SUB,13)^n5;uint32_t A=unmix(Y)-(uint32_t)n;buf[j]=A;
                fl[j]=((((A+0x05050505u)|A)&0x80808080u)==0)&((((A|0x80808080u)-0x30303030u)&0x80808080u)==0x80808080u);}
            derived+=nt;
            for(int j0=0;j0<nt;j0+=16){
              uint32_t any=0;for(int q=0;q<16;q++)any|=fl[j0+q];
              if(!any)continue;
              for(int j=j0;j<j0+16&&j<nt;j++){
                if(!fl[j])continue;
                uint32_t A=buf[j];
                uint8_t a[4];memcpy(a,&A,4);
                if(!(allowed[a[0]]&&allowed[a[1]]&&allowed[a[2]]&&allowed[a[3]]))continue;
                memcpy(name,a,4);alphabet_hits++;
                int cost=namecost(name,n);
                int y0=code[a[0]],y1=code[a[1]],y2=code[a[2]],y3=code[a[3]];
                int first=c4(0,0,0,y0)+c4(0,0,y0,y1)+c4(0,y0,y1,y2)+c4(y0,y1,y2,y3);
                if(cost<=bound&&first<=cutA&&cost-first<=cutW)emit(name,j,cost);
              }
            }
        }}}}}
    }
    fprintf(stderr,"{\"n\":%d,\"cutoff\":%d,\"targets\":%d,\"enumerated\":%llu,\"derived\":%llu,\"alphabet_level_matches\":%llu,\"hits\":%llu,\"seconds\":%.3f}\n",
            n,cutoff,nt,enumerated,derived,alphabet_hits,emitted,now()-t0);
    return 0;
}
