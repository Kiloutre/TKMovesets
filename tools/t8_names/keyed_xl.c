/* Keyed search of move names of length 21 to 24: the first n - 4 characters are enumerated within a character-model
   budget, the last four follow from the name hash, and the move key decides.

   A move record holds H = Kamui(X) and key = Kamui(CODE_X). For n = 21..24 the name X is hashed by the 13-24 branch
   (kamui_search.c, h12_24):

       H = fmix(step(B(X), mix(tail)))        tail = last four bytes, step(h, k) = 5 * (rol(h ^ k, 13) - MIX_SUB)
       B(X) = five steps over the words at offsets mid-4, 4, n-8, mid, 0 (mid = n >> 1), starting from n

   None of those words reaches the last four bytes, so for every enumerated body and every wanted hash

       tail = unmix(unstep(unfmix(H)) ^ B(X))

   CODE_X has 25 to 28 bytes and is hashed by the third branch of the hash, which gives no relation on the body alone
   (keyed_long.c has one for n = 13..20). So the tail is derived for every wanted hash (eight at a time, AVX2), kept
   when it is made of name characters, and the key of the first move record of that hash is then compared with
   Kamui(CODE_X) computed in full. The work grows with the number of wanted hashes.

   Bodies: every string of n - 4 characters whose character-model cost (4-gram table, costs in eighth-bits, no
   end-of-name cost) is at most `budget`. The last four characters are not limited at all.

   Controls: the same derivation with H ^ mask (four frozen masks), the key compared with key ^ mask (four masks),
   and the key compared with Kamui(OTHERCODE_X). None of these can be a true name.

   usage: keyed_xl lm.bin alphabet targets.txt n budget first_index [second_index]
     targets.txt  HASH CODE:KEY:OTHERCODE [CODE:KEY:OTHERCODE ...]   (as for keyed_short; an empty file only counts bodies)
   prints  kind <TAB> HASH <TAB> name <TAB> records agreeing <TAB> records <TAB> body cost <TAB> tail cost
           kind: key, hmask1..4 (hash ^ mask), kmask1..4 (key ^ mask), wrongcode
   statistics as JSON on stderr. */
