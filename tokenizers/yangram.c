/* Copyright(C) 2014 Naoya Murakami <naoya@createfield.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301  USA
*/

#include <groonga/tokenizer.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __GNUC__
#  define GNUC_UNUSED __attribute__((__unused__))
#else
#  define GNUC_UNUSED
#endif

#define GRN_STR_BLANK 0x80
#define GRN_STR_ISBLANK(c) (c & 0x80)
#define GRN_STR_CTYPE(c) (c & 0x7f)

#define VGRAM_WORD_TABLE_NAME "vgram_words"
#define KNOWN_PHRASE_TABLE_NAME "known_phrases"

#define MAX_N_HITS 1024

#define NGRAM 0
#define VGRAM 1
#define VGRAM_BOTH 2
#define VGRAM_QUAD 3

#define GRN_TOKENIZER_END_MARK_UTF8       "\xEF\xBF\xB0"
#define GRN_TOKENIZER_END_MARK_UTF8_LEN   3

static grn_hash *occur_hash = NULL;

#define EXPANDED_TOKEN 1
#define USED_TOKEN 2

typedef struct {
  grn_tokenizer_token token;
  grn_tokenizer_query *query;
  const unsigned char *next;
  const unsigned char *end;
  int rest_length;
  const unsigned char *pushed_token_tail;
  const unsigned char *ctypes;
  int ctypes_next;
  unsigned short ngram_unit;
  grn_bool ignore_blank;
  grn_bool split_symbol;
  grn_bool split_alpha;
  grn_bool split_digit;
  grn_bool skip_overlap;
  unsigned short use_vgram;
  grn_obj *vgram_table;
  grn_obj *phrase_table;
  grn_pat_scan_hit *hits;
  const char *scan_start;
  const char *scan_rest;
  unsigned int nhits;
  unsigned int current_hit;
  unsigned int n_tokens;
  grn_bool is_end;
  grn_bool is_additional_tokens;
  grn_hash_cursor *cur;
} grn_yangram_tokenizer;

static grn_bool
is_token_all_blank(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                   const unsigned char *token_top,
                   int token_size)
{
  unsigned int char_length;
  unsigned int rest_length = tokenizer->rest_length;
  int i = 0;
  if ((char_length = grn_plugin_isspace(ctx, (char *)token_top,
                                        rest_length,
                                        tokenizer->query->encoding))) {
    i = 1;
    token_top += char_length;
    rest_length -= char_length;
    while (i < token_size &&
           (char_length = grn_plugin_isspace(ctx, (char *)token_top,
                                             rest_length,
                                             tokenizer->query->encoding))) {
      token_top += char_length;
      rest_length -= char_length;
      i++;
    }
  }
  if (i == token_size) {
    return GRN_TRUE;
  }
  return GRN_FALSE;
}

static grn_bool
is_token_group(grn_yangram_tokenizer *tokenizer, const unsigned char *ctypes)
{
  if (ctypes &&
      ((tokenizer->split_alpha == GRN_FALSE &&
       GRN_STR_CTYPE(*ctypes) == GRN_CHAR_ALPHA) ||
       (tokenizer->split_digit == GRN_FALSE &&
       GRN_STR_CTYPE(*ctypes) == GRN_CHAR_DIGIT) ||
       (tokenizer->split_symbol == GRN_FALSE &&
       GRN_STR_CTYPE(*ctypes) == GRN_CHAR_SYMBOL))
    ) {
    return GRN_TRUE;
  } else {
    return GRN_FALSE;
  }
}

static int
forward_scan_hit_token_tail(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                            const unsigned char **token_tail,
                            unsigned int scan_length)
{
  int token_size = 0;
  unsigned int char_length;
  unsigned int rest_length = tokenizer->rest_length;
  const unsigned char *token_top = *token_tail;

  while ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, rest_length,
                                           tokenizer->query->encoding))) {
    token_size++;
    *token_tail += char_length;
    rest_length -= char_length;
    if (*token_tail - token_top >= scan_length) {
      break;
    }
  }
  return token_size;
}

