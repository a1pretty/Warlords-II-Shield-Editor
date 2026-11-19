/* SHLDEDIT.COM - Shield Designer for Warlords II (MS-DOS, VGA 640x480x16, mouse)
 * Edits the shield sets in SHIELD\ : A<name>.PCK army flags, S<name>.PCK large shields,
 * B<name>.PCK capital shields, <name>0-7.PCK status bars, <name>.SHL marker.
 * Version 1.04.
 * Built with gcc -m16 as a tiny-model .COM; needs a 386+, VGA, mouse driver.
 * The look (marble screen, fonts) is loaded at runtime from the game's own PICS\SCREEN*.PCK,
 * TEXT.FNT/FIN and CHANCE17.FNT/FIN, so it must run from the Warlords II directory.
 */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef short s16; typedef signed char s8;

/* ======================= low level: libc bits ======================= */
void *memset(void *d, int c, u32 n){ u8 *p=d; while(n--) *p++=(u8)c; return d; }
void *memcpy(void *d, const void *s, u32 n){ u8 *p=d; const u8 *q=s; while(n--) *p++=*q++; return d; }
static int slen(const char *s){ int n=0; while(s[n]) n++; return n; }
static void scpy(char *d, const char *s){ while((*d++=*s++)); }
static void scat(char *d, const char *s){ scpy(d+slen(d), s); }
static void sint(char *d, int v){ char b[12]; int i=11; int neg=v<0; if(neg) v=-v; b[i]=0; do{ b[--i]='0'+v%10; v/=10; }while(v); if(neg) b[--i]='-'; scpy(d,b+i); }
static int seq(const char *a, const char *b){ while(*a && *a==*b){a++;b++;} return *a==*b; }
#define NEAR(p) ((u16)(u32)(p))
#ifndef SMALL_H
#define SMALL_H 16
#endif
#ifndef ABOUT_BIG
#define ABOUT_BIG 1
#endif
#ifndef ALLGAME
#define ALLGAME 1
#endif

/* ======================= low level: BIOS / DOS ======================= */
static inline void outw(u16 port, u16 v){ asm volatile("outw %%ax,%%dx"::"a"(v),"d"(port)); }
static u16 myds(void){ u16 v; asm volatile("movw %%ds,%0":"=r"(v)); return v; }
static void int10(u16 ax, u16 bx, u16 cx, u16 dx){
  asm volatile("pushl %%ebp\n\tint $0x10\n\tpopl %%ebp":"+a"(ax),"+b"(bx),"+c"(cx),"+d"(dx)::"memory","cc"); }
static int int21(u16 *ax, u16 *bx, u16 *cx, u16 *dx){
  u16 a=*ax,b=*bx,c=*cx,d=*dx; u8 cf;
  asm volatile("int $0x21\n\tsetc %4":"+a"(a),"+b"(b),"+c"(c),"+d"(d),"=qm"(cf)::"memory","cc");
  *ax=a; *bx=b; *cx=c; *dx=d; return cf; }
static void mouse(u16 *ax, u16 *bx, u16 *cx, u16 *dx){
  u16 a=*ax,b=*bx,c=*cx,d=*dx;
  asm volatile("int $0x33":"+a"(a),"+b"(b),"+c"(c),"+d"(d)::"memory","cc");
  *ax=a; *bx=b; *cx=c; *dx=d; }
static int getkey(void){ /* 0 if none, else BIOS key (scan<<8|ascii) */
  u16 a=0x0100; u8 z;
  asm volatile("int $0x16\n\tsetz %1":"+a"(a),"=qm"(z)::"cc");
  if(z) return 0;
  a=0; asm volatile("int $0x16":"+a"(a)::"cc");
  return a ? a : 0xFFFF; }
static void dos_print(const char *s){ u16 a=0x0900,b=0,c=0,d=NEAR(s); int21(&a,&b,&c,&d); }
static void dos_exit(int code){ u16 a=0x4C00|(code&255),b=0,c=0,d=0; int21(&a,&b,&c,&d); }
static int f_open(const char *n){ u16 a=0x3D00,b=0,c=0,d=NEAR(n); return int21(&a,&b,&c,&d)?-1:a; }
static int f_create(const char *n){ u16 a=0x3C00,b=0,c=0,d=NEAR(n); return int21(&a,&b,&c,&d)?-1:a; }
static int f_read(int h, void *p, u16 n){ u16 a=0x3F00,b=h,c=n,d=NEAR(p); return int21(&a,&b,&c,&d)?-1:a; }
static int f_write(int h, const void *p, u16 n){ u16 a=0x4000,b=h,c=n,d=NEAR(p); return int21(&a,&b,&c,&d)?-1:a; }
static void f_close(int h){ u16 a=0x3E00,b=h,c=0,d=0; int21(&a,&b,&c,&d); }
static void f_mkdir(const char *n){ u16 a=0x3900,b=0,c=0,d=NEAR(n); int21(&a,&b,&c,&d); }
static int f_exists(const char *n){ int h=f_open(n); if(h<0) return 0; f_close(h); return 1; }
static u16 dos_alloc(u16 paras){ u16 a=0x4800,b=paras,c=0,d=0; return int21(&a,&b,&c,&d)?0:a; }

/* far memory: FS is reserved for us (gcc never uses it) */
static inline u8 fpeek(u16 seg, u16 off){ u8 v; asm volatile("movw %1,%%fs\n\tmovb %%fs:(%%bx),%0":"=q"(v):"r"(seg),"b"(off):"memory"); return v; }
static inline void fpoke(u16 seg, u16 off, u8 v){ asm volatile("movw %0,%%fs\n\tmovb %2,%%fs:(%%bx)"::"r"(seg),"b"(off),"q"(v):"memory"); }
static inline u16 fpeekw(u16 seg, u16 off){ u16 v; asm volatile("movw %1,%%fs\n\tmovw %%fs:(%%bx),%0":"=r"(v):"r"(seg),"b"(off):"memory"); return v; }
static inline void fpokew(u16 seg, u16 off, u16 v){ asm volatile("movw %0,%%fs\n\tmovw %2,%%fs:(%%bx)"::"r"(seg),"b"(off),"r"(v):"memory"); }
static void fmove(u16 dseg, u16 doff, u16 sseg, u16 soff, u16 n){
  u16 d=doff,s=soff,c=n; if(!n) return;
  asm volatile("pushw %%ds\n\tpushw %%es\n\tmovw %3,%%es\n\tmovw %4,%%ds\n\tcld\n\trep movsb\n\tpopw %%es\n\tpopw %%ds"
               :"+D"(d),"+S"(s),"+c"(c):"r"(dseg),"r"(sseg):"memory"); }
static void fset(u16 seg, u16 off, u8 v, u16 n){
  u16 d=off,c=n; if(!n) return;
  asm volatile("pushw %%es\n\tmovw %2,%%es\n\tcld\n\trep stosb\n\tpopw %%es":"+D"(d),"+c"(c):"r"(seg),"a"(v):"memory"); }


/* ======================= VGA mode 12h ======================= */
static const u8 PAL_PCT[16][3]={{0,0,0},{73,73,73},{56,56,56},{44,44,44},{31,31,31},{17,72,99},{0,36,81},{99,92,13},
  {99,63,0},{77,11,0},{31,67,11},{0,55,0},{0,34,0},{64,33,0},{45,20,0},{99,99,99}};
static void vga_init(void){
  static u8 attrs[17]; static u8 dac[48]; int i;
  int10(0x0012,0,0,0);
  for(i=0;i<16;i++) attrs[i]=i; attrs[16]=0;
  int10(0x1002,0,0,NEAR(attrs));
  for(i=0;i<48;i++) dac[i]=(u8)(PAL_PCT[i/3][i%3]*63/99);
  int10(0x1012,0,16,NEAR(dac));
  outw(0x3CE,0x0005); outw(0x3CE,0xFF08); outw(0x3CE,0x0001); outw(0x3CE,0x0003); outw(0x3C4,0x0F02);
}
static inline void vga_rmw(u16 off, u8 v){
  asm volatile("movw $0xA000,%%ax\n\tmovw %%ax,%%fs\n\tmovb %%fs:(%%bx),%%al\n\tmovb %1,%%fs:(%%bx)"::"b"(off),"q"(v):"ax","memory"); }
static void fill(int x, int y, int w, int h, u8 c){
  int r; if(w<=0||h<=0) return;
  outw(0x3CE,(u16)c<<8); outw(0x3CE,0x0F01);
  for(r=0;r<h;r++){
    u16 row=(y+r)*80; int b0=x>>3, b1=(x+w-1)>>3;
    u8 lm=0xFF>>(x&7), rm=(u8)(0xFF<<(7-((x+w-1)&7)));
    if(b0==b1){ outw(0x3CE,((u16)(lm&rm)<<8)|8); vga_rmw(row+b0,0); }
    else{
      outw(0x3CE,((u16)lm<<8)|8); vga_rmw(row+b0,0);
      outw(0x3CE,((u16)rm<<8)|8); vga_rmw(row+b1,0);
      if(b1-b0>1){ outw(0x3CE,0xFF08); fset(0xA000,row+b0+1,0,b1-b0-1); }
    }
  }
  outw(0x3CE,0x0001); outw(0x3CE,0xFF08);
}
/* x must be a multiple of 8 and n a multiple of 8 */
static void putrow8(int x, int y, const u8 *c, int n){
  static u8 pl[4][80]; int b,k,nb=n>>3;
  for(b=0;b<nb;b++){ u8 p0=0,p1=0,p2=0,p3=0; const u8 *q=c+b*8;
    for(k=0;k<8;k++){ u8 v=q[k], m=0x80>>k; if(v&1)p0|=m; if(v&2)p1|=m; if(v&4)p2|=m; if(v&8)p3|=m; }
    pl[0][b]=p0; pl[1][b]=p1; pl[2][b]=p2; pl[3][b]=p3; }
  for(k=0;k<4;k++){ outw(0x3C4,(u16)(1<<k)<<8|2); fmove(0xA000,y*80+(x>>3),myds(),NEAR(pl[k]),nb); }
  outw(0x3C4,0x0F02);
}
static void putpix(int x, int y, u8 c){
  outw(0x3CE,0x0205); outw(0x3CE,((u16)(0x80>>(x&7))<<8)|8); vga_rmw(y*80+(x>>3),c); outw(0x3CE,0xFF08); outw(0x3CE,0x0005); }
/* ---- software mouse pointer ----
 * We never ask the driver to draw its own cursor (AX=1): many drivers draw it wrongly or not at all
 * in 640x480x16, especially if they were reset in text mode. Instead we read position/buttons
 * (AX=3) and draw an arrow ourselves, saving and restoring the 4 bit-planes underneath it. */
static const char *ARROW[19]={
 "X...........","XX..........","XWX.........","XWWX........","XWWWX.......","XWWWWX......","XWWWWWX.....",
 "XWWWWWWX....","XWWWWWWWX...","XWWWWWWWWX..","XWWWWWWWWWX.","XWWWWWWXXXXX","XWWWXWWX....","XWWXXWWX....",
 "XWX..XWWX...","XX...XWWX...","X.....XWWX..","......XWWX..",".......XX..."};
static int cur_level, cur_drawn, cur_x=580, cur_y=440, cur_sx, cur_sy;
static u8 cur_save[4][19][3];
static void cursor_restore(void){
  int p,r,b;
  for(p=0;p<4;p++){ outw(0x3C4,(u16)(1<<p)<<8|2);
    for(r=0;r<19;r++){ int y=cur_sy+r; if(y>=480) break; for(b=0;b<3;b++){ int bx=(cur_sx>>3)+b; if(bx>=80) break; fpoke(0xA000,(u16)(y*80+bx),cur_save[p][r][b]); } } }
  outw(0x3C4,0x0F02); cur_drawn=0;
}
static void cursor_draw(void){
  int p,r,b,x;
  cur_sx=cur_x; cur_sy=cur_y;
  for(p=0;p<4;p++){ outw(0x3CE,(u16)p<<8|4);
    for(r=0;r<19;r++){ int y=cur_sy+r; if(y>=480) break; for(b=0;b<3;b++){ int bx=(cur_sx>>3)+b; if(bx>=80) break; cur_save[p][r][b]=fpeek(0xA000,(u16)(y*80+bx)); } } }
  outw(0x3CE,0x0004);
  for(r=0;r<19;r++){ int y=cur_sy+r; if(y>=480) break;
    for(x=0;x<12;x++){ char ch=ARROW[r][x]; int xx=cur_sx+x; if(xx>=640) break; if(ch=='X') putpix(xx,y,0); else if(ch=='W') putpix(xx,y,15); } }
  cur_drawn=1;
}
static void mouse_show(int on){
  if(!on){ if(cur_level==0&&cur_drawn) cursor_restore(); cur_level--; }
  else { if(cur_level<0) cur_level++; if(cur_level==0&&!cur_drawn) cursor_draw(); }
}
static int poll_mouse(int *x,int *y){ /* returns buttons, moves the pointer */
  u16 a=3,b=0,c=0,d=0; mouse(&a,&b,&c,&d);
  if((int)c>639) c=639; if((int)d>479) d=479;
  if(c!=cur_x||d!=cur_y){ if(cur_drawn) cursor_restore(); cur_x=c; cur_y=d; if(cur_level==0) cursor_draw(); }
  if(x)*x=c; if(y)*y=d; return b&3;
}
/* ======================= shield set data ======================= */
#define NSHEET 11
static const u16 SW[NSHEET]={368,288,144,320,320,320,320,320,320,320,320};
static const u16 SHT[NSHEET]={64,59,246,18,18,18,18,18,18,18,18};
static u16 sheetseg[NSHEET];
static u16 scratch;           /* 64K far scratch (encoder tables, fill queue) */
static u16 clipseg; static int clip_w, clip_h;
#define UNDO_MAX 10
static u16 undoseg[UNDO_MAX]; static int undo_n, undo_top, undo_cnt; static u8 undo_sheet[UNDO_MAX]; static u16 undo_grp[UNDO_MAX];
static u16 grp_counter; static u16 stroke_touched; static int in_stroke;
static int modified;

