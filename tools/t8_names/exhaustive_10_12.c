/* Complete search of Kamui names of one length within a character-model cost limit, without any prefix.

   There are 63^n / 2^32 names of length n per hash (2.3e8, 1.45e10, 9.1e11 for n = 10, 11, 12). A character
   4-gram model of whole names (table written by exhaustive_10_12_run.py, costs in eighth-bits) gives every
   name a cost. This tool visits EVERY name of length n whose cost is at most `limit`, hashes it and looks
   the hash up among the targets. So the search is complete within the limit: no prefix, no owner, no
   ranked list, and no dependence on the number of targets (real hashes and decoys cost the same).

   The walk is left to right with exact pruning: a partial name is dropped when its cost plus the cheapest
   possible rest (table R) is above the limit. The number of names within the limit is counted exactly,
   so the chance matches per hash are known: names_within_limit / 2^32.

   A first version derived the first four bytes from the hash for every (ending, target), as
   short_exhaustive.c does for length 9. It was correct but about fifty times slower: its work is
   (endings that some start could complete) x (targets), this one's is (names within the limit).

   At every node the 63 possible next characters are tested at once (AVX2): character x survives when
   cost of x + cheapest rest after x <= what is left of the limit. Both are rows of 64 16-bit values
   (the model row of the context, and R of the next context), so a node reads 256 bytes. The last
   character is the same test with "rest" = the cost of ending the name there.

   For n = 10, 11, 12 the hash is built incrementally (start word after 4 characters, middle word after 8,
   end word per last character); other lengths use the reference function for every name.

   usage: exhaustive_10_12 lm.bin alphabet targets.txt n limit first_character_index [second_character_index]
   targets: one hex hash per line (an empty file only counts). first_character_index: 0..62, the
   first character of the names to visit (one job per first character), or -1 for all; the optional
   second index restricts the second character too (the heavy first characters are split that way).
   prints "HASH<TAB>name<TAB>cost"; statistics as JSON on stderr. Every printed name is re-hashed with the reference function. */
#define main original_main
#include "kamui_search.c"
#undef main
#include <time.h>
#include <immintrin.h>
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
#define INF 65535
#define BITS 27
#define NS 64
#define NA 63
static uint16_t*lm;static const char*abc;
static inline int c4(int a,int b,int c,int x){return lm[((((size_t)a*NS)+b)*NS+c)*NS+x];}   /* x: 0 = end, 1..NA = character */
static inline int ctx3(int a,int b,int c){return (a*NS+b)*NS+c;}
static uint32_t*T;static int nt;static uint64_t*bitmap;
static uint64_t small[(1u<<20)/64];          /* 128 KB filter on the low 20 bits: stays in the first cache levels */
static uint32_t*slots;static uint32_t smask;
static uint16_t*R;static const size_t S3=(size_t)NS*NS*NS;   /* R[k][ctx]: cheapest way to write k more characters and then end, after the three characters ctx */
static int n,limit,fast,second=-1;static uint8_t name[32];
static unsigned long long within,hits,visited;
static uint32_t n5,h1,km;

static inline void lookup(uint32_t h,int cost){
    uint32_t tiny=h&((1u<<20)-1);
    if(!((small[tiny>>6]>>(tiny&63))&1))return;
    uint32_t low=h&((1u<<BITS)-1);
    if(!((bitmap[low>>6]>>(low&63))&1))return;
    for(uint32_t s=(h*2654435761u)&smask;slots[s];s=(s+1)&smask){
        if(T[slots[s]-1]!=h)continue;
        if(kamui(name,n)!=h){fprintf(stderr,"internal verification failed\n");exit(7);}
        printf("%08X\t%.*s\t%d\n",h,n,(const char*)name,cost);hits++;
        return;
    }
}
/* bit x (1..63) set when row[x] + rest[x] <= room; the sums saturate at 65535, which is above any limit accepted */
static inline uint64_t survivors(const uint16_t*row,const uint16_t*rest,int room){
    const __m256i lim=_mm256_set1_epi16((short)room),zero=_mm256_setzero_si256();
    uint64_t out=0;
    for(int i=0;i<4;i++){
        __m256i sum=_mm256_adds_epu16(_mm256_loadu_si256((const __m256i*)(row+16*i)),_mm256_loadu_si256((const __m256i*)(rest+16*i)));
        __m256i ok=_mm256_cmpeq_epi16(_mm256_subs_epu16(sum,lim),zero);
        uint32_t m=(uint32_t)_mm256_movemask_epi8(_mm256_packs_epi16(ok,zero));    /* lanes 0..7 in bits 0..7, lanes 8..15 in bits 16..23 */
        out|=(uint64_t)((m&0xFF)|((m>>8)&0xFF00))<<(16*i);
    }
    return out&~1ull;
}
static void walk(int pos,int a,int b,int c,int cost){
    visited++;
    const uint16_t*row=lm+((((size_t)a*NS)+b)*NS+c)*NS;
    const uint16_t*rest=R+(size_t)(n-1-pos)*S3+ctx3(b,c,0);     /* n-1-pos characters follow the one written here */
    uint64_t m=survivors(row,rest,limit-cost);
    if(pos==n-1){
        /* last character: rest[x] is the cost of ending after it */
        uint32_t lo3=0;if(fast){memcpy(&lo3,name+n-4,3);}
        while(m){
            int x=__builtin_ctzll(m);m&=m-1;
            int total=cost+row[x]+rest[x];
            within++;name[pos]=abc[x-1];
            uint32_t out;
            if(fast){
                uint32_t E=lo3|((uint32_t)(uint8_t)abc[x-1]<<24);
                uint32_t h2=5*(rol(h1^mix(n5+E),13)-MIX_SUB);
                uint32_t t=5*(rol(h2^km,13)-MIX_SUB);
                uint32_t f=FMIX1*(t^(t>>16)),r=FMIX2*(f^(f>>13));
                out=r^(r>>16);
                /* the incremental hash is compared with the reference function on one name in about a million */
                if((within&0xFFFFF)==1&&kamui(name,n)!=out){fprintf(stderr,"incremental hash differs from the reference\n");exit(8);}
            }else out=kamui(name,n);
            if(nt)lookup(out,total);
        }
        return;
    }
    if(pos==1&&second>=0)m&=1ull<<(second+1);        /* one job per second character for the heavy first characters */
    while(m){
        int x=__builtin_ctzll(m);m&=m-1;
        name[pos]=abc[x-1];
        if(fast&&pos==3){uint32_t A=rd(name,0);h1=5*(rol(n5^mix((uint32_t)n+A),13)-MIX_SUB);}
        if(fast&&pos==7){uint32_t M=rd(name,4);km=C2*rol(0x318F97D9u-MIX_MUL1*M,15);}
        walk(pos+1,b,c,x,cost+row[x]);
    }
}

