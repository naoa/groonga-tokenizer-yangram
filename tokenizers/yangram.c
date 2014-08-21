/*
  Copyright(C) 2014 Naoya Murakami <naoya@createfield.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

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

  This file includes the Groonga ngram tokenizer code.
  https://github.com/groonga/groonga/blob/master/lib/token.c

  The following is the header of the file:

    Copyright(C) 2009-2012 Brazil

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
    USA
*/

#include <groonga/tokenizer.h>

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

typedef enum {
  GRN_TOKEN_GET = 0,
  GRN_TOKEN_ADD,
  GRN_TOKEN_DEL
} grn_token_mode;


typedef struct {
  grn_tokenizer_token token;
  grn_tokenizer_query *query;
  const unsigned char *next;
  const unsigned char *end;
  const unsigned char *ctypes;
  const unsigned char *pushed_token_tail;
  unsigned int skip_size;
  int ctypes_position;
  int rest_length;
  unsigned int token_size;
  unsigned short ngram_unit;
  grn_bool split_alpha;
  grn_bool split_digit;
  grn_bool split_symbol;
  grn_bool ignore_blank;
} grn_yangram_tokenizer;

/*
  int
  ngram_token_cursor(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                     const unsigned char *ctypes,
                     const unsigned char **token_next,
                     const unsigned char **token_tail,
                     grn_bool *is_token_grouped)

  return value is token size.
  It isn't byte length but character length.(ex. ABC = 3, あいう = 3)

  ctypes specify grn_string->ctypes if the group of the alphabet/symbol/degit is required.
  token_tail and token_next specify start pointer to search.
  token_tail is setted token tail pointer.
  token_next is setted next token pointer.
  is_token_grouped is setted grouped flag.

  ungrouped trigram case(ABCD -> ABC/BCD):

  token_next is ABCD (overlap)
                 ^
  token_tail is ABCD
                   ^

  grouped trigram case(ABCDあい -> ABCD/あい):

  token_next is ABCDあい (not overlap)
                    ^
  token_tail is ABCDあい
                    ^
*/


static int
ngram_token_cursor(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                   const unsigned char *ctypes,
                   const unsigned char **token_next,
                   const unsigned char **token_tail,
                   grn_bool *is_token_grouped)
{
  int token_size = 0;
  unsigned int char_length;

  if (ctypes &&
      tokenizer->split_alpha == GRN_FALSE &&
      GRN_STR_CTYPE(*ctypes) == GRN_CHAR_ALPHA) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, tokenizer->rest_length,
                                             tokenizer->query->encoding))) {
      token_size++;
      *token_tail += char_length;
      if (GRN_STR_ISBLANK(*ctypes)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctypes) != GRN_CHAR_ALPHA) {
        break;
      }
    }
    *token_next = *token_tail;
    *is_token_grouped = GRN_TRUE;
  } else if (ctypes &&
             tokenizer->split_digit == GRN_FALSE &&
             GRN_STR_CTYPE(*ctypes) == GRN_CHAR_DIGIT) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, tokenizer->rest_length,
                                             tokenizer->query->encoding))) {
      token_size++;
      *token_tail += char_length;
      if (GRN_STR_ISBLANK(*ctypes)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctypes) != GRN_CHAR_DIGIT) {
        break;
      }
    }
    *token_next = *token_tail;
    *is_token_grouped = GRN_TRUE;
  } else if (ctypes &&
             tokenizer->split_symbol == GRN_FALSE &&
             GRN_STR_CTYPE(*ctypes) == GRN_CHAR_SYMBOL) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, tokenizer->rest_length,
                                             tokenizer->query->encoding))) {
      token_size++;
      *token_tail += char_length;
      if (!tokenizer->ignore_blank && GRN_STR_ISBLANK(*ctypes)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctypes) != GRN_CHAR_SYMBOL) {
        break;
      }
    }
    *token_next = *token_tail;
    *is_token_grouped = GRN_TRUE;
  } else {
    if ((char_length = grn_plugin_charlen(ctx, (char *)*token_tail, tokenizer->rest_length,
                                          tokenizer->query->encoding))) {
      token_size++;
      *token_tail += char_length;
      *token_next = *token_tail;
      while (token_size < tokenizer->ngram_unit &&
             (char_length = grn_plugin_charlen(ctx, (char *)*token_tail, tokenizer->rest_length,
                                               tokenizer->query->encoding))) {
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
      }
      *is_token_grouped = GRN_FALSE;
    }
  }
  return token_size;
}

