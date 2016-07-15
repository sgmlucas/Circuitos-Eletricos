/*
Programa de demonstracao de analise nodal modificada
Por Antonio Carlos M. de Queiroz acmq@coe.ufrj.br
Versao 1.0 - 6/9/2000
Versao 1.0a - 8/9/2000 Ignora comentarios
Versao 1.0b - 15/9/2000 Aumentado Yn, retirado Js
Versao 1.0c - 19/2/2001 Mais comentarios
Versao 1.0d - 16/2/2003 Tratamento correto de nomes em minusculas
Versao 1.0e - 21/8/2008 Estampas sempre somam. Ignora a primeira linha
Versao 1.0f - 21/6/2009 Corrigidos limites de alocacao de Yn
Versao 1.0g - 15/10/2009 Le as linhas inteiras
Versao 1.0h - 18/6/2011 Estampas correspondendo a modelos
Versao 1.0i - 03/11/2013 Correcoes em *p e saida com sistema singular.
Versao 1.0j - 26/11/2015 Evita operacoes com zero.
Versao 1.0k - 23/06/2016 Calcula P.O. com L, C e K (o acoplamento é ignorado, pois P.O. é análise DC)
Versao 1.0l - 24/06/2016 Leitura do netlist para elemento MOS 
Versao 1.0m - 27/06/2016 Tensões iniciais aleatórias atribuídas (para NP) e verificação dos 3 modos de operação dos MOS 
Versao 1.0n - 03/07/2016 Linearização dos transistores MOS para valores iniciais. Criação da função verMOSCond(). Falta resolver I0
Versao 1.0o - 06/07/2016 I0 "supostamente" resolvido
Versao 1.0p - 09/07/1016 Newton-Raphson implementado(by fefa), porém, os circuitos com elementos MOS não convergem....
*/

/*
Elementos aceitos e linhas do netlist:
Resistor:      R<nome> <no+> <no-> <resistencia>
Indutor:       L<nome> <nó+> <nó-> <indutancia>
Acoplamento:   K<nome> <LA> <LB> <k> (indutores LA e LB já declarados)
Capacitor:     C<nome> <nó+> <nó-> <capacitancia>
VCCS:          G<nome> <io+> <io-> <vi+> <vi-> <transcondutancia>
VCVC:          E<nome> <vo+> <vo-> <vi+> <vi-> <ganho de tensao>
CCCS:          F<nome> <io+> <io-> <ii+> <ii-> <ganho de corrente>
CCVS:          H<nome> <vo+> <vo-> <ii+> <ii-> <transresistencia>
Fonte I:       I<nome> <io+> <io-> <corrente>
Fonte V:       V<nome> <vo+> <vo-> <tensao>
Amp. op.:      O<nome> <vo1> <vo2> <vi1> <vi2>
TransistorMOS: M<nome> <nód> <nóg> <nós> <nób> <NMOS ou PMOS> L=<comprimento> W=<largura> <K> <Vt0> <lambda> <gama> <phi> <Ld>
As fontes F e H tem o ramo de entrada em curto
O amplificador operacional ideal tem a saida suspensa
Os nos podem ser nomes
*/

//Trabalho de Circuitos Elétricos 2 - 
#define versao "1.0j - 26/11/2015"
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <complex.h>
#include <time.h>
#define MAX_LINHA 80
#define MAX_NOME 11
#define MAX_ELEM 500
#define MAX_NOS 50
#define TOLG 1e-20
#define PI 3.14159265358979
#define DEBUG

typedef struct elemento { /* Elemento do netlist */
  char nome[MAX_NOME];
  double valor,modulo,fase;
  int a,b,c,d,x,y;
} elemento;

elemento netlist[MAX_ELEM]; /* Netlist */

typedef struct acoplamento {
  char lA[MAX_NOME],lB[MAX_NOME];
} acoplamento;

acoplamento acop_K[MAX_ELEM];

typedef struct transitorMOS {
   char tipo[MAX_NOME];
   double cp,lg,transK,vt0,lambda,gama,phi,ld,cox,
        cgs,cgd,cbg;
   int invertido;
} transistorMOS;

transistorMOS mos[MAX_ELEM];

double ind_L[MAX_ELEM], cap_C[MAX_ELEM], ind_M, valorLA, valorLB; /*guarda os valores de indutancia e capacitancia p/ serem utilizados no modelo de peq. sinais*/ 

int
  ne, /* Elementos */
  nv, /* Variaveis */
  nn, /* Nos */
  i,j,k, indice,
  inc_L, inc_C, tensaoMOS[MAX_ELEM][4],/*tensaoMOS[]: vínculo entre nó e tensão (não confundir com valor de tensão!)*/
  ne_extra,nao_linear;
  
short fim = 0, contadorMos = 0;
int   contador =1, convergencia[MAX_ELEM];

char
/* Foram colocados limites nos formatos de leitura para alguma protecao
   contra excesso de caracteres nestas variaveis */
  nomearquivo[MAX_LINHA+1],
  tipo,
  na[MAX_NOME],nb[MAX_NOME],nc[MAX_NOME],nd[MAX_NOME],
  lista[MAX_NOS+1][MAX_NOME+2], /*Tem que caber jx antes do nome */
  txt[MAX_LINHA+1],
  *p;
FILE *arquivo;

double
  g,aux,
  vd[MAX_ELEM][2],vs[MAX_ELEM][2],vg[MAX_ELEM][2],vb[MAX_ELEM][2],vt[MAX_ELEM][2],//tensões auxiliares do transistor MOS
  Yn[MAX_NOS+1][MAX_NOS+2],         //matriz nodal
  YnComplex[MAX_NOS+1][MAX_NOS+2];  //matriz nodal com complexos (análise da resposta em frequencia)
  
 double complex
  gComplex,
  amplitude,
  frequencia,
  fase;