int main(int argc,char**argv){
    if(argc!=7&&argc!=8){fprintf(stderr,"usage: exhaustive_10_12 lm.bin alphabet targets.txt n limit first_character_index [second_character_index]\n");return 1;}
    if(argc==8)second=atoi(argv[7]);
    abc=argv[2];if(strlen(abc)!=NA)return 3;
    size_t cells=(size_t)NS*NS*NS*NS;lm=malloc(cells*sizeof(uint16_t));if(!lm)return 4;
    FILE*f=fopen(argv[1],"rb");if(!f||fread(lm,sizeof(uint16_t),cells,f)!=cells)return 4;fclose(f);
    f=fopen(argv[3],"r");if(!f)return 4;int cap=0;unsigned v;
    while(fscanf(f,"%x",&v)==1){if(nt==cap){cap=cap?cap*2:1024;T=realloc(T,cap*sizeof(uint32_t));}T[nt++]=v;}
    fclose(f);
    n=atoi(argv[4]);limit=atoi(argv[5]);int first=atoi(argv[6]);
    if(n<5||n>24||limit<0||limit>=30000||first<-1||first>=NA||second>=NA)return 5;
    fast=n>=10&&n<=12;n5=5u*(uint32_t)n;
    bitmap=calloc((1u<<BITS)/64,sizeof(uint64_t));
    int sb=10;while((1u<<sb)<(unsigned)(4*nt+16))sb++;
    smask=(1u<<sb)-1;slots=calloc((size_t)1<<sb,sizeof(uint32_t));
    for(int i=0;i<nt;i++){
        uint32_t low=T[i]&((1u<<BITS)-1);bitmap[low>>6]|=1ull<<(low&63);
        uint32_t tiny=T[i]&((1u<<20)-1);small[tiny>>6]|=1ull<<(tiny&63);
        uint32_t s=(T[i]*2654435761u)&smask;int duplicate=0;
        while(slots[s]){if(T[slots[s]-1]==T[i]){duplicate=1;break;}s=(s+1)&smask;}
        if(!duplicate)slots[s]=i+1;
    }
    double t0=now();
    R=malloc((size_t)n*S3*sizeof(uint16_t));if(!R)return 4;
    for(int a=0;a<NS;a++)for(int b=0;b<NS;b++)for(int c=0;c<NS;c++)R[ctx3(a,b,c)]=(uint16_t)c4(a,b,c,0);
    for(int k=1;k<n;k++){
        const uint16_t*prev=R+(size_t)(k-1)*S3;uint16_t*cur=R+(size_t)k*S3;
        for(int a=0;a<NS;a++)for(int b=0;b<NS;b++)for(int c=0;c<NS;c++){
            int m=INF;
            for(int x=1;x<NS;x++){int value=c4(a,b,c,x)+prev[ctx3(b,c,x)];if(value<m)m=value;}
            cur[ctx3(a,b,c)]=(uint16_t)m;
        }
    }
    double t1=now();
    {
        const uint16_t*rest=R+(size_t)(n-1)*S3;
        for(int x=1;x<NS;x++){
            if(first>=0&&x!=first+1)continue;
            int k=c4(0,0,0,x);
            if(k+rest[ctx3(0,0,x)]>limit)continue;
            name[0]=abc[x-1];
            walk(1,0,0,x,k);
        }
    }
    fprintf(stderr,"{\"n\":%d,\"limit\":%d,\"first\":%d,\"targets\":%d,\"names_within_limit\":%llu,\"nodes\":%llu,\"hits\":%llu,\"table_seconds\":%.3f,\"seconds\":%.3f}\n",
            n,limit,first,nt,within,visited,hits,t1-t0,now()-t0);
    return 0;
}
