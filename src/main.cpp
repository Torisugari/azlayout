/* 
 *    Copyright (C) 2014 Torisugari <torisugari@gmail.com>
 *
 *     Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <math.h>

#include <cairo.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <cairo-ft.h>

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb-icu.h>
#include <assert.h>
#include <algorithm>

#include "vo/utr50.h"
namespace azlayout {

struct point_t {
  point_t () {
  }
  point_t (double aX, double aY): mX(aX), mY(aY) {}
  point_t (const point_t& aPoint): mX(aPoint.mX), mY(aPoint.mY) {}

  point_t operator+ (const point_t& aPoint) const {
    return point_t(mX + aPoint.mX, mY + aPoint.mY);
  }

  point_t operator- (const point_t& aPoint) const {
    return point_t(mX - aPoint.mX, mY - aPoint.mY);
  }

  point_t operator+= (const point_t& aPoint) {
    mX += aPoint.mX;
    mY += aPoint.mY;
    return (*this);
  }

  point_t operator-= (const point_t& aPoint) {
    mX -= aPoint.mX;
    mY -= aPoint.mY;
    return (*this);
  }

  bool operator== (const point_t& aPoint) const {
    return (mX == aPoint.mX) && (mY == aPoint.mY);
  }
  double mX, mY;
};

struct rect_t {
  rect_t () {}
  rect_t (const point_t& aStart, const point_t& aEnd):
    mStart(aStart), mEnd(aEnd) {}
  rect_t (double aSX, double aSY, double aEX, double aEY):
    mStart(aSX, aSY), mEnd(aEX, aEY){}
  rect_t (const point_t& aStart, double aWidth, double aHeight):
    mStart(aStart),
    mEnd(point_t(aStart.mX + aWidth, aStart.mY + aHeight)) {}
  rect_t (const rect_t& aRect):
    mStart(aRect.mStart), mEnd(aRect.mEnd) {}
  double width() const {
    return mEnd.mX - mStart.mX;
  }
  double height() const {
    return mEnd.mY - mStart.mY;
  }

  bool isValid(const double aMinWidth = 0.,
               const double aMinHeight = 0.) const {
    return (aMinWidth < width()) && (aMinHeight < height());
  }

  bool contains(const point_t& aPoint) const {
    return (mStart.mX <= aPoint.mX) && 
           (aPoint.mX <= mEnd.mX) &&
           (mStart.mY <= aPoint.mY) &&
           (aPoint.mY <= mEnd.mY);
  }

  bool contains(const rect_t& aRect) const {
    return (mStart.mX <= aRect.mStart.mX) && 
           (mStart.mX <= aRect.mStart.mY) &&
           (aRect.mEnd.mX <= mEnd.mX) &&
           (aRect.mEnd.mY <= mEnd.mY);
  }

  point_t mStart, mEnd;
};

void dumpPoint(const point_t& aPoint) {
  fprintf(stderr, "(%f, %f) ", aPoint.mX, aPoint.mY);
}

void dumpRect(const rect_t& aRect) {
  std::cerr << "{";
  dumpPoint(aRect.mStart);
  dumpPoint(aRect.mEnd);
  std::cerr << "}" << std::endl;
}

enum orient {
  kHorizontal,
  kVertical
};

// See <http://www.w3.org/TR/2011/WD-jlreq-20111129/#elements_of_kihonhanmen>
// XXX Why is it difficult to translate Kihon-hanmen
//     ("基本版面", lit. "basic-print-face") for them? However,
//     that's out of this application's scope.

class KihonHanmen {
protected:
  rect_t mRect;  
  double mColumnGap;
  orient mOrient;
  std::vector<rect_t> mColumns;

  uint32_t mIndex;
public:
  KihonHanmen (const rect_t& aRect, double aColumnGap = 0.,
               uint32_t aColumnCount = 1, orient aOrient = kVertical) :
    mRect(aRect), mColumnGap(aColumnGap), mOrient(aOrient), mIndex(0) {

    mColumns.resize(aColumnCount);

    double totalColumnProgress = (aOrient == kVertical)?
      aRect.height(): aRect.width();
    double columnSize = (aOrient == kVertical)?
      aRect.width() : aRect.height();

    double totalColumnGap = aColumnGap * (aColumnCount - 1);
    double columnProgress = (totalColumnProgress - totalColumnGap) /
                              aColumnCount;

    // XXX Here ensure |columnProgress| is not zero/negative;

    point_t start, end, delta;
    if (aOrient == kVertical) {
      start = aRect.mStart;
      end = start + point_t(columnSize, columnProgress);
      delta = point_t(0., columnProgress + aColumnGap); 
    }
    else {
      start = aRect.mStart;
      end = start + point_t(columnProgress, columnSize);
      delta = point_t(columnProgress + aColumnGap, 0.);
    }

    for (uint32_t i = 0; i < aColumnCount; i++) {
      mColumns[i] = rect_t(start, end);
      start += delta;
      end += delta;
    }
  }

  bool currentColumn(rect_t& aFace) const {
    aFace = mColumns[mIndex];
    return isLastColumn();
  }

  bool isLastColumn() const {
    return mColumns.size() - 1 == mIndex;
  }

  bool newColumn(rect_t& aFace) {
    if (isLastColumn()) {
      mIndex = 0;
    }
    else {
      mIndex++;
    }
    return currentColumn(aFace);
  }

  void feed() {
    mIndex = 0;
  }
};

class Page {
  rect_t mOuterRect;
  rect_t mInnerRect;
public:
  double mMarginTop;
  double mMarginBottom;
  double mMarginLeft;
  double mMarginRight;
  const rect_t& innerRect() const {
    return mInnerRect;
  }
  const rect_t& outerRect() const {
    return mOuterRect;
  }

  Page (double aWidth, double aHeight,
        double aMarginLeft = 0., double aMarginTop = 0.,
        double aMarginRight = 0., double aMarginBottom = 0.):
    mOuterRect(0., 0., aWidth, aHeight),
    mMarginTop(aMarginTop), mMarginBottom(aMarginBottom),
    mMarginLeft(aMarginLeft), mMarginRight(aMarginRight) {
#ifdef DEBUG
    fprintf(stderr, "Margin: %f %f %f %f\n", mMarginTop, mMarginBottom,
                                             mMarginLeft, mMarginRight);
#endif
    resize();
  }

  void resize() {
    mInnerRect.mStart.mX = mOuterRect.mStart.mX + mMarginLeft;
    mInnerRect.mStart.mY = mOuterRect.mStart.mY + mMarginTop;
    mInnerRect.mEnd.mX = mOuterRect.mEnd.mX - mMarginRight;
    mInnerRect.mEnd.mY = mOuterRect.mEnd.mY - mMarginBottom;
  }
};

inline
void getVerticalOriginFromLineRect(const rect_t& aRect, const double aFontSize,
                                   point_t& aOrigin) {
  aOrigin.mX = aRect.mEnd.mX - (aFontSize / 2.);
  aOrigin.mY = aRect.mStart.mY;
}

inline
void getHorizontalOriginFromLineRect(const rect_t& aRect, const double aAscent,
                                     point_t& aOrigin) {
  aOrigin.mX = aRect.mEnd.mX;
  aOrigin.mY = aRect.mStart.mY - aAscent;
}

inline
void getVerticalLineRect(const rect_t& aRect, const point_t& aOffset,
                         const double aLineTickness,
                         rect_t& aLineRect) {
  aLineRect.mStart.mX = aRect.mEnd.mX + aOffset.mX - aLineTickness;
  aLineRect.mStart.mY = aRect.mStart.mY + aOffset.mY;
  aLineRect.mEnd.mX = aRect.mEnd.mX + aOffset.mX;
  aLineRect.mEnd.mY = aRect.mEnd.mY;
}

inline
void getHorizontalLineRect(const rect_t& aRect,
                           const double aAdvanceOffset,
                           const double aLineProgressOffset,
                           const double aLineTickness,
                           rect_t& aLineRect) {
  aLineRect.mStart.mX = aRect.mEnd.mX + aAdvanceOffset;
  aLineRect.mStart.mY = aRect.mStart.mY + aLineProgressOffset;
  aLineRect.mEnd.mX = aRect.mEnd.mX;
  aLineRect.mEnd.mY = aRect.mStart.mY + aLineProgressOffset + aLineTickness ;
}

static const char _kDummyDumpcairo[] = "";
void dumpcairo(cairo_t* aC, int aLine, const char* aInfo = _kDummyDumpcairo) {
  cairo_status_t cs = cairo_status(aC);
  if (cs) {
    fprintf(stderr, "L%d: cairo_status:%s %s\n",
            aLine, cairo_status_to_string(cs), aInfo);
    exit(-1);
  }
}
#define AZ_DUMP_CAIRO(_c_,_m_) dumpcairo(_c_,__LINE__,_m_)

// @return false  If there's no room in this rectangle to draw a new glyph.

enum lineState {
  LINE_STATE_CONTINUE_LINE   = 0,// Start with previous line.
  LINE_STATE_NEW_LINE        = 1,// We don't know anything. Just a initial state.
  LINE_STATE_SOFT_LINEBREAK,     // Linebreak because of too long to render.
  LINE_STATE_HARD_LINEBREAK,     // Linebreak because of '\n', '\r' "<br>" etc.
  LINE_STATE_END_OF_COLUMN,      // No blank area for the next linebreak.
  LINE_STATE_END_OF_STRING,      // No data to write exists any more.
  LINE_STATE_TOO_SHORT_LINE
};

struct range_t {
                    // []: selected, s: mStart = 3, e: mEnd = 7
                    // 0 1 2 3 4 5 6 7 8 9
                    // * * *[s * * *]e * * 
  uint32_t mStart;  // pointer to the first byte of selected data
  uint32_t mEnd;    // pointer to the first byte of non-selected data
  range_t () {}
  range_t (uint32_t aStart, uint32_t aEnd): mStart(aStart), mEnd(aEnd) {}
  uint32_t length() const {
    return mEnd - mStart;
  }
};

struct SelectionList {
  SelectionList(){}
  range_t mRange;
  SelectionList* mNext;
};

struct RubyList : public SelectionList {
  RubyList(){}
  std::string mData;
  RubyList* mNext;
};

enum progressionProperty {
  TEXT_PROPERTY_DEFAULT = 0, // Too little infomation to decide.
  TEXT_PROPERTY_VERTICAL,    // No rotation.
  TEXT_PROPERTY_HORIZONTAL,  // Rotate 90deg clockwise.
  TEXT_PROPERTY_TATECHUYOKO // kainda ligature, 2 halfwidth makes 1 fullwidth.
};

enum connectionProperty {
  TEXT_PROPERTY_INLINE,    // Reuse the previous line.
  TEXT_PROPERTY_LINEBREAK, // Reuse the page, but start at a new line.
  TEXT_PROPERTY_PAGEBREAK  // Start with a brand new page.
};

struct TextPropertyList : public SelectionList {
  TextPropertyList(){}
  progressionProperty mProgression;
  connectionProperty mConnection;
  uint32_t mIndent;
  bool mPageFeed;
  TextPropertyList* mNext;
};

uint32_t backtrackHan(const char* aParentDocument, uint32_t aLength, 
                      uint32_t aDirty) {
  hb_buffer_t* buff = hb_buffer_create();

  hb_buffer_set_unicode_funcs(buff, hb_icu_get_unicode_funcs());
  hb_buffer_add_utf8(buff, aParentDocument + aLength - aDirty,
                     aDirty, 0, -1);
  uint32_t glyphlen;
  hb_glyph_info_t* hbInfo = hb_buffer_get_glyph_infos(buff, &glyphlen);

  if (glyphlen == 0) {
    hb_buffer_destroy(buff);
    return 0;
  }

  int i;
  for (i = glyphlen - 1; i > -1; i--) {
    hb_script_t script = hb_unicode_script(hb_icu_get_unicode_funcs(),
                                           hbInfo[i].codepoint);
    if (HB_SCRIPT_HAN != script) {
      break;
    }
  }

  i++;
  if (i == int(glyphlen)) { // unlikely, hopefully
    i = glyphlen - 1;
  }
  uint32_t cluster = hbInfo[i].cluster;
  hb_buffer_destroy(buff);

  return aLength + cluster - aDirty;
}

bool
parseEmphasisTag(const std::string& aTag, SelectionList*& aSelection,
                 uint32_t aEnd) {
  // What I need here is regexp, sigh. wating for C++11.
  static const char header[] = u8R"(＃「)";
  static const char footer[] = u8R"(」に傍点)";

  if ((sizeof(header) + sizeof(footer)) >= aTag.size()) {
    return false;
  }

  if (0 != strncmp(aTag.c_str(), header, sizeof(header) - 1)) {
    return false;
  }

  if (0 != strncmp((aTag.c_str() + (aTag.size() - sizeof(footer) + 1)),
                   footer, sizeof(footer))) {
    return false;
  }

  aSelection = new SelectionList();
  aSelection->mNext = nullptr;

  aSelection->mRange.mEnd = aEnd; 
  aSelection->mRange.mStart =
    aEnd - (aTag.size() - (sizeof(header) + sizeof(footer) - 2));

  return true;
}

void
parseStrictAozora2(std::string& aString, std::string& aParentDocument,
                   TextPropertyList* aTP,
                   RubyList*& aRuby, SelectionList*& aEm) {

  //case 0x0000FF5C: // '｜';
  //case 0x0000300A: // '《'; [0xE3, 0x80, 0x8A, 0x00]
  //case 0x0000300B: // '》'; [0xE3, 0x80, 0x8B, 0x00]
  //case 0x0000FF3B: // '［'; [0xef, 0xbc, 0xbb, 0x00]
  //case 0x0000FF3D: // '］'; [0xef, 0xbc, 0xbd, 0x00]
  //case 0x0000FF03: // '＃';
  //case 0x0000FF01: // '！';
  //case 0x0000FF1F: // '？';
  //case 0x00002049: // '⁉';
  //case 0x0000203C: // '‼';

  TextPropertyList* tp = aTP;
  tp->mNext = nullptr;
  tp->mRange.mStart = aParentDocument.size();
  tp->mProgression = TEXT_PROPERTY_VERTICAL;

  hb_buffer_t* buff = hb_buffer_create();

  hb_buffer_set_unicode_funcs(buff, hb_icu_get_unicode_funcs());

  // instead of calling strlen add some meaningless codepoint at the end of
  // the array.
  aString += "\n";
  hb_buffer_add_utf8(buff, aString.c_str(), -1, 0, -1);
  aString.resize(aString.size() - 1);

  std::ofstream error("./error.txt");

  uint32_t glyphlen;
  hb_glyph_info_t* hbInfo = hb_buffer_get_glyph_infos(buff, &glyphlen);

  // We know the last "\n" is dummy;
  glyphlen--;

  RubyList* firstRuby = nullptr;
  RubyList* ruby = nullptr;
  SelectionList* firstEm = nullptr;
  SelectionList* em = nullptr;

  uint32_t notSelected(0);

  uint32_t rubyParent(0);
  bool isInRuby = false;
  bool isInTag = false;
  bool isInHTMLTag = false;
  std::string tag("");

  int32_t ligIndex = - 1;
  int32_t rotatedLength(0);

  uint32_t i;
  for (i = 0; i < glyphlen; i++) {
    switch (hbInfo[i].codepoint) {
    case 0x0000FF5C: // '｜';
      rubyParent = aParentDocument.size();
      continue;
      break;
    case 0x0000300A: // '《'; [0xE3, 0x80, 0x8A, 0x00]
      isInRuby = true;
      if (ruby) {
        ruby->mNext = new RubyList();
        ruby = ruby->mNext;
      }
      else {
        ruby = new RubyList();
        firstRuby = ruby;
      }
      ruby->mNext = nullptr;
      ruby->mRange.mEnd = aParentDocument.size();

      ruby->mRange.mStart = (rubyParent)?
        rubyParent : backtrackHan(aParentDocument.c_str(),
                                  aParentDocument.size(), notSelected);
      rubyParent = 0;
      notSelected = 0;
      continue;
      break;
    case 0x0000300B: // '》';
      isInRuby = false;
      continue;
      break;
    case 0x0000FF3B: // '［'; [0xef, 0xbc, 0xbb, 0x00]
      if (0x0000FF03 == hbInfo[i + 1].codepoint) {
        isInTag = true;
        continue;
      }
      break;
    case 0x0000FF3D: // '］'; [0xef, 0xbc, 0xbd, 0x00]
      if (isInTag) {
        isInTag = false;
        SelectionList* tmp = nullptr;
        parseEmphasisTag(tag, tmp, aParentDocument.size());
        if (tmp) {
#ifdef DEBUG
          std::cerr << aParentDocument.c_str() + tmp->mRange.mStart << "\n";
#endif
          if (!firstEm) {
            firstEm = tmp;
            em = tmp;
          }
          else {
            em->mNext = tmp;
            em = tmp;
          }
          assert(em->mNext == nullptr);
        }
        else {
          error << tag << std::endl;
          std::cerr << "Unknown Tag: " << tag << std::endl;
        }

        tag = "";
        continue;
      }
      break;
    case uint32_t('!'):  // '!'
        // XXX we should do TATECHUYOKO instead of ligature.
        if (uint32_t('!') == hbInfo[i + 1].codepoint) {
          ligIndex = 0;
        }
        else if (uint32_t('?') == hbInfo[i + 1].codepoint) {
          ligIndex = 1;
        }
      break;
    case uint32_t('<'): // kPBegin[0]
      isInHTMLTag = true;
      continue;
      break;
    case uint32_t('>'): // kPBegin[0]
      isInHTMLTag = false;
      continue;
      break;
    }

    uint32_t byteLen = hbInfo[i + 1].cluster - hbInfo[i].cluster;
    const char* ptr = aString.c_str() + hbInfo[i].cluster;

    if (ligIndex >= 0) {
      static const char lig0[] = u8R"(‼)";
      static const char lig1[] = u8R"(⁉)";
      ptr = (ligIndex == 0)? lig0 : lig1;
      byteLen = sizeof(lig0) - 1;
      ligIndex = -1;
      i++;
    }

    if (isInRuby) {
      ruby->mData.append(ptr, byteLen);
    }
    else if (isInTag) {
      tag.append(ptr, byteLen);
    }
    else if (isInHTMLTag) {
      // To do ... what?
    }
    else {
      utr50::property property = utr50::getProperty(hbInfo[i].codepoint);
      switch (property) {
      case utr50::R:
        if (0x0a != hbInfo[i].codepoint &&
            0x2026 != hbInfo[i].codepoint &&
            0x2015 != hbInfo[i].codepoint &&
            TEXT_PROPERTY_VERTICAL == tp->mProgression) {
          uint32_t pos = aParentDocument.size();
          int j = i - 1;
          while (j > -1 && utr50::Tr == utr50::getProperty(hbInfo[j].codepoint)) {
            uint32_t length = hbInfo[j + 1].cluster - hbInfo[j].cluster;
            if (pos >= length &&
                0 == strncmp(aString.c_str() + hbInfo[j].cluster,
                             aParentDocument.c_str() + pos - length, length)) {
              pos -= length;
            }
            else {
              break;
            }
            j--;
          }
          rotatedLength = j + 1 - i;
          tp->mRange.mEnd = pos;
          tp->mNext = new TextPropertyList();
          tp = tp->mNext;
          tp->mNext = nullptr;
          tp->mRange.mStart = pos;
          tp->mProgression = TEXT_PROPERTY_HORIZONTAL;
        }
        break;
      case utr50::Tu:
      case utr50::U:
        if (0x0a !=hbInfo[i].codepoint && TEXT_PROPERTY_HORIZONTAL == tp->mProgression) {
          if (rotatedLength < 2) {
            tp->mProgression = TEXT_PROPERTY_VERTICAL;
          }
          else if (rotatedLength == 2) {
            tp->mProgression = TEXT_PROPERTY_TATECHUYOKO;
          }
          tp->mRange.mEnd = aParentDocument.size();
          tp->mNext = new TextPropertyList();
          tp = tp->mNext;
          tp->mNext = nullptr;
          tp->mRange.mStart = aParentDocument.size();
          tp->mProgression = TEXT_PROPERTY_VERTICAL;
        }
        break;
      case utr50::Tr:
        break;
      }
      if (0x0a !=hbInfo[i].codepoint && TEXT_PROPERTY_HORIZONTAL == tp->mProgression) {
        rotatedLength++;
      }
      aParentDocument.append(ptr, byteLen);
      notSelected += byteLen;
    }
  }
#ifdef DEBUG
  fprintf(stderr, "first:%x length:%d\n", hbInfo[0].codepoint, glyphlen);
#endif
  hb_buffer_destroy(buff);
  aRuby = firstRuby;
  aEm = firstEm;
  tp->mRange.mEnd = aParentDocument.size();
}

class Font {
public:
  FT_Face mFTCAFont;
  FT_Face mFTHBFont;
  hb_font_t* mHBFont;
  cairo_font_face_t* mCAFont;
  double mSize;
  orient mOrient;
  hb_position_t mHOriginY;
  Font() {}
  Font(const char* aFontName, FT_Library aFTLib, const double aSize, orient aOrient = kVertical) :
    mSize(aSize), mOrient(aOrient), mHOriginY(0) {

    if (!aFontName || !(*aFontName)) {
      aFontName = "Serif";
    }

    int fontindex = 0;
    const char* fontpath = nullptr;
    FcPattern* fcFont = nullptr;
    {
      FcPattern* pattern = FcNameParse((const FcChar8*) aFontName);
      FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
      FcDefaultSubstitute(pattern);

      FcResult fcResult;
      if (kVertical == aOrient) {
        FcPatternAddBool(pattern, FC_VERTICAL_LAYOUT, FcTrue);
      }
      fcFont = FcFontMatch(nullptr, pattern, &fcResult);
      FcPatternDestroy(pattern);

      FcChar8* ufontpath;
      FcPatternGetString(fcFont, FC_FILE, 0, &ufontpath);
      fontpath = (const char*)(ufontpath);

      FcPatternGetInteger(fcFont, FC_INDEX, 0, &fontindex);
    }

    //  This seems extremely tricky, but don't mix up
    //  the Freetype object for cairo and that for harfbuzz.
    FT_Error fte = FT_New_Face(aFTLib, fontpath, fontindex, &mFTCAFont);

    if (fte) {
      fprintf(stderr, "FT_New_Face:0x%x, %s\n", fte, aFontName);
      exit(-1);
    }

    fte = FT_New_Face(aFTLib, fontpath, fontindex, &mFTHBFont);

    if (fte) {
      fprintf(stderr, "FT_New_Face:0x%x, %s\n", fte, aFontName);
      exit(-1);
    }

    FcPatternDestroy(fcFont);

    mHBFont = hb_ft_font_create(mFTHBFont, nullptr);
    mCAFont = cairo_ft_font_face_create_for_ft_face
               (mFTCAFont, (isVertical())? FT_LOAD_VERTICAL_LAYOUT : 0);
    if (!isVertical()) {
      resize();
      hb_codepoint_t codepointM(0);
      hb_font_get_glyph (mHBFont, hb_codepoint_t('M'), 0, &codepointM);
      hb_position_t x;
      hb_font_get_glyph_h_origin(mHBFont, codepointM, &x, &mHOriginY);
      std::cerr <<"mHOriginY" << codepointM;
    }
  }

  void resize() {
    // I'm not too sure why we have to set Freetype's font size repeatedly,
    // but that seems what Harfbuzz requests.
    FT_Error fte = FT_Set_Char_Size(mFTHBFont, mSize, 0,
                                    FT_UInt(72), FT_UInt(72));

    if (fte) {
      fprintf(stderr, "FT_Set_Char_Size:0x%x\n", fte);
      exit(-1);
    }
  }
  std::vector<uint32_t> mForbiddenFirstGlyphs;
  std::vector<uint32_t> mForbiddenLastGlyphs;

  static void setVector(const char* aForbidden, uint32_t aLength,
                        std::vector<uint32_t>& aVector, hb_font_t* aHBFont) {
    hb_buffer_t* buff = hb_buffer_create();

    hb_buffer_set_unicode_funcs(buff, hb_icu_get_unicode_funcs());
    hb_buffer_set_direction(buff, HB_DIRECTION_TTB);
    hb_buffer_set_script(buff, HB_SCRIPT_KATAKANA);
    hb_buffer_set_language(buff, hb_language_from_string("ja", -1));
    hb_buffer_add_utf8(buff, aForbidden, aLength, 0, -1);
    hb_buffer_guess_segment_properties(buff);
    hb_shape(aHBFont, buff, nullptr, 0);
    uint32_t glyphLength(0);
    hb_glyph_info_t* hbInfo = hb_buffer_get_glyph_infos(buff, &glyphLength);
    aVector.resize(glyphLength);

    uint32_t i;
    for (i = 0; i < glyphLength; i++) {
      aVector[i] = hbInfo[i].codepoint;
    }
    hb_buffer_destroy(buff);
  }

  bool isForbiddenFirstGlyph(uint32_t aCodepoint) {
    if (0 == mForbiddenFirstGlyphs.size()) {
      // note that this list will be very long, in the end.
      static const char forbidden[] = u8R"(。、」』)）)";
      setVector(forbidden, sizeof(forbidden) - 1,
                mForbiddenFirstGlyphs, mHBFont);
      std::sort(mForbiddenFirstGlyphs.begin(), mForbiddenFirstGlyphs.end());
      resize();
    }
    return std::binary_search(mForbiddenFirstGlyphs.begin(),
                              mForbiddenFirstGlyphs.end(), aCodepoint);
  }

  bool isForbiddenLastGlyph(uint32_t aCodepoint) {
    if (0 == mForbiddenLastGlyphs.size()) {
      static const char forbidden[] = u8R"(「『(（)";
      setVector(forbidden, sizeof(forbidden) - 1,
                mForbiddenLastGlyphs, mHBFont);
      std::sort(mForbiddenLastGlyphs.begin(), mForbiddenLastGlyphs.end());
      resize();
    }
    return std::binary_search(mForbiddenLastGlyphs.begin(),
                              mForbiddenLastGlyphs.end(), aCodepoint);
  }

  bool isVertical () const {
    return kVertical == mOrient;
  }
  ~Font() {
    cairo_font_face_destroy(mCAFont);
    hb_font_destroy(mHBFont);
    FT_Done_Face(mFTCAFont);
    FT_Done_Face(mFTHBFont);
  }
};

uint32_t
printRuby(Font* aFont, cairo_t* aCa,
          const char* aString, const rect_t& aRect, const double aRatio = 0.) {
  const double fontsize = aFont->mSize;
  aFont->resize();

  hb_buffer_t* buff = hb_buffer_create();

  hb_buffer_set_unicode_funcs(buff, hb_icu_get_unicode_funcs());
  hb_buffer_set_direction(buff, HB_DIRECTION_TTB);
  hb_buffer_set_script(buff, HB_SCRIPT_KATAKANA);
  hb_buffer_set_language(buff, hb_language_from_string("ja", -1));

  // XXX Hmm. Here I don't need harfbuzz_buffer eats such large string,
  //     for this is at most only 1 line of the whole document.
  hb_buffer_add_utf8(buff, aString, -1, 0, -1);
  hb_buffer_guess_segment_properties(buff);
  hb_shape(aFont->mHBFont, buff, nullptr, 0);

  // Step 1. Estimate

  uint32_t wholeLength;
  hb_glyph_info_t* hbInfo = hb_buffer_get_glyph_infos(buff, &wholeLength);
  hb_glyph_position_t* hbPos = hb_buffer_get_glyph_positions(buff, &wholeLength);

  uint32_t length = (aRatio > 0. && wholeLength > 1)?
    uint32_t(wholeLength * (1.0 - aRatio)) : wholeLength;

  const int32_t maxAdvance = ::floor(aRect.height() * 64. / fontsize);

  int32_t totalAdvance(0);
  // XXX Make sure we don't handle too big data (UTF-8 stream).
  uint32_t numGlyphs;

  for (numGlyphs = 0; numGlyphs < length; numGlyphs++) {

    if (hbInfo[numGlyphs].codepoint == 0) {
      // XXX This is unexpected. Should we use another font,
      //     e.g. switching between HanaMin(花園明朝) 1 and 2?
      break;
    }

    totalAdvance += (hbPos[numGlyphs].y_advance * -1);
  }

  if (numGlyphs == 0) {
    hb_buffer_destroy(buff);
    return 0;
  }

  uint32_t dataLength(0);
  if (wholeLength == numGlyphs) {
    dataLength = ::strlen(aString);
  }
  else {
    dataLength = hbInfo[numGlyphs].cluster;
  }

  // Step 2. Draw
  const uint32_t kGlyphLength = 5;
  cairo_glyph_t glyphbuffer[kGlyphLength];

  point_t origin, previousOrigin;
  getVerticalOriginFromLineRect(aRect, fontsize, origin);

  double pad = 0.;
  if (maxAdvance < totalAdvance) {
    // centering for too long string.
    origin.mY += ((maxAdvance - totalAdvance) * fontsize) / (64. * 2.);

    if (origin.mY < 0.) {
      // This implies ruby is to be printed somewhere out of the paper. 
      origin.mY = 0.;
    }
  }
  else {
    // padding for too short string.
    pad = ((maxAdvance - totalAdvance) * fontsize) / (64. * (numGlyphs * 2));
    origin.mY += pad;
    pad *= 2.;
  }

  previousOrigin = origin;

  cairo_set_source_rgb(aCa, 0., 0., 0.);
  AZ_DUMP_CAIRO(aCa, "cairo_set_source_rgb");

  uint32_t written(0);

  while (numGlyphs > 0) {
    uint32_t i;
    uint32_t tempNumGlyphs = (kGlyphLength < numGlyphs)?
      kGlyphLength : numGlyphs;

    for (i = 0; i < tempNumGlyphs; i++) {
      uint32_t index = written + i;

      glyphbuffer[i].index = hbInfo[index].codepoint;
      glyphbuffer[i].x = origin.mX;
      glyphbuffer[i].y = origin.mY;

      origin.mX += (hbPos[index].x_advance * fontsize) / 64.;
      origin.mY -= (hbPos[index].y_advance * fontsize) / 64.;

      origin.mY += pad;

    }

    cairo_set_font_face(aCa, aFont->mCAFont);
    AZ_DUMP_CAIRO(aCa, "cairo_set_font_face");

    cairo_set_font_size(aCa, aFont->mSize);
    AZ_DUMP_CAIRO(aCa, "cairo_set_font_size");

    cairo_show_glyphs(aCa, glyphbuffer, tempNumGlyphs);
    AZ_DUMP_CAIRO(aCa, "cairo_show_glyphs");

    numGlyphs -= tempNumGlyphs;
    written += tempNumGlyphs;

  }

  hb_buffer_destroy(buff);

  return dataLength;
}


lineState
printLine(Font* aFont, cairo_t* aCa,
          const std::string& aString,
          hb_glyph_info_t* aHBInfo, hb_glyph_position_t* aHBPos,
          uint32_t aGlyphLength, uint32_t& aWritten, uint32_t aDocumentOffset,
          const rect_t& aRect,
          point_t& aDelta,
          RubyList*& aRuby, Font* aRubyFont, SelectionList*& aEm) {
  aGlyphLength -= aWritten;

  const double fontsize = aFont->mSize;

  const uint32_t& dataOffset = aHBInfo[aWritten].cluster;
  const char* document = aString.c_str();

  RubyList* ruby = aRuby;
  SelectionList* em = aEm;
  // [72 dot per inch] = [1 dot per point]


  const int32_t maxAdvance = ::floor(aRect.height() * 64. / fontsize);

  int32_t totalAdvance(0);
  // XXX Make sure we don't handle too big data (UTF-8 stream).
  uint32_t numGlyphs;
  lineState state = LINE_STATE_SOFT_LINEBREAK;

  for (numGlyphs = 0; numGlyphs < aGlyphLength; numGlyphs++) {
    if (aHBInfo[aWritten + numGlyphs].codepoint == 0) {
      state = LINE_STATE_HARD_LINEBREAK;
      break;
    }
    const hb_glyph_position_t& pos = aHBPos[aWritten + numGlyphs];
    int32_t tmpAdvance = (aFont->isVertical())? (pos.y_advance * - 1) : pos.x_advance;
    int32_t tmp = totalAdvance + tmpAdvance;
    
    if (maxAdvance < tmp) {
      break;
    }
    else {
      totalAdvance = tmp;
    }
  }

  // Process Kinsoku (禁則)
  if (LINE_STATE_SOFT_LINEBREAK == state && numGlyphs > 1) {
    // This line's last Glyph.
    const uint32_t& lastGlyph = aHBInfo[aWritten + numGlyphs - 1].codepoint;
    // The next line's first Glyph.
    const uint32_t& firstGlph = aHBInfo[aWritten + numGlyphs].codepoint;
    if (aFont->isForbiddenLastGlyph(lastGlyph)) {
      numGlyphs--;
    }
    else if (((aWritten + numGlyphs) < aGlyphLength) &&
             aFont->isForbiddenFirstGlyph(firstGlph)) {
      numGlyphs++;
    }
  }

  if (numGlyphs == 0) {
    if (aGlyphLength == 0) {
      state = LINE_STATE_END_OF_STRING;
    }
    else if (state == LINE_STATE_HARD_LINEBREAK) {
      aWritten++;
    }

    return state;
  }

  uint32_t dataLength(0);
  if (state == LINE_STATE_HARD_LINEBREAK) {
    dataLength = aHBInfo[aWritten + numGlyphs + 1].cluster - dataOffset;
  }
  else {
    dataLength = aHBInfo[aWritten + numGlyphs].cluster - dataOffset;
  }

  // Step 2. Draw
#ifdef DEBUG
  std::cerr << "data: " << dataLength  << " bytes" << std::endl;

  cairo_set_source_rgb(aCa, 1., 1., 1.);
  cairo_rectangle(aCa, aRect.mStart.mX, aRect.mStart.mY, 
                      aRect.width(), aRect.height());
  cairo_fill(aCa);

  cairo_set_source_rgb(aCa, 0.3, 0.3, 0.3);
  cairo_rectangle(aCa, aRect.mStart.mX, aRect.mStart.mY,
                      aRect.width(), aRect.height());
  cairo_stroke(aCa);
#endif

#ifdef DEBUG
  std::cerr << "num: " << numGlyphs << std::endl;
  std::cerr << "TotalAdvance: " << totalAdvance << std::endl;
  std::cerr << "maxAdvance: " << maxAdvance << std::endl;
#endif

  const uint32_t kGlyphLength = 5;
  cairo_glyph_t glyphbuffer[kGlyphLength];
  cairo_text_cluster_t clusterbuffer[kGlyphLength];
  
  point_t origin, previousOrigin;
  getVerticalOriginFromLineRect(aRect, fontsize, origin);
  previousOrigin = origin;

  cairo_set_source_rgb(aCa, 0., 0., 0.);
  AZ_DUMP_CAIRO(aCa, "cairo_set_source_rgb");

  uint32_t written(0);
  bool isInRuby = false;
  rect_t rubyRect;
  const uint32_t tmpDataOffset = dataOffset + dataLength;
  const char* clusterStr = document + dataOffset;

  while (numGlyphs > 0) {
    uint32_t i;
    uint32_t tempNumGlyphs = (kGlyphLength < numGlyphs)?
      kGlyphLength : numGlyphs;

    uint32_t clusterTotalLength = 0;
    for (i = 0; i < tempNumGlyphs; i++) {
      uint32_t index = written + i;
      glyphbuffer[i].index = aHBInfo[aWritten + index].codepoint;
      glyphbuffer[i].x = origin.mX;
      glyphbuffer[i].y = origin.mY;

      uint32_t clusterLength =
        aHBInfo[aWritten + index + 1].cluster - 
        aHBInfo[aWritten + index].cluster;

#if DEBUG
      std::string buff("");
      std::cerr << "clusterTotalLength " << aWritten;
      std::cerr << "aWritten " << aWritten;
      std::cerr << "cluster: " <<  aHBInfo[aWritten + index].cluster;

      buff.append(clusterStr + clusterTotalLength,clusterLength);

      std::cerr << " clusterLength: " << clusterLength << " " << buff;

      buff = "";
      buff.append(clusterStr, 12);

      std::cerr << "c:" << buff;

      buff = "";
      buff.append(document + aHBInfo[aWritten + index].cluster, clusterLength);

      std::cerr << "r:" << buff <<"\n";
#endif
      clusterbuffer[i].num_bytes = clusterLength;
      clusterbuffer[i].num_glyphs = 1;
      clusterTotalLength += clusterLength;
      // Set ruby
      // XXX Reduce "if" statements.
      if (ruby && (ruby->mRange.mStart - aDocumentOffset < tmpDataOffset)) {
        uint32_t cluster = aHBInfo[aWritten + index].cluster;
        if (isInRuby) {
          if (ruby->mRange.mEnd - aDocumentOffset <= cluster) {
            rubyRect.mEnd.mX = aRect.mEnd.mX + aRubyFont->mSize;
            rubyRect.mEnd.mY = origin.mY;
            isInRuby = false;

#ifdef DEBUG
          {
            char buff[1024];
            memcpy(buff, document + ruby->mRange.mStart - aDocumentOffset,
                   ruby->mRange.length());
            buff[ruby->mRange.length()] = char(0);
            std::cerr << ruby->mRange.mEnd - aDocumentOffset << " ";
            std::cerr << cluster << " " <<
                         buff << " " << ruby->mData << "\n";
          }
#endif
            printRuby(aRubyFont, aCa, ruby->mData.c_str(), rubyRect);
            ruby = ruby->mNext;

          }
        }
      }

      // Set em
      if (ruby && (ruby->mRange.mStart - aDocumentOffset < tmpDataOffset)) {
        uint32_t cluster = aHBInfo[aWritten + index].cluster;
        if (!isInRuby) {
          if (ruby->mRange.mStart - aDocumentOffset <= cluster) {
            rubyRect.mStart = point_t(aRect.mEnd.mX, origin.mY);
            isInRuby = true;
          }
        }
      }

      if (em && (em->mRange.mStart - aDocumentOffset < tmpDataOffset)) {
        uint32_t cluster = aHBInfo[aWritten + index].cluster;
        while (em && em->mRange.mEnd - aDocumentOffset <= cluster) {
          em = em->mNext;
#if 0
          if (em) {
            char buff[1024];
            memcpy(buff, document + em->mRange.mStart - aDocumentOffset,
                   em->mRange.length());
            buff[em->mRange.length()] = char(0);
            std::cerr << em->mRange.mEnd - aDocumentOffset << "\n";
            std::cerr << cluster << "\n";
            std::cerr << buff << "\n";
          }
#endif
        }
      }
      point_t advance;
      advance.mX = (aHBPos[index + aWritten].x_advance * fontsize) / 64.;
      advance.mY = -1. * (aHBPos[index + aWritten].y_advance * fontsize) / 64.;
      if (!aFont->isVertical()) {
        advance = point_t(advance.mY, advance.mX);
      }

      if (em && (em->mRange.mStart - aDocumentOffset < tmpDataOffset)) {
        uint32_t cluster = aHBInfo[aWritten + index].cluster;
        if ((em->mRange.mStart - aDocumentOffset) <= cluster &&
            cluster < (em->mRange.mEnd - aDocumentOffset)) {
          rect_t emRect(point_t(aRect.mEnd.mX, origin.mY),
                        aRubyFont->mSize, advance.mY);
          printRuby(aRubyFont, aCa, u8R"(丶)", emRect);
        }
      }

      origin += advance;
#ifdef DEBUG
      // Note that codepoint is 4bytes (i.e. UCS4) while fonts support
      // only 2-bytes index (0-65535).
      fprintf(stderr, "codepoint: 0x%08lx x: %f, y: %f\n", 
              glyphbuffer[i].index, glyphbuffer[i].x, glyphbuffer[i].y);
#endif
    }

    cairo_set_font_face(aCa, aFont->mCAFont);
    AZ_DUMP_CAIRO(aCa, "cairo_set_font_face");

    cairo_set_font_size(aCa, aFont->mSize);
    AZ_DUMP_CAIRO(aCa, "cairo_set_font_size");


    if (!aFont->isVertical()) {
      cairo_matrix_t mtx;
      cairo_get_font_matrix(aCa, &mtx);
      cairo_font_extents_t fe;
      cairo_font_extents(aCa, &fe);
      double originDelta = (fe.ascent * aFont->mSize)/ (fe.ascent + fe.descent);
      cairo_matrix_t rtm ({0., mtx.xx, mtx.xx * -1., 0., (aFont->mSize / 2.) - originDelta, 0.});

      cairo_set_font_matrix(aCa, &rtm);
      AZ_DUMP_CAIRO(aCa, "cairo_set_font_size");
    }

    cairo_show_text_glyphs(aCa, clusterStr, clusterTotalLength,
                           glyphbuffer, tempNumGlyphs,
                           clusterbuffer, tempNumGlyphs,
                           cairo_text_cluster_flags_t(0));
    AZ_DUMP_CAIRO(aCa, "cairo_show_text_glyphs");

    numGlyphs -= tempNumGlyphs;
    written += tempNumGlyphs;

    if (numGlyphs) {
      clusterStr = document + aHBInfo[aWritten + written].cluster;
    }
#ifdef DEBUG
    std::cerr << "numGlyphs: " << numGlyphs << std::endl;
    std::cerr << "written: " << written << std::endl;
#endif
  }


  if (isInRuby) {
    bool dev = (tmpDataOffset != ruby->mRange.mEnd - aDocumentOffset);
    double ratio = 0.;
    uint32_t length = ruby->mRange.length();
    if (dev && length) {
      uint32_t left = ruby->mRange.mEnd - aDocumentOffset - tmpDataOffset;
      ratio = double(left) / double(length);
    }

    // This is kinda headaching case: This ruby starts within this line
    // and ends in the next line.
    // What we can do here is cut it into 2 parts.
    rubyRect.mEnd = point_t(aRect.mEnd.mX + aRubyFont->mSize, origin.mY);

    uint32_t rubyDataLength = 
      printRuby(aRubyFont, aCa, ruby->mData.c_str(), rubyRect, ratio);
    if (dev) {
      std::string replace = (ruby->mData.c_str() + rubyDataLength);
      ruby->mData = replace;
    }
    else {
      ruby = ruby->mNext;
    }
  }

  aWritten += written;
  if (state == LINE_STATE_HARD_LINEBREAK) {
    aWritten++; // We haven't written line break yet.
  }
  aDelta = origin - previousOrigin;
  aRuby = ruby;
  aEm = em;

  return state;
}


void
insertVerticalLineBreak(const rect_t& aRect,
                        const double aFontSize, const double aLineGap,
                        point_t& aOffset) {
  aOffset.mY = 0;
  aOffset.mX -= (aLineGap + aFontSize);
}

cairo_status_t caStdout(void* aClosure,
                        const unsigned char aData[],
                        unsigned int aLength){
  for (unsigned int i = 0; i < aLength; i++) {
    std::cout << aData[i];
  }
  return CAIRO_STATUS_SUCCESS;
}

class SVGFileNameProvider {
  std::string mDirPath;
  std::string mLatestPath;
  std::string mFilesList;
  uint32_t mIndex;
public:
  SVGFileNameProvider(const char* aDirPath): mFilesList(""), mIndex(0) {
    if (aDirPath) {
      char fileNameBuffer[FILENAME_MAX];
      realpath(aDirPath, fileNameBuffer);
      mDirPath = fileNameBuffer;
    }
  }
  const char* get() {
    char fileNameBuffer[sizeof("/000000.svg") + 1];
    sprintf(fileNameBuffer, "/%06d.svg", mIndex);
    fileNameBuffer[sizeof("/000000.svg")] = char(0);
    mLatestPath = mDirPath;
    mLatestPath += fileNameBuffer;
    mIndex++;

    if (0 != mFilesList.size()) {
      mFilesList.append(",", 1);
    }

    mFilesList.append("\"", 1);
    mFilesList.append(fileNameBuffer, sizeof("/000000.svg") -1);
    mFilesList.append("\"", 1);

    return mLatestPath.c_str();
  }

  void outputJSON()  {
    mLatestPath = mDirPath;
    mLatestPath += "/info.json";

    std::ofstream ofs(mLatestPath);
    ofs << "{\"fileLeafs\":[" << mFilesList << "]}";
    ofs.close();
  }
};

void printParagraph(std::string& parentDocument, Font* aFont, Font* aRubyFont,
                    SVGFileNameProvider* aSVGFile, KihonHanmen& aKihonHanmen,
                    cairo_surface_t*& cs, cairo_t*& ca, const rect_t& aPageRect,
                    const double aLineGap, RubyList*& aRuby, SelectionList*& aEM,
                    point_t& aOffset, uint32_t aDocumentOffset, lineState aLineState) {
  aFont->resize();
  hb_buffer_t* buff = hb_buffer_create();

  hb_buffer_set_unicode_funcs(buff, hb_icu_get_unicode_funcs());
  hb_buffer_set_direction(buff, (kVertical == aFont->mOrient)? HB_DIRECTION_TTB :HB_DIRECTION_LTR);
  hb_buffer_set_language(buff, hb_language_from_string("en", -1));

  hb_buffer_add_utf8(buff, parentDocument.c_str(), -1, 0, -1);
  hb_buffer_guess_segment_properties(buff);
  hb_shape(aFont->mHBFont, buff, nullptr, 0);


  uint32_t glyphLength(0);
  uint32_t glyphWritten(0);
  hb_glyph_info_t* hbInfo = hb_buffer_get_glyph_infos(buff, &glyphLength);
  hb_glyph_position_t* hbPos =
    hb_buffer_get_glyph_positions(buff, &glyphLength);
  glyphLength--; // We don't want to render the last glyph.


  rect_t columnRect;
  bool isLastColumn = aKihonHanmen.currentColumn(columnRect);

  rect_t lineRect;
  lineState state;
  getVerticalLineRect(columnRect, aOffset, aFont->mSize, lineRect);
  state = (columnRect.mStart.mX <= lineRect.mStart.mX)?
             LINE_STATE_NEW_LINE : LINE_STATE_END_OF_COLUMN;

  int _loopcount(0);
  for (;;) {

#ifdef DEBUG
    std::cerr << "aOffset:";dumpPoint (aOffset);
    std::cerr << "mStart:";dumpPoint (lineRect.mStart);std::cerr  << "\n";
    std::cerr << "state: "<< state << std::endl;
#endif

    _loopcount++;
    assert(_loopcount < 80000000);

    switch(state) {
    case LINE_STATE_TOO_SHORT_LINE:  // This implies given rect is abnormal.
    case LINE_STATE_END_OF_STRING:
      goto BREAKLOOP; // break switch(){} and for(){}.
      break;

    case LINE_STATE_END_OF_COLUMN:
      if (isLastColumn) {
        if (!aSVGFile) {
          cairo_show_page(ca);
        }
        else {
          cairo_destroy(ca);
          cairo_surface_flush(cs);
          cairo_surface_destroy(cs);

          cs = cairo_svg_surface_create(aSVGFile->get(),
                                        aPageRect.width(),
                                        aPageRect.height());
          cairo_surface_set_fallback_resolution(cs, 72., 72.);
          ca = cairo_create(cs);
        }
      }
      isLastColumn = aKihonHanmen.newColumn(columnRect);
      aOffset = point_t(0, 0);
      getVerticalLineRect(columnRect, aOffset, aFont->mSize, lineRect);
      state = LINE_STATE_NEW_LINE;
      break;

    case LINE_STATE_SOFT_LINEBREAK:
      if (glyphLength == glyphWritten) {
        goto BREAKLOOP; // break switch(){} and for(){}.
      }
    case LINE_STATE_HARD_LINEBREAK:
      insertVerticalLineBreak(columnRect, aFont->mSize, aLineGap, aOffset);
      getVerticalLineRect(columnRect, aOffset, aFont->mSize, lineRect);
      state = (columnRect.mStart.mX <= lineRect.mStart.mX)?
                LINE_STATE_NEW_LINE : LINE_STATE_END_OF_COLUMN;
      break;

    case LINE_STATE_NEW_LINE:
      point_t delta(0., 0.);
      state = printLine(aFont, ca, parentDocument, hbInfo, hbPos, glyphLength,
                        glyphWritten, aDocumentOffset, lineRect, delta, aRuby,
                        aRubyFont, aEM);
      aOffset += delta;
#ifdef DEBUG
      std::cerr << "Left:" << std::endl 
                << (parentDocument.c_str() + hbInfo[glyphWritten].cluster)
                << std::endl;
      std::cerr << "delta:";dumpPoint (delta);std::cerr  << "\n";
#endif
      break;
    }
  }

  BREAKLOOP:

  hb_buffer_destroy(buff);
}

// @return false  If there's no room in this rectangle to draw a new glyph.
void printString(Font* aFont, Font* aHFont, const Page& aPage,
                 std::string& aString, KihonHanmen& aKihonHanmen,
                 const double aLineGap, Font* aRubyFont,
                 const char* aSVGPath) {
  point_t offset(0., 0.);

  RubyList* ruby = nullptr;
  SelectionList* em = nullptr;
  TextPropertyList* tp = new azlayout::TextPropertyList();
  std::string parentDocument = "";
  parseStrictAozora2(aString, parentDocument, tp, ruby, em);

#ifdef DEBUG
  {
    TextPropertyList* tp2 = tp;
    std::string buff;
    while (tp2) {
      if (tp2->mProgression == azlayout::TEXT_PROPERTY_HORIZONTAL) {
        buff = "";
        buff.append(parentDocument.c_str() + tp2->mRange.mStart,
                    tp2->mRange.length());
        std::cerr << buff;
      }
      tp2 = tp2->mNext;
    }
  }
#endif

  SVGFileNameProvider svgFile(aSVGPath);
  cairo_surface_t* cs;
  if (aSVGPath) {
    cs = cairo_svg_surface_create(svgFile.get(),
                                  aPage.outerRect().width(),
                                  aPage.outerRect().height());
  }
  else {
    cs = cairo_pdf_surface_create_for_stream(caStdout, nullptr,
                                             aPage.outerRect().width(),
                                             aPage.outerRect().height());
  }

  cairo_surface_set_fallback_resolution(cs, 72., 72.);

  cairo_t* ca = cairo_create(cs);

  {
    TextPropertyList* tp2 = tp;
    while (tp2->mNext) {
      if (tp2->mProgression == tp2->mNext->mProgression) {
        tp2->mRange.mEnd = tp2->mNext->mRange.mEnd;
        tp2->mNext = tp2->mNext->mNext;
      }
      else {
        tp2 = tp2->mNext;
      }
    }
  }


  while (tp) {
    std::string fragment = "";
    uint32_t documentOffset = tp->mRange.mStart;
    fragment.append(parentDocument.c_str() + documentOffset, tp->mRange.length());
    fragment += "a";
    printParagraph(fragment, (TEXT_PROPERTY_HORIZONTAL == tp->mProgression)? aHFont :aFont, aRubyFont,
                   (aSVGPath)? &svgFile: nullptr, aKihonHanmen,
                   cs, ca, aPage.outerRect(),
                   aLineGap, ruby, em,
                   offset, documentOffset, LINE_STATE_CONTINUE_LINE);
    tp = tp->mNext;
  }

  cairo_show_page(ca);
  cairo_destroy(ca);

  cairo_surface_flush(cs);
  cairo_surface_destroy(cs);

  if (aSVGPath) {
    svgFile.outputJSON();
  }
  return;
}
} // azlayout

int main (int argc, char* argv[]) {
  FT_Library ftlib;
  FT_Error fte = FT_Init_FreeType(&ftlib);

  if (fte) {
    fprintf(stderr, "FT_Init_FreeType:0x%x\n", fte);
    exit(-1);
  }

  double fontsize     = 16.;
  double rubysize     = 0.5;
  double width        = 0.;
  double height       = 0.;
  double size         = 5.;
  double ratio        = 9. / 16.;            // w:h = 9:16
  double margin       = 0.;
  double marginLeft   = 0.;
  double marginRight  = 0.;
  double marginTop    = 0.;
  double marginBottom = 0.;
  double lineGap      = 0.;
  int    columns      = 1;
  double columnGap    = 0.;
  const char* svgpath = nullptr;
  const char* fontface = nullptr;
  const char* rubyfontface = nullptr;

#define ARG_STRNCMP(_V_,_L_) (0==strncasecmp(_V_,"-"#_L_,sizeof(#_L_)+1))
#define ARG_PARSE_DOUBLE(_L_) \
  if (ARG_STRNCMP(argv[i],_L_)) {\
    _L_ = atof(argv[i + 1]); \
    i++;\
  }

#define ARG_PARSE_INT(_L_) \
  if (ARG_STRNCMP(argv[i],_L_)) {\
    _L_ = atoi(argv[i + 1]); \
    i++;\
  }

#define ARG_PARSE_STR(_L_) \
  if (ARG_STRNCMP(argv[i],_L_)) {\
    _L_ = argv[i + 1]; \
    i++;\
  }

  int32_t i;
  for (i = 0; i < argc; i++) {
    if ((i + 1) < argc) {
      ARG_PARSE_DOUBLE(fontsize)
      else
      ARG_PARSE_DOUBLE(rubysize)
      else
      ARG_PARSE_DOUBLE(height)
      else
      ARG_PARSE_DOUBLE(width)
      else
      ARG_PARSE_DOUBLE(size)
      else
      ARG_PARSE_DOUBLE(ratio)
      else
      ARG_PARSE_DOUBLE(margin)
      else
      ARG_PARSE_DOUBLE(marginTop)
      else
      ARG_PARSE_DOUBLE(marginBottom)
      else
      ARG_PARSE_DOUBLE(marginLeft)
      else
      ARG_PARSE_DOUBLE(marginRight)
      else
      ARG_PARSE_DOUBLE(columnGap)
      else
      ARG_PARSE_INT(columns)
      else
      ARG_PARSE_STR(svgpath)
      else
      ARG_PARSE_STR(fontface)
      else
      ARG_PARSE_STR(rubyfontface)
    }
  }

  if (height == 0.) {
    height = size * 72.;
  }

  if (width == 0.) {
    width = height * ratio;
  }

  if (margin != 0.) {
    if (marginTop == 0.) {
      marginTop = margin;
    }
    if (marginBottom == 0.) {
      marginBottom = margin;
    }
    if (marginLeft == 0.) {
      marginLeft = margin;
    }
    if (marginRight == 0.) {
      marginRight = margin;
    }
  }

  if (marginRight < (fontsize * rubysize)) {
    marginRight = (fontsize * rubysize);
  }

  if (marginBottom < (fontsize / 2.)) {
    marginBottom = (fontsize / 2.);
  }

  if (lineGap == 0.) {
    lineGap = fontsize;
  }

  if (columns > 1 && columnGap == 0.) {
    columnGap = lineGap;
  }

  if (!fontface) {
    fontface = "IPAexMincho";
  }

  if (!rubyfontface) {
    rubyfontface = fontface;
  }

  std::string rawUTF8Data;
  std::cin >> std::noskipws;
  std::getline(std::cin, rawUTF8Data, char(0));

  // XXX I'm not too sure what inserts this line feed. Shell?
  //     Cut it off anyway.
  if (rawUTF8Data.size() > 0 && '\n' == char(*(rawUTF8Data.end() - 1))) {
    rawUTF8Data.resize(rawUTF8Data.size() - 1);
  }

  azlayout::Page page(width, height,
                      marginLeft, marginTop, marginRight, marginBottom);
  azlayout::KihonHanmen kihonHanmen(page.innerRect(), columnGap, columns);
 
  {
    azlayout::Font vFont(fontface, ftlib, fontsize);
    azlayout::Font hFont(fontface, ftlib, fontsize, azlayout::kHorizontal);
    azlayout::Font rubyFont(rubyfontface, ftlib, (fontsize * rubysize));

    printString(&vFont, &hFont, page, rawUTF8Data,
                kihonHanmen, lineGap, &rubyFont, svgpath);
  }

  FT_Done_FreeType(ftlib);
  return 0;
}

