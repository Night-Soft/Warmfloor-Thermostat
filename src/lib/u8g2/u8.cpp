#include <string.h>
#include <algorithm> // Required header
#include <esp_log.h>

#include "u8.h"

using std::max;
using std::min;

#define I2C_SDA GPIO_NUM_5
#define I2C_SCL GPIO_NUM_9
#define OLED_ADDR 0x3C

#define OLED_COL_OFFSET 2
#define OLED_RAM_WIDTH  132

static uint8_t fb[128 * 8];

static i2c_master_bus_handle_t i2cBus = nullptr;
static i2c_master_dev_handle_t displayDev = nullptr;

struct font_metrics_t {
    int8_t max_w, max_h;
    int8_t ascent_A, descent_g;      // descent отрицательный
    int8_t ascent_paren, descent_paren;
} fontMetrics;

class PrintProp {
 public:
  PrintProp() : offsetTop(fontMetrics.max_h) {};
  int8_t& offsetTop;
  uint8_t colorIdx;  // 0-1
  bool isFontposTop;
  struct Cursor {
    uint8_t x, y;
  } cursor;
} printProps;

// static uint8_t* color = &printProps.colorIdx;

static void oled_cmd(uint8_t cmd) {
  uint8_t buf[2] = {0x00, cmd};  // 0x00 = control byte: команда
  i2c_master_transmit(displayDev, buf, 2, 100);
}

void oled_data(const uint8_t *data, size_t len) {
  uint8_t buf[OLED_RAM_WIDTH + 1];
  buf[0] = 0x40;
  memcpy(&buf[1], data, len);
  i2c_master_transmit(displayDev, buf, len + 1, 100);
}

static void oled_set_pos(uint8_t page, uint8_t col) {
  oled_cmd(0xB0 | page);
  oled_cmd(0x00 | (col & 0x0F));
  oled_cmd(0x10 | (col >> 4));
}

// void oled_clear(void) {
//   uint8_t zeros[OLED_RAM_WIDTH] = {0};
//   memset(fb, 0, sizeof(fb));
//   for (uint8_t page = 0; page < 8; page++) {
//     oled_set_pos(page, 0);                  // с колонки 0
//     oled_data(zeros, OLED_RAM_WIDTH);       // все 132 колонки
//   }
// }

void clearBuff() { memset(fb, 0, sizeof(fb)); }

void sendBuff(void) {
  for (uint8_t page = 0; page < 8; page++) {
    oled_set_pos(page, OLED_COL_OFFSET);    // видимая область с колонки 2
    oled_data(&fb[page * 128], 128);
  }
}




static void oled_init(void) {
  const uint8_t init[] = {
      0xAE,        // display off
      0xD5, 0x80,  // clock div
      0xA8, 0x3F,  // multiplex 64
      0xD3, 0x00,  // display offset
      0x40,        // start line
      0x8D, 0x14,  // charge pump ON
      0x20, 0x02,  // page addressing mode
      0xA1,        // segment remap
      0xC8,        // COM scan direction
      0xDA, 0x12,  // COM pins
      0x81, 0xCF,  // contrast
      0xD9, 0xF1,  // precharge
      0xDB, 0x40,  // VCOMH
      0xA4,        // resume from RAM
      0xA6,        // normal display
      0xAF         // display on
  };
  for (size_t i = 0; i < sizeof(init); i++) oled_cmd(init[i]);
}


/* ---- framebuffer под SSD1306 128x64 ---- */
static void set_pixel(int x, int y) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
  fb[(y >> 3) * 128 + x] |= printProps.colorIdx << (y & 7);
}

static void set_pixel(int x, int y, const uint8_t color) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
  fb[(y >> 3) * 128 + x] |= color << (y & 7);
}

/* ---- чтение битов ---- */
typedef struct {
  const uint8_t* p;
  uint8_t bit;
} bitreader_t;

