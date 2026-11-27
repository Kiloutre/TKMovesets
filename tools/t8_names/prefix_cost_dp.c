/* Saturating prefix-cost histograms. No hash targets or guessed names here. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
typedef struct { int next,cost; } Edge;
typedef struct { int terminal,degree; Edge *edges; } State;
static uint32_t addcap(uint32_t a,uint32_t b,uint32_t cap){uint64_t s=(uint64_t)a+b;return s>cap?cap:(uint32_t)s;}
int main(int argc,char **argv){
    if(argc!=2)return 1;
    FILE*f=fopen(argv[1],"r");if(!f)return 2;
    int ns,depth,limit,start;unsigned cap;
    if(fscanf(f,"%d %d %d %u %d",&ns,&depth,&limit,&cap,&start)!=5)return 3;
    if(ns<1||ns>200000||depth<0||depth>100||limit<0||limit>10000||!cap||start<0||start>=ns)return 3;
    State*s=calloc(ns,sizeof(State));if(!s)return 4;
    for(int i=0;i<ns;i++){
        if(fscanf(f,"%d %d",&s[i].terminal,&s[i].degree)!=2||s[i].degree<0||s[i].degree>256)return 3;
        s[i].edges=calloc(s[i].degree,sizeof(Edge));
        for(int j=0;j<s[i].degree;j++){
            Edge*e=&s[i].edges[j];
            if(fscanf(f,"%d %d",&e->next,&e->cost)!=2||e->next<0||e->next>=ns||e->cost<0)return 3;
        }
    }fclose(f);
    size_t width=(size_t)limit+1,sz=(size_t)ns*width;
    uint32_t *cur=calloc(sz,sizeof(uint32_t)),*next=calloc(sz,sizeof(uint32_t)),*hist=calloc(width,sizeof(uint32_t));
    int *low=malloc(ns*sizeof(int)),*high=malloc(ns*sizeof(int)),*nlow=malloc(ns*sizeof(int)),*nhigh=malloc(ns*sizeof(int));
    if(!cur||!next||!hist||!low||!high||!nlow||!nhigh)return 4;
    for(int i=0;i<ns;i++){low[i]=limit+1;high[i]=-1;}
    low[start]=high[start]=0;
    cur[(size_t)start*width]=1;
    for(int d=0;d<=depth;d++){
        memset(hist,0,width*sizeof(uint32_t));
        for(int i=0;i<ns;i++){
            if(s[i].terminal<0||s[i].terminal>limit)continue;
            uint32_t*row=cur+(size_t)i*width;
            int top=high[i]<limit-s[i].terminal?high[i]:limit-s[i].terminal;
            for(int c=low[i];c<=top;c++)if(row[c])
                hist[c+s[i].terminal]=addcap(hist[c+s[i].terminal],row[c],cap);
        }
        for(int c=0;c<=limit;c++)if(hist[c])printf("%d %d %u\n",d,c,hist[c]);
        if(d==depth)break;
        memset(next,0,sz*sizeof(uint32_t));
        for(int i=0;i<ns;i++){nlow[i]=limit+1;nhigh[i]=-1;}
        for(int i=0;i<ns;i++){
            if(high[i]<0)continue;
            uint32_t*row=cur+(size_t)i*width;
            for(int j=0;j<s[i].degree;j++){
                Edge e=s[i].edges[j];if(e.cost>limit)continue;
                uint32_t*dest=next+(size_t)e.next*width;
                int top=high[i]<limit-e.cost?high[i]:limit-e.cost;
                for(int c=low[i];c<=top;c++)if(row[c]){
                    int k=c+e.cost;dest[k]=addcap(dest[k],row[c],cap);
                    if(k<nlow[e.next])nlow[e.next]=k;
                    if(k>nhigh[e.next])nhigh[e.next]=k;
                }
            }
        }
        uint32_t*tmp=cur;cur=next;next=tmp;
        int *itmp=low;low=nlow;nlow=itmp;itmp=high;high=nhigh;nhigh=itmp;
    }
    for(int i=0;i<ns;i++)free(s[i].edges);
    free(s);free(cur);free(next);free(hist);free(low);free(high);free(nlow);free(nhigh);return 0;
}
