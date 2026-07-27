/*
 *
 *    Compucorp Alpha 327 emulator
 * 
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    Gergely Gati 2003
 *      email:           gati.gergely@yahoo.com
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#define LINELEN   40

#define DISP_INIT   "          0.0000"
#define DISP_ERR    "   E-----       "

#define CALCEXP(m)  m->mant*(pow(10,(m->exp*m->exp_sign)))

//flags
#define M_ENTERING  (1L<<0)
#define M_ERROR     (1L<<1)
#define M_FRACT     (1L<<2)
#define M_EXP       (1L<<3)
#define M_DPEXP     (1L<<4)

// numop codes
#define C_NONE      0
#define C_PLUS      1
#define C_MINUS     2
#define C_MUL       3
#define C_DIV       4
#define C_POW       5


struct cc327
{
  int stack[12];          // fixed size stack
  int stackp;             // current stackpointer grows up
  char display[22];       // display contents
  double numop;           // extended register
  int numop_code;         // numop register operator code
  double e;               // accumlator
  int e_sign;             // 1 vagy -1 az elojelnek megfeleloen
  double r[12];           // normal registers
  int labels[18];         // labels
  int pc;                 // program counter
  int fract;              // fraction entering (1/10,1/100,1/1000,..)
  double par[4];          // parentheses registers
  int par_code[4];        // parentheses operator codes
  int parpos;             // parentheses stack pointer
  int dp;                 // decimal point position
  unsigned long flags;
  double st[44];          // data storage registers
  double mant;            // exp form
  int exp;                // exp form
  int exp_sign;           // exp form
  int par_flags[5];       // par flags (noexec)
};



static char signs[]=" +-*/^";
static int opt_debug=0;
static int opt_verbose=0;
static FILE *mlog;
static char mnev[512];
static char spc[70];

static void mac_init(struct cc327 *m)
{
  int i;

  for(i=0;i<12;i++) m->stack[i]=0;
  m->stackp=0;
  strcpy(m->display,DISP_INIT);
  m->numop=0;
  m->numop_code=C_NONE;
  m->e=0;
  m->e_sign=1;
  for(i=0;i<12;i++) m->r[i]=0;
  m->fract=1;
  for(i=0;i<4;i++) m->par[i]=0;
  for(i=0;i<4;i++) m->par_code[i]=C_NONE;
  m->parpos=0;
  m->dp=4;
  m->flags=M_ENTERING;
  for(i=0;i<44;i++) m->st[i]=0;
  m->mant=1;
  m->exp=0;
  m->exp_sign=1;
  for(i=0;i<5;i++) m->par_flags[i]=0;
}


static double calcnumop(struct cc327 *m, double op)
{
  double ret=0;

  switch(m->numop_code)
  {
    case C_NONE:
    {
      ret=op;
      break;
    }
    case C_PLUS:
    {
      ret=m->numop+op;
      break;
    }
    case C_MINUS:
    {
      ret=m->numop-op;
      break;
    }
    case C_MUL:
    {
      ret=m->numop*op;
      break;
    }
    case C_DIV:
    {
      ret=m->numop/op;
      break;
    }
    case C_POW:
    {
      ret=pow(m->numop,op);
      break;
    }
    default:
    {
      printf("ERR: UNKNOWN NUMOP_CODE\n");
      break;
    }
  }
  return(ret);
}


static void disp_num(struct cc327 *m)
{
 static char format[16];

  if(m->dp>0)
  {
    sprintf(format,"%%%dd.%%0%dd",15-m->dp,m->dp);
    sprintf(m->display,format,(int)m->e,((int)(abs((int)((m->e-((double)(int)m->e))*pow(10,m->dp+1)))+5))/10);
  }
  else sprintf(m->display,"%16d",(int)m->e);
}