static uint8_t get_bits(bitreader_t* r, uint8_t cnt) {
  uint8_t val = *r->p >> r->bit;
  uint8_t pos = r->bit + cnt;
  if (pos >= 8) {
    r->p++;
    val |= *r->p << (8 - r->bit);
    pos -= 8;
  }
  r->bit = pos;
  return val & ((1U << cnt) - 1);
}

static int8_t get_signed(bitreader_t* r, uint8_t cnt) {
  return (int8_t)get_bits(r, cnt) - (1 << (cnt - 1));
}

/* ---- шрифт ---- */
typedef struct {
  const uint8_t* data;  // начало данных глифов (font + 23)
  const uint8_t* glyphs;  // начало данных глифов (font + 23)
  uint8_t m0, m1, bw, bh, bx, by, bd;
  uint16_t pos_A, pos_a;
} font_prop_t;

font_prop_t currentFont;

void setFontMetrics(const uint8_t* font, font_metrics_t* metrics) {
  metrics->max_w = (int8_t)font[9]; // макс. ширина (bbx w)	
  metrics->max_h = (int8_t)font[10]; // макс. высота (bbx h)	
  metrics->ascent_A = (int8_t)font[13]; // ascent для 'A'	
  metrics->descent_g = (int8_t)font[14]; // descent для 'g' (знаковый, обычно отрицательный)	
  metrics->ascent_paren = (int8_t)font[15]; // ascent для '('	
  metrics->descent_paren = (int8_t)font[16]; // descent для ')' (знаковый)
}

static void font_init(font_prop_t * f, const uint8_t* font) {
  f->data = font;
  f->glyphs = font + 23;
  f->m0 = font[2];
  f->m1 = font[3];
  f->bw = font[4];
  f->bh = font[5];
  f->bx = font[6];
  f->by = font[7];
  f->bd = font[8];
  f->pos_A = (font[17] << 8) | font[18];
  f->pos_a = (font[19] << 8) | font[20];
}

static const uint8_t* find_glyph(const font_prop_t* f, uint8_t enc) {
  const uint8_t* p = f->glyphs;
  if (enc >= 'a')
    p += f->pos_a;
  else if (enc >= 'A')
    p += f->pos_A;

  for (;;) {
    if (p[1] == 0) return NULL;     // конец таблицы
    if (p[0] == enc) return p + 2;  // данные глифа
    p += p[1];                      // прыжок к следующему
  }
}

uint16_t getStrWidth16(const char* str) {
  uint8_t* ptr = (uint8_t*)str;
  bitreader_t r = {};  //{g, 0};
  uint16_t width = 0;
  
  while (*ptr) {
    uint8_t enc = (uint8_t)*ptr++;
    const uint8_t* g = find_glyph(&currentFont, enc);
    if (!g) return 0;

    r.p = g;
    width += get_bits(&r, currentFont.bw);
  }

  return width;
}

/* y = базовая линия; возвращает сдвиг по x */
static int draw_char(const font_prop_t* f, int x, int y, uint8_t enc) {
  const uint8_t* g = find_glyph(f, enc);
  if (!g) return 0;

  bitreader_t r = {g, 0};
  int w = get_bits(&r, f->bw);
  int h = get_bits(&r, f->bh);
  int xo = get_signed(&r, f->bx);
  int yo = get_signed(&r, f->by);
  int dx = get_signed(&r, f->bd);

  if (w && h) {
    int left = x + xo;
    int top = y - (h + yo);
    int px = 0, py = 0;
    while (py < h) {
      int a = get_bits(&r, f->m0);
      int b = get_bits(&r, f->m1);
      uint8_t again;
      do {
        for (int i = 0; i < a + b && py < h; i++) {
          if (i >= a) set_pixel(left + px, top + py);
          if (++px >= w) {
            px = 0;
            py++;
          }
        }
        again = get_bits(&r, 1);
      } while (again);
    }
  }
  return dx;
}



void printStr(const char* s) {
  uint8_t x = printProps.cursor.x, y = printProps.cursor.y;
  if(printProps.isFontposTop) y += printProps.offsetTop;

  const font_prop_t* f = &currentFont;
  while (*s) x += draw_char(f, x, y, (uint8_t)*s++);
}

