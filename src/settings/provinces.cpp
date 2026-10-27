#include "provinces.h"

#include <stdlib.h>
#include <string.h>

namespace provinces {

namespace {

void fold(const char* in, char* out, size_t cap) {
  size_t k = 0;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(in);
  while (*p && k + 1 < cap) {
    unsigned char c = *p;
    char o = 0;
    size_t adv = 1;
    if (c == 0xC3 && p[1]) {
      unsigned char d = p[1];
      adv = 2;
      if (d == 0x87 || d == 0xA7) o = 'c';
      else if (d == 0x96 || d == 0xB6) o = 'o';
      else if (d == 0x9C || d == 0xBC) o = 'u';
      else if (d == 0x82 || d == 0xA2) o = 'a';
      else if (d == 0x8E || d == 0xAE) o = 'i';
    } else if (c == 0xC4 && p[1]) {
      unsigned char d = p[1];
      adv = 2;
      if (d == 0x9E || d == 0x9F) o = 'g';
      else if (d == 0xB0 || d == 0xB1) o = 'i';
    } else if (c == 0xC5 && p[1]) {
      unsigned char d = p[1];
      adv = 2;
      if (d == 0x9E || d == 0x9F) o = 's';
    } else if (c >= 'A' && c <= 'Z') {
      o = static_cast<char>(c - 'A' + 'a');
    } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      o = static_cast<char>(c);
    }
    if (o) out[k++] = o;
    p += adv;
  }
  out[k] = 0;
}

const Province kProvinces[COUNT] = {
    {1, "Adana", 37.000f, 35.321f},          {2, "Adıyaman", 37.764f, 38.276f},     {3, "Afyonkarahisar", 38.757f, 30.538f},
    {4, "Ağrı", 39.719f, 43.051f},           {5, "Amasya", 40.653f, 35.833f},       {6, "Ankara", 39.925f, 32.837f},
    {7, "Antalya", 36.897f, 30.713f},        {8, "Artvin", 41.183f, 41.818f},       {9, "Aydın", 37.845f, 27.839f},
    {10, "Balıkesir", 39.648f, 27.883f},     {11, "Bilecik", 40.142f, 29.979f},     {12, "Bingöl", 38.885f, 40.498f},
    {13, "Bitlis", 38.401f, 42.108f},        {14, "Bolu", 40.735f, 31.608f},        {15, "Burdur", 37.720f, 30.290f},
    {16, "Bursa", 40.183f, 29.067f},         {17, "Çanakkale", 40.155f, 26.414f},   {18, "Çankırı", 40.601f, 33.614f},
    {19, "Çorum", 40.549f, 34.953f},         {20, "Denizli", 37.776f, 29.086f},     {21, "Diyarbakır", 37.914f, 40.231f},
    {22, "Edirne", 41.677f, 26.555f},        {23, "Elazığ", 38.681f, 39.223f},      {24, "Erzincan", 39.750f, 39.492f},
    {25, "Erzurum", 39.904f, 41.267f},       {26, "Eskişehir", 39.776f, 30.520f},   {27, "Gaziantep", 37.066f, 37.383f},
    {28, "Giresun", 40.913f, 38.390f},       {29, "Gümüşhane", 40.460f, 39.481f},   {30, "Hakkari", 37.574f, 43.741f},
    {31, "Hatay", 36.202f, 36.160f},         {32, "Isparta", 37.764f, 30.556f},     {33, "Mersin", 36.812f, 34.641f},
    {34, "İstanbul", 41.008f, 28.978f},      {35, "İzmir", 38.419f, 27.129f},       {36, "Kars", 40.608f, 43.097f},
    {37, "Kastamonu", 41.389f, 33.783f},     {38, "Kayseri", 38.720f, 35.483f},     {39, "Kırklareli", 41.735f, 27.225f},
    {40, "Kırşehir", 39.146f, 34.161f},      {41, "Kocaeli", 40.766f, 29.917f},     {42, "Konya", 37.871f, 32.485f},
    {43, "Kütahya", 39.420f, 29.983f},       {44, "Malatya", 38.355f, 38.309f},     {45, "Manisa", 38.612f, 27.426f},
    {46, "Kahramanmaraş", 37.576f, 36.937f}, {47, "Mardin", 37.313f, 40.735f},      {48, "Muğla", 37.215f, 28.364f},
    {49, "Muş", 38.746f, 41.506f},           {50, "Nevşehir", 38.625f, 34.712f},    {51, "Niğde", 37.970f, 34.679f},
    {52, "Ordu", 40.984f, 37.879f},          {53, "Rize", 41.025f, 40.517f},        {54, "Sakarya", 40.774f, 30.394f},
    {55, "Samsun", 41.287f, 36.330f},        {56, "Siirt", 37.933f, 41.941f},       {57, "Sinop", 42.027f, 35.151f},
    {58, "Sivas", 39.748f, 37.017f},         {59, "Tekirdağ", 40.978f, 27.511f},    {60, "Tokat", 40.314f, 36.554f},
    {61, "Trabzon", 41.003f, 39.717f},       {62, "Tunceli", 39.107f, 39.548f},     {63, "Şanlıurfa", 37.159f, 38.797f},
    {64, "Uşak", 38.682f, 29.408f},          {65, "Van", 38.494f, 43.380f},         {66, "Yozgat", 39.820f, 34.808f},
    {67, "Zonguldak", 41.456f, 31.799f},     {68, "Aksaray", 38.368f, 34.037f},     {69, "Bayburt", 40.255f, 40.225f},
    {70, "Karaman", 37.181f, 33.215f},       {71, "Kırıkkale", 39.847f, 33.515f},   {72, "Batman", 37.887f, 41.132f},
    {73, "Şırnak", 37.518f, 42.454f},        {74, "Bartın", 41.636f, 32.337f},      {75, "Ardahan", 41.110f, 42.702f},
    {76, "Iğdır", 39.920f, 44.045f},         {77, "Yalova", 40.655f, 29.276f},      {78, "Karabük", 41.204f, 32.627f},
    {79, "Kilis", 36.717f, 37.121f},         {80, "Osmaniye", 37.074f, 36.247f},    {81, "Düzce", 40.843f, 31.156f},
};

}

const Province& at(int index) { return kProvinces[(index < 0 || index >= COUNT) ? 58 : index]; }

const Province* byPlate(int plate) {
  if (plate < 1 || plate > COUNT) return nullptr;
  return &kProvinces[plate - 1];
}

int indexOfPlate(int plate) { return (plate >= 1 && plate <= COUNT) ? plate - 1 : -1; }

int findByName(const char* name) {
  if (!name) return -1;
  for (int i = 0; i < COUNT; i++) {
    if (strcmp(kProvinces[i].name, name) == 0) return i;
  }
  char want[40];
  fold(name, want, sizeof(want));
  if (!want[0]) return -1;
  bool digits = true;
  for (const char* p = want; *p; p++) digits = digits && (*p >= '0' && *p <= '9');
  if (digits) return indexOfPlate(atoi(want));
  char have[40];
  for (int i = 0; i < COUNT; i++) {
    fold(kProvinces[i].name, have, sizeof(have));
    if (strcmp(have, want) == 0) return i;
  }
  return -1;
}

}