double verMOSCond(void){ //verifica as tensões do transistor MOS e calcula adequadamente as condutâncias linearizadas
   if(vd[nao_linear][0]>=vs[nao_linear][0]){
    if(vd[nao_linear][0]=vs[nao_linear][0]){
      vd[nao_linear][0]+=1e-3;
    }
    if(mos[ne].tipo[0]=='N' || mos[ne].tipo[0]=='P'){
      
      if(mos[ne].tipo[0]=='P'){
        mos[ne].invertido=1;
        aux=vd[nao_linear][0];
        vd[nao_linear][0]=vs[nao_linear][0];
        vs[nao_linear][0]=aux;  
      }
      
      if((vg[nao_linear][0]-vs[nao_linear][0])<vt[nao_linear][0]){ //corte
        
    mos[ne].cgs=mos[ne].cox*mos[ne].cp*mos[ne].ld;
        mos[ne].cgd=mos[ne].cgs;
        mos[ne].cbg=mos[ne].cox*mos[ne].cp*mos[ne].lg;
    
    return 0;
    }//em todos os 3 casos
        
      else if((vd[nao_linear][0]-vs[nao_linear][0])<=(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])){//triodo 
        
    mos[ne].cgs=mos[ne].cox*mos[ne].cp*mos[ne].ld+(mos[ne].cox*mos[ne].cp*mos[ne].lg)/2;
        mos[ne].cgd=mos[ne].cgs;
        mos[ne].cbg=0;
        
        if(strcmp(netlist[ne].nome,"MRGds")==0)//se for RGds
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])-2*(vd[nao_linear][0]-vs[nao_linear][0])+4*mos[ne].lambda*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(vd[nao_linear][0]-vs[nao_linear][0])-3*mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0])*(vd[nao_linear][0]-vs[nao_linear][0])));
  
        else if(strcmp(netlist[ne].nome,"MGm")==0)//se for Gm
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vd[nao_linear][0]-vs[nao_linear][0])*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0]))));
      
        else if(strcmp(netlist[ne].nome,"MGmb")==0)//se for Gmb
          return ((((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vd[nao_linear][0]-vs[nao_linear][0])*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0]))))*mos[ne].gama)/(sqrt(mos[ne].phi-vb[nao_linear][0]+vs[nao_linear][0])));       
        
        else if(strcmp(netlist[ne].nome,"MIds")==0)
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(vd[nao_linear][0]-vs[nao_linear][0])-(vd[nao_linear][0]-vs[nao_linear][0])*(vd[nao_linear][0]-vs[nao_linear][0]))*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0])));
        
      }
      
      else if((vd[nao_linear][0]-vs[nao_linear][0])>(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])){//saturação        
        
    mos[ne].cgs=mos[ne].cox*mos[ne].cp*mos[ne].ld+2*(mos[ne].cox*mos[ne].cp*mos[ne].lg)/3;
        mos[ne].cgd=mos[ne].cox*mos[ne].cp*mos[ne].ld;
        mos[ne].cbg=0;
    
    if(strcmp(netlist[ne].nome,"MRGds")==0)//se for RGds
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*mos[ne].lambda);
        
        else if(strcmp(netlist[ne].nome,"MGm")==0)//se for Gm
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0]))));
          
        else if(strcmp(netlist[ne].nome,"MGmb")==0)//se for Gmb
          return ((((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0]))))*mos[ne].gama)/(sqrt(mos[ne].phi-vb[nao_linear][0]+vs[nao_linear][0])));
        
        else if(strcmp(netlist[ne].nome,"MIds")==0)
          return  (mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0]));
      }
    }
    
  }
//  else if(vd[nao_linear][0]=vs[nao_linear][0]){
//  }
  else if(vd[nao_linear][0]<vs[nao_linear][0]){
    
    if(mos[ne].tipo[0]=='P' || mos[ne].tipo[0]=='N'){
      
      if(mos[ne].tipo[0]=='N'){
        mos[ne].invertido=1;
        aux=vd[nao_linear][0];
        vd[nao_linear][0]=vs[nao_linear][0];
        vs[nao_linear][0]=aux;  
      }
      
      if((vg[nao_linear][0]-vs[nao_linear][0])>vt[nao_linear][0]){ //corte
        
    mos[ne].cgs=mos[ne].cox*mos[ne].cp*mos[ne].ld;
        mos[ne].cgd=mos[ne].cgs;
        mos[ne].cbg=mos[ne].cox*mos[ne].cp*mos[ne].lg;
    
    return 0;//em todos os 3 casos
      }
        
      else if((vd[nao_linear][0]-vs[nao_linear][0])>(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])){//triodo 
        
        mos[ne].cgs=mos[ne].cox*mos[ne].cp*mos[ne].ld+(mos[ne].cox*mos[ne].cp*mos[ne].lg)/2;
        mos[ne].cgd=mos[ne].cgs;
        mos[ne].cbg=0;
    
    if(strcmp(netlist[ne].nome,"MRGds")==0)//se for RGds
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vs[nao_linear][0]-vg[nao_linear][0]+vt[nao_linear][0])-2*(vs[nao_linear][0]-vd[nao_linear][0])+4*mos[ne].lambda*(vs[nao_linear][0]-vg[nao_linear][0]+vt[nao_linear][0])*(vs[nao_linear][0]-vd[nao_linear][0])-3*mos[ne].lambda*(vs[nao_linear][0]-vd[nao_linear][0])*(vs[nao_linear][0]-vd[nao_linear][0])));
      
        else if(strcmp(netlist[ne].nome,"MGm")==0)//se for Gm
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vs[nao_linear][0]-vd[nao_linear][0])*(1+mos[ne].lambda*(vs[nao_linear][0]-vd[nao_linear][0]))));
          
        else if(strcmp(netlist[ne].nome,"MGmb")==0)//se for Gmb
          return ((((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vs[nao_linear][0]-vd[nao_linear][0])*(1+mos[ne].lambda*(vs[nao_linear][0]-vd[nao_linear][0]))))*mos[ne].gama)/(sqrt(mos[ne].phi+vb[nao_linear][0]-vs[nao_linear][0])));       
      
    else if(strcmp(netlist[ne].nome,"MIds")==0)
          return -((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(vd[nao_linear][0]-vs[nao_linear][0])-(vd[nao_linear][0]-vs[nao_linear][0])*(vd[nao_linear][0]-vs[nao_linear][0]))*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0])));
      }
      
      else if((vd[nao_linear][0]-vs[nao_linear][0])<=(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])){//saturação       
        
        mos[ne].cgs=mos[ne].cox*mos[ne].cp*mos[ne].ld+2*(mos[ne].cox*mos[ne].cp*mos[ne].lg)/3;
        mos[ne].cgd=mos[ne].cox*mos[ne].cp*mos[ne].ld;
        mos[ne].cbg=0;
    
    if(strcmp(netlist[ne].nome,"MRGds")==0)//se for RGds
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(vs[nao_linear][0]-vg[nao_linear][0]+vt[nao_linear][0])*(vs[nao_linear][0]-vg[nao_linear][0]+vt[nao_linear][0])*mos[ne].lambda);
        
        else if(strcmp(netlist[ne].nome,"MGm")==0)//se for Gm
          return ((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vs[nao_linear][0]-vg[nao_linear][0]+vt[nao_linear][0])*(1+mos[ne].lambda*(vs[nao_linear][0]-vd[nao_linear][0]))));
          
        else if(strcmp(netlist[ne].nome,"MGmb")==0)//se for Gmb
          return ((((mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(2*(vs[nao_linear][0]-vg[nao_linear][0]+vt[nao_linear][0])*(1+mos[ne].lambda*(vs[nao_linear][0]-vd[nao_linear][0]))))*mos[ne].gama)/(sqrt(mos[ne].phi-vs[nao_linear][0]+vb[nao_linear][0])));
          
          else if(strcmp(netlist[ne].nome,"MIds")==0)
          return  -(mos[ne].transK)*(mos[ne].cp/mos[ne].lg)*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(vg[nao_linear][0]-vs[nao_linear][0]-vt[nao_linear][0])*(1+mos[ne].lambda*(vd[nao_linear][0]-vs[nao_linear][0]));
      }
    }
    
  } 
}


