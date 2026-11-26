/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include "../wayland/meta-wayland-layer-layout.h"
int main(void) {
  LayerRect b={1920,0,1920,1080}, r;
  LayerLayout s={.anchor=LAYER_TOP|LAYER_LEFT|LAYER_RIGHT,.height=32,.left=8,.right=8,.top=4,.zone=36};
  assert(layer_layout_geometry(s,b,&r));
  assert(r.x==1928 && r.y==4 && r.width==1904 && r.height==32);
  assert(layer_layout_exclusive_edge(s)==LAYER_TOP);
  s=(LayerLayout){.anchor=LAYER_BOTTOM|LAYER_RIGHT,.width=200,.height=100,.right=12,.bottom=20,.zone=100};
  assert(layer_layout_geometry(s,b,&r));
  assert(r.x==3628 && r.y==960 && layer_layout_exclusive_edge(s)==0);
  s=(LayerLayout){.width=300,.height=200};
  assert(layer_layout_geometry(s,b,&r));
  assert(r.x==2730 && r.y==440);
  s.width=0; assert(!layer_layout_geometry(s,b,&r));
  s.anchor=16; assert(!layer_layout_geometry(s,b,&r));
  s=(LayerLayout){.anchor=15,.left=INT32_MAX,.right=INT32_MAX};
  assert(!layer_layout_geometry(s,b,&r));
  LayerRect usable=b, band;
  s=(LayerLayout){.anchor=LAYER_TOP|LAYER_LEFT|LAYER_RIGHT,.zone=40,.top=8};
  assert(layer_layout_reserve(s,b,&usable,&band));
  assert(usable.y==40 && usable.height==1040 && band.width==1920 && band.height==40);
  s.zone=24;
  assert(layer_layout_reserve(s,b,&usable,&band));
  assert(band.y==40 && band.height==24 && usable.y==64);
  s=(LayerLayout){.anchor=LAYER_LEFT,.zone=60};
  assert(layer_layout_reserve(s,b,&usable,&band));
  assert(band.x==1920 && band.y==0 && band.height==1080 && usable.x==1980);
  s=(LayerLayout){.anchor=LAYER_BOTTOM,.zone=INT32_MAX};
  assert(layer_layout_reserve(s,b,&usable,&band));
  assert(usable.height==1);
  assert(!layer_layout_reserve(s,b,&usable,&band));
  s=(LayerLayout){.anchor=LAYER_TOP|LAYER_LEFT,.zone=20};
  assert(!layer_layout_reserve(s,b,&usable,&band));
  s=(LayerLayout){.anchor=LAYER_RIGHT,.zone=-1};
  assert(!layer_layout_reserve(s,b,&usable,&band));
  s.zone=20;
  assert(layer_layout_reserve(s,b,&usable,&band));
  assert(band.x==3820 && band.width==20 && usable.width==1840);
  return 0;
}