#define main original_main
#include "kamui_search.c"
#undef main
#include <immintrin.h>
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
#define NS 64
#define NA 63
static uint32_t inverse(uint32_t a){uint32_t x=a;for(int i=0;i<6;i++)x*=2u-a*x;return x;}
static uint32_t INV5,INVC1,INVC2,INVF1,INVF2;
static inline uint32_t unfmix(uint32_t f){f^=f>>16;f*=INVF2;f^=(f>>13)^(f>>26);f*=INVF1;f^=f>>16;return f;}
static inline uint32_t step(uint32_t h,uint32_t k){return 5*(rol(h^k,13)-MIX_SUB);}
static inline uint32_t unstep(uint32_t y){return ror(y*INV5+MIX_SUB,13);}
static inline uint32_t unmix(uint32_t m){return INVC1*ror(INVC2*m,15);}
static inline int is_name_char(uint8_t c){return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';}

static uint16_t*lm;static const char*abc;static int code_of_char[256];
static inline int c4(int a,int b,int c,int x){return lm[((((size_t)a*NS)+b)*NS+c)*NS+x];}
static inline int ctx3(int a,int b,int c){return (a*NS+b)*NS+c;}
static const size_t S3=(size_t)NS*NS*NS;
static uint16_t*R;                                   /* R[k][ctx]: cheapest k more characters after ctx (no end cost) */

typedef struct{uint32_t code,key,other;}Record;
typedef struct{uint32_t hash;int first,count;}Target;
typedef struct{uint32_t hash,key,code,delta,u_hash;int target,kind,cidx;}Entry;      /* kind 0 real, 1-4 hash masks, 5-8 key masks, 9 wrong code */
static Target*T;static Record*Rec;static int nt,nr;
static Entry*E;static int ne;
static const uint32_t MASK[4]={0x13371337u,0x5A5A5A5Au,0xA5A5A5A5u,0x0F0F0F0Fu};
static const char*KIND[10]={"key","hmask1","hmask2","hmask3","hmask4","kmask1","kmask2","kmask3","kmask4","wrongcode"};
/* delta mode: entries grouped by code */
/* a 16 KB bit filter over (delta, code) that stays in the first cache level, then an exact table */
static uint32_t codes[128],code_mix[128],code_salt[128];static int ncodes;
#define FBITS 17
#define TBITS 16
static uint64_t filter[(1u<<FBITS)/64];
static uint32_t table[1u<<TBITS];                    /* entry index + 1, open addressing */
static inline uint32_t spread(uint32_t delta,int code){return (delta^code_salt[code])*0x9E3779B1u;}
/* per-pair mode: arrays over entries */
static uint32_t*EU;

static int n,p,budget,second=-1;
static uint8_t Y[40];static uint8_t*const X=Y+4;       /* Y = "COD_" + name; only byte 3 ('_') and the name are read */
static unsigned long long bodies,matches,kept,tally[10];

static int tail_cost(void){
    /* cost of the characters after the body, and of ending the name */
    int a=p>=3?code_of_char[X[p-3]]:0,b=p>=2?code_of_char[X[p-2]]:0,c=code_of_char[X[p-1]],cost=0;
    for(int i=p;i<n;i++){int x=code_of_char[X[i]];cost+=c4(a,b,c,x);a=b;b=c;c=x;}
    return cost+c4(a,b,c,0);
}
static void report(const Entry*e,int cost){
    int agree=1,total=1;
    if(e->kind==0){
        const Record*r=Rec+T[e->target].first;agree=0;total=T[e->target].count;
        uint8_t buffer[40];memcpy(buffer+4,X,n);
        for(int i=0;i<total;i++){memcpy(buffer,&r[i].code,4);agree+=kamui(buffer,n+4)==r[i].key;}
    }
    printf("%s\t%08X\t%.*s\t%d\t%d\t%d\t%d\n",KIND[e->kind],T[e->target].hash,n,(const char*)X,agree,total,cost,tail_cost());
    tally[e->kind]++;
}
static inline __m256i ror_v(__m256i x,int c){return _mm256_or_si256(_mm256_srli_epi32(x,c),_mm256_slli_epi32(x,32-c));}
static inline __m256i unmix_v(__m256i m){return _mm256_mullo_epi32(ror_v(_mm256_mullo_epi32(m,_mm256_set1_epi32((int)INVC2)),15),_mm256_set1_epi32((int)INVC1));}
static inline __m256i step_v(__m256i h,__m256i k){
    __m256i t=_mm256_sub_epi32(ror_v(_mm256_xor_si256(h,k),19),_mm256_set1_epi32((int)MIX_SUB));
    return _mm256_add_epi32(t,_mm256_slli_epi32(t,2));
}
static inline int name_lanes(__m256i w){
    const __m256i lower=_mm256_or_si256(w,_mm256_set1_epi8(0x20));
    const __m256i a=_mm256_sub_epi8(lower,_mm256_set1_epi8('a')),d=_mm256_sub_epi8(w,_mm256_set1_epi8('0'));
    __m256i ok=_mm256_cmpeq_epi8(_mm256_min_epu8(a,_mm256_set1_epi8(25)),a);
    ok=_mm256_or_si256(ok,_mm256_cmpeq_epi8(_mm256_min_epu8(d,_mm256_set1_epi8(9)),d));
    ok=_mm256_or_si256(ok,_mm256_cmpeq_epi8(w,_mm256_set1_epi8('_')));
    return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(ok,_mm256_set1_epi32(-1))));
}
/* the body X[0..p-1] is complete */
static void body(int cost){
    bodies++;
    if(!ne)return;
    const uint32_t mid=(uint32_t)n>>1;
    uint32_t h=(uint32_t)n;h=step(h,mix(rd(X,mid-4)));h=step(h,mix(rd(X,4)));h=step(h,mix(rd(X,n-8)));h=step(h,mix(rd(X,mid)));h=step(h,mix(rd(X,0)));
    const __m256i hv=_mm256_set1_epi32((int)h);
    for(int v=0;v<ne;v+=8){
        __m256i f=unmix_v(_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(EU+v)),hv));
        int lanes=name_lanes(f);
        if(!lanes)continue;
        uint32_t found[8];_mm256_storeu_si256((__m256i*)found,f);
        while(lanes){
            int i=__builtin_ctz(lanes);lanes&=lanes-1;
            if(v+i>=ne)break;
            const Entry*e=E+v+i;
            memcpy(X+p,found+i,4);kept++;
            if((kept&0xFFFF)==1&&kamui(X,n)!=e->hash){fprintf(stderr,"a derived tail does not reproduce the hash\n");exit(8);}
            uint8_t buffer[40];memcpy(buffer+4,X,n);memcpy(buffer,&e->code,4);
            uint32_t k=kamui(buffer,n+4);
            if(k==e->key)report(e,cost);
            if(e->kind==0){
                for(int j=0;j<4;j++)if(k==(e->key^MASK[j])){Entry d=*e;d.kind=5+j;report(&d,cost);}
                memcpy(buffer,&Rec[T[e->target].first].other,4);
                if(kamui(buffer,n+4)==e->key){Entry d=*e;d.kind=9;report(&d,cost);}
            }
        }
    }
}
static inline uint64_t survivors(const uint16_t*row,const uint16_t*rest,int room){
    const __m256i lim=_mm256_set1_epi16((short)room),zero=_mm256_setzero_si256();
    uint64_t out=0;
    for(int i=0;i<4;i++){
        __m256i sum=_mm256_adds_epu16(_mm256_loadu_si256((const __m256i*)(row+16*i)),_mm256_loadu_si256((const __m256i*)(rest+16*i)));
        __m256i ok=_mm256_cmpeq_epi16(_mm256_subs_epu16(sum,lim),zero);
        uint32_t m=(uint32_t)_mm256_movemask_epi8(_mm256_packs_epi16(ok,zero));
        out|=(uint64_t)((m&0xFF)|((m>>8)&0xFF00))<<(16*i);
    }
    return out&~1ull;
}
static void walk(int pos,int a,int b,int c,int cost){
    const uint16_t*row=lm+((((size_t)a*NS)+b)*NS+c)*NS;
    const uint16_t*rest=R+(size_t)(p-1-pos)*S3+ctx3(b,c,0);
    uint64_t m=survivors(row,rest,budget-cost);
    if(pos==1&&second>=0)m&=1ull<<(second+1);
    while(m){
        int x=__builtin_ctzll(m);m&=m-1;
        X[pos]=(uint8_t)abc[x-1];
        if(pos==p-1)body(cost+row[x]);else walk(pos+1,b,c,x,cost+row[x]);
    }
}