/* Resolucao de sistema de equacoes lineares.
   Metodo de Gauss-Jordan com condensacao pivotal */
int resolversistema(void)
{
  int i,j,l,a;
  double t, p;

  for (i=1; i<=nv; i++) {
    t=0.0;
    a=i;
    for (l=i; l<=nv; l++) {
      if (fabs(Yn[l][i])>fabs(t)) {
  a=l;
  t=Yn[l][i];
      }
    }
    if (i!=a) {
      for (l=1; l<=nv+1; l++) {
  p=Yn[i][l];
  Yn[i][l]=Yn[a][l];
  Yn[a][l]=p;
      }
    }
    if (fabs(t)<TOLG) {
      printf("Sistema singular\n");
      return 1;
    }
    for (j=nv+1; j>0; j--) {  /* Basta j>i em vez de j>0 */
      Yn[i][j]/= t;
      p=Yn[i][j];
      if (p!=0)  /* Evita operacoes com zero */
        for (l=1; l<=nv; l++) {  
    if (l!=i)
      Yn[l][j]-=Yn[l][i]*p;
        }
    }
  }
  return 0;
}

int resolversistemaAC(void)
{
  int i,j,l,a;
  double complex t, p;

  for (i=1; i<=nv; i++) {
    t=0.0 + 0.0 * I;
    a=i;
    for (l=i; l<=nv; l++) {
      if (cabs(YnComplex[l][i])>cabs(t)) {
  a=l;
  t=YnComplex[l][i];
      }
    }
    if (i!=a) {
      for (l=1; l<=nv+1; l++) {
  p=YnComplex[i][l];
  YnComplex[i][l]=YnComplex[a][l];
  YnComplex[a][l]=p;
      }
    }
    if (cabs(t)<TOLG) {
      printf("Sistema singular\n");
      return 1;
    }
    for (j=nv+1; j>0; j--) {  /* Basta j>i em vez de j>0 */
      YnComplex[i][j]/= t;
      p=YnComplex[i][j];
      if (cabs(p)!=0.0)  /* Evita operacoes com zero */
        for (l=1; l<=nv; l++) {  
    if (l!=i)
      YnComplex[l][j]-=YnComplex[l][i]*p;
        }
    }
  }
  return 0;
}

/* Rotina que conta os nos e atribui numeros a eles */
int numero(char *nome)
{
  int i,achou;

  i=0; achou=0;
  while (!achou && i<=nv)
    if (!(achou=!strcmp(nome,lista[i]))) i++;
  if (!achou) {
    if (nv==MAX_NOS) {
      printf("O programa so aceita ate %d nos\n",nv);
      exit(1);
    }
    nv++;
    strcpy(lista[nv],nome);
    return nv; /* novo no */
  }
  else {
    return i; /* no ja conhecido */
  }
}



/*void clrscr() {
  #ifdef WINDOWS
  system("cls");
  #endif
  #ifdef LINUX
  system("clear");
  #endif
}*/