static int
forward_grouped_token_tail(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                           const unsigned char *ctypes,
                           const unsigned char **token_tail)
{
  int token_size = 0;
  unsigned int char_length;
  unsigned int rest_length = tokenizer->rest_length;
  if (ctypes &&
      tokenizer->split_alpha == GRN_FALSE &&
      GRN_STR_CTYPE(*ctypes) == GRN_CHAR_ALPHA) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, rest_length,
                                             tokenizer->query->encoding))) {
      token_size++;
      *token_tail += char_length;
      rest_length -= char_length;
      if (GRN_STR_ISBLANK(*ctypes)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctypes) != GRN_CHAR_ALPHA) {
        break;
      }
    }
  } else if (ctypes &&
             tokenizer->split_digit == GRN_FALSE &&
             GRN_STR_CTYPE(*ctypes) == GRN_CHAR_DIGIT) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, rest_length,
                                             tokenizer->query->encoding))) {
      token_size++;
      *token_tail += char_length;
      rest_length -= char_length;
      if (GRN_STR_ISBLANK(*ctypes)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctypes) != GRN_CHAR_DIGIT) {
        break;
      }
    }
  } else if (ctypes &&
             tokenizer->split_symbol == GRN_FALSE &&
             GRN_STR_CTYPE(*ctypes) == GRN_CHAR_SYMBOL) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, rest_length,
                                             tokenizer->query->encoding))) {
      token_size++;
      *token_tail += char_length;
      rest_length -= char_length;
      if (!tokenizer->ignore_blank && GRN_STR_ISBLANK(*ctypes)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctypes) != GRN_CHAR_SYMBOL) {
        break;
      }
    }
  }
  return token_size;
}

static int
forward_ngram_token_tail(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                         const unsigned char *ctypes,
                         const unsigned char **token_tail)
{
  int token_size = 0;
  unsigned int char_length;
  unsigned int rest_length = tokenizer->rest_length;

  if ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, rest_length,
                                        tokenizer->query->encoding))) {
    token_size++;
    *token_tail += char_length;
    rest_length -= char_length;
    while (token_size < tokenizer->ngram_unit &&
           (char_length = grn_plugin_charlen(ctx, (char *)*token_tail, rest_length,
                                             tokenizer->query->encoding))) {
      if (tokenizer->phrase_table) {
        if (tokenizer->nhits > 0 &&
            tokenizer->current_hit < tokenizer->nhits &&
            *token_tail - (const unsigned char *)tokenizer->scan_start ==
            tokenizer->hits[tokenizer->current_hit].offset) {
          break;
        }
      }

      if (ctypes) {
        if (!tokenizer->ignore_blank && GRN_STR_ISBLANK(*ctypes)) {
          break;
        }
       
        ctypes++;
        if ((tokenizer->split_alpha == GRN_FALSE &&
            GRN_STR_CTYPE(*ctypes) == GRN_CHAR_ALPHA) ||
            (tokenizer->split_digit == GRN_FALSE &&
            GRN_STR_CTYPE(*ctypes) == GRN_CHAR_DIGIT) ||
            (tokenizer->split_symbol == GRN_FALSE &&
            GRN_STR_CTYPE(*ctypes) == GRN_CHAR_SYMBOL)) {
          break;
        }
      }
      token_size++;
      *token_tail += char_length;
      rest_length -= char_length;
    }
  }
  return token_size;
}

static grn_bool
is_group_border(GNUC_UNUSED grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                const unsigned char *token_tail, const unsigned char *ctypes, int token_size)
{
  if (ctypes) {
    ctypes = ctypes + token_size;
  }
  if (ctypes) {
    if (is_token_group(tokenizer, ctypes)) {
      return GRN_TRUE;
    }
  }
  if (tokenizer->phrase_table) {
    if (tokenizer->nhits > 0 &&
        tokenizer->current_hit < tokenizer->nhits &&
        token_tail - (const unsigned char *)tokenizer->scan_start ==
        tokenizer->hits[tokenizer->current_hit].offset) {
      return GRN_TRUE;
    }
  }
  return GRN_FALSE;
}

