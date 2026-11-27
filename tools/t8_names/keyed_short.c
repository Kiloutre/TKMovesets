/* Every name of one length (5 to 10) that has a wanted hash, checked against the move key. No character model.

   The hash of a name X of 5 to 12 bytes is (kamui_search.c, branch n > 4)

       H = fmix(step(step(step(5n, mix(n + A)), mix(5n + E)), mix(9 + M)))      step(h, k) = 5 * (rol(h ^ k, 13) - MIX_SUB)

   with three 4-byte words of X: A = bytes 0..3, E = the last four bytes, M = bytes 4..7 (bytes 0..3 for n < 8).
   mix, step and fmix are invertible, so any one word follows from the other two and the hash.

   n = 8, 9, 10   the bytes 4..n-1 (M and E, which overlap) are enumerated: 63^4, 63^5, 63^6 strings. For every one
                  and every wanted hash, A follows; it is kept when its four bytes are name characters (1 in 271).
   n = 5, 6, 7    A (which is also M) is enumerated: 63^4. For every wanted hash E follows; it is kept when it agrees
                  with A on the bytes they share and its other bytes are name characters.

   A kept name has the wanted hash by construction. It is then checked against the move key of the first move
   record of the hash: Kamui(CODE_name) == key. When that agrees, all records of the hash are counted.
   The same comparison is made, for every kept name,
     - with the key XORed with each of the four frozen masks (decoys on the key: no true name can pass), and
     - with the code of another list (wrong-code control).
   Decoys on the hash (hash ^ mask, same records) are just more wanted hashes in the input file.

   The derivation is done for eight wanted hashes at a time (AVX2).

   usage: keyed_short targets.txt alphabet n first_index
     targets.txt   one wanted hash per line: HASH CODE:KEY:OTHERCODE [CODE:KEY:OTHERCODE ...]   (hex, three-letter codes in upper case)
     first_index   0..62: only strings whose first enumerated character is that one (one job each), -1: all
   prints  kind <TAB> HASH <TAB> name <TAB> records agreeing <TAB> records     kind: key, mask1..mask4, wrongcode
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
static uint32_t inverse(uint32_t a){uint32_t x=a;for(int i=0;i<6;i++)x*=2u-a*x;return x;}      /* odd a: a * x = 1 (mod 2^32) */
static uint32_t INV5,INVC1,INVC2,INVF1,INVF2;
static inline uint32_t unfmix(uint32_t f){f^=f>>16;f*=INVF2;f^=(f>>13)^(f>>26);f*=INVF1;f^=f>>16;return f;}
static inline uint32_t unstep(uint32_t y){return ror(y*INV5+MIX_SUB,13);}                         /* h ^ k for y = step(h, k) */
static inline uint32_t unmix(uint32_t m){return INVC1*ror(INVC2*m,15);}
static inline int is_name_char(uint8_t c){return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';}

typedef struct{uint32_t code,key,other;}Record;                 /* code and other: "XXX_" as a little-endian word */
typedef struct{uint32_t hash;int first,count;}Target;
static Target*T;static Record*Rec;static int nt,nr;
static uint32_t*U;                                               /* per wanted hash: h2 ^ mix(9 + M) */
static uint32_t*W2;                                              /* per wanted hash, for the current M: unstep(h2) */
static const uint32_t MASK[4]={0x13371337u,0x5A5A5A5Au,0xA5A5A5A5u,0x0F0F0F0Fu};
static const char*abc;static int NA;
static int n;static uint8_t name[16];static uint8_t keyed[20];
static unsigned long long steps,kept,tally[6];

static void report(const char*kind,int t,int agree,int total){
    printf("%s\t%08X\t%.*s\t%d\t%d\n",kind,T[t].hash,n,(const char*)name,agree,total);
}
/* name[0..n-1] is complete and has the hash of wanted hash t (checked on a sample) */
static void candidate(int t){
    kept++;
    if((kept&0xFFFFF)==1&&kamui(name,n)!=T[t].hash){fprintf(stderr,"derived name does not have the hash\n");exit(8);}
    const Record*r=Rec+T[t].first;
    memcpy(keyed+4,name,n);
    memcpy(keyed,&r->code,4);uint32_t k=kamui(keyed,n+4);
    if(k==r->key){
        int agree=0;
        for(int i=0;i<T[t].count;i++){memcpy(keyed,&r[i].code,4);agree+=kamui(keyed,n+4)==r[i].key;}
        report("key",t,agree,T[t].count);tally[0]++;
    }
    for(int j=0;j<4;j++)if(k==(r->key^MASK[j])){char kind[8];sprintf(kind,"mask%d",j+1);report(kind,t,1,T[t].count);tally[1+j]++;}
    memcpy(keyed,&r->other,4);
    if(kamui(keyed,n+4)==r->key){report("wrongcode",t,1,T[t].count);tally[5]++;}
}
static inline __m256i ror_v(__m256i x,int c){return _mm256_or_si256(_mm256_srli_epi32(x,c),_mm256_slli_epi32(x,32-c));}
static inline __m256i unstep_v(__m256i y){return ror_v(_mm256_add_epi32(_mm256_mullo_epi32(y,_mm256_set1_epi32((int)INV5)),_mm256_set1_epi32((int)MIX_SUB)),13);}
static inline __m256i unmix_v(__m256i m){return _mm256_mullo_epi32(ror_v(_mm256_mullo_epi32(m,_mm256_set1_epi32((int)INVC2)),15),_mm256_set1_epi32((int)INVC1));}
/* bit i set when all four bytes of lane i are name characters */
static inline int name_lanes(__m256i w){
    const __m256i lower=_mm256_or_si256(w,_mm256_set1_epi8(0x20));
    const __m256i a=_mm256_sub_epi8(lower,_mm256_set1_epi8('a')),d=_mm256_sub_epi8(w,_mm256_set1_epi8('0'));
    __m256i ok=_mm256_cmpeq_epi8(_mm256_min_epu8(a,_mm256_set1_epi8(25)),a);
    ok=_mm256_or_si256(ok,_mm256_cmpeq_epi8(_mm256_min_epu8(d,_mm256_set1_epi8(9)),d));
    ok=_mm256_or_si256(ok,_mm256_cmpeq_epi8(w,_mm256_set1_epi8('_')));
    return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(ok,_mm256_set1_epi32(-1))));
}

