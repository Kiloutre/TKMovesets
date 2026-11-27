/* Exact final-word solving for Kamui names of length 9..12 (the 5..12 branch of the hash).

   Includes the unmodified reference C hash. For a prefix of n-4 bytes and a target hash:

     h1 = 5*(rol(5n ^ mix(n + A), 13) - S)          A = bytes 0..3            (inside the prefix)
     h2 = 5*(rol(h1 ^ mix(5n + E), 13) - S)         E = bytes n-4..n-1        (the unknown tail)
     v  = rol(h2 ^ km, 13),  km = C2*rol(K - M1*M, 15)   M = bytes 4..7
     out = fmix(5*(v - S))

   fmix, the two rounds and mix are invertible, so E follows from (prefix, M, target).
   n = 12: M is inside the prefix, one solve per (prefix, target).
   n = 11: M's last byte is the first tail byte: it is enumerated over the alphabet and must
           equal the solved byte. n = 10: two shared bytes. n = 9: three shared bytes.
   Every emitted name is re-hashed with the reference function.

   usage: name_judge_short_solver targets prefixes alphabet
   targets: lines "HEXHASH 0"; prefixes: one per line, total length - 4 bytes (5..8 bytes). */
#define main original_main
#include "kamui_search.c"
#undef main
#include <time.h>

static uint32_t inv(uint32_t x){uint32_t y=x;for(int i=0;i<5;i++)y*=2-x*y;return y;}
static uint32_t unxor(uint32_t x,int k){uint32_t y=x;for(int j=k;j<32;j+=k)y^=x>>j;return y;}
static uint32_t unmix(uint32_t k){return ror(k*inv(C2),15)*inv(C1);}
/* target -> value of (h2 ^ km) */
static uint32_t before_final(uint32_t out){
    uint32_t r=unxor(out,16)*inv(FMIX2);
    uint32_t f=unxor(r,13)*inv(FMIX1);
    uint32_t t=unxor(f,16);
    uint32_t v=t*inv(5)+MIX_SUB;
    return ror(v,13);
}
static uint32_t targets[65536],pre[65536];static int nt;
static uint8_t allowed[256],buf[16];static const char*abc;static int na;
static unsigned long long stems,solves,matches;

static void solve(int n,uint32_t h1,int shared){
    /* buf[0..n-5] is the prefix; buf[n-4..n-4+shared-1] already hold the enumerated shared bytes */
    uint32_t M=rd(buf,(n>>1)&4);
    uint32_t km=C2*rol(0x318F97D9u-MIX_MUL1*M,15);
    for(int j=0;j<nt;j++){
        uint32_t h2=pre[j]^km;
        uint32_t x=ror(h2*inv(5)+MIX_SUB,13);
        uint32_t E=unmix(x^h1)-5u*(uint32_t)n;
        solves++;
        uint8_t tail[4];memcpy(tail,&E,4);
        int ok=1;
        for(int k=0;k<4;k++){
            if(k<shared){if(tail[k]!=buf[n-4+k]){ok=0;break;}}
            else if(!allowed[tail[k]]){ok=0;break;}
        }
        if(!ok)continue;
        uint8_t full[16];memcpy(full,buf,n-4);memcpy(full+n-4,tail,4);full[n]=0;
        if(kamui(full,n)!=targets[j]){fprintf(stderr,"internal verification failed\n");exit(7);}
        printf("%08X\t%s\n",targets[j],full);matches++;
    }
}
static void expand(int n,uint32_t h1,int shared,int done){
    if(done==shared){solve(n,h1,shared);return;}
    for(int i=0;i<na;i++){buf[n-4+done]=(uint8_t)abc[i];expand(n,h1,shared,done+1);}
}
int main(int argc,char**argv){
    if(argc!=4){fprintf(stderr,"usage: name_judge_short_solver targets prefixes alphabet\n");return 1;}
    abc=argv[3];na=strlen(abc);if(!na)return 3;
    for(int i=0;i<na;i++){unsigned ch=(uint8_t)abc[i];if(allowed[ch])return 3;allowed[ch]=1;}
    FILE*f=fopen(argv[1],"r");if(!f)return 4;unsigned h;int length;
    while(fscanf(f,"%x %d",&h,&length)==2){if(nt>=65535)return 4;targets[nt]=h;pre[nt++]=before_final(h);}
    fclose(f);
    f=fopen(argv[2],"r");if(!f)return 5;char line[64];clock_t start=clock();
    while(fgets(line,sizeof line,f)){
        int len=strlen(line);while(len&&(line[len-1]=='\n'||line[len-1]=='\r'))line[--len]=0;
        int n=len+4;if(n<9||n>12)return 6;
        memset(buf,0,sizeof buf);memcpy(buf,line,len);stems++;
        uint32_t A=rd(buf,0);
        uint32_t h1=5*(rol((5u*(uint32_t)n)^mix((uint32_t)n+A),13)-MIX_SUB);
        expand(n,h1,12-n,0);          /* shared bytes between M (4..7) and the tail (n-4..n-1) */
    }
    fclose(f);
    fprintf(stderr,"{\"stems\":%llu,\"solves\":%llu,\"hits\":%llu,\"targets\":%d,\"seconds\":%.6f}\n",stems,solves,matches,nt,(double)(clock()-start)/CLOCKS_PER_SEC);
    return 0;
}
