/* 組み込み関数の説明文テーブル (UI 非依存)。
 * GUI の補完ポップアップと TUI から共有される。
 * 移植元: Calctus/Model/Functions/BuiltIns/ 各ファイルの Description 文字列
 * ローカライズが必要になったときは gettext の _() でラップする */

#include "builtin_docs.h"
#include <string.h>

static const struct { const char *name; const char *desc; } DOCS[] = {
    // 指数・対数
    { "pow",      "`y` power of `x`" },
    { "sqrt",     "Square root of `x`" },
    { "exp",      "Exponential of `x`" },
    { "log",      "Logarithm of `x`" },
    { "log2",     "Binary logarithm of `x`" },
    { "log10",    "Common logarithm of `x`" },
    { "clog2",    "Ceiling of binary logarithm of `x`" },
    { "clog10",   "Ceiling of common logarithm of `x`" },
    // 三角関数
    { "sin",      "Sine" },
    { "cos",      "Cosine" },
    { "tan",      "Tangent" },
    { "asin",     "Arcsine" },
    { "acos",     "Arccosine" },
    { "atan",     "Arctangent" },
    { "atan2",    "Arctangent of a / b" },
    { "sinh",     "Hyperbolic sine" },
    { "cosh",     "Hyperbolic cosine" },
    { "tanh",     "Hyperbolic tangent" },
    // 丸め
    { "floor",    "Largest integral value less than or equal to `x`" },
    { "ceil",     "Smallest integral value greater than or equal to `x`" },
    { "trunc",    "Integral part of `x`" },
    { "round",    "Nearest integer to `x`" },
    // 絶対値・符号
    { "abs",      "Absolute value of `x`" },
    { "sign",     "Returns 1 for positives, -1 for negatives, 0 otherwise." },
    { "mag",      "Magnitude of vector `x`" },
    // 最大・最小
    { "max",      "Maximum value of elements of the `array`" },
    { "min",      "Minimum value of elements of the `array`" },
    // GCD / LCM
    { "gcd",      "Greatest common divisor of elements of the `array`." },
    { "lcm",      "Least common multiple of elements of the `array`." },
    // アサーション
    { "assert",   "Asserts that `x` is true (non-zero)." },
    { "all",      "Returns true if all elements of `array` are non-zero." },
    { "any",      "Returns true if any element of `array` is non-zero." },
    // 日時・フォーマット
    { "datetime", "Converts `x` to datetime representation." },
    { "now",      "Current epoch time" },
    { "fromDays",    "Converts from days to epoch time." },
    { "fromHours",   "Converts from hours to epoch time." },
    { "fromMinutes", "Converts from minutes to epoch time." },
    { "fromSeconds", "Converts from seconds to epoch time." },
    { "toDays",      "Converts from epoch time to days." },
    { "toHours",     "Converts from epoch time to hours." },
    { "toMinutes",   "Converts from epoch time to minutes." },
    { "toSeconds",   "Converts from epoch time to seconds." },
    // 表示形式
    { "bin",      "Converts `x` to binary representation." },
    { "dec",      "Converts `x` to decimal representation." },
    { "hex",      "Converts `x` to hexadecimal representation." },
    { "oct",      "Converts `x` to octal representation." },
    { "char",     "Converts `x` to character representation." },
    { "kibi",     "Converts `x` to binary prefixed representation." },
    { "si",       "Converts `x` to SI prefixed representation." },
    // 乱数
    { "rand",     "Generates a random value between 0.0 and 1.0." },
    { "rand32",   "Generates a 32bit random integer." },
    { "rand64",   "Generates a 64bit random integer." },
    // 配列操作
    { "len",           "Length of `array`." },
    { "range",         "Array from `start` to `stop` (exclusive)." },
    { "rangeInclusive","Array from `start` to `stop` (inclusive)." },
    { "concat",        "Concatenates two arrays." },
    { "reverseArray",  "Reverses the order of elements in `array`." },
    { "map",           "Applies `func` to each element of `array`." },
    { "filter",        "Filters `array` by `func` predicate." },
    { "count",         "Counts elements of `array` matching `func`." },
    { "sort",          "Sorts `array`, optionally by `func` comparator." },
    { "aggregate",     "Aggregates `array` by applying `func` cumulatively." },
    { "extend",        "Extends `array` by applying `func` `count` times." },
    { "indexOf",       "First index of `val` in `array`, or -1." },
    { "lastIndexOf",   "Last index of `val` in `array`, or -1." },
    { "contains",      "Returns true if `array` contains `val`." },
    { "except",        "Elements of `array0` not in `array1`." },
    { "intersect",     "Elements common to both arrays." },
    { "union",         "Union of two arrays (no duplicates)." },
    { "unique",        "Removes duplicate elements from `array`." },
    // 統計
    { "sum",      "Sum of elements of the `array`." },
    { "ave",      "Arithmetic mean of elements of the `array`." },
    { "geoMean",  "Geometric mean of elements of the `array`." },
    { "harMean",  "Harmonic mean of elements of the `array`." },
    { "invSum",   "Inverse of the sum of the inverses. Composite resistance of parallel resistors." },
    // 素数
    { "isPrime",  "Returns whether `x` is prime or not." },
    { "prime",    "`x`-th prime number." },
    { "primeFact","Returns prime factors of `x`." },
    // 方程式
    { "solve",    "Finds a root of `func` using Newton's method." },
    // 文字列
    { "str",          "Converts byte-array to string." },
    { "array",        "Converts string to byte-array." },
    { "trim",         "Removes whitespace from both ends of string." },
    { "trimStart",    "Removes leading whitespace from string." },
    { "trimEnd",      "Removes trailing whitespace from string." },
    { "replace",      "Replaces occurrences of `old` with `new` in string." },
    { "toLower",      "Converts string to lower case." },
    { "toUpper",      "Converts string to upper case." },
    { "startsWith",   "Returns true if string starts with `prefix`." },
    { "endsWith",     "Returns true if string ends with `suffix`." },
    { "split",        "Splits string by delimiter into an array." },
    { "join",         "Joins array elements into a string with separator." },
    // グレイコード
    { "toGray",   "Converts the value from binary to gray-code." },
    { "fromGray", "Converts the value from gray-code to binary." },
    // ビット・バイト操作
    { "count1",      "Number of bits of `x` that have the value 1." },
    { "pack",        "Packs array elements into an integer." },
    { "unpack",      "Separates the value of `x` into elements of `b` bit width." },
    { "reverseBits", "Reverses the lower `b` bits of `x`." },
    { "reverseBytes","Reverses the lower `b` bytes of `x`." },
    { "rotateL",     "Rotates the lower `b` bits of `x` to the left." },
    { "rotateR",     "Rotates the lower `b` bits of `x` to the right." },
    { "swap2",       "Swaps even and odd bytes of `x`." },
    { "swap4",       "Reverses the order of each 4 bytes of `x`." },
    { "swap8",       "Reverses the byte-order of `x`." },
    { "swapNib",     "Swaps the nibble of each byte of `x`." },
    // 色変換
    { "rgb",         "Generates 24 bit color value from R, G, B." },
    { "hsv2rgb",     "Converts from H, S, V to 24 bit RGB color value." },
    { "rgb2hsv",     "Converts the 24 bit RGB color value to HSV." },
    { "hsl2rgb",     "Converts from H, S, L to 24 bit color RGB value." },
    { "rgb2hsl",     "Converts the 24 bit RGB color value to HSL." },
    { "rgb2yuv",     "Converts 24bit RGB color to 24 bit YUV." },
    { "yuv2rgb",     "Converts the 24 bit YUV color to 24 bit RGB." },
    { "rgbTo565",    "Downconverts RGB888 color to RGB565." },
    { "rgbFrom565",  "Upconverts RGB565 color to RGB888." },
    { "pack565",     "Packs the 3 values to an RGB565 color." },
    { "unpack565",   "Unpacks the RGB565 color to 3 values." },
    // ECC / パリティ
    { "xorReduce",   "Reduction XOR of `x` (same as even parity)." },
    { "oddParity",   "Odd parity of `x`." },
    { "eccWidth",    "Width of ECC for `b`-bit data." },
    { "eccEnc",      "Generates ECC code (`b`: data width, `x`: data)." },
    { "eccDec",      "Decodes and corrects ECC (`b`: data width, `ecc`: ECC, `x`: data)." },
    // エンコーディング
    { "utf8Enc",        "Encode `str` to UTF8 byte sequence." },
    { "utf8Dec",        "Decode UTF8 byte sequence." },
    { "urlEnc",         "Escape URL string." },
    { "urlDec",         "Unescape URL string." },
    { "base64Enc",      "Encode string to Base64." },
    { "base64Dec",      "Decode Base64 to string." },
    { "base64EncBytes", "Encode byte-array to Base64." },
    { "base64DecBytes", "Decode Base64 to byte-array." },
    // E系列
    { "esFloor",  "Largest E-series value less than or equal to `x`." },
    { "esCeil",   "Smallest E-series value greater than or equal to `x`." },
    { "esRound",  "Nearest E-series value to `x`." },
    { "esRatio",  "E-series ratio for `x`." },
    // キャスト
    { "rat",      "Rational fraction approximation of `x`." },
    { "real",     "Converts `x` to a real number." },
};

const char *builtin_doc(const char *name) {
    size_t n = sizeof(DOCS) / sizeof(DOCS[0]);
    for (size_t i = 0; i < n; i++)
        if (strcmp(DOCS[i].name, name) == 0) return DOCS[i].desc;
    return NULL;
}