static grn_obj *
yangram_init(grn_ctx *ctx, int nargs, grn_obj **args, grn_user_data *user_data,
             unsigned short ngram_unit, grn_bool ignore_blank,
             grn_bool split_symbol, grn_bool split_alpha, grn_bool split_digit,
             grn_bool skip_overlap, unsigned short use_vgram)
{
  grn_tokenizer_query *query;
  unsigned int normalize_flags =
    GRN_STRING_WITH_TYPES |
    GRN_STRING_REMOVE_TOKENIZED_DELIMITER;

  const char *normalized;
  unsigned int normalized_length_in_bytes;
  grn_yangram_tokenizer *tokenizer;

  if (!skip_overlap || ignore_blank) {
    normalize_flags |= GRN_STRING_REMOVE_BLANK;
  }

  query = grn_tokenizer_query_open(ctx, nargs, args, normalize_flags);
  if (!query) {
    return NULL;
  }
  if (!(tokenizer = GRN_PLUGIN_MALLOC(ctx, sizeof(grn_yangram_tokenizer)))) {
    GRN_PLUGIN_ERROR(ctx,GRN_NO_MEMORY_AVAILABLE,
                     "[tokenizer][yangram] "
                     "memory allocation to grn_yangram_tokenizer failed");
    grn_tokenizer_query_close(ctx, query);
    return NULL;
  }
  user_data->ptr = tokenizer;
  grn_tokenizer_token_init(ctx, &(tokenizer->token));
  tokenizer->query = query;
  tokenizer->skip_overlap = skip_overlap;
  tokenizer->ignore_blank = ignore_blank;
  tokenizer->ngram_unit = ngram_unit;
  tokenizer->split_symbol = split_symbol;
  tokenizer->split_alpha = split_alpha;
  tokenizer->split_digit = split_digit;
  tokenizer->use_vgram = use_vgram;
  if (tokenizer->use_vgram > 0) {
    const char *vgram_word_table_name_env;
    vgram_word_table_name_env = getenv("GRN_VGRAM_WORD_TABLE_NAME");

    if (vgram_word_table_name_env) {
      tokenizer->vgram_table = grn_ctx_get(ctx,
                                           vgram_word_table_name_env,
                                           strlen(vgram_word_table_name_env));
    } else {
      tokenizer->vgram_table = grn_ctx_get(ctx,
                                           VGRAM_WORD_TABLE_NAME,
                                           strlen(VGRAM_WORD_TABLE_NAME));
    }
    if (!tokenizer->vgram_table) {
       GRN_PLUGIN_ERROR(ctx, GRN_NO_MEMORY_AVAILABLE,
                        "[tokenizer][yangram] "
                        "couldn't open a vgram table");
       tokenizer->vgram_table = NULL;
       return NULL;
    }
  } else {
    tokenizer->vgram_table = NULL;
  }
  grn_string_get_normalized(ctx, tokenizer->query->normalized_query,
                            &normalized, &normalized_length_in_bytes,
                            NULL);
  {
    const char *phrase_table_name_env;
    phrase_table_name_env = getenv("GRN_KNOWN_PHRASE_TABLE_NAME");

    if (phrase_table_name_env) {
      tokenizer->phrase_table = grn_ctx_get(ctx,
                                            phrase_table_name_env,
                                            strlen(phrase_table_name_env));
    } else {
      tokenizer->phrase_table = grn_ctx_get(ctx,
                                            KNOWN_PHRASE_TABLE_NAME,
                                            strlen(KNOWN_PHRASE_TABLE_NAME));
    }
    if (tokenizer->phrase_table) {
      if (!(tokenizer->hits =
          GRN_PLUGIN_MALLOC(ctx, sizeof(grn_pat_scan_hit) * MAX_N_HITS))) {
        GRN_PLUGIN_ERROR(ctx,GRN_NO_MEMORY_AVAILABLE,
                        "[tokenizer][yangram] "
                        "memory allocation to grn_pat_scan_hit failed");
        grn_tokenizer_query_close(ctx, query);
        return NULL;
      } else {
        tokenizer->scan_rest = normalized;
        tokenizer->nhits = 0;
        tokenizer->current_hit = 0;
      }
    } else {
     tokenizer->phrase_table = NULL;
    }
  }

  tokenizer->next = (const unsigned char *)normalized;
  tokenizer->end = tokenizer->next + normalized_length_in_bytes;
  tokenizer->rest_length = tokenizer->end - tokenizer->next;
  tokenizer->ctypes =
    grn_string_get_types(ctx, tokenizer->query->normalized_query);

  tokenizer->pushed_token_tail = NULL;
  tokenizer->ctypes_next = 0;
  tokenizer->n_tokens = 0;

  tokenizer->is_end = GRN_FALSE;
  tokenizer->is_additional_tokens = GRN_FALSE;

  return NULL;
}