void demo(void) {
 // print(0, 0, "Hello, World!");  // y=12: базовая линия
 // print(0, printProps.offsetTop + 1, "ESP32-C3");
}

/* Шаг курсора (delta_x) для одного символа, 0 если глифа нет */
static int glyph_advance(const font_prop_t *f, uint8_t enc)
{
    const uint8_t *g = find_glyph(f, enc);
    if (!g) return 0;

    bitreader_t r = { g, 0 };
    get_bits(&r, f->bw);        // ширина (пропускаем)
    get_bits(&r, f->bh);        // высота
    get_signed(&r, f->bx);      // x-смещение
    get_signed(&r, f->by);      // y-смещение
    return get_signed(&r, f->bd);   // delta_x
}

/* Ширина строки как сумма шагов курсора.
   Именно так считает u8g2_GetStrWidth для моноширинных и обычных шрифтов:
   удобно для выравнивания, позиционирования следующего текста. */
static int str_advance(const char *s)
{
    int w = 0;
    while (*s) w += glyph_advance(&currentFont, (uint8_t)*s++);
    return w;
}

/* Точная ширина по закрашенным пикселям: от левого края первого символа
   до правого края последнего (без «пустого» хвоста после последнего символа). */
static int str_pixel_width(const font_prop_t *f, const char *s)
{
    if (!*s) return 0;

    int adv_before_last = 0;
    int first_xo = 0;
    int last_xo = 0, last_w = 0;

    for (int i = 0; s[i]; i++) {
        const uint8_t *g = find_glyph(f, (uint8_t)s[i]);
        int w = 0, xo = 0, dx = 0;
        if (g) {
            bitreader_t r = { g, 0 };
            w  = get_bits(&r, f->bw);
            get_bits(&r, f->bh);
            xo = get_signed(&r, f->bx);
            get_signed(&r, f->by);
            dx = get_signed(&r, f->bd);
        }
        if (i == 0) first_xo = xo;
        if (s[i + 1] == '\0') {          // последний символ
            last_xo = xo;
            last_w  = w;
        } else {
            adv_before_last += dx;
        }
    }
    return adv_before_last + last_xo + last_w - first_xo;
}

// uint8_t pixels[8][10];

// void convertBitmap(const uint8_t* bitmap) {
//   memset(pixels, 8 * 10, 0);

//   for (uint8_t y = 0; y < 8; ++y) {
//     for (uint8_t x = 0; x < 10; ++x) {
//       uint8_t byteIndex = y * 2 + x / 8;
//       uint8_t bitIndex = x % 8;

//       pixels[y][x] = (bitmap[byteIndex] >> bitIndex) & 1;
//     }
//   }

// }

// void pr() {
//   for (int y = 0; y < 8; ++y) {
//     for (int x = 0; x < 10; ++x) {
//       std::cout << (pixels[y][x] ? '#' : '.');
//     }

//     std::endl;
//   }
// }

// void pr() {
//   for(int i = 0; i < 64*128; i++) {
    
//     if(i%128)std::cout << std::endl;
//       std::cout << (*(fb + y*x) ? '#' : '.');
//   }
// }

// void drawXBM(const uint8_t xS, const uint8_t yS, const uint8_t w, const uint8_t h,
//              const uint8_t* bitmap) {
//   for (uint8_t y = 0; y < h; ++y) {
//     for (uint8_t x = 0; x < w; ++x) {
//       uint8_t byteIndex = y * 2 + x / h;
//       uint8_t bitIndex = x % 8;
//       uint16_t fbPos = (y + xS) * 128 + (x + xS);
//       *(fb + fbPos) = (bitmap[byteIndex] >> bitIndex) & 1;
//     }
//   }
// }