typedef struct { u8 sheet; s16 x,y,w,h; const char *name; s16 lx,ly,lw,lh; u8 ln, ldx, ldy; } Part;
/* link: base rect lx,ly,lw,lh repeated ln times at (k*ldx, k*ldy) */
static Part parts[12]; static int nparts;
static void addpart(int sh,int x,int y,int w,int h,const char *n){ Part *p=&parts[nparts++]; p->sheet=sh;p->x=x;p->y=y;p->w=w;p->h=h;p->name=n;p->ln=0; }
static void addlink(int lx,int ly,int lw,int lh,int n,int dx,int dy){ Part *p=&parts[nparts-1]; p->lx=lx;p->ly=ly;p->lw=lw;p->lh=lh;p->ln=n;p->ldx=dx;p->ldy=dy; }
static void build_parts(int p){
  nparts=0;
  if(p<8){ addpart(0,40*p,0,40,40,"Large shield"); addpart(0,40*p,40,40,24,"Army icon"); addpart(0,320+8*(p%4),40+8*(p/4),8,8,"Mini flag"); }
  else { addpart(0,320,0,40,40,"Large shield"); addpart(0,352,46,16,18,"Small shield"); }
  addpart(1,32*p,0,32,43,"Capital"); addpart(1,32*p,43,32,16,"City shield");
  addpart(2,16*p,30,16,64,"Army stack"); addlink(16*p,30,8,8,8,1,8);
  addpart(2,16*p,164,16,82,"Large stack"); addlink(16*p,164,10,10,8,1,10);
  if(p<8){ addpart(2,112+16*(p/4),94+14*(p%4),16,14,"Banner");
           addpart(3+p,0,0,320,18,"Status bar"); addlink(11,3,13,15,9,32,0); }
}
typedef struct { u8 sheet; s16 x,y,w,h; u8 n,dx,dy; } Link;
static Link links[32]; static int nlinks;
static void build_links(void){
  int p,i; nlinks=0;
  for(p=0;p<9;p++){ build_parts(p); for(i=0;i<nparts;i++) if(parts[i].ln){ Link *L=&links[nlinks++]; Part *q=&parts[i];
      L->sheet=q->sheet; L->x=q->lx; L->y=q->ly; L->w=q->lw; L->h=q->lh; L->n=q->ln; L->dx=q->ldx; L->dy=q->ldy; } }
  for(i=0;i<7;i++){ Link *L=&links[nlinks++]; L->sheet=2; L->x=16*i; L->y=94; L->w=11; L->h=10; L->n=7; L->dx=1; L->dy=10; }
}

/* ======================= editor state ======================= */
static int cur_tab=0;        /* 0=S 1=B 2=A 3=bars */
static int player=0, part_sel=-1, tool=0; /* tool: 0 pen 1 fill 2 pick 3 select */
static u8 prim=15, sec=0;
static int mirror_on=1, grid_on=1, zoom=1, fitzoom=1;
static int sel_on, sel_x, sel_y, sel_w, sel_h, pasting;
static int rc_from=9, rc_to=6, rc_scope=0, rc_swap=0;   /* scope 0 player 1 view 2 selection */
static int cp_from=0, cp_to=1;
static char setname[9]="MYSET", origin[9]="DEFAULT";
static char msg[96];
static int paste_ox=-1, paste_oy;
static void paste_ox_reset(void){ paste_ox=-1; }

static int cur_sheet(void){ return cur_tab<3 ? cur_tab : 3+(player>7?7:player); }
static inline u8 getpx(int s,int x,int y){ return fpeek(sheetseg[s],(u16)(y*SW[s]+x)); }

/* ======================= undo ======================= */
static void begin_stroke(void){ in_stroke=1; stroke_touched=0; grp_counter++; }
static void snapshot(int s){
  int slot; if(!undo_n) return;
  slot=undo_top; undo_top=(undo_top+1)%undo_n; if(undo_cnt<undo_n) undo_cnt++;
  undo_sheet[slot]=s; undo_grp[slot]=grp_counter;
  fmove(undoseg[slot],0,sheetseg[s],0,SW[s]*SHT[s]);
}
static void touch(int s){ if(in_stroke && !(stroke_touched&(1<<s))){ stroke_touched|=1<<s; snapshot(s); } modified=1; }
static void end_stroke(void){ in_stroke=0; }
static int undo(void){
  u16 g; int any=0;
  if(!undo_cnt) return 0;
  g=undo_grp[(undo_top+undo_n-1)%undo_n];
  while(undo_cnt){ int slot=(undo_top+undo_n-1)%undo_n; int s; if(undo_grp[slot]!=g) break;
    s=undo_sheet[slot]; fmove(sheetseg[s],0,undoseg[slot],0,SW[s]*SHT[s]); undo_top=slot; undo_cnt--; any=1; }
  return any;
}


/* ======================= screen layout (matches PICS\SCREEN0-3.PCK) ======================= */
#define MBH 17          /* menu bar height */
#define CX 16           /* big left panel = editing canvas */
#define CY 30
#define CW 360
#define CH 360
static int vx,vy,vw,vh, imgx,imgy;
static void view_rect(void){
  int s=cur_sheet(); vx=0;vy=0;vw=SW[s];vh=SHT[s];
  if(part_sel>=0){ int i,k=0; build_parts(player); for(i=0;i<nparts;i++) if(parts[i].sheet==s){ if(k==part_sel){ vx=parts[i].x;vy=parts[i].y;vw=parts[i].w;vh=parts[i].h; } k++; } }
}
#define ZMAX 24
static int panx, pany;
static void compute_fit(void){ int a,b; view_rect(); a=CW/vw; b=CH/vh; fitzoom=a<b?a:b; if(fitzoom<1)fitzoom=1; if(fitzoom>ZMAX)fitzoom=ZMAX; zoom=fitzoom; panx=pany=0; }
static int scrollable(void){ return vw*zoom>CW || vh*zoom>CH; }
static void layout(void){
  view_rect();
  if(vw*zoom<=CW){ imgx=CX+(CW-vw*zoom)/2; panx=0; }
  else { int m=vw-CW/zoom; if(panx>m)panx=m; if(panx<0)panx=0; imgx=CX-panx*zoom; }
  if(vh*zoom<=CH){ imgy=CY+(CH-vh*zoom)/2; pany=0; }
  else { int m=vh-CH/zoom; if(pany>m)pany=m; if(pany<0)pany=0; imgy=CY-pany*zoom; }
}
static void xor_rect(int x, int y, int w, int h){ /* XOR outline, drawn twice = erased; clipped to the canvas */
  int i;
  if(x<CX){ w-=CX-x; x=CX; } if(y<CY){ h-=CY-y; y=CY; } if(x+w>CX+CW) w=CX+CW-x; if(y+h>CY+CH) h=CY+CH-y; if(w<=0||h<=0) return;
  outw(0x3CE,0x1803); outw(0x3CE,0x0205);
  for(i=0;i<w;i++){ int xx=x+i; outw(0x3CE,((u16)(0x80>>(xx&7))<<8)|8); vga_rmw(y*80+(xx>>3),15); if(h>1) vga_rmw((y+h-1)*80+(xx>>3),15); }
  for(i=1;i<h-1;i++){ int yy=y+i; outw(0x3CE,((u16)(0x80>>(x&7))<<8)|8); vga_rmw(yy*80+(x>>3),15);
    if(w>1){ outw(0x3CE,((u16)(0x80>>((x+w-1)&7))<<8)|8); vga_rmw(yy*80+((x+w-1)>>3),15); } }
  outw(0x3CE,0x0003); outw(0x3CE,0x0005); outw(0x3CE,0xFF08);
}

/* ======================= generic PCK decoding through the far scratch buffer ======================= */
static u8 iobuf[1024]; static int io_pos, io_len, io_h, io_err;
static u8 plane[4428];
static int rdbyte(void){ if(io_pos>=io_len){ io_len=f_read(io_h,iobuf,sizeof iobuf); io_pos=0; if(io_len<=0){ io_err=1; return 0; } } return iobuf[io_pos++]; }
static int dec_w, dec_h, dec_bpr;
typedef void (*Sink)(int p);
static int decode_file(const char *path, int ew, int eh, Sink sink){
  int p, size;
  io_h=f_open(path); if(io_h<0) return 0;
  io_pos=io_len=0; io_err=0;
  rdbyte(); rdbyte(); dec_w=rdbyte(); dec_w|=rdbyte()<<8; dec_h=rdbyte(); dec_h|=rdbyte()<<8;
  if(io_err||(ew&&dec_w!=ew)||(eh&&dec_h!=eh)||(dec_w&7)||!dec_w||!dec_h){ f_close(io_h); return 0; }
  dec_bpr=dec_w>>3; size=dec_bpr*dec_h; if(size>60000){ f_close(io_h); return 0; }
  for(p=0;p<4&&!io_err;p++){
    int o=0;
    while(o<size&&!io_err){ int b=rdbyte();
      if(b<0x80){ int n=b+1; if(o+n>size){io_err=1;break;} while(n--) fpoke(scratch,(u16)o++,rdbyte()); }
      else { int lo=rdbyte(), n=rdbyte()+1; int off=(s16)((b<<8)|lo); if(o+off<0||o+n>size){io_err=1;break;}
             while(n--){ fpoke(scratch,(u16)o,fpeek(scratch,(u16)(o+off))); o++; } } }
    if(!io_err) sink(p);
  }
  f_close(io_h); return !io_err;
}
static int sink_sheet_id;
static void sink_sheet(int p){
  int i, size=dec_bpr*dec_h; u16 seg=sheetseg[sink_sheet_id];
  for(i=0;i<size;i++){ u8 v=fpeek(scratch,(u16)i); int y=i/dec_bpr, x=(i%dec_bpr)*8, k; u16 base=(u16)(y*dec_w+x);
    for(k=0;k<8;k++){ u8 bit=(v>>(7-k))&1; if(p==0) fpoke(seg,base+k,bit); else if(bit) fpoke(seg,base+k,fpeek(seg,base+k)|(1<<p)); } }
}
static int load_pck(const char *path, int s){ sink_sheet_id=s; return decode_file(path,SW[s],SHT[s],sink_sheet); }

