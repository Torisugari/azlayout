#!/usr/bin/env python
# -*- coding: utf-8 -*-
#    Copyright (C) 2014 Torisugari <torisugari@gmail.com>
#
#     Permission is hereby granted, free of charge, to any person obtaining
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
#     The above copyright notice and this permission notice shall be in cluded
# in all copies or substantial portions of the Software.
#
#    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

from string import maketrans
import sys

tmp = sys.stdin.read();

dic = {
  "※［＃「馬＋矣」、第3水準1-94-13］": "騃",
  "※［＃「飲のへん＋氣」、第4水準2-92-67］": "餼",
  "※［＃「口＋據のつくり」、第3水準1-15-24］": "噱",
  "※［＃「丞／犯のつくり」、第4水準2-3-54］": "卺",
  "※［＃「虫＋車」、第3水準1-91-55］": "蛼",
  "※［＃「さんずい＋玄」、第3水準1-86-62］": "泫",
  "※［＃「王＋膠のつくり」、第3水準1-88-22］": "璆",
  "※［＃「彑／（「比」の間に「矢」）」、第3水準1-84-28］": "彘",
  "※［＃「りっしんべん＋（旬／子）」、第3水準1-84-55］": "惸",
  "※［＃「勹＜夕」、第3水準1-14-76］": "匇",
  "※［＃「爿＋戈」、第4水準2-12-83］": "戕",
  "※［＃「焉＋おおざと」、第3水準1-92-78］": "鄢",
  "※［＃「女＋咼」、第3水準1-15-89］": "媧",
  "※［＃「りっしんべん＋兄」、第3水準1-84-45］": "怳",
  "※［＃「りっしんべん＋淌のつくり」、第3水準1-84-54］": "惝",
  "※［＃「目＋爭」、第3水準1-88-85］": "睜",
  "※［＃「木＋厥」、第3水準1-86-15］": "橛",
  "※［＃小書き片仮名ヒ、1-6-84］": "ㇶ",
  "※［＃「火＋（麈－鹿）」、第3水準1-87-40］": "炷",
  "※［＃「飲のへん＋善」、第4水準2-92-71］": "饍󠄂", # Note: IVD U+994d U+E0102
  "※［＃「足へん＋厨」、第3水準1-92-39］": "蹰",
  "※［＃「飲のへん＋稻のつくり」、第4水準2-92-68］": "饀",
  "※［＃「言＋墟のつくり」、第4水準2-88-74］": "譃",
  "※［＃「言＋虚」、第4水準2-88-74］": "譃",
  "※［＃「陷のつくり＋炎」、第3水準1-87-64］": "燄"
}

def multipleReplace(aText, aDicttionary):
    for key in aDicttionary:
        aText = aText.replace(key, aDicttionary[key])
    return aText

print multipleReplace(tmp, dic)
