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

typedef struct {
  grn_tokenizer_token token;
  grn_tokenizer_query *query;
  const unsigned char *next;
  const unsigned char *end;
  const unsigned char *ctypes;
  grn_bool is_token_grouped;
  unsigned int skip_size;
  int token_top_position;
  unsigned int token_tail_position;
  unsigned int token_length;
  unsigned short ngram_unit;
  grn_bool split_alpha;
  grn_bool split_digit;
  grn_bool split_symbol;
  grn_bool ignore_blank;
} grn_yangram_tokenizer;

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
  tokenizer->ctypes =
    grn_string_get_types(ctx, tokenizer->query->normalized_query);

  tokenizer->is_token_grouped = GRN_FALSE;
  tokenizer->token_top_position = 0;
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

  const unsigned char *token_top_pointer = tokenizer->next;
  const unsigned char *token_next_pointer = tokenizer->next;
  const unsigned char *token_cursor = token_top_pointer;
  const unsigned char *string_end = tokenizer->end;

  unsigned int char_length;
  unsigned int rest_length = string_end - token_top_pointer;
  int token_length = 0;
  int token_top_position = tokenizer->token_top_position + tokenizer->skip_size;
  grn_tokenizer_status status = 0;
  grn_bool is_current_token_grouped = GRN_FALSE;
  const unsigned char *ctype_cursor;

  if (tokenizer->ctypes) {
    ctype_cursor = tokenizer->ctypes + token_top_position;
  } else {
    ctype_cursor = NULL;
  }

  if (ctype_cursor &&
      tokenizer->split_alpha == GRN_FALSE &&
      GRN_STR_CTYPE(*ctype_cursor) == GRN_CHAR_ALPHA) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)token_cursor, rest_length,
                                             tokenizer->query->encoding))) {
      token_length++;
      token_cursor += char_length;
      if (GRN_STR_ISBLANK(*ctype_cursor)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctype_cursor) != GRN_CHAR_ALPHA) {
        break;
      }
    }
    token_next_pointer = token_cursor;
    is_current_token_grouped = GRN_TRUE;
  } else if (ctype_cursor &&
             tokenizer->split_digit == GRN_FALSE &&
             GRN_STR_CTYPE(*ctype_cursor) == GRN_CHAR_DIGIT) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)token_cursor, rest_length,
                                             tokenizer->query->encoding))) {
      token_length++;
      token_cursor += char_length;
      if (GRN_STR_ISBLANK(*ctype_cursor)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctype_cursor) != GRN_CHAR_DIGIT) {
        break;
      }
    }
    token_next_pointer = token_cursor;
    is_current_token_grouped = GRN_TRUE;
  } else if (ctype_cursor &&
             tokenizer->split_symbol == GRN_FALSE &&
             GRN_STR_CTYPE(*ctype_cursor) == GRN_CHAR_SYMBOL) {
    while ((char_length = grn_plugin_charlen(ctx, (char *)token_cursor, rest_length,
                                             tokenizer->query->encoding))) {
      token_length++;
      token_cursor += char_length;
      if (!tokenizer->ignore_blank && GRN_STR_ISBLANK(*ctype_cursor)) {
        break;
      }
      if (GRN_STR_CTYPE(*++ctype_cursor) != GRN_CHAR_SYMBOL) {
        break;
      }
    }
    token_next_pointer = token_cursor;
    is_current_token_grouped = GRN_TRUE;
  } else {
    if ((char_length = grn_plugin_charlen(ctx, (char *)token_cursor, rest_length,
                                          tokenizer->query->encoding))) {
      token_length++;
      token_cursor += char_length;
      token_next_pointer = token_cursor;
      while (token_length < tokenizer->ngram_unit &&
             (char_length = grn_plugin_charlen(ctx, (char *)token_cursor, rest_length,
                                               tokenizer->query->encoding))) {
        if (ctype_cursor) {
          if (!tokenizer->ignore_blank && GRN_STR_ISBLANK(*ctype_cursor)) {
            break;
          }
          ctype_cursor++;
          if ((tokenizer->split_alpha == GRN_FALSE &&
              GRN_STR_CTYPE(*ctype_cursor) == GRN_CHAR_ALPHA) ||
              (tokenizer->split_digit == GRN_FALSE &&
              GRN_STR_CTYPE(*ctype_cursor) == GRN_CHAR_DIGIT) ||
              (tokenizer->split_symbol == GRN_FALSE &&
              GRN_STR_CTYPE(*ctype_cursor) == GRN_CHAR_SYMBOL)) {
            break;
          }
        }
        token_length++;
        token_cursor += char_length;
      }
      is_current_token_grouped = GRN_FALSE;
    }
  }

  if (is_current_token_grouped == GRN_FALSE) {
    if (tokenizer->is_token_grouped == GRN_FALSE && tokenizer->token_length > 1) {
      status |= GRN_TOKENIZER_TOKEN_OVERLAP;
    }
    if (token_length < tokenizer->ngram_unit) {
      status |= GRN_TOKENIZER_TOKEN_UNMATURED;
    }
  }

  tokenizer->is_token_grouped = is_current_token_grouped;
  tokenizer->next = token_next_pointer;

  tokenizer->token_top_position = token_top_position;
  tokenizer->token_length = token_length;
  tokenizer->token_tail_position = token_top_position + token_length - 1;

  if (token_top_pointer == token_cursor || tokenizer->next == string_end) {
    tokenizer->skip_size = 0;
    status |= GRN_TOKENIZER_TOKEN_LAST;
  } else {
    if (is_current_token_grouped) {
      tokenizer->skip_size = token_length;
    } else {
      tokenizer->skip_size = 1;
    }
  }
  if (token_cursor == string_end) {
    status |= GRN_TOKENIZER_TOKEN_REACH_END;
  }
  grn_tokenizer_token_push(ctx,
                           &(tokenizer->token),
                           (const char *)token_top_pointer,
                           token_cursor - token_top_pointer,
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