/* n = 8..10: name[4..n-1] is set; for every wanted hash derive A */
static void suffix_mode(int first){
    const uint32_t n5=5u*(uint32_t)n;int extra=n-8;
    for(int i4=0;i4<NA;i4++){
        if(first>=0&&i4!=first)continue;
        name[4]=abc[i4];
        for(int i5=0;i5<NA;i5++){name[5]=abc[i5];
        for(int i6=0;i6<NA;i6++){name[6]=abc[i6];
        for(int i7=0;i7<NA;i7++){name[7]=abc[i7];
            uint32_t km=mix(9u+rd(name,4));
            const __m256i kmv=_mm256_set1_epi32((int)km);
            for(int v=0;v<nt;v+=8)_mm256_storeu_si256((__m256i*)(W2+v),unstep_v(_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(U+v)),kmv)));
            int combos=extra==0?1:extra==1?NA:NA*NA;
            for(int e=0;e<combos;e++){
                if(extra>=1)name[8]=abc[extra==1?e:e/NA];
                if(extra==2)name[9]=abc[e%NA];
                const __m256i kev=_mm256_set1_epi32((int)mix(n5+rd(name,n-4))),n5v=_mm256_set1_epi32((int)n5),nv=_mm256_set1_epi32(n);
                steps++;
                for(int v=0;v<nt;v+=8){
                    __m256i h1=_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(W2+v)),kev);
                    __m256i A=_mm256_sub_epi32(unmix_v(_mm256_xor_si256(unstep_v(h1),n5v)),nv);
                    int lanes=name_lanes(A);
                    if(!lanes)continue;
                    uint32_t found[8];_mm256_storeu_si256((__m256i*)found,A);
                    while(lanes){
                        int i=__builtin_ctz(lanes);lanes&=lanes-1;
                        if(v+i>=nt)break;
                        memcpy(name,found+i,4);
                        candidate(v+i);
                    }
                }
            }
        }}}
    }
}
/* n = 5..7: A = name[0..3] is enumerated; for every wanted hash derive E */
static void prefix_mode(int first){
    const uint32_t n5=5u*(uint32_t)n;int shared=8-n;                     /* bytes of A that are also in E */
    const uint32_t low=shared==3?0x00FFFFFFu:shared==2?0x0000FFFFu:0x000000FFu;
    uint32_t*Eout=malloc((nt+8)*sizeof(uint32_t));
    for(int i0=0;i0<NA;i0++){
        if(first>=0&&i0!=first)continue;
        name[0]=abc[i0];
        for(int i1=0;i1<NA;i1++){name[1]=abc[i1];
        for(int i2=0;i2<NA;i2++){name[2]=abc[i2];
        for(int i3=0;i3<NA;i3++){name[3]=abc[i3];
            uint32_t A=rd(name,0),h1=5*(rol(n5^mix((uint32_t)n+A),13)-MIX_SUB),km=mix(9u+A),want=(A>>(8*(n-4)))&low;
            const __m256i kmv=_mm256_set1_epi32((int)km),h1v=_mm256_set1_epi32((int)h1),n5v=_mm256_set1_epi32((int)n5),lowv=_mm256_set1_epi32((int)low),wantv=_mm256_set1_epi32((int)want);
            steps++;
            for(int v=0;v<nt;v+=8){
                __m256i h2=_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(U+v)),kmv);
                __m256i E=_mm256_sub_epi32(unmix_v(_mm256_xor_si256(unstep_v(h2),h1v)),n5v);
                int lanes=_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(_mm256_and_si256(E,lowv),wantv)));
                if(!lanes)continue;
                _mm256_storeu_si256((__m256i*)(Eout+v),E);
                while(lanes){
                    int i=__builtin_ctz(lanes);lanes&=lanes-1;
                    if(v+i>=nt)break;
                    uint8_t e[4];memcpy(e,Eout+v+i,4);int good=1;
                    for(int b=shared;b<4;b++)good&=is_name_char(e[b]);
                    if(!good)continue;
                    memcpy(name+n-4,e,4);
                    candidate(v+i);
                    memcpy(name,&A,4);
                }
            }
        }}}
    }
    free(Eout);
}

