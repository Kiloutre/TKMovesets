/* Enumerate the exact selected prefix-cost region using O(depth) traversal. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define INF 1000000000
typedef struct { int next,cost,ch; } Edge;
typedef struct { int terminal,degree; Edge*edges; } State;
typedef struct { char head[128]; int depth,offset,singleton; } Head;
static State*states;static int*best;static int ns,start,total_length,cutoff,wanted;
static int emitted;static char prefix[128];
static void emit(Head*h,int depth,int cost){
    if(emitted>=wanted){fprintf(stderr,"enumeration exceeds budget\n");exit(8);}
    prefix[depth]=0;
    if(h->singleton)printf("%d\t%s\t\t%.*s\n",cost,h->head,total_length-4,h->head);
    else printf("%d\t%s\t%s\t%s%s\n",cost,h->head,prefix,h->head,prefix);
    emitted++;
}
static void visit(Head*h,int state,int remaining,int pos,int cost,int bound,int exact){
    if(exact&&emitted>=wanted)return;
    int lower=best[remaining*ns+state];
    if(lower>=INF||cost+lower>bound)return;
    if(!remaining){
        int score=cost+states[state].terminal;
        if(score==bound||(!exact&&score<bound))emit(h,pos,score+h->offset);
        return;
    }
    State*s=states+state;
    for(int i=0;i<s->degree;i++){
        Edge e=s->edges[i];prefix[pos]=e.ch;
        visit(h,e.next,remaining-1,pos+1,cost+e.cost,bound,exact);
    }
}
int main(int argc,char**argv){
    if(argc!=3)return 1;
    FILE*f=fopen(argv[1],"r");if(!f)return 2;
    int maxdepth;
    if(fscanf(f,"%d %d %d",&ns,&maxdepth,&start)!=3||ns<1||ns>200000||maxdepth<0||maxdepth>100||start<0||start>=ns)return 3;
    states=calloc(ns,sizeof(State));if(!states)return 4;
    for(int i=0;i<ns;i++){
        State*s=states+i;
        if(fscanf(f,"%d %d",&s->terminal,&s->degree)!=2||s->degree<0||s->degree>256)return 3;
        if(s->terminal<0)s->terminal=INF;
        s->edges=calloc(s->degree,sizeof(Edge));
        for(int j=0;j<s->degree;j++){
            Edge*e=s->edges+j;
            if(fscanf(f,"%d %d %d",&e->next,&e->cost,&e->ch)!=3||e->next<0||e->next>=ns||e->cost<0||e->ch<1||e->ch>127)return 3;
        }
    }fclose(f);
    best=malloc((maxdepth+1)*ns*sizeof(int));if(!best)return 4;
    for(int i=0;i<ns;i++)best[i]=states[i].terminal;
    for(int d=1;d<=maxdepth;d++)for(int i=0;i<ns;i++){
        int minimum=INF;
        for(int j=0;j<states[i].degree;j++){
            Edge e=states[i].edges[j];int previous=best[(d-1)*ns+e.next];
            if(previous<INF&&previous+e.cost<minimum)minimum=previous+e.cost;
        }best[d*ns+i]=minimum;
    }
    f=fopen(argv[2],"r");if(!f)return 2;
    int count;
    if(fscanf(f,"%d %d %d %d",&total_length,&cutoff,&wanted,&count)!=4||total_length<13||total_length>24||wanted<1||count<0||count>100000)return 3;
    Head*heads=calloc(count,sizeof(Head));if(!heads)return 4;
    for(int i=0;i<count;i++){
        Head*h=heads+i;
        if(fscanf(f,"%127s %d %d %d",h->head,&h->depth,&h->offset,&h->singleton)!=4||h->depth<0||h->depth>maxdepth)return 3;
    }fclose(f);
    for(int pass=0;pass<2;pass++)for(int i=0;i<count;i++){
        Head*h=heads+i;if(pass&&emitted>=wanted)break;
        if(h->singleton){
            if((!pass&&h->offset<cutoff)||(pass&&h->offset==cutoff))emit(h,0,h->offset);
        }else visit(h,start,h->depth,0,0,cutoff-h->offset-(pass?0:1),pass);
    }
    if(emitted!=wanted){fprintf(stderr,"count mismatch %d %d\n",emitted,wanted);return 5;}
    for(int i=0;i<ns;i++)free(states[i].edges);
    free(states);free(best);free(heads);return 0;
}
