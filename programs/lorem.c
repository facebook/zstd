/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


/* Implementation notes:
 *
 * This is a very simple lorem ipsum generator
 * which features a static list of words
 * and print them one after another randomly
 * with a fake sentence / paragraph structure.
 *
 * The goal is to generate a printable text
 * that can be used to fake a text compression scenario.
 * The resulting compression / ratio curve of the lorem ipsum generator
 * is more satisfying than the previous statistical generator,
 * which was initially designed for entropy compression,
 * and lacks a regularity more representative of text.
 *
 * The compression ratio achievable on the generated lorem ipsum
 * is still a bit too good, presumably because the dictionary is too small.
 * It would be possible to create some more complex scheme,
 * notably by enlarging the dictionary with a word generator,
 * and adding grammatical rules (composition) and syntax rules.
 * But that's probably overkill for the intended goal.
 */

#include "lorem.h"
#include <string.h>  /* memcpy */
#include <limits.h>  /* INT_MAX */
#include <assert.h>

#define WORD_MAX_SIZE 20

/* Define the word pool */
static const char *words[] = {
    "lorem",       "ipsum",      "dolor",      "sit",          "amet",
    "consectetur", "adipiscing", "elit",       "sed",          "do",
    "eiusmod",     "tempor",     "incididunt", "ut",           "labore",
    "et",          "dolore",     "magna",      "aliqua",       "dis",
    "lectus",      "vestibulum", "mattis",     "ullamcorper",  "velit",
    "commodo",     "a",          "lacus",      "arcu",         "magnis",
    "parturient",  "montes",     "nascetur",   "ridiculus",    "mus",
    "mauris",      "nulla",      "malesuada",  "pellentesque", "eget",
    "gravida",     "in",         "dictum",     "non",          "erat",
    "nam",         "voluptat",   "maecenas",   "blandit",      "aliquam",
    "etiam",       "enim",       "lobortis",   "scelerisque",  "fermentum",
    "dui",         "faucibus",   "ornare",     "at",           "elementum",
    "eu",          "facilisis",  "odio",       "morbi",        "quis",
    "eros",        "donec",      "ac",         "orci",         "purus",
    "turpis",      "cursus",     "leo",        "vel",          "porta"};

/* simple distribution that favors small words :
 * 1 letter : weight 3
 * 2-3 letters : weight 2
 * 4+ letters : weight 1
 * This is expected to be a bit more difficult to compress */
static const int distrib[] = {
    0, 1, 2, 3, 3, 4, 5, 6, 7, 8,
    8,9, 9, 10, 11, 12, 13, 13, 14, 15,
    15, 16, 17, 18, 19, 19, 20, 21, 22, 23,
    24, 25, 26, 26, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 34, 35, 36, 37, 38, 39, 40,
    41, 41, 42, 43, 43, 44, 45, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 55, 56,
    57, 58, 58, 59, 60, 60, 61, 62, 63, 64,
    65, 66, 67, 67, 68, 69, 70, 71, 72, 72,
    73, 73, 74 };
static const unsigned distribCount = sizeof(distrib) / sizeof(distrib[0]);

/* Note: this unit only works when invoked sequentially.
 * No concurrent access is allowed */
static char *g_ptr = NULL;
static size_t g_nbChars = 0;
static size_t g_maxChars = 10000000;
static unsigned g_randRoot = 0;

#define RDG_rotl32(x, r) ((x << r) | (x >> (32 - r)))
static unsigned LOREM_rand(unsigned range) {
  static const unsigned prime1 = 2654435761U;
  static const unsigned prime2 = 2246822519U;
  unsigned rand32 = g_randRoot;
  rand32 *= prime1;
  rand32 ^= prime2;
  rand32 = RDG_rotl32(rand32, 13);
  g_randRoot = rand32;
  return (unsigned)(((unsigned long long)rand32 * range) >> 32);
}

static void writeLastCharacters(void) {
  size_t lastChars = g_maxChars - g_nbChars;
  assert(g_maxChars >= g_nbChars);
  if (lastChars == 0)
    return;
  g_ptr[g_nbChars++] = '.';
  if (lastChars > 2) {
    memset(g_ptr + g_nbChars, ' ', lastChars - 2);
  }
  if (lastChars > 1) {
    g_ptr[g_maxChars-1] = '\n';
  }
  g_nbChars = g_maxChars;
}

static void generateWord(const char *word, const char *separator, int upCase)
{
    size_t const len = strlen(word) + strlen(separator);
    if (g_nbChars + len > g_maxChars) {
        writeLastCharacters();
        return;
    }
    memcpy(g_ptr + g_nbChars, word, strlen(word));
    if (upCase) {
        static const char toUp = 'A' - 'a';
        g_ptr[g_nbChars] = (char)(g_ptr[g_nbChars] + toUp);
    }
    g_nbChars += strlen(word);
    memcpy(g_ptr + g_nbChars, separator, strlen(separator));
    g_nbChars += strlen(separator);
}

static int about(unsigned target) {
  return (int)(LOREM_rand(target) + LOREM_rand(target) + 1);
}

/* Function to generate a random sentence */
static void generateSentence(int nbWords) {
  int commaPos = about(9);
  int comma2 = commaPos + about(7);
  int i;
  for (i = 0; i < nbWords; i++) {
    int const wordID = distrib[LOREM_rand(distribCount)];
    const char *const word = words[wordID];
    const char* sep = " ";
    if (i == commaPos)
      sep = ", ";
    if (i == comma2)
      sep = ", ";
    if (i == nbWords - 1)
      sep = ". ";
    generateWord(word, sep, i==0);
  }
}

static void generateParagraph(int nbSentences) {
  int i;
  for (i = 0; i < nbSentences; i++) {
    int wordsPerSentence = about(8);
    generateSentence(wordsPerSentence);
  }
  if (g_nbChars < g_maxChars) {
    g_ptr[g_nbChars++] = '\n';
  }
  if (g_nbChars < g_maxChars) {
    g_ptr[g_nbChars++] = '\n';
  }
}

/* It's "common" for lorem ipsum generators to start with the same first
 * pre-defined sentence */
static void generateFirstSentence(void) {
  int i;
  for (i = 0; i < 18; i++) {
    const char *word = words[i];
    const char *separator = " ";
    if (i == 4)
      separator = ", ";
    if (i == 7)
      separator = ", ";
    generateWord(word, separator, i==0);
  }
  generateWord(words[18], ". ", 0);
}

size_t LOREM_genBlock(void* buffer, size_t size,
                      unsigned seed,
                      int first, int fill)
{
  g_ptr = (char*)buffer;
  assert(size < INT_MAX);
  g_maxChars = size;
  g_nbChars = 0;
  g_randRoot = seed;
  if (first) {
    generateFirstSentence();
  }
  while (g_nbChars < g_maxChars) {
    int sentencePerParagraph = about(7);
    generateParagraph(sentencePerParagraph);
    if (!fill)
      break; /* only generate one paragraph in not-fill mode */
  }
  g_ptr = NULL;
  return g_nbChars;
}

void LOREM_genBuffer(void* buffer, size_t size, unsigned seed)
{
  LOREM_genBlock(buffer, size, seed, 1, 1);
}