void dXBM(const uint8_t x, const uint8_t y, const uint8_t w, const uint8_t h,
          const uint8_t* bitmap) {
  for (uint8_t yB = 0; yB < h; ++yB) {
    for (uint8_t xB = 0; xB < w; ++xB) {
      uint8_t byteIndex = yB * ((w + 7) / 8) + xB / 8;
      uint8_t bitIndex = xB % 8;


      uint8_t v = (bitmap[byteIndex] >> bitIndex) & 1;

      uint16_t xF = xB + x;
      uint16_t yF = yB + y;

      if (xF >= 128 || yF >= 64) continue;

      uint16_t index = (yF >> 3) * 128 + xF;
      uint8_t mask = 1U << (yF & 7);

      if (v) {
        fb[index] |= mask;
      } else {
        fb[index] &= ~mask;
      }
    }
  }
}

U8::U8(i2c_master_dev_handle_t &i2cHandle) { displayDev = i2cHandle; } // no need
U8::U8() {};
U8::~U8() {};

void U8::begin(i2c_master_bus_handle_t &i2cBus) {
  ::i2cBus = i2cBus;

  i2c_device_config_t display_cfg = {};
  display_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  display_cfg.device_address = OLED_ADDR;
  display_cfg.scl_speed_hz = 400000;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(::i2cBus, &display_cfg, &displayDev));
  ESP_LOGI("OLED", "add_device OK, handle=%p", (void*)displayDev);
  oled_init();
  ESP_LOGI("OLED alredy init", "add_device OK, handle=%p", (void*)displayDev);
  this->setFont(u8g2_font_profont15_tf);
  this->setColorIndex(1);
}  //

void U8::sendBuffer() { sendBuff(); }    //
void U8::clearBuffer() { clearBuff(); }  //

void U8::setFont(font_t font) {  //
  font_init(&currentFont, (uint8_t*)font);
  setFontMetrics(currentFont.data, &fontMetrics);
}  //

void U8::setFontPosTop() {
  printProps.isFontposTop = true;
  // const font_metrics_t& m = fontMetrics;
  // uint8_t v =
  //     max(m.ascent_A, m.ascent_paren) - min(m.descent_g, m.descent_paren);

}  //

void U8::setFontPosBottom() { printProps.isFontposTop = false; }  //

void U8::setColorIndex(const uint8_t colorIdx) {
  printProps.colorIdx = colorIdx;
}  //

void U8::setCursor(const uint8_t x, const uint8_t y) {
  printProps.cursor.x = x;
  printProps.cursor.y = y;
}  //

void U8::print(const char* str) { printStr(str); }  //

uint8_t U8::getMaxCharHeight() { return (uint8_t)fontMetrics.max_h; }
uint8_t U8::getMaxCharWidth() { return (uint8_t)fontMetrics.max_w; }
uint8_t U8::getStrWidth(const char* str) {
  return (uint8_t)str_advance(str);
  //return (uint8_t)getStrWidth16(str);
}  // ?

void U8::drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (uint16_t yB = 0; yB < h; ++yB) {
        for (uint16_t xB = 0; xB < w; ++xB) {
            uint16_t xF = xB + x;
            uint16_t yF = yB + y;

            if (xF >= 128 || yF >= 64) {
                continue;
            }

            uint16_t index = (yF >> 3) * 128 + xF;
            uint8_t mask = 1U << (yF & 7);

            if (printProps.colorIdx) {
                fb[index] |= mask;
            } else {
                fb[index] &= ~mask;
            }
        }
    }
}

void U8::drawRBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r) {
  drawBox(x, y, w, h);  // todo
}  // ?
void U8::drawXBM(const uint8_t xS, const uint8_t yS, const uint8_t w,
                 const uint8_t h, const uint8_t* bitmap) {
  dXBM(xS, yS, w, h, bitmap);
}

enum class PowerSave : uint8_t {
  disable = 0x0af,  // display on
  enable = 0x0ae  // display off
};

void U8::setPowerSave(bool value) {
  PowerSave powerCmd = value ? PowerSave::enable : PowerSave::disable;
  oled_cmd((uint8_t)powerCmd);
}  // todo