/* ---- the game's marble screen: PICS\SCREEN0..3.PCK are its four 320x240 quarters, kept as 4 planes ---- */
static u16 bgseg[4]; static int bg_q;
static void sink_bg(int p){ int y, qx=bg_q&1, qy=bg_q>>1; for(y=0;y<240;y++) fmove(bgseg[p],(u16)((qy*240+y)*80+qx*40),scratch,(u16)(y*40),40); }
static int load_bg(void){ int q; char path[20]; for(q=0;q<4;q++){ scpy(path,"PICS\\SCREEN0.PCK"); path[11]='0'+q; bg_q=q; if(!decode_file(path,320,240,sink_bg)) return 0; } return 1; }
/* copy background rectangle (sx,sy) -> screen (dx,dy); sx and dx must have the same x&7 */
static void bg_copy(int sx,int sy,int dx,int dy,int w,int h){
  int p,r; if(w<=0||h<=0) return;
  for(p=0;p<4;p++){ outw(0x3C4,(u16)(1<<p)<<8|2);
    for(r=0;r<h;r++){ u16 srow=(sy+r)*80, drow=(dy+r)*80; int b0=dx>>3,b1=(dx+w-1)>>3, sb=sx>>3;
      u8 lm=0xFF>>(dx&7), rm=(u8)(0xFF<<(7-((dx+w-1)&7)));
      if(b0==b1){ outw(0x3CE,((u16)(lm&rm)<<8)|8); vga_rmw(drow+b0,fpeek(bgseg[p],srow+sb)); }
      else { outw(0x3CE,((u16)lm<<8)|8); vga_rmw(drow+b0,fpeek(bgseg[p],srow+sb));
             outw(0x3CE,((u16)rm<<8)|8); vga_rmw(drow+b1,fpeek(bgseg[p],srow+sb+(b1-b0)));
             outw(0x3CE,0xFF08); if(b1-b0>1) fmove(0xA000,drow+b0+1,bgseg[p],srow+sb+1,b1-b0-1); }
      outw(0x3CE,0xFF08); } }
  outw(0x3C4,0x0F02);
}
static void bg_blit(int x,int y,int w,int h){ bg_copy(x,y,x,y,w,h); }
static void bg_row(int y,int x,int n,u8 *out){ /* background pixels; x and n multiples of 8 */
  int b,k,nb=n>>3; u16 row=y*80+(x>>3);
  for(b=0;b<nb;b++){ u8 p0=fpeek(bgseg[0],row+b),p1=fpeek(bgseg[1],row+b),p2=fpeek(bgseg[2],row+b),p3=fpeek(bgseg[3],row+b);
    for(k=0;k<8;k++){ u8 m=0x80>>k; out[b*8+k]=(p0&m?1:0)|(p1&m?2:0)|(p2&m?4:0)|(p3&m?8:0); } }
}

/* ---- the game's fonts: TEXT.FNT (menus, buttons) and CHANCE17.FNT (Chancery captions) ----
 * .FNT is a PCK glyph sheet; .FIN: [0]=count [2..3]=sheet width (big endian) [4]=height,
 * [12..] cell widths from '!' then advance widths. Glyph cells start on 8-pixel boundaries. */
typedef struct { u8 h, rows, space; u16 bpr, mfill, mout; u8 cw[96], adv[96]; u16 gx[96]; u8 gy[96]; } Font;
static Font FT, FC, FS, FB; static u16 fontseg;
static Font *sk_font; static u8 sk_col[2]; static u16 sk_off[2]; static int sk_nm, sk_row0;
static void sink_font(int p){
  Font *f=sk_font; int m,r,b, rows=f->rows*f->h;
  for(m=0;m<sk_nm;m++){ u8 c=sk_col[m];
    for(r=0;r<rows;r++) for(b=0;b<f->bpr;b++){
      u8 v=fpeek(scratch,(u16)((sk_row0+r)*dec_bpr+b)); u16 o=sk_off[m]+r*f->bpr+b;
      u8 cur = p==0 ? 0xFF : fpeek(fontseg,o);
      cur &= ((c>>p)&1) ? v : (u8)~v; fpoke(fontseg,o,cur); } }
}
static int load_font(Font *f,const char *name,int nm,u8 c0,u8 c1,int variant,u16 off){
  char path[16]; static u8 fin[256]; int h,n,i,x,y,sw;
  scpy(path,name); scat(path,".FIN"); h=f_open(path); if(h<0) return 0; n=f_read(h,fin,256); f_close(h); if(n<12+96+94) return 0;
  f->h=fin[4]; sw=fin[2]*256+fin[3]; f->bpr=sw/8; x=0; y=0;
  for(i=0;i<96;i++){ int c8; f->cw[i]=fin[12+i]; f->adv[i]=fin[12+96+i]; c8=(f->cw[i]+7)&~7; if(x+c8>sw){ x=0; y++; } f->gx[i]=x; f->gy[i]=y; x+=c8; }
  f->rows=f->gy[93]+1; f->space=5;
  f->mfill=off; f->mout=off+f->rows*f->h*f->bpr;
  sk_font=f; sk_nm=nm; sk_col[0]=c0; sk_col[1]=c1; sk_off[0]=f->mfill; sk_off[1]=f->mout; sk_row0=variant*f->rows*f->h;
  scpy(path,name); scat(path,".FNT"); if(!decode_file(path,0,0,sink_font)) return 0;
  return dec_bpr==f->bpr;
}
/* smaller Chancery: nearest-pixel scale of the 17px glyphs (fill mask only) */
static void make_scaled(Font *d,Font *s,int th,u16 off){
  int i,x=0,y=0,sw=s->bpr*8,oy,ox,b;
  d->h=th; d->bpr=s->bpr; d->space=s->space*th/s->h+1; d->mfill=off;
  for(i=0;i<96;i++){ int c8; d->cw[i]=(s->cw[i]*th*2+s->h)/(2*s->h); if(!d->cw[i]) d->cw[i]=1; d->adv[i]=(s->adv[i]*th*2+s->h)/(2*s->h);
    c8=(d->cw[i]+7)&~7; if(x+c8>sw){ x=0; y++; } d->gx[i]=x; d->gy[i]=y; x+=c8; }
  d->rows=d->gy[93]+1; d->mout=off+d->rows*d->h*d->bpr;
  for(b=0;b<d->rows*d->h*d->bpr;b++) fpoke(fontseg,(u16)(off+b),0);
  for(i=0;i<94;i++) for(oy=0;oy<th;oy++) for(ox=0;ox<d->cw[i];ox++){
    int sy=((2*oy+1)*s->h)/(2*th), sx=((2*ox+1)*s->h)/(2*th); u16 so,dof; if(sy>=s->h) sy=s->h-1; if(sx>=s->cw[i]) sx=s->cw[i]-1;
    so=(u16)(s->mfill+(s->gy[i]*s->h+sy)*s->bpr+((s->gx[i]+sx)>>3));
    if(fpeek(fontseg,so)&(0x80>>((s->gx[i]+sx)&7))){ int dx=d->gx[i]+ox; dof=(u16)(off+(d->gy[i]*th+oy)*d->bpr+(dx>>3)); fpoke(fontseg,dof,fpeek(fontseg,dof)|(0x80>>(dx&7))); } }
}
static int text_w(Font *f,const char *s){ int w=0; for(;*s;s++){ int c=(u8)*s; if(c==' ') w+=f->space; else if(c>32&&c<127) w+=f->adv[c-33]; } return w; }
static void mask_text(Font *f,u16 moff,int x,int y,const char *s,u8 col){
  outw(0x3CE,(u16)col<<8); outw(0x3CE,0x0F01);
  for(;*s;s++){ int c=(u8)*s,i,r,b,nb,sh;
    if(c==' '){ x+=f->space; continue; } if(c<=32||c>126) continue; i=c-33;
    nb=(f->cw[i]+7)>>3; sh=x&7;
    for(r=0;r<f->h;r++){ int yy=y+r; u16 src=(u16)(moff+(f->gy[i]*f->h+r)*f->bpr+(f->gx[i]>>3));
      if(yy<0||yy>=480) continue;
      for(b=0;b<nb;b++){ u8 bits=fpeek(fontseg,src+b); int xx=x+b*8; u16 off;
        if(!bits||xx<0||xx>=640) continue; off=yy*80+(xx>>3);
        outw(0x3CE,((u16)(u8)(bits>>sh)<<8)|8); vga_rmw(off,0);
        if(sh&&(xx>>3)<79){ outw(0x3CE,((u16)(u8)(bits<<(8-sh))<<8)|8); vga_rmw(off+1,0); } } }
    x+=f->adv[i]; }
  outw(0x3CE,0x0001); outw(0x3CE,0xFF08);
}
static void ttext(int x,int y,const char *s,u8 col){ mask_text(&FT,FT.mfill,x,y,s,col); }          /* menu font */
static void ctext(int x,int y,const char *s){ mask_text(&FC,FC.mout,x,y,s,0); mask_text(&FC,FC.mfill,x,y,s,15); } /* white Chancery, black edge */
static void ctext_c(int x,int y,const char *s,u8 c){ mask_text(&FC,FC.mfill,x,y,s,c); }
/* the game's button lettering: Chancery drawn bold (twice, 1px apart) with a light emboss below-right */
static void gtext(Font *f,int x,int y,const char *s,u8 c,int bold){ mask_text(f,f->mfill,x+1,y+1,s,1); if(bold) mask_text(f,f->mfill,x+2,y+1,s,1); mask_text(f,f->mfill,x,y,s,c); if(bold) mask_text(f,f->mfill,x+1,y,s,c); }
static int gtext_w(Font *f,const char *s,int bold){ return text_w(f,s)+1+bold; }