int main(int argc,char**argv){
    if(argc!=7&&argc!=8){fprintf(stderr,"usage: keyed_xl lm.bin alphabet targets.txt n budget first_index [second_index]\n");return 1;}
    INV5=inverse(5);INVC1=inverse(C1);INVC2=inverse(C2);INVF1=inverse(FMIX1);INVF2=inverse(FMIX2);
    abc=argv[2];if(strlen(abc)!=NA)return 3;
    for(int i=0;i<NA;i++)code_of_char[(uint8_t)abc[i]]=i+1;
    size_t cells=(size_t)NS*NS*NS*NS;lm=malloc(cells*sizeof(uint16_t));if(!lm)return 4;
    FILE*f=fopen(argv[1],"rb");if(!f||fread(lm,sizeof(uint16_t),cells,f)!=cells)return 4;fclose(f);
    n=atoi(argv[4]);budget=atoi(argv[5]);int first=atoi(argv[6]);if(argc==8)second=atoi(argv[7]);
    if(n<21||n>24||budget<0||budget>=30000||first<-1||first>=NA||second>=NA)return 5;
    p=n-4;
    f=fopen(argv[3],"r");if(!f)return 4;
    int capt=0,capr=0;char line[65536];
    while(fgets(line,sizeof line,f)){
        char*q=strtok(line," \t\r\n");if(!q)continue;
        if(nt==capt){capt=capt?capt*2:1024;T=realloc(T,capt*sizeof(Target));}
        T[nt].hash=(uint32_t)strtoul(q,NULL,16);T[nt].first=nr;T[nt].count=0;
        while((q=strtok(NULL," \t\r\n"))){
            if(strlen(q)!=16||q[3]!=':'||q[12]!=':')return 5;
            if(nr==capr){capr=capr?capr*2:4096;Rec=realloc(Rec,capr*sizeof(Record));}
            uint8_t w[4]={(uint8_t)q[0],(uint8_t)q[1],(uint8_t)q[2],'_'};memcpy(&Rec[nr].code,w,4);
            uint8_t o[4]={(uint8_t)q[13],(uint8_t)q[14],(uint8_t)q[15],'_'};memcpy(&Rec[nr].other,o,4);
            char hex[9];memcpy(hex,q+4,8);hex[8]=0;Rec[nr].key=(uint32_t)strtoul(hex,NULL,16);
            nr++;T[nt].count++;
        }
        if(!T[nt].count)return 5;
        nt++;
    }
    fclose(f);
    /* entries: the first record of every wanted hash, and its controls */
    E=calloc((size_t)nt*5+8,sizeof(Entry));
    for(int t=0;t<nt;t++){
        const Record*r=Rec+T[t].first;
        for(int k=0;k<5;k++){
            Entry e;memset(&e,0,sizeof e);e.target=t;e.kind=k;e.hash=T[t].hash^(k?MASK[k-1]:0);e.key=r->key;e.code=r->code;
            e.u_hash=unstep(unfmix(e.hash));
            E[ne++]=e;
        }
    }
    EU=calloc((size_t)ne+8,sizeof(uint32_t));
    for(int i=0;i<ne;i++)EU[i]=E[i].u_hash;
    /* self-test of the tail derivation and of the third branch on a fixed name of 21 characters */
    {const char*probe="Kz_tsshoryu_6RP_story";int len=21;uint8_t b[40];memcpy(b,"GRL_",4);memcpy(b+4,probe,len);
     const uint8_t*q=(const uint8_t*)probe;uint32_t H=kamui(q,len),mid=len>>1;
     uint32_t h=len;h=step(h,mix(rd(q,mid-4)));h=step(h,mix(rd(q,4)));h=step(h,mix(rd(q,len-8)));h=step(h,mix(rd(q,mid)));h=step(h,mix(rd(q,0)));
     if(H!=0x8C0FBB05u||unmix(unstep(unfmix(H))^h)!=rd(q,len-4)||kamui(b,len+4)==H){fprintf(stderr,"tail self-test failed\n");return 9;}}
    double t0=now();
    R=malloc((size_t)(p+1)*S3*sizeof(uint16_t));if(!R)return 4;
    for(size_t i=0;i<S3;i++)R[i]=0;
    for(int k=1;k<=p;k++){
        const uint16_t*prev=R+(size_t)(k-1)*S3;uint16_t*cur=R+(size_t)k*S3;
        for(int a=0;a<NS;a++)for(int b=0;b<NS;b++)for(int c=0;c<NS;c++){
            int m=65535;
            for(int x=1;x<NS;x++){int value=c4(a,b,c,x)+prev[ctx3(b,c,x)];if(value<m)m=value;}
            cur[ctx3(a,b,c)]=(uint16_t)m;
        }
    }
    Y[3]='_';
    {
        const uint16_t*rest=R+(size_t)(p-1)*S3;
        for(int x=1;x<NS;x++){
            if(first>=0&&x!=first+1)continue;
            int k=c4(0,0,0,x);
            if(k+rest[ctx3(0,0,x)]>budget)continue;
            X[0]=(uint8_t)abc[x-1];
            walk(1,0,0,x,k);
        }
    }
    fprintf(stderr,"{\"n\":%d,\"body\":%d,\"budget\":%d,\"first\":%d,\"targets\":%d,\"pairs\":%d,\"codes\":%d,\"bodies\":%llu,\"delta_matches\":%llu,\"names_derived\":%llu,"
            "\"key\":%llu,\"hmask\":%llu,\"kmask\":%llu,\"wrongcode\":%llu,\"seconds\":%.3f}\n",
            n,p,budget,first,nt,ne,ncodes,bodies,matches,kept,tally[0],tally[1]+tally[2]+tally[3]+tally[4],tally[5]+tally[6]+tally[7]+tally[8],tally[9],now()-t0);
    return 0;
}