static grn_obj *
yangram_next(grn_ctx *ctx, GNUC_UNUSED int nargs, GNUC_UNUSED grn_obj **args,
             grn_user_data *user_data)
{
  grn_yangram_tokenizer *tokenizer = user_data->ptr;
  const unsigned char *string_end = tokenizer->end;
  const unsigned char *token_top = tokenizer->next;
  const unsigned char *token_next = token_top;
  const unsigned char *token_tail = token_top;
  int token_size = 0;
  grn_bool is_token_grouped = GRN_FALSE;
  const unsigned char *token_ctypes = NULL;
  unsigned int ctypes_skip_size;
  int char_length = 0;
  grn_tokenizer_status status = 0;
  grn_bool is_token_hit = GRN_FALSE;

  /* append occurrence vgram tokens to be able to exact match */
  if (occur_hash &&
      (tokenizer->query->tokenize_mode == GRN_TOKENIZE_ADD ||
       tokenizer->query->tokenize_mode == GRN_TOKENIZE_DELETE)) {
    if (tokenizer->is_end) {
      grn_tokenizer_token_push(ctx,
                               &(tokenizer->token),
                               GRN_TOKENIZER_END_MARK_UTF8,
                               GRN_TOKENIZER_END_MARK_UTF8_LEN,
                               status);
      tokenizer->is_end = GRN_FALSE;
      tokenizer->is_additional_tokens = GRN_TRUE;
      tokenizer->cur = grn_hash_cursor_open(ctx, occur_hash,
                                           NULL, 0, NULL, 0, 0, -1, 0);
      return NULL;
    } else if (tokenizer->is_additional_tokens) {
      void *key;
      unsigned int key_size = 0;
      unsigned int token_size = 0;
      grn_id tid;
      int *value;
      while ((tid = grn_hash_cursor_next(ctx, tokenizer->cur))) {
        grn_hash_cursor_get_key_value(ctx, tokenizer->cur,
                                      &key, &key_size,
                                      (void **)&value);
        grn_hash_delete(ctx, occur_hash, key, key_size, NULL);
        if (*value == EXPANDED_TOKEN) {
          token_size = key_size;
          *value = 0;
          break;
        } else {
          token_size = 0;
          *value = 0;
        }
      }

      if (!tid) {
        grn_hash_cursor_close(ctx, tokenizer->cur);
        status |= GRN_TOKEN_LAST;
      }
      grn_tokenizer_token_push(ctx,
                               &(tokenizer->token),
                               (const char *)key,
                               token_size,
                               status);
      return NULL;
    }
  }

  if (tokenizer->phrase_table) {
    if (tokenizer->nhits > 0 &&
        token_top - (const unsigned char *)tokenizer->scan_start >
        tokenizer->hits[tokenizer->current_hit].offset) {
      tokenizer->current_hit++;
    }
    if (tokenizer->current_hit >= tokenizer->nhits) {
      tokenizer->scan_start = tokenizer->scan_rest;
      unsigned int scan_rest_length = tokenizer->end - (const unsigned char *)tokenizer->scan_rest;
      if (scan_rest_length > 0) {
        tokenizer->nhits = grn_pat_scan(ctx, (grn_pat *)tokenizer->phrase_table,
                                        tokenizer->scan_rest,
                                        scan_rest_length,
                                        tokenizer->hits, MAX_N_HITS, &(tokenizer->scan_rest));
        tokenizer->current_hit = 0;
      }
    }
    if (tokenizer->nhits > 0 &&
        tokenizer->current_hit < tokenizer->nhits &&
        token_top - (const unsigned char *)tokenizer->scan_start ==
        tokenizer->hits[tokenizer->current_hit].offset) {
      is_token_hit = GRN_TRUE;
    }
  }

  if (tokenizer->ctypes) {
    token_ctypes = tokenizer->ctypes + tokenizer->ctypes_next;
  } else {
    token_ctypes = NULL;
  }

  if (is_token_hit) {
   token_size = forward_scan_hit_token_tail(ctx, tokenizer, &token_tail,
                                            tokenizer->hits[tokenizer->current_hit].length);
   token_next = token_tail;
   tokenizer->current_hit++;
  } else {
    is_token_grouped = is_token_group(tokenizer, token_ctypes);
    if (is_token_grouped) {
      token_size = forward_grouped_token_tail(ctx, tokenizer, token_ctypes, &token_tail);
      token_next = token_tail;
    } else {
      token_size = forward_ngram_token_tail(ctx, tokenizer, token_ctypes, &token_tail);
      char_length = grn_plugin_charlen(ctx, (char *)token_next,
                                       tokenizer->rest_length,
                                       tokenizer->query->encoding);
      token_next += char_length;
    }
  }

  if (token_top == token_tail || token_next == string_end) {
    ctypes_skip_size = 0;
  } else {
    if (is_token_grouped || is_token_hit) {
      ctypes_skip_size = token_size;
    } else {
      ctypes_skip_size = 1;
    }
  }

  if (tokenizer->use_vgram > 0 && !is_token_grouped) {
    grn_bool maybe_vgram = GRN_FALSE;

    grn_id id;
    id = grn_table_get(ctx, tokenizer->vgram_table,
                       (const char *)token_top, token_tail - token_top);
    if (id) {
      maybe_vgram = GRN_TRUE;
    }

    /* check next token is vgram or not */
    if (tokenizer->use_vgram >= VGRAM_BOTH && !maybe_vgram) {
      if (token_tail < string_end &&
          !is_group_border(ctx, tokenizer, token_tail, token_ctypes, token_size)) {
        grn_id id;
        const unsigned char *token_next_tail;
        char_length = grn_plugin_charlen(ctx, (char *)token_tail,
                                         tokenizer->rest_length,
                                         tokenizer->query->encoding);
        token_next_tail = token_tail + char_length;
        id = grn_table_get(ctx, tokenizer->vgram_table,
                           (const char *)token_next, token_next_tail - token_next);
        if (id) {
          maybe_vgram = GRN_TRUE;
        }
      } else if (token_tail == string_end &&
                 tokenizer->query->tokenize_mode == GRN_TOKENIZE_GET) {
        maybe_vgram = GRN_TRUE;
      }
    }

    /* expand token */
    if (maybe_vgram) {

      /* append occurrence vgram original token */
      if (occur_hash&&
          (tokenizer->query->tokenize_mode == GRN_TOKENIZE_ADD ||
           tokenizer->query->tokenize_mode == GRN_TOKENIZE_DELETE)) {
        if (token_size >= tokenizer->ngram_unit) {
          int *pvalue;
          int added;
          grn_hash_add(ctx, occur_hash,
                       token_top, token_tail - token_top, (void **)&pvalue, &added);
          if (added) {
            *pvalue = EXPANDED_TOKEN;
          } else if (*pvalue != USED_TOKEN) {
            *pvalue = EXPANDED_TOKEN;
          }
        }
      }

      if (token_tail < string_end &&
          !is_group_border(ctx, tokenizer, token_tail, token_ctypes, token_size)) {
        char_length = grn_plugin_charlen(ctx, (char *)token_tail,
                                         tokenizer->rest_length,
                                         tokenizer->query->encoding);
        token_size++;
        token_tail += char_length;


        if (tokenizer->use_vgram == VGRAM_QUAD) {
          if (token_tail < string_end &&
              !is_group_border(ctx, tokenizer, token_tail, token_ctypes, token_size)) {
            id = grn_table_get(ctx, tokenizer->vgram_table,
                               (const char *)token_top, token_tail - token_top);
            if (id) {

              /* append occurrence vgram original token */
              if (occur_hash &&
                  (tokenizer->query->tokenize_mode == GRN_TOKENIZE_ADD ||
                   tokenizer->query->tokenize_mode == GRN_TOKENIZE_DELETE)) {
                if (token_size >= tokenizer->ngram_unit) {
                  int *pvalue;
                  int added;
                  grn_hash_add(ctx, occur_hash,
                               token_top, token_tail - token_top, (void **)&pvalue, &added);
                  if (added) {
                    *pvalue = EXPANDED_TOKEN;
                   } else if (*pvalue != USED_TOKEN) {
                     *pvalue = EXPANDED_TOKEN;
                   }
                }
              }

              char_length = grn_plugin_charlen(ctx, (char *)token_tail,
                                               tokenizer->rest_length,
                                               tokenizer->query->encoding);
              token_size++;
              token_tail += char_length;
            }
          } else {
            /* add failed expand token flag */
            if (occur_hash &&
                (tokenizer->query->tokenize_mode == GRN_TOKENIZE_ADD ||
                 tokenizer->query->tokenize_mode == GRN_TOKENIZE_DELETE)) {
              int *pvalue;
              int added;
              grn_hash_add(ctx, occur_hash,
                           token_top, token_tail - token_top, (void **)&pvalue, &added);
              *pvalue = 2;
            }

            if (token_tail == string_end &&
              tokenizer->query->tokenize_mode == GRN_TOKENIZE_GET) {
              if (occur_hash && tokenizer->n_tokens == 0 &&
                  token_size >= tokenizer->ngram_unit) {
                /* can exact match by added occurrence vgram tokens */
              } else {
                status |= GRN_TOKEN_FORCE_PREFIX;
              }
            }
          }
        }


      } else {
        /* add failed expand token flag */
        if (occur_hash &&
            (tokenizer->query->tokenize_mode == GRN_TOKENIZE_ADD ||
             tokenizer->query->tokenize_mode == GRN_TOKENIZE_DELETE)) {
          int *pvalue;
          int added;
          grn_hash_add(ctx, occur_hash,
                       token_top, token_tail - token_top, (void **)&pvalue, &added);
          *pvalue = EXPANDED_TOKEN;
        }

        if (token_tail == string_end &&
            tokenizer->query->tokenize_mode == GRN_TOKENIZE_GET) {
          if (occur_hash && tokenizer->n_tokens == 0 &&
              token_size >= tokenizer->ngram_unit) {
            /* can exact match by added occurrence vgram tokens */
          } else {
            status |= GRN_TOKEN_FORCE_PREFIX;
          }
        }
      }
    }
  }

  if (token_top == token_tail || token_next == string_end) {
    status |= GRN_TOKEN_LAST;
  }

  if (token_tail == string_end) {
    status |= GRN_TOKEN_REACH_END;
  }

  if (!is_token_grouped && !is_token_hit && token_size < tokenizer->ngram_unit) {
    status |= GRN_TOKEN_UNMATURED;
  }

  if (token_size) {
    if (tokenizer->skip_overlap &&
        is_token_all_blank(ctx, tokenizer, token_top, token_size)) {
      status |= GRN_TOKEN_SKIP_WITH_POSITION;
    }
  }

  if (tokenizer->pushed_token_tail &&
      token_top < tokenizer->pushed_token_tail) {
    status |= GRN_TOKEN_OVERLAP;
    if (tokenizer->skip_overlap &&
        !(status & GRN_TOKEN_REACH_END) &&
        !(status & GRN_TOKEN_SKIP_WITH_POSITION) &&
      tokenizer->query->tokenize_mode == GRN_TOKENIZE_GET) {
      if (token_tail <= tokenizer->pushed_token_tail) {
        status |= GRN_TOKEN_SKIP;
      } else {
        if (!is_group_border(ctx, tokenizer, token_tail, token_ctypes, token_size)) {
          status |= GRN_TOKEN_SKIP;
        }
      }
    }
  }

  if (!(status & GRN_TOKEN_SKIP) &&
      !(status & GRN_TOKEN_SKIP_WITH_POSITION)) {
    tokenizer->pushed_token_tail = token_tail;
    tokenizer->n_tokens++;
  }

  tokenizer->next = token_next;
  tokenizer->rest_length = string_end - token_next;
  tokenizer->ctypes_next = tokenizer->ctypes_next + ctypes_skip_size;

  /* pending to add occurrence vgram tokens */
  if (occur_hash &&
      (tokenizer->query->tokenize_mode == GRN_TOKENIZE_ADD ||
       tokenizer->query->tokenize_mode == GRN_TOKENIZE_DELETE)) {
    if (status & GRN_TOKEN_LAST) {
      if (grn_hash_size(ctx, occur_hash)) {
        tokenizer->is_end = GRN_TRUE;
        status &= ~GRN_TOKEN_LAST;
      }
    }
  }

  grn_tokenizer_token_push(ctx,
                           &(tokenizer->token),
                           (const char *)token_top,
                           token_tail - token_top,
                           status);

  return NULL;
}

