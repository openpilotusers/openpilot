#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/params.h"
#include "ui.hpp"


bool control_button_clicked1(int touch_x, int touch_y) {
  if (touch_x >= 1585 && touch_x <= 1725) {
    if (touch_y >= 905 && touch_y <= 1045) {
      return true;
    }
  }
  return false;
}

bool control_button_clicked2(int touch_x, int touch_y) {
  if (touch_x >= 1425 && touch_x <= 1565) {
    if (touch_y >= 905 && touch_y <= 1045) {
      return true;
    }
  }
  return false;
}

static void draw_control_buttons(UIState *s, int touch_x, int touch_y) {
  if (s->vision_connected){
    int btn_w = 140;
    int btn_h = 140;
    int btn_x1 = 1920 - btn_w - 195;
    int btn_x2 = 1920 - btn_w - 355;
    int btn_y = 1080 - btn_h - 35;
    int btn_xc1 = btn_x1 + (btn_w/2);
    int btn_xc2 = btn_x2 + (btn_w/2);
    int btn_yc = btn_y + (btn_h/2);
    nvgBeginPath(s->vg);
    nvgRoundedRect(s->vg, btn_x1, btn_y, btn_w, btn_h, 100);
    nvgRoundedRect(s->vg, btn_x2, btn_y, btn_w, btn_h, 100);
    nvgStrokeColor(s->vg, nvgRGBA(255,255,255,80));
    nvgStrokeWidth(s->vg, 6);
    nvgStroke(s->vg);
    
    nvgFontSize(s->vg, 45);
    
    if (s->lat_mode == 0) {
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc1,btn_yc,"OPA",NULL);
    } else if (s->lat_mode == 1) {
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc1,btn_yc,"CITY",NULL);
    } else if (s->lat_mode == 2) {
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc1,btn_yc,"HIGW",NULL);
    } else if (s->lat_mode == 3) {
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc1,btn_yc,"ONEW",NULL);
    }
    if (s->acc_mode == 0) {
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc2,btn_yc,"ECO",NULL);
    } else if (s->acc_mode == 1) {
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc2,btn_yc,"NORM",NULL);
    } else if (s->acc_mode == 2) {
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc2,btn_yc,"SPRT",NULL);
    }
  }
}

bool latcontrol( UIState *s, int touch_x, int touch_y ) {

  bool touched = false;
  
  draw_control_buttons(s, touch_x, touch_y);

  if ((control_button_clicked1(touch_x,touch_y)) && (s->scene.uilayout_sidebarcollapsed == true)) {
    s->lat_mode = s->lat_mode + 1;
    if (s->lat_mode > 3) {
      s->lat_mode = 0;
    }
    if (s->lat_mode == 0) {
      Params().write_db_value("OpkrLatMode", "0", 1);
    } else if (s->lat_mode == 1) {
      Params().write_db_value("OpkrLatMode", "1", 1);
    } else if (s->lat_mode == 2) {
      Params().write_db_value("OpkrLatMode", "2", 1);
    } else if (s->lat_mode == 3) {
      Params().write_db_value("OpkrLatMode", "3", 1);
    }
    touched = true;
  }
  if ((control_button_clicked2(touch_x,touch_y)) && (s->scene.uilayout_sidebarcollapsed == true)) {
    s->acc_mode = s->acc_mode + 1;
    if (s->acc_mode > 2) {
      s->acc_mode = 0;
    }
    if (s->acc_mode == 0) {
      Params().write_db_value("OpkrAccMode", "0", 1);
    } else if (s->acc_mode == 1) {
      Params().write_db_value("OpkrAccMode", "1", 1);
    } else if (s->acc_mode == 2) {
      Params().write_db_value("OpkrAccMode", "2", 1);
    }
    touched = true;
  }
  
  return touched;
}