/* ======================= canvas rendering ======================= */
static u8 linebuf[640];
static int in_player_rect(int s,int ix,int iy,int *edge,int subx,int suby){
  int i; *edge=0;
  for(i=0;i<nparts;i++){ Part *p=&parts[i]; if(p->sheet!=s) continue;
    if(ix>=p->x&&iy>=p->y&&ix<p->x+p->w&&iy<p->y+p->h){
      if((ix==p->x&&subx==0)||(iy==p->y&&suby==0)||(ix==p->x+p->w-1&&subx==zoom-1)||(iy==p->y+p->h-1&&suby==zoom-1)) *edge=1;
      return 1; } }
  return 0;
}
static u8 overlay(int s,int ix,int iy,int subx,int suby,int X,int Y,u8 c){
  int i;
  if(part_sel<0 && cur_tab<3){ int e; if(!in_player_rect(s,ix,iy,&e,subx,suby)){ if((X^Y)&1) c=0; } else if(e) return 7; }
  if(grid_on && zoom>=6 && (subx==0||suby==0)) c = (c==0)?3:0;
  if(mirror_on && part_sel>=0 && zoom>=4){
    for(i=0;i<nlinks;i++){ Link *L=&links[i]; if(L->sheet!=s) continue;
      if(ix>=L->x&&iy>=L->y&&ix<L->x+L->w&&iy<L->y+L->h){
        if((ix==L->x&&subx==0)||(iy==L->y&&suby==0)||(ix==L->x+L->w-1&&subx==zoom-1)||(iy==L->y+L->h-1&&suby==zoom-1)) return ((X+Y)>>2)&1?5:6; } } }
  if(sel_on && ix>=sel_x&&iy>=sel_y&&ix<sel_x+sel_w&&iy<sel_y+sel_h){
    if((ix==sel_x&&subx==0)||(iy==sel_y&&suby==0)||(ix==sel_x+sel_w-1&&subx==zoom-1)||(iy==sel_y+sel_h-1&&suby==zoom-1)) return ((X+Y)>>1)&1?15:0; }
  return c;
}
static void render_canvas(void){
  static s16 colmap[CW]; static u8 colsub[CW]; static u8 rowbuf[400];
  int s=cur_sheet(), X, Y, lastrow=-1;
  layout(); build_parts(player);
  for(X=0;X<CW;X++){ int ix=CX+X-imgx; if(ix<0||ix>=vw*zoom){ colmap[X]=(ix==-1||ix==vw*zoom)?-2:-1; } else { colmap[X]=ix/zoom; colsub[X]=ix%zoom; } }
  for(Y=CY;Y<CY+CH;Y++){
    int iy=Y-imgy;
    bg_row(Y,CX,CW,linebuf);
    if(iy==-1||iy==vh*zoom){ for(X=0;X<CW;X++) if(colmap[X]!=-1) linebuf[X]=0; }
    else if(iy>=0&&iy<vh*zoom){
      int ry=iy/zoom, sub=iy%zoom;
      if(ry!=lastrow){ fmove(myds(),NEAR(rowbuf),sheetseg[s],(u16)((vy+ry)*SW[s]+vx),vw); lastrow=ry; }
      for(X=0;X<CW;X++){ s16 m=colmap[X];
        if(m==-2) linebuf[X]=0;
        else if(m>=0) linebuf[X]=overlay(s,vx+m,vy+ry,colsub[X],sub,CX+X,Y,rowbuf[m]); }
    }
    putrow8(CX,Y,linebuf,CW);
  }
}
static void render_cell(int ix,int iy){
  int s=cur_sheet(), sx,sy, X,Y;
  if(ix<vx||iy<vy||ix>=vx+vw||iy>=vy+vh) return;
  sx=imgx+(ix-vx)*zoom; sy=imgy+(iy-vy)*zoom;
  { u8 c=getpx(s,ix,iy); int plain=(part_sel>=0||cur_tab==3)&&!(grid_on&&zoom>=6)&&!sel_on&&!(mirror_on&&zoom>=4&&part_sel>=0);
    if(plain){ int x0=sx,y0=sy,x1=sx+zoom,y1=sy+zoom; if(x0<CX)x0=CX; if(y0<CY)y0=CY; if(x1>CX+CW)x1=CX+CW; if(y1>CY+CH)y1=CY+CH;
      if(x1>x0&&y1>y0) fill(x0,y0,x1-x0,y1-y0,c); return; }
    for(Y=0;Y<zoom;Y++) for(X=0;X<zoom;X++){ int px=sx+X, py=sy+Y; if(px<CX||py<CY||px>=CX+CW||py>=CY+CH) continue; putpix(px,py,overlay(s,ix,iy,X,Y,px,py,c)); } }
}
static void setpx(int s,int x,int y,u8 c,int mir,int draw){
  int i,k;
  if(x<0||y<0||x>=SW[s]||y>=SHT[s]) return;
  touch(s);
  fpoke(sheetseg[s],(u16)(y*SW[s]+x),c);
  if(draw && s==cur_sheet()) render_cell(x,y);
  if(mir && mirror_on){
    for(i=0;i<nlinks;i++){ Link *L=&links[i]; if(L->sheet!=s) continue;
      for(k=0;k<L->n;k++){ int bx=x-k*L->dx-L->x, by=y-k*L->dy-L->y;
        if(bx>=0&&by>=0&&bx<L->w&&by<L->h){ int j;
          for(j=0;j<L->n;j++){ int tx=L->x+bx+j*L->dx, ty=L->y+by+j*L->dy;
            if(tx<SW[s]&&ty<SHT[s]){ fpoke(sheetseg[s],(u16)(ty*SW[s]+tx),c); if(draw&&s==cur_sheet()) render_cell(tx,ty); } }
          goto nextlink; } }
      nextlink:; } }
}
static void ob_put(u8 v){ iobuf[io_len++]=v; }
static void ob_flush(int force){ if(io_len>=(int)sizeof(iobuf)-140||(force&&io_len)){ if(f_write(io_h,iobuf,io_len)!=io_len) io_err=1; io_len=0; } }
#define HEAD(h) fpeekw(scratch,(u16)((h)*2))
#define PREV(i) fpeekw(scratch,(u16)(8192+(i)*2))
static u16 hash3(int i){ return (u16)((plane[i]*1089+plane[i+1]*33+plane[i+2])&4095); }
static void insert(int j,int n){ if(j+2<n){ u16 h=hash3(j); fpokew(scratch,(u16)(8192+j*2),HEAD(h)); fpokew(scratch,(u16)(h*2),(u16)(j+1)); } }
static void emit_lits(int start,int n){ while(n>0){ int k=n>128?128:n, i; ob_put(k-1); for(i=0;i<k;i++) ob_put(plane[start+i]); start+=k; n-=k; ob_flush(0); } }
static void compress_plane(int n){
  int i=0, ls=0, ln=0;
  fset(scratch,0,0,8192);
  while(i<n){
    int bestLen=0, bestOff=0;
    if(i+2<n){ u16 c=HEAD(hash3(i)); int tries=0, lim=n-i; if(lim>256) lim=256;
      while(c && tries++<48){ int cp=c-1, l=0; while(l<lim && plane[cp+l]==plane[i+l]) l++;
        if(l>bestLen){ bestLen=l; bestOff=i-cp; if(l==lim) break; } c=PREV(cp); } }
    if(i>0){ int lim=n-i, l=0; if(lim>256) lim=256; while(l<lim && plane[i-1]==plane[i+l]) l++; if(l>bestLen){ bestLen=l; bestOff=1; } }
    if(bestLen>=4||(bestLen==3&&ln==0)){
      u16 o=(u16)(0x10000-bestOff); int k;
      emit_lits(ls,ln); ln=0;
      ob_put(o>>8); ob_put(o&255); ob_put(bestLen-1); ob_flush(0);
      for(k=0;k<bestLen;k++) insert(i+k,n);
      i+=bestLen;
    } else { if(!ln) ls=i; ln++; insert(i,n); i++; if(ln==128){ emit_lits(ls,ln); ln=0; } }
  }
  emit_lits(ls,ln);
}
static int save_pck(const char *path, int s){
  int p, w=SW[s], h=SHT[s], bpr=w>>3, size=bpr*h, i;
  io_h=f_create(path); if(io_h<0) return 0;
  io_len=0; io_err=0;
  ob_put(1); ob_put(0); ob_put(w&255); ob_put(w>>8); ob_put(h&255); ob_put(h>>8);
  for(p=0;p<4;p++){
    for(i=0;i<size;i++){ int y=i/bpr, x=(i%bpr)*8, k; u16 base=(u16)(y*w+x); u8 v=0;
      for(k=0;k<8;k++) if(fpeek(sheetseg[s],base+k)&(1<<p)) v|=0x80>>k; plane[i]=v; }
    compress_plane(size);
  }
  ob_flush(1); f_close(io_h);
  return !io_err;
}
static void sheet_path(char *out, const char *set, int s){
  if(seq(set,"DEFAULT")){
    if(s==0) scpy(out,"TERRAIN0\\SHIELDS.PCK"); else if(s==1) scpy(out,"TERRAIN0\\BSHIELD.PCK"); else if(s==2) scpy(out,"TERRAIN0\\ATRANS2.PCK");
    else { char d[2]={(char)('0'+s-3),0}; scpy(out,"TERRAIN0\\MOVEBAR"); scat(out,d); scat(out,".PCK"); }
    return; }
  scpy(out,"SHIELD\\");
  if(s<3){ char pfx[2]={"SBA"[s],0}; scat(out,pfx); scat(out,set); }
  else { char d[2]={(char)('0'+s-3),0}; scat(out,set); scat(out,d); }
  scat(out,".PCK");
}

static int load_set(const char *name){
  int s, ok=0; char path[40];
  for(s=0;s<NSHEET;s++){ sheet_path(path,name,s); if(load_pck(path,s)) ok++; }
  undo_cnt=0; modified=0;
  return ok;
}

/* ======================= names ======================= */
static const char *PLNAME[9]={"Player 1","Player 2","Player 3","Player 4","Player 5","Player 6","Player 7","Player 8","Neutral"};
static const char *SHEETN[4]={"Large shields","Capital shields","Army flags","Status bars"};
static const char *TOOLN[4]={"Pencil","Fill","Pick","Select"};
static const char *COLN[16]={"Black","Light grey","Grey","Dark grey","Charcoal","Sky blue","Blue","Yellow","Orange","Red","Light green","Green","Dark green","Brown","Dark brown","White"};
static const char *part_name(void){ int i,k=0,s=cur_sheet(); if(part_sel<0) return "Whole sheet"; build_parts(player);
  for(i=0;i<nparts;i++) if(parts[i].sheet==s){ if(k==part_sel) return parts[i].name; k++; } return "Whole sheet"; }

/* ======================= actions ======================= */
enum { A_NONE, A_ABOUT, A_HELP, A_LOAD, A_SAVE, A_QUIT, A_UNDO, A_COPY, A_PASTE, A_FLIP, A_RECOLOUR, A_COPYPL,
       A_SHEET, A_PLAYER, A_TOOL, A_MIRROR, A_GRID, A_PART, A_ZIN, A_ZOUT, A_FIT, A_PAN, A_PAL, A_PREVIEW };

/* ======================= widgets in the game's style ======================= */
static void fill_dither(int x,int y,int w,int h,u8 c){ /* 50% pattern, like the game's drop shadows */
  int r; outw(0x3CE,(u16)c<<8); outw(0x3CE,0x0F01);
  for(r=0;r<h;r++){ int i; u8 pat=((y+r)&1)?0xAA:0x55;
    for(i=x>>3;i<=(x+w-1)>>3;i++){ u8 m=pat; if(i==x>>3) m&=0xFF>>(x&7); if(i==(x+w-1)>>3) m&=(u8)(0xFF<<(7-((x+w-1)&7)));
      outw(0x3CE,((u16)m<<8)|8); vga_rmw((y+r)*80+i,0); } }
  outw(0x3CE,0x0001); outw(0x3CE,0xFF08);
}
#define ST_ON 1
#define ST_OFF 2
static void gbutton(int x,int y,int w,int h,const char *label,int st,int big){
  int game=(big==2||big==3); u8 oc=((st&ST_ON)&&!game)?8:0; if(game&&(st&ST_ON)) st=(st&~ST_ON)|8;
  fill(x+1,y,w-2,h,oc); fill(x,y+1,w,h-2,oc);                 /* chamfered outline */
  if(st&ST_ON){ fill(x+1,y+1,w-2,h-2,8); }
  fill(x+2,y+2,w-4,h-4,2);                                     /* face */
  fill(x+2,y+1,w-4,1,(st&ST_ON)?8:1); fill(x+1,y+2,1,h-4,(st&ST_ON)?8:1);
  fill(x+3,y+2,w-6,1,1); fill(x+2,y+3,1,h-6,1);               /* highlight */
  fill(x+3,y+h-3,w-5,1,4); fill(x+w-3,y+3,1,h-5,4);           /* shadow */
  if(big==2||big==3){ Font *f=big==2?&FC:&FS; u8 c=(st&ST_OFF)?3:(st&8)?9:0;
    gtext(f,x+(w-gtext_w(f,label,1))/2,y+(h-f->h)/2,label,c,1); }
  else if(big) ctext_c(x+(w-text_w(&FC,label))/2,y+(h-17)/2,label,(st&ST_OFF)?3:0);
  else ttext(x+(w-text_w(&FT,label))/2,y+(h-15)/2,label,(st&ST_OFF)?3:0);
}
static void frame_box(int x,int y,int w,int h){ /* popup frame like PICS\POPUP.PCK */
  fill_dither(x+6,y+h,w,6,0); fill_dither(x+w,y+6,6,h-6,0);
  fill(x,y,w,1,0); fill(x,y+h-1,w,1,0); fill(x,y,1,h,0); fill(x+w-1,y,1,h,0);
  fill(x+1,y+1,w-2,2,1); fill(x+1,y+1,2,h-2,1); fill(x+1,y+h-3,w-2,2,4); fill(x+w-3,y+1,2,h-2,4);
  fill(x+3,y+3,w-6,1,0); fill(x+3,y+h-4,w-6,1,0); fill(x+3,y+3,1,h-6,0); fill(x+w-4,y+3,1,h-6,0);
}

/* ======================= hotspots ======================= */
#define BTN_MAX 64
typedef struct { s16 x,y,w,h; u8 act, arg; } Btn;
static Btn btns[BTN_MAX]; static int nbtn;
static void hot(int x,int y,int w,int h,int act,int arg){ if(nbtn<BTN_MAX){ Btn *b=&btns[nbtn++]; b->x=x;b->y=y;b->w=w;b->h=h;b->act=act;b->arg=arg; } }
static int hit(int mx,int my){ int i; for(i=nbtn-1;i>=0;i--){ Btn *b=&btns[i]; if(mx>=b->x&&mx<b->x+b->w&&my>=b->y&&my<b->y+b->h) return i; } return -1; }

/* bottom-right panel buttons */
typedef struct { s16 x,y,w,h; u8 act,arg,big; const char *label; } PBtn;
static const PBtn PD[]={
 {404,360,52,20,A_TOOL,0,3,"Pencil"},{458,360,52,20,A_TOOL,1,3,"Fill"},{512,360,52,20,A_TOOL,2,3,"Pick"},{566,360,52,20,A_TOOL,3,3,"Select"},
 {404,383,52,20,A_UNDO,0,ALLGAME*2,"Undo"},{458,383,52,20,A_COPY,0,ALLGAME*2,"Copy"},{512,383,52,20,A_PASTE,0,ALLGAME*2,"Paste"},{566,383,52,20,A_FLIP,0,ALLGAME*2,"Flip"},
 {404,406,28,20,A_ZOUT,0,0,"-"},{434,406,28,20,A_ZIN,0,0,"+"},{464,406,40,20,A_FIT,0,3,"Fit"},
 {508,406,26,20,A_PAN,0,0,"<"},{536,406,26,20,A_PAN,1,0,"^"},{564,406,26,20,A_PAN,2,0,"v"},{592,406,26,20,A_PAN,3,0,">"},
 {404,431,106,28,A_RECOLOUR,0,3,"Recolour"},{514,431,104,28,A_COPYPL,0,3,"Copy Player"} };