static grn_obj *
yangram_fin(grn_ctx *ctx, GNUC_UNUSED int nargs, GNUC_UNUSED grn_obj **args,
            grn_user_data *user_data)
{
  grn_yangram_tokenizer *tokenizer = user_data->ptr;

  if (!tokenizer) {
    return NULL;
  }
  if (tokenizer->vgram_table) {
    grn_obj_unlink(ctx, tokenizer->vgram_table);
  }
  if (tokenizer->phrase_table) {
    grn_obj_unlink(ctx, tokenizer->phrase_table);
    GRN_PLUGIN_FREE(ctx, tokenizer->hits);
  }
  grn_tokenizer_query_close(ctx, tokenizer->query);
  grn_tokenizer_token_fin(ctx, &(tokenizer->token));
  GRN_PLUGIN_FREE(ctx, tokenizer);
  return NULL;
}

static grn_obj *
yabigram_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 0, 0, 0, 1, NGRAM);
}

static grn_obj *
yabigram_ib_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 1, 0, 0, 0, 1, NGRAM);
}

static grn_obj *
yabigram_sa_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 1, 1, 0, 1, NGRAM);
}

static grn_obj *
yatrigram_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 3, 0, 0, 0, 0, 1, NGRAM);
}

static grn_obj *
yatrigram_ib_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 3, 1, 0, 0, 0, 1, NGRAM);
}