static grn_obj *
yangram_init(grn_ctx *ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  grn_tokenizer_query *query;
  unsigned int normalize_flags =
    GRN_STRING_REMOVE_BLANK |
    GRN_STRING_WITH_TYPES |
    GRN_STRING_REMOVE_TOKENIZED_DELIMITER;

  const char *normalized;
  unsigned int normalized_length_in_bytes;
  grn_yangram_tokenizer *tokenizer;

  query = grn_tokenizer_query_open(ctx, nargs, args, normalize_flags);
  if (!query) {
    return NULL;
  }

  if (!(tokenizer = GRN_PLUGIN_MALLOC(ctx,sizeof(grn_yangram_tokenizer)))) {
    GRN_PLUGIN_ERROR(ctx,GRN_NO_MEMORY_AVAILABLE,
                     "[tokenizer][yangram] "
                     "memory allocation to grn_yangram_tokenizer failed");
    grn_tokenizer_query_close(ctx, query);
    return NULL;
  }
  user_data->ptr = tokenizer;
  grn_tokenizer_token_init(ctx, &(tokenizer->token));
  tokenizer->query = query;

  grn_string_get_normalized(ctx, tokenizer->query->normalized_query,
                            &normalized, &normalized_length_in_bytes,
                            NULL);
  tokenizer->next = (const unsigned char *)normalized;
  tokenizer->end = tokenizer->next + normalized_length_in_bytes;
  tokenizer->rest_length = tokenizer->end - tokenizer->next;
  tokenizer->ctypes =
    grn_string_get_types(ctx, tokenizer->query->normalized_query);

  tokenizer->pushed_token_tail = NULL;
  tokenizer->ctypes_position = 0;
  tokenizer->skip_size = 0;

  tokenizer->ngram_unit = 2;
  tokenizer->split_alpha = GRN_FALSE;
  tokenizer->split_digit = GRN_FALSE;
  tokenizer->split_symbol = GRN_FALSE;
  tokenizer->ignore_blank = GRN_FALSE;

  return NULL;
}

static grn_obj *
yangram_next(grn_ctx *ctx, GNUC_UNUSED int nargs, GNUC_UNUSED grn_obj **args,
             grn_user_data *user_data)
{
  grn_yangram_tokenizer *tokenizer = user_data->ptr;

  const unsigned char *token_top = tokenizer->next;
  const unsigned char *token_next = token_top;
  const unsigned char *token_tail = token_top;
  const unsigned char *string_end = tokenizer->end;

  int token_size = 0;
  int ctypes_position = tokenizer->ctypes_position + tokenizer->skip_size;
  grn_tokenizer_status status = 0;
  grn_bool is_token_grouped = GRN_FALSE;
  const unsigned char *ctypes;

  if (tokenizer->ctypes) {
    ctypes = tokenizer->ctypes + ctypes_position;
  } else {
    ctypes = NULL;
  }

  token_size = ngram_token_cursor(ctx, tokenizer,
                                  ctypes,
                                  &token_next,
                                  &token_tail,
                                  &is_token_grouped);

  if (tokenizer->pushed_token_tail && token_top < tokenizer->pushed_token_tail) {
    status |= GRN_TOKENIZER_TOKEN_OVERLAP;
  }

  if (!is_token_grouped && token_size < tokenizer->ngram_unit) {
      status |= GRN_TOKENIZER_TOKEN_UNMATURED;
      if (status & GRN_TOKENIZER_TOKEN_OVERLAP &&
          tokenizer->query->token_mode == GRN_TOKEN_GET) {
        status |= GRN_TOKENIZER_TOKEN_SKIP;
      }
  }

  tokenizer->next = token_next;
  tokenizer->rest_length = string_end - token_next;
  tokenizer->ctypes_position = ctypes_position;
  tokenizer->token_size = token_size;

  if (token_top == token_tail || tokenizer->next == string_end) {
    tokenizer->skip_size = 0;
    status |= GRN_TOKENIZER_TOKEN_LAST;
  } else {
    if (is_token_grouped) {
      tokenizer->skip_size = token_size;
    } else {
      tokenizer->skip_size = 1;
    }
  }
  if (token_tail == string_end) {
    status |= GRN_TOKENIZER_TOKEN_REACH_END;
  }

  {
    int token_length = token_tail - token_top;
    if (!(status & GRN_TOKENIZER_TOKEN_SKIP)) {
      tokenizer->pushed_token_tail = token_tail;
    }

    grn_tokenizer_token_push(ctx,
                             &(tokenizer->token),
                             (const char *)token_top,
                             token_length,
                             status);
  }
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
  grn_tokenizer_query_close(ctx, tokenizer->query);
  grn_tokenizer_token_fin(ctx, &(tokenizer->token));
  GRN_PLUGIN_FREE(ctx,tokenizer);
  return NULL;
}

grn_rc
GRN_PLUGIN_INIT(grn_ctx *ctx)
{
  return ctx->rc;
}


grn_rc
GRN_PLUGIN_REGISTER(grn_ctx *ctx)
{
  grn_rc rc;

  rc = grn_tokenizer_register(ctx, "TokenYaBigram", -1,
                              yangram_init, yangram_next, yangram_fin);

  return rc;
}

grn_rc
GRN_PLUGIN_FIN(GNUC_UNUSED grn_ctx *ctx)
{
  return GRN_SUCCESS;
}