#define NPD ((int)(sizeof PD/sizeof PD[0]))
static int pd_state(const PBtn *b){
  switch(b->act){
    case A_TOOL: return tool==b->arg ? ST_ON : 0;
    case A_UNDO: return undo_cnt ? 0 : ST_OFF;
    case A_COPY: case A_FLIP: return sel_on ? 0 : ST_OFF;
    case A_PASTE: return !clip_w ? ST_OFF : pasting ? ST_ON : 0;
    case A_PAN: return scrollable() ? 0 : ST_OFF;
    case A_ZIN: return zoom<ZMAX ? 0 : ST_OFF;
    case A_ZOUT: return zoom>1 ? 0 : ST_OFF;
  }
  return 0;
}
/* palette + preview geometry (top-right panel) */
#define SWX(c) (408+((c)%4)*52)
#define SWY(c) (54+((c)/4)*24)
typedef struct { u8 s; s16 sx,sy,w,h,dx,dy; s8 part; } PV;
static PV pv[12]; static int npv;
#define PVX 406
#define PVY 200
static void build_preview(void){
  int p=player, i;
  npv=0;
  #define ADD(S_,X_,Y_,W_,H_,DX,DY) { pv[npv].s=S_;pv[npv].sx=X_;pv[npv].sy=Y_;pv[npv].w=W_;pv[npv].h=H_;pv[npv].dx=DX;pv[npv].dy=DY;pv[npv].part=-1;npv++; }
  if(p<8){ ADD(0,40*p,0,40,40,0,0) ADD(0,40*p,40,40,24,84,0) ADD(0,320+8*(p%4),40+8*(p/4),8,8,152,3) ADD(2,112+16*(p/4),94+14*(p%4),16,14,130,0) }
  else { ADD(0,320,0,40,40,0,0) ADD(0,352,46,16,18,130,0) }
  ADD(1,32*p,0,32,43,46,0) ADD(1,32*p,43,32,16,84,28) ADD(2,16*p,30,16,64,176,0) ADD(2,16*p,164,16,82,198,0)
  if(p<8) ADD(3+p,8,0,96,18,0,52)
  /* which part each preview item belongs to (for click-to-edit) */
  build_parts(p);
  for(i=0;i<npv;i++){ int j; for(j=0;j<nparts;j++){ Part *q=&parts[j];
      if(q->sheet==pv[i].s && pv[i].sx>=q->x && pv[i].sy>=q->y && pv[i].sx<q->x+q->w && pv[i].sy<q->y+q->h){ pv[i].part=j; break; } } }
}
static void rebuild_hotspots(void){
  int i;
  nbtn=0;
  for(i=0;i<NPD;i++) hot(PD[i].x,PD[i].y,PD[i].w,PD[i].h,PD[i].act,PD[i].arg);
  for(i=0;i<16;i++) hot(SWX(i),SWY(i),46,20,A_PAL,i);
  build_preview(); for(i=0;i<npv;i++) hot(PVX+pv[i].dx,PVY+pv[i].dy,pv[i].w,pv[i].h,A_PREVIEW,i);
}

/* ======================= panels ======================= */
static void draw_swatch(int c){
  int x=SWX(c), y=SWY(c);
  bg_blit(x-2,y-2,50,24);
  if(c==prim||c==sec){ fill(x-2,y-2,50,24,c==prim?8:7); }
  fill(x,y,46,20,0); fill(x+1,y+1,44,18,c);
  if(c==prim) ttext(x+5,y+3,"L",c==0||c==4||c==6||c==12||c==14?15:0);
  if(c==sec) ttext(x+31,y+3,"R",c==0||c==4||c==6||c==12||c==14?15:0);
}
static void draw_preview(void){
  int y,i; build_preview();
  for(y=0;y<138;y++){
    bg_row(PVY+y,400,224,linebuf);
    for(i=0;i<npv;i++){ PV *v=&pv[i]; int r=y-v->dy; if(r>=0&&r<v->h){ int x; for(x=0;x<v->w;x++) linebuf[PVX-400+v->dx+x]=getpx(v->s,v->sx+x,v->sy+r); } }
    putrow8(400,PVY+y,linebuf,224);
  }
}
static void draw_panel_b(void){
  int c; char b[40];
  bg_blit(400,30,224,312);
  ctext(400+(224-text_w(&FC,"Palette"))/2,32,"Palette");
  for(c=0;c<16;c++) draw_swatch(c);
  ctext(408,152,"Left"); fill(448,152,34,18,0); fill(449,153,32,16,prim);
  ctext(510,152,"Right"); fill(556,152,34,18,0); fill(557,153,32,16,sec);
  scpy(b,"Preview: "); scat(b,PLNAME[player]);
  ctext(400+(224-text_w(&FC,b))/2,177,b);
  draw_preview();
}
static void draw_panel_d(void){
  int i; bg_blit(400,355,224,114);
  for(i=0;i<NPD;i++) gbutton(PD[i].x,PD[i].y,PD[i].w,PD[i].h,PD[i].label,pd_state(&PD[i]),PD[i].big);
}
static int hover_x=-1, hover_y=-1;
static void draw_info_line3(void){
  char b[96], t[12];
  bg_blit(16,444,360,24);
  if(msg[0]) scpy(b,msg);
  else if(hover_x>=0){ u8 c=getpx(cur_sheet(),hover_x,hover_y);
    scpy(b,"Position "); sint(t,hover_x); scat(b,t); scat(b,", "); sint(t,hover_y); scat(b,t); scat(b,"   Colour: "); scat(b,COLN[c]); }
  else if(scrollable()) scpy(b,"Arrow keys scroll the picture");
  else scpy(b,"Press F1 for help");
  ctext(22,445,b);
}
static void draw_panel_c(void){
  char b[64], t[12];
  bg_blit(16,403,360,41);
  scpy(b,"Shield set: "); scat(b,setname); if(modified) scat(b," *"); ctext(22,404,b);
  scpy(b,"Zoom: "); sint(t,zoom); scat(b,t); scat(b,"x"); ctext(370-text_w(&FC,b),404,b);
  scpy(b,SHEETN[cur_tab]); scat(b," - "); scat(b,PLNAME[player]); ctext(22,424,b);
  ctext(370-text_w(&FC,part_name()),424,part_name());
  draw_info_line3();
}

/* ======================= menu bar ======================= */
enum { M_SSG, M_SHIELD, M_EDIT, M_SHEET, M_PLAYER, M_TOOLS, M_VIEW, NMENU };
static const char *MTITLE[NMENU]={"Info","Shield","Edit","Sheet","Player","Tools","View"};
static int mtx[NMENU];
static void menubar_layout(void){ int m,x=12; for(m=0;m<NMENU;m++){ mtx[m]=x; x+=text_w(&FT,MTITLE[m])+26; } }
static void draw_menubar(int open){
  int m; fill(0,0,640,MBH,15);
  for(m=0;m<NMENU;m++){ int w=text_w(&FT,MTITLE[m]);
    if(m==open){ fill(mtx[m]-5,0,w+10,MBH,8); ttext(mtx[m],1,MTITLE[m],7); } else ttext(mtx[m],1,MTITLE[m],0); }
}
static int menu_title_at(int mx,int my){ int m; if(my>=MBH||my<0) return -1;
  for(m=0;m<NMENU;m++) if(mx>=mtx[m]-5&&mx<mtx[m]+text_w(&FT,MTITLE[m])+5) return m; return -1; }
typedef struct { const char *label, *key; u8 act, arg, en, chk; } MItem;
static MItem mi[20]; static int nmi, mchk;
static void additem(const char *l,const char *k,int act,int arg,int en,int chk){ MItem *m=&mi[nmi++]; m->label=l; m->key=k; m->act=act; m->arg=arg; m->en=en; m->chk=chk; if(chk) mchk=1; }
static void addsep(void){ additem(0,0,0,0,0,0); mchk=mchk; }
static const char *PLKEY[9]={"alt 1","alt 2","alt 3","alt 4","alt 5","alt 6","alt 7","alt 8","alt 9"};
static void build_menu(int m){
  int i,k,s=cur_sheet(); nmi=0; mchk=0;
  switch(m){
    case M_SSG: additem("About Shield Designer",0,A_ABOUT,0,1,0); additem("Keys and help","F1",A_HELP,0,1,0); break;
    case M_SHIELD: additem("Load shield set","F3",A_LOAD,0,1,0); additem("Save shield set","F2",A_SAVE,0,1,0); addsep(); additem("Quit","^Q",A_QUIT,0,1,0); break;
    case M_EDIT: additem("Undo","^Z",A_UNDO,0,undo_cnt>0,0); addsep();
      additem("Copy","^C",A_COPY,0,sel_on,0); additem("Paste","^V",A_PASTE,0,clip_w>0,0); additem("Flip",0,A_FLIP,0,sel_on,0); addsep();
      additem("Recolour","alt R",A_RECOLOUR,0,1,0); additem("Copy player","alt C",A_COPYPL,0,1,0); break;
    case M_SHEET: for(i=0;i<4;i++) additem(SHEETN[i],0,A_SHEET,i,!(i==3&&player==8),cur_tab==i?1:0); if(cur_tab) {} mchk=1; break;
    case M_PLAYER: for(i=0;i<9;i++) additem(PLNAME[i],PLKEY[i],A_PLAYER,i,!(i==8&&cur_tab==3),player==i); mchk=1; break;
    case M_TOOLS: { static const char *K[4]={"B","G","I","M"}; for(i=0;i<4;i++) additem(TOOLN[i],K[i],A_TOOL,i,1,tool==i); }
      addsep(); additem("Mirror to copies",0,A_MIRROR,0,1,mirror_on); additem("Grid",0,A_GRID,0,1,grid_on); mchk=1; break;
    case M_VIEW: additem("Whole sheet",0,A_PART,255,1,part_sel<0); build_parts(player); k=0;
      for(i=0;i<nparts;i++) if(parts[i].sheet==s){ additem(parts[i].name,0,A_PART,k,1,part_sel==k); k++; }
      addsep(); additem("Zoom in","+",A_ZIN,0,zoom<ZMAX,0); additem("Zoom out","-",A_ZOUT,0,zoom>1,0); additem("Fit to screen","0",A_FIT,0,1,0); mchk=1; break;
  }
}
static int dd_x,dd_y,dd_w,dd_h; static s16 dd_iy[20];
static void layout_dd(int m){
  int i,lw=0,kw=0,y;
  for(i=0;i<nmi;i++) if(mi[i].label){ int a=text_w(&FT,mi[i].label), b=mi[i].key?text_w(&FT,mi[i].key):0; if(a>lw)lw=a; if(b>kw)kw=b; }
  dd_w=lw+(kw?kw+28:0)+14+(mchk?16:0); dd_x=mtx[m]-5; if(dd_x+dd_w>638) dd_x=638-dd_w; dd_y=MBH;
  y=dd_y+3; for(i=0;i<nmi;i++){ dd_iy[i]=y; y+=mi[i].label?16:5; } dd_h=y+2-dd_y;
}
static void draw_item(int i,int hi){
  MItem *m=&mi[i]; u8 col;
  if(!m->label){ fill(dd_x+1,dd_iy[i]+2,dd_w-2,1,0); return; }
  fill(dd_x+1,dd_iy[i],dd_w-2,16,hi?8:15);
  col = !m->en ? 2 : hi ? 7 : 0;
  if(m->chk) ttext(dd_x+5,dd_iy[i],"&",col);
  ttext(dd_x+6+(mchk?16:0),dd_iy[i],m->label,col);
  if(m->key) ttext(dd_x+dd_w-7-text_w(&FT,m->key),dd_iy[i],m->key,col);
}
static int su_b0, su_nb, su_y, su_h;
static void save_under(int x,int y,int w,int h){
  int p,r; su_b0=x>>3; su_nb=((x+w-1)>>3)-su_b0+1; su_y=y; su_h=h;
  for(p=0;p<4;p++){ outw(0x3CE,(u16)p<<8|4); for(r=0;r<h;r++) fmove(scratch,(u16)((p*h+r)*su_nb),0xA000,(u16)((y+r)*80+su_b0),su_nb); }
  outw(0x3CE,0x0004);
}
static void restore_under(void){
  int p,r;
  for(p=0;p<4;p++){ outw(0x3C4,(u16)(1<<p)<<8|2); for(r=0;r<su_h;r++) fmove(0xA000,(u16)((su_y+r)*80+su_b0),scratch,(u16)((p*su_h+r)*su_nb),su_nb); }
  outw(0x3C4,0x0F02);
}
static void open_dd(int m){
  int i; build_menu(m); layout_dd(m);
  mouse_show(0); draw_menubar(m); save_under(dd_x,dd_y,dd_w+1,dd_h+1);
  fill(dd_x,dd_y,dd_w,dd_h,15); fill(dd_x,dd_y,dd_w,1,0); fill(dd_x,dd_y+dd_h-1,dd_w,1,0); fill(dd_x,dd_y,1,dd_h,0); fill(dd_x+dd_w-1,dd_y,1,dd_h,0);
  for(i=0;i<nmi;i++) draw_item(i,0);
  mouse_show(1);
}
static void close_dd(void){ mouse_show(0); restore_under(); draw_menubar(-1); mouse_show(1); }
static int item_at(int mx,int my){ int i; if(mx<dd_x||mx>=dd_x+dd_w) return -1;
  for(i=0;i<nmi;i++) if(mi[i].label&&mi[i].en&&my>=dd_iy[i]&&my<dd_iy[i]+16) return i; return -1; }