static grn_obj *
yatrigram_sa_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 3, 0, 1, 1, 0, 1, NGRAM);
}

static grn_obj *
yavgram_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 0, 0, 0, 1, VGRAM);
}

static grn_obj *
yavgram_sa_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 1, 1, 0, 1, VGRAM);
}

static grn_obj *
yavgramb_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 0, 0, 0, 1, VGRAM_BOTH);
}

static grn_obj *
yavgramb_sa_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 1, 1, 0, 1, VGRAM_BOTH);
}

static grn_obj *
yavgramq_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 0, 0, 0, 1, VGRAM_QUAD);
}

static grn_obj *
yavgramq_d_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 0, 0, 1, 1, VGRAM_QUAD);
}

grn_rc
GRN_PLUGIN_INIT(grn_ctx *ctx)
{
  const char *value;
  uint32_t value_size;
  grn_config_get(ctx,
                 "tokenizer_yangram.append_occured_vgram_token",
                 strlen("tokenizer_yangram.append_occured_vgram_token"),
                 &value, &value_size);
  if (ctx->rc) {
    return ctx->rc;
  }

  if (value) {
    occur_hash = grn_hash_create(ctx, NULL,
                                 GRN_TABLE_MAX_KEY_SIZE,
                                 0,
                                 GRN_OBJ_TABLE_HASH_KEY|GRN_OBJ_KEY_VAR_SIZE);
    if (!occur_hash) {
      GRN_PLUGIN_ERROR(ctx, GRN_NO_MEMORY_AVAILABLE,
                       "[tokenizer][yangram] "
                       "couldn't create temp table");
      return ctx->rc;
    }
  }

  return ctx->rc;
}