int main(void)
{
  //clrscr();
  printf("Programa demonstrativo de analise nodal modificada\n");
  printf("Por Antonio Carlos M. de Queiroz - acmq@coe.ufrj.br\n");
  printf("Versao %s\n",versao);
 denovo:
  /* Leitura do netlist */
  ne=0; nv=0; inc_L=0; inc_C=0; 
  ne_extra=0; nao_linear=0; strcpy(lista[0],"0");
  printf("Nome do arquivo com o netlist (ex: mna.net): ");
  scanf("%50s",nomearquivo);
  arquivo=fopen(nomearquivo,"r");
  if (arquivo==0) {
    printf("Arquivo %s inexistente\n",nomearquivo);
    goto denovo;
  }
  printf("\nAnalise no Ponto de Operacao (P.O.)\n\n");
  printf("Lendo netlist:\n");
  fgets(txt,MAX_LINHA,arquivo);
  printf("Titulo: %s",txt);
  
  char largura[11], comprimento[11], subLarg[10], subComp[10];
  
  
  while (fgets(txt,MAX_LINHA,arquivo)) { //leitura do netlist linha por linha
    ne++; /* Nao usa o netlist[0] */
    if (ne>MAX_ELEM) {
      printf("O programa so aceita ate %d elementos\n",MAX_ELEM);
      exit(1);
    }
    txt[0]=toupper(txt[0]);
    tipo=txt[0];
    sscanf(txt,"%10s",netlist[ne].nome);
    p=txt+strlen(netlist[ne].nome);/* Inicio dos parametros */
    /* O que e lido depende do tipo */
    if (tipo=='R' || tipo=='L' || tipo=='C') {
      sscanf(p,"%10s%10s%lg",na,nb,&netlist[ne].valor);
    if (tipo=='L') {     //substitui a indutancia pela baixa resistencia e armazena a indutancia em outra var
      inc_L++;  
      ind_L[inc_L] = netlist[ne].valor;
      netlist[ne].valor = 1e-9;
      printf("%s %s %s %g\n",netlist[ne].nome,na,nb,ind_L[inc_L]);
    }
    else if (tipo=='C') {     //substitui a capacitancia pela alta resistencia e armazena a capacitancia em outra var
      inc_C++;
          cap_C[inc_C] = netlist[ne].valor;
      netlist[ne].valor = 1e9;
      printf("%s %s %s %g\n",netlist[ne].nome,na,nb,cap_C[inc_C]);
    }
    else{ 
      printf("%s %s %s %g\n",netlist[ne].nome,na,nb,netlist[ne].valor);
    }
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
  }
  else if (tipo=='I' || tipo=='V'){
    sscanf(p,"%10s%10s%lg%lg%lg",na,nb,&netlist[ne].modulo,&netlist[ne].fase,&netlist[ne].valor);
    printf("%s %s %s %g %g %g\n",netlist[ne].nome,na,nb,netlist[ne].modulo,netlist[ne].fase,netlist[ne].valor);
    netlist[ne].a=numero(na);
    netlist[ne].b=numero(nb);
    }
  
  else if (tipo=='K') {
    sscanf(p,"%10s%10s%lg",acop_K[ne].lA,acop_K[ne].lB,&netlist[ne].valor);
    printf("%s %s %s %g\n",netlist[ne].nome,acop_K[ne].lA,acop_K[ne].lB,netlist[ne].valor);
  }
  
    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      sscanf(p,"%10s%10s%10s%10s%lg",na,nb,nc,nd,&netlist[ne].valor);
      printf("%s %s %s %s %s %g\n",netlist[ne].nome,na,nb,nc,nd,netlist[ne].valor);
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
      netlist[ne].c=numero(nc);
      netlist[ne].d=numero(nd);
    }
    else if (tipo=='O') {
      sscanf(p,"%10s%10s%10s%10s",na,nb,nc,nd);
      printf("%s %s %s %s %s\n",netlist[ne].nome,na,nb,nc,nd);
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
      netlist[ne].c=numero(nc);
      netlist[ne].d=numero(nd);
    }
    
    else if (tipo=='M') {
      srand(time(NULL));
      nao_linear++;
      sscanf(p,"%10s%10s%10s%10s%10s%10s%10s%lg%lg%lg%lg%lg%lg",na,nb,nc,nd,mos[ne].tipo,comprimento,largura,&mos[ne].transK,&mos[ne].vt0,&mos[ne].lambda,&mos[ne].gama,&mos[ne].phi,&mos[ne].ld);
      
      if (mos[ne].tipo[0]=='N')
      mos[ne].cox=(2*mos[ne].transK)/0.05;
    else if (mos[ne].tipo[0]=='P')
      mos[ne].cox=(2*mos[ne].transK)/0.02;
      
    //retira os termos "L=" e "W="
    strncpy(subLarg, largura + 2, 9);
      strncpy(subComp, comprimento + 2, 9);
      subLarg[9] = '\0';
      subComp[9] = '\0';
      sscanf(subLarg, "%lg", &mos[ne].lg);
      sscanf(subComp, "%lg", &mos[ne].cp);
      
    printf("%s %s %s %s %s %s %g %g %g %g %g %g %g %g\n",netlist[ne].nome,na,nb,nc,nd,mos[ne].tipo,mos[ne].cp,mos[ne].lg,mos[ne].transK,mos[ne].vt0,mos[ne].lambda,mos[ne].gama,mos[ne].phi,mos[ne].ld);
      //TransistorMOS: M<nome> <nód> <nóg> <nós> <nób> <NMOS ou PMOS> L=<comprimento> W=<largura> <K> <Vt0> <lambda> <gama> <phi> <Ld>
      
      for(i=1;i<=4;i++){ //preenche todos os campos dos elementos extras lineares com os parâmetros do transistor
        strcpy(mos[ne+i].tipo,mos[ne].tipo);
      mos[ne+i].cp=mos[ne].cp;
      mos[ne+i].lg=mos[ne].lg;
      mos[ne+i].transK=mos[ne].transK;
      mos[ne+i].vt0=mos[ne].vt0;
      mos[ne+i].lambda=mos[ne].lambda;
      mos[ne+i].gama=mos[ne].gama;
      mos[ne+i].phi=mos[ne].phi;
      mos[ne+i].ld=mos[ne].ld;
    }
      
      vd[nao_linear][0]=rand()%10; vd[nao_linear][0]=vd[nao_linear][0]/10;
      vg[nao_linear][0]=rand()%10; vg[nao_linear][0]=vg[nao_linear][0]/10;
      vs[nao_linear][0]=rand()%10; vs[nao_linear][0]=vs[nao_linear][0]/10; 
      //vb[nao_linear][0]=rand()%10; vb[nao_linear][0]=vb[nao_linear][0]/10; 
      vb[nao_linear][0]=mos[ne].phi/2+vs[nao_linear][0]; //valores iniciais aleatórios entre 0 e 1 para as tensões
      vt[nao_linear][0]=mos[ne].vt0+mos[ne].gama*(sqrt(mos[ne].phi-(vb[nao_linear][0]-vs[nao_linear][0]))-sqrt(mos[ne].phi));
     ne++;
    //resistor RDS
      strcpy(netlist[ne].nome,"MRGds");
      netlist[ne].a=numero(na); tensaoMOS[nao_linear][0]=netlist[ne].a; // 0 -> vd associado ao numero do no pela 1 vez
      netlist[ne].b=numero(nc); tensaoMOS[nao_linear][2]=netlist[ne].b; // 2 -> vs associado ao numero do no pela 1 vez
      netlist[ne].valor=verMOSCond();
      
      ne++;
      //transcondutancia Gm
      strcpy(netlist[ne].nome,"MGm");
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nc);
      netlist[ne].c=numero(nb); tensaoMOS[nao_linear][1]=netlist[ne].c; // 1 -> vg associado ao numero do no pela 1 vez
      netlist[ne].d=numero(nc);
      netlist[ne].valor=verMOSCond();
      
      ne++;
      //transcondutancia Gmb
      strcpy(netlist[ne].nome,"MGmb");
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nc);
      netlist[ne].c=numero(nd); tensaoMOS[nao_linear][3]=netlist[ne].c; // 3 -> vb associado ao numero do no pela 1 vez
      netlist[ne].d=numero(nc);
      netlist[ne].valor=verMOSCond();
      
      ne++;
      //fonte de corrente I0
      strcpy(netlist[ne].nome,"MIds");
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nc);
      
    if(mos[ne].tipo[0]=='N' || (mos[ne].tipo[0]=='P' && mos[ne].invertido==1 )){
    netlist[ne].valor= verMOSCond()-netlist[ne-2].valor*(vg[nao_linear][0]-vs[nao_linear][0])-netlist[ne-1].valor*(vb[nao_linear][0]-vs[nao_linear][0])-netlist[ne-3].valor*(vd[nao_linear][0]-vs[nao_linear][0]); //I0 = id - Gm*vgs - Gmb*vbs - Gds*vds
      }
      //else if(mos[ne].tipo[0]=='P'|| (mos[ne].tipo[0]=='N' && mos[ne].invertido==1)) {
  //  netlist[ne].valor= verMOSCond()-netlist[ne-2].valor*(vs[nao_linear][0]-vg[nao_linear][0])-netlist[ne-1].valor*(vs[nao_linear][0]-vb[nao_linear][0])-netlist[ne-3].valor*(vs[nao_linear][0]-vd[nao_linear][0]); //I0 = id - Gm*vgs - Gmb*vbs - Gds*vds
      //} 
      ne++;
      //capacitancia CGD
      strcpy(netlist[ne].nome,"MCgd");
      netlist[ne].a=numero(nb);
      netlist[ne].b=numero(na);
      netlist[ne].valor=1e9;

      ne++;
      //capacitancia CGS
      strcpy(netlist[ne].nome,"MCgs");
      netlist[ne].a=numero(nb);
      netlist[ne].b=numero(nc);
      netlist[ne].valor=1e9;
      
      ne++;
    //capacitancia CGB
      strcpy(netlist[ne].nome,"MCgb");
      netlist[ne].a=numero(nb);
      netlist[ne].b=numero(nd);
      netlist[ne].valor=1e9;    
    }
    
    else if (tipo=='*') { /* Comentario comeca com "*" */
      printf("Comentario: %s",txt);
      ne--;
    }
    else {
      printf("Elemento desconhecido: %s\n",txt);
      getch();
      exit(1);
    }
  }
 
  
  fclose(arquivo);
  /* Acrescenta variaveis de corrente acima dos nos, anotando no netlist */
  nn=nv;
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O' || tipo=='L') {
      nv++;
      if (nv>MAX_NOS) {
        printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv],"j"); /* Tem espaco para mais dois caracteres */
      strcat(lista[nv],netlist[i].nome);
      netlist[i].x=nv;
    }
    else if (tipo=='H') {
      nv=nv+2;
      if (nv>MAX_NOS) {
        printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv-1],"jx"); strcat(lista[nv-1],netlist[i].nome);
      netlist[i].x=nv-1;
      strcpy(lista[nv],"jy"); strcat(lista[nv],netlist[i].nome);
      netlist[i].y=nv;
    }
  }
  getch();
  /* Lista tudo */
  printf("Variaveis internas: \n");
  for (i=0; i<=nv; i++)
    printf("%d -> %s\n",i,lista[i]);
  getch();
   /* Monta o sistema nodal modificado */
  if(nao_linear>0) {
    printf("O circuito e nao linear. Seu modelo linearizado tem %d nos, %d variaveis, %d elementos lineares e %d elementos nao lineares (que se decompoe em %d elementos linearizados)., com ne=%d\n",nn,nv,ne-8*nao_linear,nao_linear,nao_linear*7,ne);
  }
  else {
    printf("O circuito e linear.  Tem %d nos, %d variaveis e %d elementos\n",nn,nv,ne);
  }
  getch();
  /* Zera sistema */
  for (i=0; i<=nv; i++) {
    for (j=0; j<=nv+1; j++)
      Yn[i][j]=0;
  }
  /* Monta estampas */
  while(fim==0){
      nao_linear=0;
    for (i=1; i<=ne; i++) {
        tipo=netlist[i].nome[0];
        if (tipo=='R' || tipo=='C' ) {
          g=1/netlist[i].valor;
          Yn[netlist[i].a][netlist[i].a]+=g;
          Yn[netlist[i].b][netlist[i].b]+=g;
          Yn[netlist[i].a][netlist[i].b]-=g;
          Yn[netlist[i].b][netlist[i].a]-=g;
        }
        else if (tipo=='L'){//estampa do indutor controlado a corrente (P.O.)
          g=netlist[i].valor;
          Yn[netlist[i].a][netlist[i].x]+=1;
          Yn[netlist[i].b][netlist[i].x]-=1;
          Yn[netlist[i].x][netlist[i].a]-=1;
          Yn[netlist[i].x][netlist[i].b]+=1;
          Yn[netlist[i].x][netlist[i].x]+=g;
      }
        else if (tipo=='G') {
          g=netlist[i].valor;
          Yn[netlist[i].a][netlist[i].c]+=g;
          Yn[netlist[i].b][netlist[i].d]+=g;
          Yn[netlist[i].a][netlist[i].d]-=g;
          Yn[netlist[i].b][netlist[i].c]-=g;
          
        }
        else if (tipo=='I') {
          g=netlist[i].valor;
          Yn[netlist[i].a][nv+1]-=g;
          Yn[netlist[i].b][nv+1]+=g;
        }
        else if (tipo=='V') {
          Yn[netlist[i].a][netlist[i].x]+=1;
          Yn[netlist[i].b][netlist[i].x]-=1;
          Yn[netlist[i].x][netlist[i].a]-=1;
          Yn[netlist[i].x][netlist[i].b]+=1;
          Yn[netlist[i].x][nv+1]-=netlist[i].valor;
        }
        else if (tipo=='E') {
          g=netlist[i].valor;
          Yn[netlist[i].a][netlist[i].x]+=1;
          Yn[netlist[i].b][netlist[i].x]-=1;
          Yn[netlist[i].x][netlist[i].a]-=1;
          Yn[netlist[i].x][netlist[i].b]+=1;
          Yn[netlist[i].x][netlist[i].c]+=g;
          Yn[netlist[i].x][netlist[i].d]-=g;
        }
        else if (tipo=='F') {
          g=netlist[i].valor;
          Yn[netlist[i].a][netlist[i].x]+=g;
          Yn[netlist[i].b][netlist[i].x]-=g;
          Yn[netlist[i].c][netlist[i].x]+=1;
          Yn[netlist[i].d][netlist[i].x]-=1;
          Yn[netlist[i].x][netlist[i].c]-=1;
          Yn[netlist[i].x][netlist[i].d]+=1;
        }
        else if (tipo=='H') {
          g=netlist[i].valor;
          Yn[netlist[i].a][netlist[i].y]+=1;
          Yn[netlist[i].b][netlist[i].y]-=1;
          Yn[netlist[i].c][netlist[i].x]+=1;
          Yn[netlist[i].d][netlist[i].x]-=1;
          Yn[netlist[i].y][netlist[i].a]-=1;
          Yn[netlist[i].y][netlist[i].b]+=1;
          Yn[netlist[i].x][netlist[i].c]-=1;
          Yn[netlist[i].x][netlist[i].d]+=1;
          Yn[netlist[i].y][netlist[i].x]+=g;
        }
        else if (tipo=='O') {
          Yn[netlist[i].a][netlist[i].x]+=1;
          Yn[netlist[i].b][netlist[i].x]-=1;
          Yn[netlist[i].x][netlist[i].c]+=1;
          Yn[netlist[i].x][netlist[i].d]-=1;
        }
      else if (tipo=='M') {
          nao_linear++;//trabalhar o nao_linear 
        if(contador>1){//entra aqui apenas a partir da segunda iteração do Newton-Raphson
          for(j=0;j<=3;j++){
                if(j==0 && tensaoMOS[nao_linear][j]==netlist[i].a){                     
                     if (convergencia[4*nao_linear-3] == 0 && contador % 1000 == 0){vd[nao_linear][0] = rand()%21 - 10;}
                     else {vd[nao_linear][0] = vd[nao_linear][1];}
                } 
              
                else if(j==1 && tensaoMOS[nao_linear][j]==netlist[i].c){            
                     if (convergencia[4*nao_linear-2] == 0 && contador % 1000 == 0){vg[nao_linear][0] = rand()%21 - 10;}
                     else {vg[nao_linear][0] = vg[nao_linear][1];}  
                }
              
                else if(j==2 && tensaoMOS[nao_linear][j]==netlist[i].b){            
                     if (convergencia[4*nao_linear-1] == 0 && contador % 1000 == 0){vs[nao_linear][0] = rand()%21 - 10;}
                     else {vs[nao_linear][0] = vs[nao_linear][1];}
                }
          
                            else if(j==3 && tensaoMOS[nao_linear][j]==netlist[i].c){
                     if (convergencia[4*nao_linear] == 0 && contador % 1000 == 0){vb[nao_linear][0] = rand()%21 - 10;}
                     else {vb[nao_linear][0] = vb[nao_linear][1];}
                }
          }
          
            if (fabs(vb[nao_linear][0]-vs[nao_linear][0])>(mos[i].phi)/2){
              vt[nao_linear][0]=mos[i].vt0+mos[i].gama*(sqrt((mos[i].phi)/2)-sqrt(mos[i].phi));
            }
            else {
            vt[nao_linear][0]=mos[i].vt0+mos[i].gama*(sqrt(mos[i].phi-(vb[nao_linear][0]-vs[nao_linear][0]))-sqrt(mos[i].phi));
            }
            netlist[i].valor=verMOSCond();
            
        }
        
        g=netlist[i].valor;  
          
        if(strcmp(netlist[i].nome,"MRGds")==0){
        Yn[netlist[i].a][netlist[i].a]+=g;
            Yn[netlist[i].b][netlist[i].b]+=g;
              Yn[netlist[i].a][netlist[i].b]-=g;
              Yn[netlist[i].b][netlist[i].a]-=g;  
        }
        else if(strcmp(netlist[i].nome,"MIds")==0){
          Yn[netlist[i].a][nv+1]-=g;
              Yn[netlist[i].b][nv+1]+=g;
              }
        else if(strcmp(netlist[i].nome,"MGm")==0){
          Yn[netlist[i].a][netlist[i].c]+=g;
              Yn[netlist[i].b][netlist[i].d]+=g;
              Yn[netlist[i].a][netlist[i].d]-=g;
              Yn[netlist[i].b][netlist[i].c]-=g;
        }
        else if(strcmp(netlist[i].nome,"MGmb")==0){
          Yn[netlist[i].a][netlist[i].c]+=g;
              Yn[netlist[i].b][netlist[i].d]+=g;
              Yn[netlist[i].a][netlist[i].d]-=g;
              Yn[netlist[i].b][netlist[i].c]-=g;
        }
        else if(strcmp(netlist[i].nome,"MCgd")==0){//não esquecer de manter os mesmos valores prs capacitores!!!
          g=1e-9;
          Yn[netlist[i].a][netlist[i].a]+=g;
            Yn[netlist[i].b][netlist[i].b]+=g;
            Yn[netlist[i].a][netlist[i].b]-=g;
            Yn[netlist[i].b][netlist[i].a]-=g;
        }
        else if(strcmp(netlist[i].nome,"MCgs")==0){//não esquecer de manter os mesmos valores prs capacitores!!!
          g=1e-9;
          Yn[netlist[i].a][netlist[i].a]+=g;
            Yn[netlist[i].b][netlist[i].b]+=g;
            Yn[netlist[i].a][netlist[i].b]-=g;
            Yn[netlist[i].b][netlist[i].a]-=g;
        }
        else if(strcmp(netlist[i].nome,"MCgb")==0){//não esquecer de manter os mesmos valores prs capacitores!!!
          g=1e-9;
          Yn[netlist[i].a][netlist[i].a]+=g;
            Yn[netlist[i].b][netlist[i].b]+=g;
            Yn[netlist[i].a][netlist[i].b]-=g;
            Yn[netlist[i].b][netlist[i].a]-=g;
            nao_linear++;
        }
        nao_linear--;
      }
    /*  if (netlist[i].nome[0] != 'K') {
    #ifdef DEBUG
          // Opcional: Mostra o sistema apos a montagem da estampa 
          printf("Sistema apos a estampa de %s\n",netlist[i].nome);
          for (k=1; k<=nv; k++) {
            for (j=1; j<=nv+1; j++)
              if (Yn[k][j]!=0) printf("%+3.1f ",Yn[k][j]);
              else printf(" ... ");
            printf("\n");
          }
          getch();
      }
    #endif*/
    }
    /* Resolve o sistema */
    if (resolversistema()) {
      getch();
      exit;
    }
  /*#ifdef DEBUG
    /* Opcional: Mostra o sistema resolvido
    printf("Sistema resolvido:\n");
    for (i=1; i<=nv; i++) {
        for (j=1; j<=nv+1; j++)
          if (Yn[i][j]!=0) printf("%+3.1f ",Yn[i][j]);
          else printf(" ... ");
        printf("\n");
      }
    getch();
  #endif*/
  
      for(k=1;k<=nao_linear;k++) {//inverti o for de k com o for de i, na minha cabeça faz mais sentido
      
        
      /*se nv estiver associada a alguma das 4 tensóes de cada um dos MOSFETS*/
      /*i: roda o numero de variáveis do sistema, j: roda as 4 tensões de cada MOS, k: roda o numero de MOS(qtde de elementos nao lineares no circuito)*/
       for (i=1; i<=ne; i++){
        if (netlist[i].nome[0]=='M'){//criado por Lucas as 01:00
        
              for(j=0;j<=3;j++){
  
          if(j==0 && tensaoMOS[k][j]== netlist[i].a ){//incrementar o i, pq ele está rodando cada M do netlist
              vd[k][1]=Yn[i][nv+1];
              if (((vd[k][1]) > 1) && (fabs((vd[k][1]-vd[k][0])/vd[k][1]) < 1e-12))
                  {convergencia[4*k-3] = 1;}
              else if ((vd[k][1] <= 1) && (fabs(vd[k][1]-vd[k][0])<1e-12))
                  {convergencia[4*k-3] = 1;}                  
              else {
                  (convergencia[4*k-3] = 0);
                  vd[k][0] = vd[k][1];
              }
          }
            
          else if(j==1 && tensaoMOS[k][j]==netlist[i].c ){
            vg[k][1]=Yn[i][nv+1];
            if (((vg[k][1]) > 1) && (fabs((vg[k][1]-vg[k][0])/vg[k][1]) < 1e-12))
                {convergencia[4*k-2] = 1;}
            else if ((vg[k][1] <= 1) && (fabs(vg[k][1]-vg[k][0])<1e-12))
                {convergencia[4*k-2] = 1;}                  
            else {
               (convergencia[4*k-2] = 0);
               vg[k][0] = vg[k][1];
            }
          }
            
        else if(j==2 && tensaoMOS[k][j]==netlist[i].b){
            vs[k][1]=Yn[i][nv+1];
            if (((vs[k][1]) > 1) && (fabs((vs[k][1]-vs[k][0])/vs[k][1]) < 1e-12))
                        {convergencia[4*k-1] = 1;}
                      else if ((vs[k][1] <= 1) && (fabs(vs[k][1]-vs[k][0])<1e-12))
                            {convergencia[4*k-1] = 1;}                  
                          else 
                              {(convergencia[4*k-1] = 0);
                                vs[k][0] = vs[k][1];}
          }
        else if(j==3 && tensaoMOS[k][j]==netlist[i].c){
            vb[k][1]=Yn[i][nv+1];
            if (((vb[k][1]) > 1) && (fabs((vb[k][1]-vb[k][0])/vb[k][1]) < 1e-12))
                        {convergencia[4*k] = 1;}
                      else if ((vb[k][1] <= 1) && (fabs(vb[k][1]-vb[k][0])<1e-12))
                            {convergencia[4*k] = 1;}      
                          else 
                              {(convergencia[4*k] = 0);
                                vb[k][0] = vb[k][1];}
          }
        
      }

      } 
      
    }
              
    }
    contador++;
    for (i = 1; (i <= (4*nao_linear))&&(i != -1);){
      if (convergencia[i] == 1) {i++;}
      else {i = -1;}
    }
    //printf("FIM %d, contador %d",fim, contador);
    if (i == 4*nao_linear){fim = 1;}

    if (contador==10000){fim =1;}
    
  }
  printf("Netlist interno final:\n");
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
    if (tipo=='R'|| tipo=='C') {
      printf("%s %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].valor);
    }
    else if (tipo=='I' || tipo=='V'){
      printf("%s %d %d %g %g %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].modulo,netlist[i].fase,netlist[i].valor);
    }
    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      printf("%s %d %d %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d,netlist[i].valor);
    }
    else if (tipo=='O') {
      printf("%s %d %d %d %d\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d);
    }
    else if (tipo=='K') {
      printf("%s %s %s %g\n",netlist[i].nome,acop_K[i].lA,acop_K[i].lB,netlist[i].valor);
    }
  
  else if (tipo=='M') {
    if(strcmp(netlist[i].nome,"MRGds")==0){
      printf("%s %d %d %e\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].valor);
    }
    else if(strcmp(netlist[i].nome,"MIds")==0){
      printf("%s %d %d %e\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].valor);
    }
    else if(strcmp(netlist[i].nome,"MGm")==0){
      printf("%s %d %d %d %d %e\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d,netlist[i].valor);
    }
    else if(strcmp(netlist[i].nome,"MGmb")==0){
      printf("%s %d %d %d %d %e\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d,netlist[i].valor);
    }
    else if(netlist[i].nome[1]=='C'){
      printf("%s %d %d %e\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].valor);
    }
  }
  
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O' || tipo=='L')
      printf("Corrente jx: %d\n",netlist[i].x);
    else if (tipo=='H')
      printf("Correntes jx e jy: %d, %d\n",netlist[i].x,netlist[i].y);
  }
  getch();
  printf("\n%d iteracoes foram realizadas.\n",contador);
  contador=0;
  printf("\n%d Elementos nao lineares\n",nao_linear);
  for(j = 1; j <= 4*nao_linear; j++){
    if (convergencia[j] == 0){contador++;}
  }
  for(j=1;j<=4*nao_linear;j++){
    printf("\n Convergencia %d %d",j,convergencia[j]);
  }
  if(contador!=0)
    printf("\n%d solucoes nao convergiram. Ultima solucao do sistema:\n",contador);
  else
    printf("Solucao do Ponto de Operacao:\n");

  strcpy(txt,"Tensao");
  for (i=1; i<=nv; i++) {
    if (i==nn+1) strcpy(txt,"Corrente");
    printf("%s %s: %g\n",txt,lista[i],Yn[i][nv+1]);
  }
  
  
  printf("\nAnalise de Resposta em Frequencia:\n");
  
  inc_L=0; inc_C=0;
  
  
  for (i=0; i<=nv; i++) {
      for (j=0; j<=nv+1; j++)
        Yn[i][j]=0;
    }
  
  for (i=1; i<=ne; i++) {
        tipo = netlist[i].nome[0];
        if (tipo=='R') {
          g=1/netlist[i].valor;
          YnComplex[netlist[i].a][netlist[i].a]+=g;
          YnComplex[netlist[i].b][netlist[i].b]+=g;
          YnComplex[netlist[i].a][netlist[i].b]-=g;
          YnComplex[netlist[i].b][netlist[i].a]-=g;
        }
        else if (tipo=='C' ) {//estampa do capacitor (resp em freq)
          inc_C++;      
          gComplex=2*PI*frequencia*cap_C[inc_C]*I;
          YnComplex[netlist[i].a][netlist[i].a]+=gComplex;
          YnComplex[netlist[i].b][netlist[i].b]+=gComplex;
          YnComplex[netlist[i].a][netlist[i].b]-=gComplex;
          YnComplex[netlist[i].b][netlist[i].a]-=gComplex;
        }
        else if (tipo=='L'){//estampa do indutor controlado a corrente (resp em freq)
          inc_L++;
          gComplex=1/(2*PI*frequencia*ind_L[inc_L]*I);
          YnComplex[netlist[i].a][netlist[i].x]+=1;
          YnComplex[netlist[i].b][netlist[i].x]-=1;
          YnComplex[netlist[i].x][netlist[i].a]-=1;
          YnComplex[netlist[i].x][netlist[i].b]+=1;
          YnComplex[netlist[i].x][netlist[i].x]+=gComplex;
        }
        else if (tipo=='G') {
          g=netlist[i].valor;
          YnComplex[netlist[i].a][netlist[i].c]+=g;
          YnComplex[netlist[i].b][netlist[i].d]+=g;
          YnComplex[netlist[i].a][netlist[i].d]-=g;
          YnComplex[netlist[i].b][netlist[i].c]-=g;
          
        }
        else if (tipo=='I') {
            YnComplex[netlist[i].a][nv+1]= -(netlist[ne].modulo*cos(netlist[ne].fase) + I*netlist[ne].modulo*sin(netlist[ne].fase));
            YnComplex[netlist[i].b][nv+1]= netlist[ne].modulo*cos(netlist[ne].fase) + I*netlist[ne].modulo*sin(netlist[ne].fase);
        }
        else if (tipo=='V') {
            YnComplex[netlist[i].a][netlist[i].x]+=1;
            YnComplex[netlist[i].b][netlist[i].x]-=1;
            YnComplex[netlist[i].x][netlist[i].a]-=1;
            YnComplex[netlist[i].x][netlist[i].b]+=1;
            YnComplex[netlist[i].x][nv+1]= -(netlist[ne].modulo*cos(netlist[ne].fase) + I*netlist[ne].modulo*sin(netlist[ne].fase)); 
        }
        else if (tipo=='E') {
          g=netlist[i].valor;
          YnComplex[netlist[i].a][netlist[i].x]+=1;
          YnComplex[netlist[i].b][netlist[i].x]-=1;
          YnComplex[netlist[i].x][netlist[i].a]-=1;
          YnComplex[netlist[i].x][netlist[i].b]+=1;
          YnComplex[netlist[i].x][netlist[i].c]+=g;
          YnComplex[netlist[i].x][netlist[i].d]-=g;
        }
        else if (tipo=='F') {
          g=netlist[i].valor;
          YnComplex[netlist[i].a][netlist[i].x]+=g;
          YnComplex[netlist[i].b][netlist[i].x]-=g;
          YnComplex[netlist[i].c][netlist[i].x]+=1;
          YnComplex[netlist[i].d][netlist[i].x]-=1;
          YnComplex[netlist[i].x][netlist[i].c]-=1;
          YnComplex[netlist[i].x][netlist[i].d]+=1;
        }
        else if (tipo=='H') {
          g=netlist[i].valor;
          YnComplex[netlist[i].a][netlist[i].y]+=1;
          YnComplex[netlist[i].b][netlist[i].y]-=1;
          YnComplex[netlist[i].c][netlist[i].x]+=1;
          YnComplex[netlist[i].d][netlist[i].x]-=1;
          YnComplex[netlist[i].y][netlist[i].a]-=1;
          YnComplex[netlist[i].y][netlist[i].b]+=1;
          YnComplex[netlist[i].x][netlist[i].c]-=1;
          YnComplex[netlist[i].x][netlist[i].d]+=1;
          YnComplex[netlist[i].y][netlist[i].x]+=g;
        }
        else if (tipo=='O') {
          Yn[netlist[i].a][netlist[i].x]+=1;
          Yn[netlist[i].b][netlist[i].x]-=1;
          Yn[netlist[i].x][netlist[i].c]+=1;
          Yn[netlist[i].x][netlist[i].d]-=1;
        }
      else if (tipo=='M'){   
        g=netlist[i].valor;  
          
        if(strcmp(netlist[i].nome,"MRGds")==0){
            YnComplex[netlist[i].a][netlist[i].a]+=g;
            YnComplex[netlist[i].b][netlist[i].b]+=g;
            YnComplex[netlist[i].a][netlist[i].b]-=g;
            YnComplex[netlist[i].b][netlist[i].a]-=g;  
        }
        else if(strcmp(netlist[i].nome,"MIds")==0){
            YnComplex[netlist[i].a][nv+1]=0;
            YnComplex[netlist[i].b][nv+1]=0;
        }
        else if(strcmp(netlist[i].nome,"MGm")==0){
              YnComplex[netlist[i].a][netlist[i].c]+=g;
              YnComplex[netlist[i].b][netlist[i].d]+=g;
              YnComplex[netlist[i].a][netlist[i].d]-=g;
              YnComplex[netlist[i].b][netlist[i].c]-=g;
        }
        else if(strcmp(netlist[i].nome,"MGmb")==0){
              YnComplex[netlist[i].a][netlist[i].c]+=g;
              YnComplex[netlist[i].b][netlist[i].d]+=g;
              YnComplex[netlist[i].a][netlist[i].d]-=g;
              YnComplex[netlist[i].b][netlist[i].c]-=g;
        }
        else if(strcmp(netlist[i].nome,"MCgd")==0){//não esquecer de manter os mesmos valores prs capacitores!!!
          gComplex=2*PI*frequencia*mos[i].cgd*I;
          YnComplex[netlist[i].a][netlist[i].a]+=gComplex;
          YnComplex[netlist[i].b][netlist[i].b]+=gComplex;
          YnComplex[netlist[i].a][netlist[i].b]-=gComplex;
          YnComplex[netlist[i].b][netlist[i].a]-=gComplex;
        }
        else if(strcmp(netlist[i].nome,"MCgs")==0){//não esquecer de manter os mesmos valores prs capacitores!!!
          gComplex=2*PI*frequencia*mos[i].cgs*I;
          YnComplex[netlist[i].a][netlist[i].a]+=gComplex;
          YnComplex[netlist[i].b][netlist[i].b]+=gComplex;
          YnComplex[netlist[i].a][netlist[i].b]-=gComplex;
          YnComplex[netlist[i].b][netlist[i].a]-=gComplex;
        }
        else if(strcmp(netlist[i].nome,"MCgb")==0){
          gComplex=2*PI*frequencia*mos[i].cbg*I;
          YnComplex[netlist[i].a][netlist[i].a]+=gComplex;
          YnComplex[netlist[i].b][netlist[i].b]+=gComplex;
          YnComplex[netlist[i].a][netlist[i].b]-=gComplex;
          YnComplex[netlist[i].b][netlist[i].a]-=gComplex;
        }
      }
      else if (tipo=='K'){
        fim = 0;
        for (indice = 1; indice <= ne && fim != 2; indice++){
            if(strcmp(acop_K[i].lA, netlist[indice].nome) == 0){
                fim++;
                valorLA = netlist[i].valor;
            }
            else if(strcmp(acop_K[i].lB, netlist[indice].nome) == 0){
                fim++;
                valorLB = netlist[i].valor;
            }
        }

        ind_M = netlist[i].valor*(sqrt(valorLA*valorLB));
      
        YnComplex[netlist[i].a][netlist[i].x]+=1;
        YnComplex[netlist[i].b][netlist[i].x]-=1;
        YnComplex[netlist[i].c][netlist[i].y]+=1;
        YnComplex[netlist[i].d][netlist[i].y]-=1;
        YnComplex[netlist[i].x][netlist[i].a]-=1;
        YnComplex[netlist[i].x][netlist[i].b]+=1;
        YnComplex[netlist[i].y][netlist[i].c]-=1;
        YnComplex[netlist[i].y][netlist[i].d]+=1;
        YnComplex[netlist[i].x][netlist[i].x]+=2*PI*frequencia*valorLA*I;
        YnComplex[netlist[i].x][netlist[i].y]+=2*PI*frequencia*ind_M*I;
        YnComplex[netlist[i].y][netlist[i].x]+=2*PI*frequencia*ind_M*I;
        YnComplex[netlist[i].y][netlist[i].y]+=2*PI*frequencia*valorLB*I;
  
    }
  
  }
  
  if (resolversistemaAC()) {
      getch();
      exit;
  } 
  
  getch();

  strcpy(txt,"Tensao");
  for (i=1; i<=nv; i++) {
    if (i==nn+1) strcpy(txt,"Corrente");
    printf("%s %s: %e + %ei \n",txt,lista[i],creal(YnComplex[i][nv+1]),cimag(YnComplex[i][nv+1]));
  }
  return 0;

}