static int wait_mouse_release(void){ for(;;){ if(!poll_mouse(0,0)) return 0; } }
/* run the menus; returns act<<8|arg or 0 */
static int run_menus(int m, int pressed){
  int hi=-1, pb=pressed, dragged=0, res=0, mx0, my0;
  poll_mouse(&mx0,&my0);
  open_dd(m);
  for(;;){
    int mx,my,b=poll_mouse(&mx,&my), k=getkey(), t, it;
    if(k){ int ch=k&0xFF, sc=k>>8;
      if(ch==27){ break; }
      if(ch==0&&(sc==0x4B||sc==0x4D)){ close_dd(); m=(m+(sc==0x4D?1:NMENU-1))%NMENU; hi=-1; open_dd(m); continue; }
      if(ch==0&&(sc==0x48||sc==0x50)){ int n=nmi, i=hi; do{ i=(i+(sc==0x50?1:n-1)+n)%n; }while(!(mi[i].label&&mi[i].en)&&i!=hi);
        mouse_show(0); if(hi>=0) draw_item(hi,0); hi=i; draw_item(hi,1); mouse_show(1); continue; }
      if(ch==13&&hi>=0){ res=mi[hi].act<<8|mi[hi].arg; break; }
    }
    if(mx!=mx0||my!=my0) dragged=1;
    t=menu_title_at(mx,my);
    if(t>=0&&t!=m){ close_dd(); m=t; hi=-1; open_dd(m); }
    it=item_at(mx,my);
    if(it!=hi&&(it>=0||mx>=dd_x&&mx<dd_x+dd_w&&my>=dd_y&&my<dd_y+dd_h)){ mouse_show(0); if(hi>=0) draw_item(hi,0); hi=it; if(hi>=0) draw_item(hi,1); mouse_show(1); }
    if(b&&!pb){ if(it>=0){ res=mi[it].act<<8|mi[it].arg; break; } if(t==m) break; if(t<0&&it<0&&!(mx>=dd_x&&mx<dd_x+dd_w&&my>=dd_y&&my<dd_y+dd_h)) break; }
    if(!b&&pb&&pressed&&dragged&&it>=0){ res=mi[it].act<<8|mi[it].arg; break; }
    if(!b&&pb) pressed=0;
    pb=b;
  }
  close_dd(); wait_mouse_release();
  return res;
}

/* ======================= full redraw ======================= */
static void redraw_all(void){
  mouse_show(0);
  bg_blit(0,MBH,640,480-MBH);   /* whole marble screen, clears dialog shadows */
  draw_menubar(-1);
  render_canvas(); draw_panel_b(); draw_panel_c(); draw_panel_d();
  rebuild_hotspots();
  mouse_show(1);
}
static void redraw_panels(void){ mouse_show(0); draw_menubar(-1); draw_panel_b(); draw_panel_c(); draw_panel_d(); rebuild_hotspots(); mouse_show(1); }
static void say(const char *s){ scpy(msg,s); mouse_show(0); draw_info_line3(); mouse_show(1); }