grn_rc
GRN_PLUGIN_REGISTER(grn_ctx *ctx)
{
  grn_rc rc;

  rc = grn_tokenizer_register(ctx, "TokenYaBigram", -1,
                              yabigram_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaBigramIgnoreBlank", -1,
                              yabigram_ib_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaBigramSplitSymbolAlpha", -1,
                              yabigram_sa_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaTrigram", -1,
                              yatrigram_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaTrigramIgnoreBlank", -1,
                              yatrigram_ib_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaTrigramSplitSymbolAlpha", -1,
                              yatrigram_sa_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaVgram", -1,
                              yavgram_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaVgramSplitSymbolAlpha", -1,
                              yavgram_sa_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaVgramBoth", -1,
                              yavgramb_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaVgramBothSplitSymbolAlpha", -1,
                              yavgramb_sa_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaVgramQuad", -1,
                              yavgramq_init, yangram_next, yangram_fin);

  rc = grn_tokenizer_register(ctx, "TokenYaVgramQuadSplitDigit", -1,
                              yavgramq_d_init, yangram_next, yangram_fin);

  return ctx->rc;
}

grn_rc
GRN_PLUGIN_FIN(GNUC_UNUSED grn_ctx *ctx)
{
  if (occur_hash) {
    grn_hash_close(ctx, occur_hash);
    occur_hash = NULL;
  }
  return GRN_SUCCESS;
}
