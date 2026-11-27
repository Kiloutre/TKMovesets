/* Exact final-word solving; includes the unmodified reference C hash. */
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
static uint32_t uf(uint32_t x){x=unxor(x,16)*inv(FMIX2);x=unxor(x,13)*inv(FMIX1);return unxor(x,16);}
static uint32_t um(uint32_t x){return ror(x*inv(C2),15)*inv(C1);}
static uint32_t endstate(uint32_t target){return ror(uf(target)*0xCCCCCCCDu+MIX_SUB,13);}
static uint32_t before_tail(const uint8_t*d,int n){
    int off[]={n/2-4,4,n-8,n/2,0};uint32_t x=n;
    for(int i=0;i<5;i++)x=5*rol(x^mix(rd(d,off[i])),13)-((i==1||i==3)?MIX_SUB2:5*MIX_SUB);
    return x;
}
static uint32_t targets[65536],ends[65536];static int nt;
static uint8_t allowed[256],buf[25];static const char*abc;static const char*tailmask="????";static int na,forward_mode;
static unsigned long long stems,solves,full_hashes,matches;
static void emit(uint32_t target,int n){buf[n]=0;printf("%08X\t%s\n",target,buf);matches++;}
static void forward_tail(int pos,int n){
    if(pos==n){uint32_t h=kamui(buf,n);full_hashes++;if(tfind(h,n))emit(h,n);return;}
    if(tailmask[pos-(n-4)]!='?'){buf[pos]=tailmask[pos-(n-4)];forward_tail(pos+1,n);return;}
    for(int i=0;i<na;i++){buf[pos]=abc[i];forward_tail(pos+1,n);}
}
static void solve_stem(int n){
    stems++;
    if(forward_mode){forward_tail(n-4,n);return;}
    int count=n<15&&tailmask[0]=='?'?na:1;
    for(int a=0;a<count;a++){
        uint8_t boundary=tailmask[0]=='?'?abc[a]:tailmask[0];
        if(n<15)buf[n-4]=boundary;
        uint32_t state=before_tail(buf,n);
        for(int j=0;j<nt;j++){
            uint32_t word=um(state^ends[j]);solves++;
            uint8_t tail[4];memcpy(tail,&word,4);
            if(n<15&&tail[0]!=boundary)continue;
            int valid=1;
            for(int k=0;k<4;k++)if(tailmask[k]=='?'?!allowed[tail[k]]:tail[k]!=(uint8_t)tailmask[k]){valid=0;break;}
            if(!valid)continue;
            memcpy(buf+n-4,tail,4);full_hashes++;
            if(kamui(buf,n)!=targets[j]){fprintf(stderr,"internal verification failed\n");exit(7);}
            emit(targets[j],n);
        }
    }
}
static void expand(const char*pattern,int pos,int stemlen){
    if(pos==stemlen){solve_stem(stemlen+4);return;}
    if(pattern[pos]=='?')for(int i=0;i<na;i++){buf[pos]=abc[i];expand(pattern,pos+1,stemlen);}
    else {buf[pos]=pattern[pos];expand(pattern,pos+1,stemlen);}
}
int main(int argc,char**argv){
    if(argc==3&&!strcmp(argv[1],"--check-known")){
        FILE*f=fopen(argv[2],"r");if(!f)return 1;char name[512];unsigned h;int checked=0;
        while(fscanf(f,"%x %511s",&h,name)==2){int n=strlen(name);if(n<13||n>24)continue;
            uint32_t word=um(before_tail((uint8_t*)name,n)^endstate(h));
            if(word!=rd((uint8_t*)name,n-4)||kamui((uint8_t*)name,n)!=h)return 2;
            checked++;
        }fclose(f);printf("{\"known_names_verified\":%d}\n",checked);return 0;
    }
    if(argc<4){fprintf(stderr,"usage: tail_solver targets prefixes alphabet [--tail=????] [--forward]\nPrefixes have total_length-4 bytes, with ? wildcards. Total lengths 13..24.\n");return 1;}
    for(int i=4;i<argc;i++){
        if(!strcmp(argv[i],"--forward"))forward_mode=1;
        else if(!strncmp(argv[i],"--tail=",7)&&strlen(argv[i]+7)==4)tailmask=argv[i]+7;
        else return 2;
    }
    abc=argv[3];na=strlen(abc);if(!na)return 3;
    for(int i=0;i<na;i++){unsigned ch=(uint8_t)abc[i];if(allowed[ch])return 3;allowed[ch]=1;}
    FILE*f=fopen(argv[1],"r");if(!f)return 4;unsigned h;int length;
    while(fscanf(f,"%x %d",&h,&length)==2){
        if(nt>=65535||length!=0){fprintf(stderr,"Use length 0; constrain length in the prefix templates.\n");return 4;}
        targets[nt]=h;ends[nt++]=endstate(h);tadd(h,0);
    }fclose(f);
    List patterns=load(argv[2]);double start=now();
    for(int i=0;i<patterns.n;i++){int n=patterns.l[i]+4;if(n<13||n>24)return 5;expand(patterns.s[i],0,n-4);}
    double elapsed=now()-start;
    fprintf(stderr,"{\"mode\":\"%s\",\"stems\":%llu,\"solves\":%llu,\"full_hashes\":%llu,\"hits\":%llu,\"targets\":%d,\"seconds\":%.9f}\n",forward_mode?"forward":"inverse",stems,solves,full_hashes,matches,nt,elapsed);
    return 0;
}