/* ======================= dialogs (marble popups) ======================= */
static int dlg_x, dlg_y, dlg_w, dlg_h;
static void dialog_open(int w,int h,const char *title){
  dlg_w=w; dlg_h=h; dlg_x=((640-w)/2)&~7; dlg_y=(480-h)/2;
  mouse_show(0);
  frame_box(dlg_x,dlg_y,w,h);
  { int ox, oy; /* tile marble from inside the big panel (source x&7 matches dlg_x+4) */
    for(oy=0;oy<h-8;oy+=344) for(ox=0;ox<w-8;ox+=352){ int cw_=w-8-ox, ch_=h-8-oy; if(cw_>352) cw_=352; if(ch_>344) ch_=344;
      bg_copy(20,40,dlg_x+4+ox,dlg_y+4+oy,cw_,ch_); } }
  if(title) ctext(dlg_x+(w-text_w(&FC,title))/2,dlg_y+8,title);
  nbtn=0; mouse_show(1);
}
static void dbutton(int x,int y,int w,int h,const char *l,int st,int act,int arg,int big){ if(ALLGAME&&big==1) big=2; mouse_show(0); gbutton(dlg_x+x,dlg_y+y,w,h,l,st,big); mouse_show(1); hot(dlg_x+x,dlg_y+y,w,h,act,arg); }
static void dtext_center(int y,const char *s){ mouse_show(0); ttext(dlg_x+(dlg_w-text_w(&FT,s))/2,dlg_y+y,s,0); mouse_show(1); }
/* waits for a click on a dialog button (returns index) or a key (returns -1, *key set) */
static int dialog_wait(int *key){
  wait_mouse_release();
  for(;;){ int k=getkey(), mx, my, b;
    if(k){ *key=k; return -1; }
    b=poll_mouse(&mx,&my);
    if(b&1){ int i=hit(mx,my); if(i>=0){ wait_mouse_release(); *key=0; return i; } } }
}
static int confirm(const char *q){
  int k,i,w=text_w(&FT,q)+48; if(w<300) w=300;
  dialog_open(w,104,"Confirm"); dtext_center(36,q);
  dbutton(w/2-110,66,100,26,"Yes",0,1,0,1); dbutton(w/2+10,66,100,26,"No",0,2,0,1);
  for(;;){ i=dialog_wait(&k);
    if(i>=0) return btns[i].act==1;
    k&=0xFF; if(k=='y'||k=='Y'||k==13) return 1; if(k=='n'||k=='N'||k==27) return 0; }
}
static void alertn(const char *title,const char **lines,int n){
  int k,i,w=280,h=n*16+86;
  for(i=0;i<n;i++){ int lw=text_w(&FT,lines[i])+40; if(lw>w) w=lw; }
  if(w>620) w=620;
  dialog_open(w,h,title);
  mouse_show(0); for(i=0;i<n;i++) ttext(dlg_x+20,dlg_y+34+i*16,lines[i],0); mouse_show(1);
  dbutton(w/2-50,h-36,100,26,"OK",0,1,0,1);
  for(;;){ i=dialog_wait(&k); if(i>=0) return; k&=0xFF; if(k==13||k==27||k==' ') return; }
}
static void alert(const char *a,const char *b){ const char *l[2]={a,b}; alertn("Shield Designer",l,b?2:1); }
static int input_name(char *name){
  int len=slen(name), k, i; char b[12];
  dialog_open(320,132,"Save Shield Set");
  dtext_center(32,"Name (1-7 letters or digits):");
  dbutton(50,96,100,26,"Save",0,1,0,1); dbutton(170,96,100,26,"Cancel",0,2,0,1);
  for(;;){
    mouse_show(0); fill(dlg_x+60,dlg_y+56,200,24,0); fill(dlg_x+61,dlg_y+57,198,22,15);
    scpy(b,name); scat(b,"_"); ttext(dlg_x+68,dlg_y+61,b,0); mouse_show(1);
    i=dialog_wait(&k);
    if(i>=0){ if(btns[i].act==2) return 0; if(len>0) return 1; continue; }
    { int ch=k&0xFF;
      if(ch==27) return 0;
      if(ch==13){ if(len>0) return 1; continue; }
      if(ch==8){ if(len) name[--len]=0; continue; }
      if(ch>='a'&&ch<='z') ch-=32;
      if(((ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_')&&len<7){ name[len++]=ch; name[len]=0; } }
  }
}
static char setlist[24][9]; static int nsets;
static void scan_sets(void){
  u16 a,b,c,d; nsets=0; scpy(setlist[nsets++],"DEFAULT");
  a=0x1A00; b=0; c=0; d=0x80; int21(&a,&b,&c,&d);
  a=0x4E00; b=0; c=0; d=NEAR("SHIELD\\*.SHL");
  if(int21(&a,&b,&c,&d)) return;
  do{ char *fn=(char*)(u32)0x9E; int i=0; char nm[9]; asm volatile("":"+r"(fn));
      while(fn[i]&&fn[i]!='.'&&i<8){ nm[i]=fn[i]; i++; } nm[i]=0;
      if(nsets<24&&i>0) scpy(setlist[nsets++],nm);
      a=0x4F00; b=0; c=0; d=0; } while(!int21(&a,&b,&c,&d));
}
static int choose_set(char *out){
  int i,k,rows; scan_sets(); rows=(nsets+1)/2;
  dialog_open(340,84+rows*30,"Load Shield Set");
  for(i=0;i<nsets;i++) dbutton(20+(i%2)*156,34+(i/2)*30,144,26,setlist[i],seq(setlist[i],origin)?ST_ON:0,1,i,0);
  dbutton(120,40+rows*30,100,26,"Cancel",0,2,0,1);
  for(;;){ i=dialog_wait(&k); if(i>=0){ if(btns[i].act==2) return 0; scpy(out,setlist[btns[i].arg]); return 1; } if((k&0xFF)==27) return 0; }
}
static void draw_player_big(int x,int y,int p){
  static u8 m[30][30]; static u8 q[900*2]; int r,c,qh=0,qt=0,i; u8 bgc;
  int sx=40*p+6, sy=6;
  bgc=getpx(0,sx,sy);
  for(r=0;r<30;r++) for(c=0;c<30;c++) m[r][c]=0;
  for(i=0;i<30;i++){ int pts[4][2]={{i,0},{i,29},{0,i},{29,i}}; int k;
    for(k=0;k<4;k++){ int cx=pts[k][0], cy=pts[k][1]; if(!m[cy][cx]&&getpx(0,sx+cx,sy+cy)==bgc){ m[cy][cx]=1; q[qt++]=cx; q[qt++]=cy; } } }
  while(qh<qt){ int cx=q[qh++], cy=q[qh++], d; for(d=0;d<4;d++){ int nx=cx+(d==0)-(d==1), ny=cy+(d==2)-(d==3);
      if(nx<0||ny<0||nx>29||ny>29||m[ny][nx]) continue; if(getpx(0,sx+nx,sy+ny)!=bgc) continue; m[ny][nx]=1; q[qt++]=nx; q[qt++]=ny; } }
  for(r=0;r<30;r++) for(c=0;c<30;c++) if(!m[r][c]) putpix(x+c,y+r,getpx(0,sx+c,sy+r));
}
static void draw_player_chip(int x,int y,int p){ /* the player's small status-bar shield, 13x15 */
  int r,c; if(p>7){ for(r=0;r<15;r++) for(c=0;c<13;c++) putpix(x+c,y+r,getpx(0,353+c,47+r)); return; }
  for(r=0;r<15;r++) for(c=0;c<13;c++) putpix(x+c,y+r,getpx(3+p,11+c,3+r));
}

/* ======================= commands ======================= */
static void set_view_changed(void){ sel_on=0; pasting=0; compute_fit(); redraw_all(); }
static void set_zoom(int nz){
  int cx, cy;
  if(nz<1) nz=1; if(nz>ZMAX) nz=ZMAX; if(nz==zoom) return;
  layout(); cx=(CX+CW/2-imgx)/zoom; cy=(CY+CH/2-imgy)/zoom;
  zoom=nz; panx=cx-(CW/2)/zoom; pany=cy-(CH/2)/zoom; paste_ox_reset();
  msg[0]=0; redraw_all();
}
static void pan(int dir){
  int step;
  if(!scrollable()){ say("The picture fits - zoom in first"); return; }
  step=(dir<2?CH:CW)/zoom/4; if(step<1) step=1;
  if(dir==0) panx-=step; else if(dir==1) pany-=step; else if(dir==2) pany+=step; else panx+=step;
  paste_ox_reset(); redraw_all();
}
static void do_load(void){
  char nm[9];
  if(modified && !confirm("Discard unsaved changes?")){ redraw_all(); return; }
  if(choose_set(nm)){
    int ok=load_set(nm); scpy(origin,nm); if(!seq(nm,"DEFAULT")) scpy(setname,nm);
    if(ok<NSHEET){ char t[8]; scpy(msg,"Loaded "); sint(t,ok); scat(msg,t); scat(msg," of 11 files"); }
    else { scpy(msg,"Loaded shield set "); scat(msg,nm); } }
  set_view_changed();
}
static int do_save(void){
  char nm[9], path[40]; int s, ok=1;
  scpy(nm,setname);
  if(!input_name(nm)){ redraw_all(); return 0; }
  if(seq(nm,"DEFAULT")){ alert("DEFAULT is reserved - please pick another name.",0); redraw_all(); return 0; }
  scpy(path,"SHIELD\\"); scat(path,nm); scat(path,".SHL");
  if(f_exists(path) && !confirm("That shield set exists - overwrite it?")){ redraw_all(); return 0; }
  f_mkdir("SHIELD");
  say("Saving...");
  for(s=0;s<NSHEET&&ok;s++){ sheet_path(path,nm,s); ok=save_pck(path,s); }
  if(ok){ int h; scpy(path,"SHIELD\\"); scat(path,nm); scat(path,".SHL"); h=f_create(path); if(h<0) ok=0; else { f_write(h,"1\r\n\r\n",5); f_close(h); } }
  if(!ok){ alert("Save failed - is the disk full or read-only?",0); redraw_all(); return 0; }
  scpy(setname,nm); scpy(origin,nm); modified=0;
  scpy(msg,"Saved! In the game pick Shield: "); scat(msg,nm);
  redraw_all(); return 1;
}
static void copy_design(int from,int to){
  Part A[12], B[12]; int na, nb, i, x, y;
  build_parts(from); na=nparts; memcpy(A,parts,sizeof A);
  build_parts(to); nb=nparts; memcpy(B,parts,sizeof B);
  begin_stroke();
  for(i=0;i<na&&i<nb;i++){ if(!seq(A[i].name,B[i].name)) continue;
    for(y=0;y<A[i].h;y++) for(x=0;x<A[i].w;x++) setpx(B[i].sheet,B[i].x+x,B[i].y+y,getpx(A[i].sheet,A[i].x+x,A[i].y+y),0,0); }
  end_stroke();
}
static void dlg_copy_player(void){
  int k,i,p; cp_from=player; if(cp_to==cp_from) cp_to=(cp_from+1)%8; if(cp_from>7) cp_from=0;
  for(;;){
    char b[64];
    dialog_open(472,232,"Copy Player");
    mouse_show(0); ctext(dlg_x+18,dlg_y+48,"From"); ctext(dlg_x+18,dlg_y+102,"Onto"); mouse_show(1);
    for(p=0;p<8;p++){ dbutton(70+p*48,34,44,46,"",p==cp_from?ST_ON:0,1,p,0); mouse_show(0); draw_player_big(dlg_x+70+p*48+7,dlg_y+34+8,p); mouse_show(1); }
    for(p=0;p<8;p++){ dbutton(70+p*48,88,44,46,"",p==cp_to?ST_ON:0,2,p,0); mouse_show(0); draw_player_big(dlg_x+70+p*48+7,dlg_y+88+8,p); mouse_show(1); }
    scpy(b,PLNAME[cp_from]); scat(b,"  >  "); scat(b,PLNAME[cp_to]); mouse_show(0); ctext(dlg_x+(472-text_w(&FC,b))/2,dlg_y+146,b); mouse_show(1);
    dbutton(126,184,100,28,"Copy",cp_from==cp_to?ST_OFF:0,3,0,1); dbutton(246,184,100,28,"Cancel",0,4,0,1);
    i=dialog_wait(&k);
    if(i<0){ if((k&0xFF)==27){ redraw_all(); return; } if((k&0xFF)!=13) continue; i=-2; }
    if(i>=0&&btns[i].act==1){ cp_from=btns[i].arg; continue; }
    if(i>=0&&btns[i].act==2){ cp_to=btns[i].arg; continue; }
    if(i>=0&&btns[i].act==4){ redraw_all(); return; }
    if(cp_from==cp_to) continue;
    copy_design(cp_from,cp_to); player=cp_to;
    scpy(msg,"Copied "); scat(msg,PLNAME[cp_from]); scat(msg," onto "); scat(msg,PLNAME[cp_to]);
    set_view_changed(); return;
  }
}
static int do_recolour(void){
  int r, n=0, x, y; struct { u8 s; s16 x,y,w,h; } rects[12]; int nr=0;
  if(rc_scope==0){ build_parts(player); for(r=0;r<nparts;r++){ rects[nr].s=parts[r].sheet; rects[nr].x=parts[r].x; rects[nr].y=parts[r].y; rects[nr].w=parts[r].w; rects[nr].h=parts[r].h; nr++; } }
  else if(rc_scope==1){ view_rect(); rects[0].s=cur_sheet(); rects[0].x=vx; rects[0].y=vy; rects[0].w=vw; rects[0].h=vh; nr=1; }
  else { rects[0].s=cur_sheet(); rects[0].x=sel_x; rects[0].y=sel_y; rects[0].w=sel_w; rects[0].h=sel_h; nr=1; }
  begin_stroke();
  for(r=0;r<nr;r++) for(y=rects[r].y;y<rects[r].y+rects[r].h;y++) for(x=rects[r].x;x<rects[r].x+rects[r].w;x++){
    u8 c=getpx(rects[r].s,x,y);
    if(c==rc_from){ setpx(rects[r].s,x,y,rc_to,0,0); n++; } else if(rc_swap&&c==rc_to){ setpx(rects[r].s,x,y,rc_from,0,0); n++; } }
  end_stroke(); return n;
}
static void dlg_recolour(void){
  static const char *SC[3]={"This player","Current view","Selection"};
  int k,i,c;
  if(rc_scope==2&&!sel_on) rc_scope=0;
  for(;;){
    dialog_open(400,236,"Recolour");
    mouse_show(0); ctext(dlg_x+20,dlg_y+36,"From"); ctext(dlg_x+20,dlg_y+70,"To"); mouse_show(1);
    for(c=0;c<16;c++){ int x=dlg_x+70+c*20;
      hot(x,dlg_y+36,18,18,1,c); hot(x,dlg_y+70,18,18,2,c);
      mouse_show(0);
      fill(x-1,dlg_y+35,20,20,c==rc_from?8:0); fill(x+1,dlg_y+37,16,16,c);
      fill(x-1,dlg_y+69,20,20,c==rc_to?8:0); fill(x+1,dlg_y+71,16,16,c);
      mouse_show(1); }
    for(i=0;i<3;i++) dbutton(20+i*122,108,114,24,SC[i],(rc_scope==i?ST_ON:0)|(i==2&&!sel_on?ST_OFF:0),3,i,0);
    dbutton(20,140,236,24,rc_swap?"Swap both ways":"One way only",rc_swap?ST_ON:0,4,0,0);
    { char b[48]; scpy(b,COLN[rc_from]); scat(b," > "); scat(b,COLN[rc_to]); mouse_show(0); ttext(dlg_x+266,dlg_y+144,b,0); mouse_show(1); }
    dbutton(90,186,100,30,"Apply",rc_from==rc_to?ST_OFF:0,5,0,1); dbutton(210,186,100,30,"Cancel",0,6,0,1);
    i=dialog_wait(&k);
    if(i<0){ if((k&0xFF)==27){ redraw_all(); return; } continue; }
    switch(btns[i].act){
      case 1: rc_from=btns[i].arg; break; case 2: rc_to=btns[i].arg; break;
      case 3: if(btns[i].arg!=2||sel_on) rc_scope=btns[i].arg; break;
      case 4: rc_swap=!rc_swap; break;
      case 6: redraw_all(); return;
      case 5: if(rc_from==rc_to) break; { int n=do_recolour(); char t[8]; sint(t,n); scpy(msg,"Recoloured "); scat(msg,t); scat(msg," pixels"); redraw_all(); return; }
    }
  }
}
static void do_flip(void){
  int s=cur_sheet(), x, y; u8 row[400];
  if(!sel_on){ say("Select an area first (Select tool)"); return; }
  begin_stroke();
  for(y=sel_y;y<sel_y+sel_h;y++){ for(x=0;x<sel_w;x++) row[x]=getpx(s,sel_x+x,y); for(x=0;x<sel_w;x++) setpx(s,sel_x+x,y,row[sel_w-1-x],1,0); }
  end_stroke(); redraw_all();
}
static void do_copy(void){
  int s=cur_sheet(), y;
  if(!sel_on){ say("Select an area first (Select tool)"); return; }
  for(y=0;y<sel_h;y++) fmove(clipseg,(u16)(y*sel_w),sheetseg[s],(u16)((sel_y+y)*SW[s]+sel_x),sel_w);
  clip_w=sel_w; clip_h=sel_h;
  scpy(msg,"Copied - now Paste and click to place"); redraw_panels();
}
static void stamp(int x0,int y0){
  int s=cur_sheet(), x, y; begin_stroke();
  for(y=0;y<clip_h;y++) for(x=0;x<clip_w;x++) setpx(s,x0+x,y0+y,fpeek(clipseg,(u16)(y*clip_w+x)),1,0);
  end_stroke();
}
static void flood(int x0,int y0,u8 c){
  int s=cur_sheet(); u8 t=getpx(s,x0,y0); u16 qh=0, qt=0;
  if(t==c) return;
  begin_stroke();
  setpx(s,x0,y0,c,1,1); fpokew(scratch,qt,x0); fpokew(scratch,qt+2,y0); qt+=4;
  while(qh!=qt){ int x=fpeekw(scratch,qh), y=fpeekw(scratch,qh+2), d; qh+=4;
    for(d=0;d<4;d++){ int nx=x+(d==0)-(d==1), ny=y+(d==2)-(d==3);
      if(nx<vx||ny<vy||nx>=vx+vw||ny>=vy+vh) continue;
      if(getpx(s,nx,ny)!=t) continue;
      setpx(s,nx,ny,c,1,1); fpokew(scratch,qt,nx); fpokew(scratch,qt+2,ny); qt+=4; } }
  end_stroke();
}
static const char *HELP[]={
  "Left button paints with the L colour, right button with the R colour.",
  "Choose sheet, player and part from the Sheet, Player and View menus,",
  "or click a picture in the Preview to edit that part.",
  "Mirror to copies: edits repeat on stacked flags and the status bar.",
  "",
  "B Pencil    G Fill    I Pick colour    M Select    X swap L and R",
  "U or ^Z Undo    ^C Copy    ^V Paste (Shift+click places several)",
  "+ / - Zoom    0 Fit    arrow keys scroll when zoomed in",
  "F2 Save set    F3 Load set    ^Q or F10 Quit    Esc cancel",
  "",
  "Saved sets go to SHIELD\\ - in a game choose Shield: <name>."};
static const char *ABOUT[]={
#if !ABOUT_BIG
  "Shield Designer",
#endif
  "Draw new shields for all eight sides and save them",
  "as a shield set that the game can use.",
  "",
  "The marble screen and the fonts are read from your",
  "own Warlords II files in this directory.",
  "",
  "Shield Designer for Warlords II",
  "Made with love for the Warlords series by A. Pretty",
  "Shield Designer v1.04"};
#define NABOUT ((int)(sizeof ABOUT/sizeof ABOUT[0]))
static void about_dialog(void){
  int i,k,w=300,h,top, big=ABOUT_BIG&&FB.h;
  for(i=0;i<NABOUT;i++){ int lw=text_w(&FC,ABOUT[i])+48; if(lw>w) w=lw; }
  if(big){ int lw=text_w(&FB,"Shield Designer")+48; if(lw>w) w=lw; }
  top = big ? 34+FB.h : 34;
  h=top+NABOUT*18+18+50;
  dialog_open(w,h,big?0:"About");
  mouse_show(0);
  if(big){ mask_text(&FB,FB.mout,dlg_x+(w-text_w(&FB,"Shield Designer"))/2,dlg_y+14,"Shield Designer",0);
           mask_text(&FB,FB.mfill,dlg_x+(w-text_w(&FB,"Shield Designer"))/2,dlg_y+14,"Shield Designer",15); }
  for(i=0;i<NABOUT;i++) ctext(dlg_x+(w-text_w(&FC,ABOUT[i]))/2,dlg_y+top+i*18,ABOUT[i]);
  mouse_show(1);
  dbutton(w/2-50,h-38,100,28,"OK",0,1,0,2);
  for(;;){ i=dialog_wait(&k); if(i>=0) return; k&=0xFF; if(k==13||k==27||k==' ') return; }
}
static int want_quit(void){ if(!modified) return 1; if(confirm("Quit without saving?")) return 1; redraw_all(); return 0; }

/* returns 1 to quit */
static int do_action(int act,int arg,int right){
  msg[0]=0;
  switch(act){
    case A_ABOUT: about_dialog(); redraw_all(); break;
    case A_HELP: alertn("Keys and Help",HELP,11); redraw_all(); break;
    case A_LOAD: do_load(); break;
    case A_SAVE: do_save(); break;
    case A_QUIT: return want_quit();
    case A_UNDO: if(undo()){ scpy(msg,"Undone"); redraw_all(); } else say("Nothing to undo"); break;
    case A_COPY: do_copy(); break;
    case A_PASTE: if(clip_w){ pasting=1; scpy(msg,"Click to place - Esc to stop"); redraw_panels(); } else say("Copy a selection first"); break;
    case A_FLIP: do_flip(); break;
    case A_RECOLOUR: dlg_recolour(); break;
    case A_COPYPL: dlg_copy_player(); break;
    case A_SHEET: if(arg==3&&player==8) break; cur_tab=arg; part_sel=-1; set_view_changed(); break;
    case A_PLAYER: if(arg==8&&cur_tab==3) cur_tab=0; player=arg; part_sel=-1; set_view_changed(); break;
    case A_TOOL: tool=arg; pasting=0; redraw_panels(); break;
    case A_MIRROR: mirror_on=!mirror_on; scpy(msg,mirror_on?"Mirror to copies: on":"Mirror to copies: off"); redraw_all(); break;
    case A_GRID: grid_on=!grid_on; redraw_all(); break;
    case A_PART: part_sel = arg==255 ? -1 : arg; set_view_changed(); break;
    case A_ZIN: set_zoom(zoom+1); break;
    case A_ZOUT: set_zoom(zoom-1); break;
    case A_FIT: compute_fit(); redraw_all(); break;
    case A_PAN: pan(arg); break;
    case A_PAL: if(right) sec=arg; else prim=arg; mouse_show(0); draw_panel_b(); mouse_show(1); break;
    case A_PREVIEW: { PV *v=&pv[arg]; int j,k=0;
        if(v->part<0) break; build_parts(player);
        for(j=0;j<v->part;j++) if(parts[j].sheet==parts[v->part].sheet) k++;
        cur_tab = v->s>=3 ? 3 : v->s; part_sel=k; set_view_changed(); } break;
  }
  return 0;
}

/* ======================= input ======================= */
static int canvas_cell(int mx,int my,int *ix,int *iy,int clamp){
  int x=(mx-imgx), y=(my-imgy), in;
  x = x<0 ? -1 : x/zoom; y = y<0 ? -1 : y/zoom;
  in = x>=0&&y>=0&&x<vw&&y<vh;
  if(clamp){ if(x<0)x=0; if(y<0)y=0; if(x>=vw)x=vw-1; if(y>=vh)y=vh-1; }
  *ix=vx+x; *iy=vy+y; return in;
}
static int over_canvas(int mx,int my){ return mx>=CX&&mx<CX+CW&&my>=CY&&my<CY+CH; }
static void paste_outline(int ix,int iy){ xor_rect(imgx+(ix-vx)*zoom, imgy+(iy-vy)*zoom, clip_w*zoom, clip_h*zoom); }
static void paste_hide(void){ if(paste_ox>=0){ mouse_show(0); paste_outline(paste_ox,paste_oy); mouse_show(1); paste_ox=-1; } }

static const char *need_files[]={"TERRAIN0\\SHIELDS.PCK","PICS\\SCREEN0.PCK","TEXT.FNT","TEXT.FIN","CHANCE17.FNT","CHANCE17.FIN"};
int main(void){
  int s, i; u16 a,b,c,d;
  a=0x4A00; b=0x1000; c=0; d=0; int21(&a,&b,&c,&d);
  for(i=0;i<6;i++) if(!f_exists(need_files[i])){
    dos_print("SHLDEDIT: run me from your Warlords II directory\r\n(could not find $"); dos_print(need_files[i]); dos_print(").\r\n$"); dos_exit(1); }
  a=0; b=0; c=0; d=0; mouse(&a,&b,&c,&d);
  if(a!=0xFFFF){ dos_print("SHLDEDIT: a mouse driver is required (load MOUSE.COM first).\r\n$"); dos_exit(1); }
  for(s=0;s<NSHEET;s++){ sheetseg[s]=dos_alloc((SW[s]*SHT[s]+15)/16); if(!sheetseg[s]) goto nomem; }
  scratch=dos_alloc(4096); clipseg=dos_alloc(2214); fontseg=dos_alloc(2048); if(!scratch||!clipseg||!fontseg) goto nomem;
  for(i=0;i<4;i++){ bgseg[i]=dos_alloc(2400); if(!bgseg[i]) goto nomem; }
  for(undo_n=0;undo_n<UNDO_MAX;undo_n++){ undoseg[undo_n]=dos_alloc(2214); if(!undoseg[undo_n]) break; }
  if(undo_n<1) goto nomem;
  dos_print("Shield Designer: loading...\r\n$");
  if(!load_font(&FT,"TEXT",1,0,0,1,0)||!load_font(&FC,"CHANCE17",2,15,0,0,2048)){ dos_print("SHLDEDIT: could not read the game's fonts.\r\n$"); dos_exit(1); }
  make_scaled(&FS,&FC,SMALL_H,7424);
  if(ABOUT_BIG && !load_font(&FB,"CHANCE36",2,15,0,0,10240)) FB.h=0;
  if(!load_bg()){ dos_print("SHLDEDIT: could not read PICS\\SCREEN0-3.PCK.\r\n$"); dos_exit(1); }
  build_links(); menubar_layout();
  if(load_set("DEFAULT")<NSHEET){ dos_print("SHLDEDIT: could not read the default shields in TERRAIN0\\.\r\n$"); dos_exit(1); }
  vga_init();
  a=0; b=0; c=0; d=0; mouse(&a,&b,&c,&d);
  a=2; b=0; c=0; d=0; mouse(&a,&b,&c,&d);
  a=0x0F; b=0; c=8; d=8; mouse(&a,&b,&c,&d);
  a=7; b=0; c=0; d=639; mouse(&a,&b,&c,&d); a=8; b=0; c=0; d=479; mouse(&a,&b,&c,&d);
  a=4; b=0; c=520; d=300; mouse(&a,&b,&c,&d);
  compute_fit();
  scpy(msg,"Welcome! Menus are at the top - F1 for help");
  redraw_all();
  {
    int lastb=0, lx=-1, ly=-1, drawing=0, last_ix=-1, last_iy=-1, sel_ax=0, sel_ay=0, rb_x=-1, rb_y=0, rb_w=0, rb_h=0;
    u8 dcol=0;
    for(;;){
      int k=getkey(), mx, my, btn, ix, iy, in;
      if(k){ int ch=k&0xFF, sc=k>>8, q=0;
        if(ch==27){ if(pasting){ paste_hide(); pasting=0; msg[0]=0; redraw_panels(); } else if(sel_on){ sel_on=0; redraw_all(); } }
        else if(ch==26||ch=='u'||ch=='U') q=do_action(A_UNDO,0,0);
        else if(ch=='b'||ch=='B') q=do_action(A_TOOL,0,0); else if(ch=='g'||ch=='G') q=do_action(A_TOOL,1,0);
        else if(ch=='i'||ch=='I') q=do_action(A_TOOL,2,0); else if(ch=='m'||ch=='M') q=do_action(A_TOOL,3,0);
        else if(ch=='x'||ch=='X'){ u8 t=prim; prim=sec; sec=t; mouse_show(0); draw_panel_b(); mouse_show(1); }
        else if(ch==3) q=do_action(A_COPY,0,0); else if(ch==22) q=do_action(A_PASTE,0,0);
        else if(ch==17) q=do_action(A_QUIT,0,0);
        else if(ch=='+'||ch=='=') set_zoom(zoom+1); else if(ch=='-') set_zoom(zoom-1);
        else if(ch=='0') do_action(A_FIT,0,0);
        else if((ch==0||ch==0xE0)&&(sc==0x4B||sc==0x48||sc==0x50||sc==0x4D)) pan(sc==0x4B?0:sc==0x48?1:sc==0x50?2:3);
        else if(ch==0&&sc>=0x78&&sc<=0x80) q=do_action(A_PLAYER,sc-0x78,0);          /* alt 1..9 */
        else if(ch==0&&sc==0x13) q=do_action(A_RECOLOUR,0,0);                        /* alt R */
        else if(ch==0&&sc==0x2E) q=do_action(A_COPYPL,0,0);                          /* alt C */
        else if(sc==0x3B) q=do_action(A_HELP,0,0);
        else if(sc==0x3C) q=do_action(A_SAVE,0,0); else if(sc==0x3D) q=do_action(A_LOAD,0,0);
        else if(sc==0x44) q=do_action(A_QUIT,0,0);
        if(q) break;
      }
      btn=poll_mouse(&mx,&my);
      in = over_canvas(mx,my) ? canvas_cell(mx,my,&ix,&iy,0) : 0;
      if(mx!=lx||my!=ly){
        int hx=in?ix:-1, hy=in?iy:-1;
        if(hx!=hover_x||hy!=hover_y){ hover_x=hx; hover_y=hy; if(!msg[0]||hx>=0){ msg[0]=0; mouse_show(0); draw_info_line3(); mouse_show(1); } }
        if(pasting){ int px,py; if(over_canvas(mx,my)){ canvas_cell(mx,my,&px,&py,0); if(px!=paste_ox||py!=paste_oy){ paste_hide(); mouse_show(0); paste_outline(px,py); mouse_show(1); paste_ox=px; paste_oy=py; } } else paste_hide(); }
        lx=mx; ly=my;
      }
      if(btn && !lastb){
        int right=(btn&2)?1:0;
        if(my<MBH){ int t=menu_title_at(mx,my); if(t>=0){ int r; paste_hide(); r=run_menus(t,1); lastb=0; if(r&&do_action(r>>8,r&255,0)) break; continue; } }
        else if(over_canvas(mx,my)){
          int cx_,cy_; int inside=canvas_cell(mx,my,&cx_,&cy_,0); int s=cur_sheet();
          if(pasting){ if(inside){ u16 kk=0x0200; asm volatile("int $0x16":"+a"(kk)::"cc");
              paste_hide(); stamp(cx_,cy_); if(!(kk&3)){ pasting=0; msg[0]=0; } redraw_all(); } }
          else if(tool==2){ if(inside){ if(right) sec=getpx(s,cx_,cy_); else prim=getpx(s,cx_,cy_); mouse_show(0); draw_panel_b(); mouse_show(1); } }
          else if(tool==1){ if(inside){ mouse_show(0); flood(cx_,cy_,right?sec:prim); draw_preview(); draw_panel_c(); draw_panel_d(); mouse_show(1); } }
          else if(tool==3){ canvas_cell(mx,my,&sel_ax,&sel_ay,1); drawing=2; if(sel_on){ sel_on=0; mouse_show(0); render_canvas(); mouse_show(1); } rb_x=-1; }
          else if(inside){ drawing=1; dcol=right?sec:prim; begin_stroke(); mouse_show(0); setpx(s,cx_,cy_,dcol,1,1); mouse_show(1); last_ix=cx_; last_iy=cy_; }
        } else {
          int h=hit(mx,my); if(h>=0){ Btn *t=&btns[h]; int st=0;
            for(i=0;i<NPD;i++) if(PD[i].act==t->act&&PD[i].arg==t->arg&&PD[i].x==t->x&&PD[i].y==t->y) st=pd_state(&PD[i]);
            if(!(st&ST_OFF)){ if(do_action(t->act,t->arg,right)) break; } }
        }
      }
      else if(btn && drawing==1){
        int cx_,cy_; canvas_cell(mx,my,&cx_,&cy_,1);
        if(cx_!=last_ix||cy_!=last_iy){
          int x0=last_ix,y0=last_iy,dx=cx_>x0?cx_-x0:x0-cx_, dy=-(cy_>y0?cy_-y0:y0-cy_), sx=x0<cx_?1:-1, sy=y0<cy_?1:-1, err=dx+dy, s=cur_sheet();
          mouse_show(0);
          for(;;){ setpx(s,x0,y0,dcol,1,1); if(x0==cx_&&y0==cy_) break; { int e2=2*err; if(e2>=dy){err+=dy;x0+=sx;} if(e2<=dx){err+=dx;y0+=sy;} } }
          mouse_show(1); last_ix=cx_; last_iy=cy_; }
      }
      else if(btn && drawing==2){
        int cx_,cy_,x0,y0,w,h; canvas_cell(mx,my,&cx_,&cy_,1);
        x0=sel_ax<cx_?sel_ax:cx_; y0=sel_ay<cy_?sel_ay:cy_; w=(sel_ax>cx_?sel_ax-cx_:cx_-sel_ax)+1; h=(sel_ay>cy_?sel_ay-cy_:cy_-sel_ay)+1;
        if(x0!=sel_x||y0!=sel_y||w!=sel_w||h!=sel_h||rb_x<0){
          mouse_show(0); if(rb_x>=0) xor_rect(rb_x,rb_y,rb_w,rb_h);
          sel_x=x0; sel_y=y0; sel_w=w; sel_h=h; rb_x=imgx+(x0-vx)*zoom; rb_y=imgy+(y0-vy)*zoom; rb_w=w*zoom; rb_h=h*zoom; xor_rect(rb_x,rb_y,rb_w,rb_h);
          mouse_show(1); }
      }
      if(!btn && lastb){
        if(drawing==1){ end_stroke(); mouse_show(0); draw_preview(); draw_panel_c(); draw_panel_d(); mouse_show(1); }
        if(drawing==2){ char t[8]; sel_on=1; rb_x=-1; scpy(msg,"Selection "); sint(t,sel_w); scat(msg,t); scat(msg," x "); sint(t,sel_h); scat(msg,t); redraw_all(); }
        drawing=0;
      }
      lastb=btn;
    }
  }
  int10(0x0003,0,0,0);
  dos_print("Shield Designer for Warlords II - farewell!\r\n$");
  return 0;
nomem:
  dos_print("SHLDEDIT: not enough conventional memory (need about 480K free).\r\n$");
  return 1;
}