int main(int argc,char**argv){
    if(argc!=5){fprintf(stderr,"usage: keyed_short targets.txt alphabet n first_index\n");return 1;}
    INV5=inverse(5);INVC1=inverse(C1);INVC2=inverse(C2);INVF1=inverse(FMIX1);INVF2=inverse(FMIX2);
    {uint32_t t=0x12345678u,g=t;g^=g>>16;g*=FMIX1;g^=g>>13;g*=FMIX2;g^=g>>16;
     if(5u*INV5!=1u||C1*INVC1!=1u||C2*INVC2!=1u||unfmix(g)!=t)return 2;}
    abc=argv[2];NA=(int)strlen(abc);n=atoi(argv[3]);int first=atoi(argv[4]);
    if(NA<1||NA>64||n<5||n>10||first<-1||first>=NA)return 3;
    FILE*f=fopen(argv[1],"r");if(!f)return 4;
    int capt=0,capr=0;char line[65536];
    while(fgets(line,sizeof line,f)){
        char*p=strtok(line," \t\r\n");if(!p)continue;
        if(nt==capt){capt=capt?capt*2:1024;T=realloc(T,capt*sizeof(Target));}
        T[nt].hash=(uint32_t)strtoul(p,NULL,16);T[nt].first=nr;T[nt].count=0;
        while((p=strtok(NULL," \t\r\n"))){
            if(strlen(p)!=16||p[3]!=':'||p[12]!=':')return 5;
            if(nr==capr){capr=capr?capr*2:4096;Rec=realloc(Rec,capr*sizeof(Record));}
            uint8_t w[4]={(uint8_t)p[0],(uint8_t)p[1],(uint8_t)p[2],'_'};memcpy(&Rec[nr].code,w,4);
            uint8_t o[4]={(uint8_t)p[13],(uint8_t)p[14],(uint8_t)p[15],'_'};memcpy(&Rec[nr].other,o,4);
            char hex[9];memcpy(hex,p+4,8);hex[8]=0;Rec[nr].key=(uint32_t)strtoul(hex,NULL,16);
            nr++;T[nt].count++;
        }
        if(!T[nt].count)return 5;
        nt++;
    }
    fclose(f);
    if(!nt)return 5;
    U=calloc(nt+8,sizeof(uint32_t));W2=calloc(nt+8,sizeof(uint32_t));
    for(int t=0;t<nt;t++)U[t]=ror(unfmix(T[t].hash)*INV5+MIX_SUB,13);
    for(int t=nt;t<nt+8;t++)U[t]=U[0];
    /* self-test of the inversion on a fixed name */
    {const uint8_t probe[10]={'R','g','_','R','o','c','k','e','t','3'};uint32_t h=kamui(probe,10);
     uint32_t u=ror(unfmix(h)*INV5+MIX_SUB,13),h2=u^mix(9u+rd(probe,4)),h1=unstep(h2)^mix(50u+rd(probe,6)),a=unmix(unstep(h1)^50u)-10u;
     if(a!=rd(probe,0)){fprintf(stderr,"inversion self-test failed\n");return 9;}}
    double t0=now();
    if(n>=8)suffix_mode(first);else prefix_mode(first);
    fprintf(stderr,"{\"n\":%d,\"first\":%d,\"targets\":%d,\"records\":%d,\"strings\":%llu,\"names_with_a_wanted_hash\":%llu,\"key\":%llu,\"mask1\":%llu,\"mask2\":%llu,\"mask3\":%llu,\"mask4\":%llu,\"wrongcode\":%llu,\"seconds\":%.3f}\n",
            n,first,nt,nr,steps,kept,tally[0],tally[1],tally[2],tally[3],tally[4],tally[5],now()-t0);
    return 0;
}