// retval: 0 tovabb, nem nulla - vege
static int execute(struct cc327 *m, int *p, FILE *pr)
{
 static char input[256];
  int ret=0,code,i,noprint;

  if(opt_verbose!=0) printf(".%03d %03d ",m->pc,p[m->pc]);

  noprint=0;
  code=p[m->pc++];
  if(code>14&&code!=34) m->flags&=~(M_ENTERING|M_FRACT|M_EXP);
  switch(code)
  {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    {
      if((m->flags&M_EXP)==0)
      {
        if((m->flags&M_ENTERING)==0)
        {
          m->e=0;
          m->e_sign=1;
          m->flags|=M_ENTERING;
        }
        if((m->flags&M_FRACT)==0)
        {
          m->e*=10;
          m->e+=code*m->e_sign;
        }
        else
        {
          m->e+=(code*(1/(double)m->fract))*m->e_sign;
          m->fract*=10;
        }
      }
      else
      {
        m->exp*=10;
        m->exp+=code;
        if(m->exp>100) m->exp=m->exp%100;
        m->e=CALCEXP(m);
      }
      if(opt_verbose!=0) printf("   %d",code);
      break;
    }
    case 10:
    case 11:
    {
      if((m->flags&M_EXP)==0)
      {
        if((m->flags&M_ENTERING)==0)
        {
          m->e=0;
          m->e_sign=1;
          m->flags|=M_ENTERING;
        }
        if((m->flags&M_FRACT)==0)
        {
          m->e*=10;
          m->e+=(code-2)*m->e_sign;
        }
        else
        {
          m->e+=((code-2)*(1/(double)m->fract))*m->e_sign;
          m->fract*=10;
        }
      }
      else
      {
        m->exp*=10;
        m->exp+=code-2;
        if(m->exp>100) m->exp=m->exp%100;
        m->e=CALCEXP(m);
      }
      if(opt_verbose!=0) printf("   %d",code-2);
      break;
    }
    case 12:     // "."
    {
      if((m->flags&M_EXP)==0&&(m->flags&M_FRACT)==0)
      {
        if((m->flags&M_ENTERING)==0)
        {
          m->e=0;
          m->e_sign=1;
          m->flags|=M_ENTERING;
        }
        m->flags|=M_FRACT;
        m->fract=10;
      }
      if(opt_verbose!=0) printf("d");
      break;
    }
    case 13:     // "S" CHG SIGN
    {
      if((m->flags&M_EXP)==0)
      {
        m->e_sign*=-1;
        m->e*=-1;
      }
      else
      {
        m->exp_sign*=-1;
        m->e=CALCEXP(m);
      }
      if(opt_verbose!=0) printf("S");
      break;
    }
    case 14:     // EXP
    {
      if((m->flags&M_EXP)==0)
      {
        if((m->flags&M_ENTERING)==0) m->e=0;
        m->exp=0;
        m->exp_sign=1;
        m->flags|=M_EXP;
        if(m->e==0) m->e=1;
        m->mant=m->e;
      }
      else
      {
        // EXP allapotban nyomta meg ujra. Mit tegyunk??
        m->exp=0;
        m->exp_sign=1;
      }
      if(opt_verbose!=0) printf("EX");
      break;
    }
    case 20:
    {
      m->e=calcnumop(m,m->e);
      m->par_flags[m->parpos]=1;
      if(opt_verbose!=0) printf("=");
      break;
    }
    case 21:
    {
      if(m->par_flags[m->parpos]==0) m->e=calcnumop(m,m->e);
      m->par_flags[m->parpos]=0;
      m->numop=m->e;
      m->numop_code=C_PLUS;
      if(opt_verbose!=0) printf("+");
      break;
    }
    case 22:
    {
      if(m->par_flags[m->parpos]==0) m->e=calcnumop(m,m->e);
      m->par_flags[m->parpos]=0;
      m->numop=m->e;
      m->numop_code=C_MINUS;
      if(opt_verbose!=0) printf("-");
      break;
    }
    case 23:
    {
      if(m->par_flags[m->parpos]==0) m->e=calcnumop(m,m->e);
      m->par_flags[m->parpos]=0;
      m->numop=m->e;
      m->numop_code=C_MUL;
      if(opt_verbose!=0) printf("x");
      break;
    }
    case 24:
    {
      if(m->par_flags[m->parpos]==0) m->e=calcnumop(m,m->e);
      m->par_flags[m->parpos]=0;
      m->numop=m->e;
      m->numop_code=C_DIV;
      if(opt_verbose!=0) printf("/");
      break;
    }
    case 25:
    {
      if(m->par_flags[m->parpos]==0) m->e=calcnumop(m,m->e);
      m->par_flags[m->parpos]=0;
      m->numop=m->e;
      m->numop_code=C_POW;
      if(opt_verbose!=0) printf("ax");
      break;
    }
    case 26:
    {
      noprint=1;
      for(i=3;i>0;i--)
      {
        m->par[i]=m->par[i-1];
        m->par_code[i]=m->par_code[i-1];
      }
      m->par[0]=m->numop;
      m->par_code[0]=m->numop_code;
      m->parpos++;
      m->par_flags[m->parpos]=1;
      if(opt_verbose!=0) printf("( %d",m->parpos);
      break;
    }
    case 27:
    {
      m->e=calcnumop(m,m->e);
      m->numop=m->par[0];
      m->numop_code=m->par_code[0];
      for(i=0;i<3;i++)
      {
        m->par[i]=m->par[i+1];
        m->par_code[i]=m->par_code[i+1];
      }
      m->par[3]=0;
      m->par_code[3]=C_NONE;
      m->parpos--;
      if(opt_verbose!=0) printf(") %d",m->parpos);
      break;
    }
    case 30:  //ret
    {
      noprint=1;
      if(m->stackp>0)
      {
        m->pc=m->stack[--m->stackp]+1;
        if(opt_verbose!=0) printf("RET");
      }
      else printf("  -- ERR: STACK UNDERFLOW");
      break;
    }
    case 32:
    {
      if(opt_verbose!=0) printf("I D   <%#f     >",m->e);
      sprintf(m->display,"%-16G",m->e);
      noprint=1;
      break;
    }
    case 33:
    {
      float in;
      if(opt_verbose==0) printf(">>%s<<\n",m->display);
      if(opt_verbose!=0) printf(" S  S\n$ ");
      else printf("$ ");
      input[0]='\0';
      fgets(input,255,stdin);
      if(isalpha(input[0])) ret=-1;
      if(input[0]=='\n') input[0]='\0';
      if(input[0]!='\0')
      {
        sscanf(input,"%g",&in);
        m->e=(double)in;
        if(m->e>=0) m->e_sign=1;
        else m->e_sign=-1;
      }
      break;
    }
    case 34:     // "PT" PRINT
    {
      noprint=1;
      if(opt_verbose!=0) printf("PT");
      if(pr!=NULL) fprintf(pr,"%s\n",m->display);
      if(mlog!=NULL) fprintf(mlog,"%s\n",m->display);
      printf("%s        %s",spc,m->display);
      if(opt_verbose==0) printf("\n");
      break;
    }
    case 35:
    {
      noprint=1;
      if(opt_verbose!=0) printf("AD");
      else printf("\n");
      if(mlog!=NULL) fprintf(mlog,"\n");
      break;
    }
    case 37:     // "CL" CLEAR
    {
      if((m->flags&M_ERROR)==0)
      {
        m->e=0;
        m->e_sign=1;
        strcpy(m->display,DISP_INIT);
      }
      if(opt_verbose!=0) printf("CL");
      break;
    }
    case 60:    // ln
    {
      if(m->e>0)
      {
        m->e=log(m->e);
      }
      else strcpy(m->display,DISP_ERR);
      if(opt_verbose!=0) printf("LN");
      break;
    }
    case 61:    // log10
    {
      if(m->e>0)
      {
        m->e=log10(m->e);
      }
      else strcpy(m->display,DISP_ERR);
      if(opt_verbose!=0) printf("LOG");
      break;
    }
    case 62:    // sqrt
    {
      if(m->e>=0)
      {
        m->e=sqrt(m->e);
      }
      else strcpy(m->display,DISP_ERR);
      if(opt_verbose!=0) printf("SQ");
      break;
    }
    case 63:    // 1/x
    {
      if(m->e!=0)
      {
        m->e=((double)1)/m->e;
      }
      else strcpy(m->display,DISP_ERR);
      if(opt_verbose!=0) printf("1/X");
      break;
    }
    case 70:    // sin
    {
      m->e=sin(m->e);
      if(opt_verbose!=0) printf("sin (%g)",m->e);
      break;
    }
    case 71:    // cos
    {
      m->e=cos(m->e);
      if(opt_verbose!=0) printf("cos");
      break;
    }
    case 72:    // tan
    {
      m->e=tan(m->e);
      if(opt_verbose!=0) printf("tan");
      break;
    }
    case 100:   // F0 (clear 1-3 data storage regs)
    {
      noprint=1;
      m->st[0]=0;  // 0,1,2 vagy 1,2,3????
      m->st[1]=0;
      m->st[2]=0;
      if(opt_verbose!=0) printf("f0");
      break;
    }
    case 101:   // e
    {
      m->e=exp(1);
      if(opt_verbose!=0) printf("e");
      break;
    }
    case 102:   // abs
    {
      m->e=(m->e<0?-m->e:m->e);
      if(opt_verbose!=0) printf("ABS");
      break;
    }
    case 105:   // fraction
    {
      m->e=m->e-((int)m->e);
      if(opt_verbose!=0) printf("F");
      break;
    }
    case 106:   // integer
    {
      m->e=(int)m->e;
      if(opt_verbose!=0) printf("I");
      break;
    }
    case 107:   // PI
    {
      m->e=3.14159265358979323846;
      if(opt_verbose!=0) printf("pi");
      break;
    }
    case 110:   // round (dp szerint)
    {
      if((m->flags&M_DPEXP)==0)
      {
        if(m->e>0) m->e=((double)((int)((((double)5)/pow(10,m->dp+1)+m->e)*pow(10,m->dp))))/pow(10,m->dp);
        else if(m->e<0) m->e=-((double)((int)((((double)5)/pow(10,m->dp+1)-m->e)*pow(10,m->dp))))/pow(10,m->dp);
      }
      if(opt_verbose!=0) printf("R (%g)",m->e);
      break;
    }
    case 112:   // dotted line
    {
      noprint=1;
      if(opt_verbose!=0) printf("DT");
      printf("%s...........................",spc);
      if(mlog!=NULL) fprintf(mlog,"................\n");
      if(opt_verbose==0) printf("\n");
      break;
    }
    case 114:
    {
      noprint=1;
      if(opt_verbose!=0) printf("pause [%s]",m->display);
      else printf(">>%s<<\n",m->display);
      break;
    }
    case 137:
    {
      mac_init(m);
      if(opt_verbose!=0) printf("CA");
      break;
    }
    case 160:    // ex
    {
      m->e=exp(m->e);
      if(opt_verbose!=0) printf("ex");
      break;
    }
    case 161:    // 10x
    {
      m->e=pow(10,m->e);
      if(opt_verbose!=0) printf("10x");
      break;
    }
    case 162:    // x2
    {
      m->e*=m->e;
      if(opt_verbose!=0) printf("x2");
      break;
    }
    case 163:    // n!
    {
      if(m->e>=0&&m->e<=69&&m->e==((int)m->e))
      {
        if(m->e!=0)
        {
          i=(int)m->e;
          for(m->e=1;i>=2;i--) m->e*=i;
        }
        else m->e=1;
      }
      else strcpy(m->display,DISP_ERR);
      if(opt_verbose!=0) printf("n!");
      break;
    }
    case 170:    // asin
    {
      m->e=asin(m->e);
      if(opt_verbose!=0) printf("asin");
      break;
    }
    case 171:    // acos
    {
      m->e=acos(m->e);
      if(opt_verbose!=0) printf("acos");
      break;
    }
    case 172:    // atan
    {
      m->e=atan(m->e);
      if(opt_verbose!=0) printf("atan (%g)",m->e);
      break;
    }
    case 200:   // labels (0-7)
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
    {
      noprint=1;
      if(opt_verbose!=0) printf("L %d",code-200);
      break;
    }
    case 210:   // labels (8-15)
    case 211:
    case 212:
    case 213:
    case 214:
    case 215:
    case 216:
    case 217:
    {
      noprint=1;
      if(opt_verbose!=0) printf("L %d",code-202);
      break;
    }
    case 220:
    case 221:
    case 222:
    case 223:
    case 224:
    case 225:
    case 226:
    case 227:
    case 228:
    case 229:
    case 230:
    case 231:
    case 232:
    case 233:
    {
      m->dp=code-220;
      if(opt_verbose!=0) printf("DP %d",m->dp);
      break;
    }
    case 234:   // set dp + exp
    {
      m->flags^=M_DPEXP;
      if(opt_verbose!=0) printf("DP EX");
      break;
    }
    case 300:
    case 301:
    case 302:
    case 303:
    case 304:
    case 305:
    {
      int reg,ind=0;
      double v=0;

      noprint=1;
      reg=p[m->pc];
      if(reg>=100) { ind=1; reg=(int)m->st[reg-100]; }
      if(reg>=0&&reg<44)
      {
        switch(code-300)
        {
          case C_NONE:
          {
            v=m->st[reg]=m->e;
            break;
          }
          case C_PLUS:
          {
            v=m->st[reg]+=m->e;
            break;
          }
          case C_MINUS:
          {
            v=m->st[reg]-=m->e;
            break;
          }
          case C_MUL:
          {
            v=m->st[reg]*=m->e;
            break;
          }
          case C_DIV:
          {
            v=m->st[reg]/=m->e;
            break;
          }
          case C_POW:
          {
            v=m->st[reg]=pow(m->st[reg],m->e);
            break;
          }
        }
        if(opt_verbose!=0) printf("ST%c %c%02d   (%g)",signs[code-300],(ind==0?' ':'i'),reg,v);
      }
      else printf("ST -- INVALID PARAMETER reg=%d [%c%03d]",reg,(ind==0?' ':'i'),p[m->pc]);
      m->pc++;
      break;
    }
    case 310:
    case 311:
    case 312:
    case 313:
    case 314:
    case 315:
    {
      int reg,ind=0;
      double v=0;

      reg=p[m->pc];
      if(reg>=100) { ind=1; reg=(int)m->st[reg-100]; }
      if(reg>=0&&reg<44)
      {
        switch(code-310)
        {
          case C_NONE:
          {
            v=m->e=m->st[reg];
            break;
          }
          case C_PLUS:
          {
            v=m->e+=m->st[reg];
            break;
          }
          case C_MINUS:
          {
            v=m->e-=m->st[reg];
            break;
          }
          case C_MUL:
          {
            v=m->e*=m->st[reg];
            break;
          }
          case C_DIV:
          {
            v=m->e/=m->st[reg];
            break;
          }
          case C_POW:
          {
            v=m->e=pow(m->e,m->st[reg]);
            break;
          }
        }
        if(opt_verbose!=0) printf("RC%c %c%02d  (%g)",signs[code-310],(ind==0?' ':'i'),reg,v);
      }
      else printf("RC -- INVALID PARAMETER [%03d]",p[m->pc]);
      m->pc++;
      break;
    }
    case 320:
    case 321:
    case 322:
    case 323:
    case 324:
    case 325:
    {
      int reg,ind=0;
      double v=0;

      reg=p[m->pc];
      if(reg>=100) { ind=1; reg=(int)m->st[reg-100]; }
      if(reg>=0&&reg<44)
      {
        switch(code-320)
        {
          case C_NONE:
          {
            double t;
            t=m->e;
            m->e=m->st[reg];
            m->st[reg]=t;
            break;
          }
          case C_PLUS:
          {
            v=m->st[reg]=m->e=m->st[reg]+m->e;
            break;
          }
          case C_MINUS:
          {
            v=m->st[reg]=m->e=m->st[reg]-m->e;
            break;
          }
          case C_MUL:
          {
            v=m->st[reg]=m->e=(m->st[reg]*m->e);
            break;
          }
          case C_DIV:
          {
            v=m->st[reg]=m->e=m->st[reg]/m->e;
            break;
          }
          case C_POW:
          {
            v=m->st[reg]=m->e=pow(m->st[reg],m->e);
            break;
          }
        }
        if(opt_verbose!=0) printf("XC%c %c%02d  (%g)",signs[code-320],(ind==0?' ':'i'),reg,v);
      }
      else printf("XC -- INVALID PARAMETER [%03d]",p[m->pc]);
      m->pc++;
      break;
    }
    case 350: // jump
    {
      if(opt_verbose!=0) printf("J   %03d",p[m->pc]);
      if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
      else m->pc=1;
      break;
    }
    case 351: // jump if +
    {
      if(opt_verbose!=0) printf("Jgt %03d",p[m->pc]);
      if(m->e>0)
      {
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else m->pc++;
      break;
    }
    case 352: // jump if -
    {
      if(opt_verbose!=0) printf("Jlt %03d",p[m->pc]);
      if(m->e<0)
      {
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else m->pc++;
      break;
    }
    case 353: // jump if !=0
    {
      if(opt_verbose!=0) printf("Jne %03d",p[m->pc]);
      if(m->e!=0)
      {
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else m->pc++;
      break;
    }
    case 354: // jump if ==0
    {
      if(opt_verbose!=0) printf("Jeq %03d",p[m->pc]);
      if(m->e==0)
      {
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else m->pc++;
      break;
    }
    case 355: // jump if >=0
    {
      if(opt_verbose!=0) printf("Jge %03d",p[m->pc]);
      if(m->e>=0)
      {
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else m->pc++;
      break;
    }
    case 356: // jump if <=0
    {
      if(opt_verbose!=0) printf("Jle %03d",p[m->pc]);
      if(m->e<=0)
      {
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else m->pc++;
      break;
    }
    case 357: // jump if input entered
    {
      if(opt_verbose!=0) printf("JC  %03d",p[m->pc]);
      if(input[0]!='\0')
      {
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else m->pc++;
      break;
    }

    case 360: // branch
    {
      if(opt_verbose!=0) printf("B   %03d",p[m->pc]);
      if(m->stackp<12)
      {
        m->stack[m->stackp++]=m->pc;
        if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
        else m->pc=1;
      }
      else printf(" -- ERR: STACK OVERFLOW");
      break;
    }
    case 361: // branch if +
    {
      if(opt_verbose!=0) printf("Bgt %03d",p[m->pc]);
      if(m->e>0)
      {
        if(m->stackp<12)
        {
          m->stack[m->stackp++]=m->pc;
          if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
          else m->pc=1;
        }
        else printf(" -- ERR: STACK OVERFLOW");
      }
      else m->pc++;
      break;
    }
    case 362: // branch if -
    {
      if(opt_verbose!=0) printf("Blt %03d",p[m->pc]);
      if(m->e<0)
      {
        if(m->stackp<12)
        {
          m->stack[m->stackp++]=m->pc;
          if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
          else m->pc=1;
        }
        else printf(" -- ERR: STACK OVERFLOW");
      }
      else m->pc++;
      break;
    }
    case 363: // branch if !=0
    {
      if(opt_verbose!=0) printf("Bne %03d",p[m->pc]);
      if(m->e!=0)
      {
        if(m->stackp<12)
        {
          m->stack[m->stackp++]=m->pc;
          if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
          else m->pc=1;
        }
        else printf(" -- ERR: STACK OVERFLOW");
      }
      else m->pc++;
      break;
    }
    case 364: // branch if ==0
    {
      if(opt_verbose!=0) printf("Beq %03d",p[m->pc]);
      if(m->e==0)
      {
        if(m->stackp<12)
        {
          m->stack[m->stackp++]=m->pc;
          if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
          else m->pc=1;
        }
        else printf(" -- ERR: STACK OVERFLOW");
      }
      else m->pc++;
      break;
    }
    case 365: // branch if >=0
    {
      if(opt_verbose!=0) printf("Bge %03d",p[m->pc]);
      if(m->e>=0)
      {
        if(m->stackp<12)
        {
          m->stack[m->stackp++]=m->pc;
          if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
          else m->pc=1;
        }
        else printf(" -- ERR: STACK OVERFLOW");
      }
      else m->pc++;
      break;
    }
    case 366: // branch if <=0
    {
      if(opt_verbose!=0) printf("Ble %03d",p[m->pc]);
      if(m->e<=0)
      {
        if(m->stackp<12)
        {
          m->stack[m->stackp++]=m->pc;
          if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
          else m->pc=1;
        }
        else printf(" -- ERR: STACK OVERFLOW");
      }
      else m->pc++;
      break;
    }
    case 367: // branch if input entered
    {
      if(opt_verbose!=0) printf("BC  %03d",p[m->pc]);
      if(input[0]!='\0')
      {
        if(m->stackp<12)
        {
          m->stack[m->stackp++]=m->pc;
          if(p[m->pc]<=17&&p[m->pc]>=0) m->pc=m->labels[p[m->pc]];
          else m->pc=1;
        }
        else printf(" -- ERR: STACK OVERFLOW");
      }
      else m->pc++;
      break;
    }

    default:
    {
      printf("UNIMPLEMENTED CODE [%03d]",code);
      if(opt_verbose==0) printf("\n");
    }
  }
  if(code>=350) noprint=1;
  if(noprint==0) disp_num(m);
  if(opt_verbose!=0) printf("\n");
  if(opt_debug!=0) printf("           [e=%6.3g, %4.2g%c => %4.2g%c - %4.2g%c - %4.2g%c - %4.2g%c]\n",m->e,m->numop,signs[m->numop_code],m->par[0],signs[m->par_code[0]],m->par[1],signs[m->par_code[1]],m->par[2],signs[m->par_code[2]],m->par[3],signs[m->par_code[3]]);

  if(p[m->pc]<0) ret=-1;

  return(ret);
}


int main(int argc, char **argv)
{
static char line[LINELEN];
static int program[512];
  FILE *f,*pr=NULL;
  int i,done;
  struct cc327 mac;
  char *infile=NULL,*outfile=NULL;
  time_t tm;
  struct tm *tms;
  char logname[256];
  char dt[8];

  spc[0]='\0';
  for(i=0;i<25;i++) strcat(spc," ");
  time(&tm);
  tms=localtime(&tm);
  strftime(mnev,256,"%Y.%b.%d. %H:%M - ",tms);
  strftime(dt,7,"%Y",tms);
  strcpy(logname,"mereslog");
  strcat(logname,dt);
  strcat(logname,".log");
  if(argc<2)
  {
    fprintf(stderr,"Usage: %s [options] program_file\n  -d  -  debug output\n  -v  -  verbose output\n",argv[0]);
    return(0);
  }
  for(i=1;i<argc;i++)
  {
    if(argv[i][0]=='-')
    {
      switch(argv[i][1])
      {
        case 'd':
        {
          opt_debug=1;
          break;
        }
        case 'v':
        {
          opt_verbose=1;
          break;
        }
        default:
        {
          fprintf(stderr,"%s: unknown option '%c'\n",argv[0],argv[i][1]);
          break;
        }
      }
    }
    else
    {
      if(infile==NULL) infile=argv[i];
      else if(outfile==NULL) outfile=argv[i];
      else fprintf(stderr,"%s: too much argument\n",argv[0]);
    }
  }
  if(infile==NULL)
  {
    fprintf(stderr,"%s: missing input file\n",argv[0]);
    exit(0);
  }
  strcat(mnev,infile);
  for(i=0;i<18;i++) mac.labels[i]=1;
  mac_init(&mac);
  program[0]=0;
  mlog=fopen(logname,"a");
  if(NULL!=(f=fopen(infile,"r")))
  {
    if(mlog!=NULL) fprintf(mlog,"\n--------------START\n%s\n\n",mnev);
    if(outfile==NULL||NULL!=(pr=fopen(outfile,"a")))
    {
      i=1;
      while(NULL!=fgets(line,LINELEN,f))
      {
        line[strlen(line)-1]='\0';
        if(strlen(line)==3) program[i]=atoi(line);
        else program[i]=atoi(&line[4]);
        if(program[i]>=200&&program[i]<=217) mac.labels[program[i]-200]=i;
        i++;
      }
      program[i]=-1;
      mac.pc=1;
      done=0;
      if(pr!=NULL) fprintf(pr,"RUNNING %s BEGIN\n",infile);
      while(done==0) done=execute(&mac,program,pr);
      if(pr!=NULL) fprintf(pr,"RUNNING %s END\n\n",infile);
      if(pr!=NULL) fclose(pr);
      if(mlog!=NULL) fprintf(mlog,"\n--------------END\n");
    }
    fclose(f);
  }
  if(mlog!=NULL) fclose(mlog);

  return(0);
}
