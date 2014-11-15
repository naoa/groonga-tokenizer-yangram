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

typedef struct {
  grn_tokenizer_token token;
  grn_tokenizer_query *query;
  const unsigned char *next;
  const unsigned char *start;
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
  grn_bool use_vgram;
  grn_obj *vgram_table;
  grn_obj value;
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
                    const unsigned char *ctypes, int token_size)
{
  if (ctypes) {
    ctypes = ctypes + token_size;
  }
  if (ctypes) {
    if (is_token_group(tokenizer, ctypes)) {
      return GRN_TRUE;
    }
  }
  return GRN_FALSE;
}

/*
static grn_bool
is_next_token_blank(GNUC_UNUSED grn_ctx *ctx, GNUC_UNUSED grn_yangram_tokenizer *tokenizer,
                    const unsigned char *ctypes,
                    int token_size)
{
  if (ctypes) {
    ctypes = ctypes + token_size;
  }
  if (ctypes) {
    if (GRN_STR_ISBLANK(*--ctypes)) {
      return GRN_TRUE;
    }
  }
  return GRN_FALSE;
}
*/

/*
static grn_bool
is_token_all_same(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                  const unsigned char *token_top,
                  int token_size)
{
  unsigned int char_length;
  unsidned int rest_length;
  const unsigned char *token_before = token_top;

  int i = 0;

  if ((char_length = grn_plugin_charlen(ctx, (char *)token_top, rest_length,
                                       tokenizer->query->encoding))) {
    token_top += char_length;
    rest_length -= char_length;
    i = 1;
    while (i < token_size &&
      (char_length = grn_plugin_charlen(ctx, (char *)token_top, rest_length,
                                        tokenizer->query->encoding))) {
      if (token_top + char_length &&
          !memcmp(token_before, token_top, char_length)){
        break;
      }
      token_before = token_top;
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
*/

static grn_obj *
yangram_init(grn_ctx *ctx, int nargs, grn_obj **args, grn_user_data *user_data,
             unsigned short ngram_unit, grn_bool ignore_blank,
             grn_bool split_symbol, grn_bool split_alpha, grn_bool split_digit,
             grn_bool skip_overlap, grn_bool use_vgram)
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
  tokenizer->skip_overlap = skip_overlap;
  tokenizer->ignore_blank = ignore_blank;
  tokenizer->ngram_unit = ngram_unit;
  tokenizer->split_symbol = split_symbol;
  tokenizer->split_alpha = split_alpha;
  tokenizer->split_digit = split_digit;
  tokenizer->use_vgram = use_vgram;
  if (tokenizer->use_vgram) {
    const char *vgram_word_table_name_env;
    vgram_word_table_name_env = getenv("GRN_YANGRAM_VGRAM_WORD_TABLE_NAME");

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
  tokenizer->next = (const unsigned char *)normalized;
  tokenizer->start = tokenizer->next;
  tokenizer->end = tokenizer->next + normalized_length_in_bytes;
  tokenizer->rest_length = tokenizer->end - tokenizer->next;
  tokenizer->ctypes =
    grn_string_get_types(ctx, tokenizer->query->normalized_query);

  tokenizer->pushed_token_tail = NULL;
  tokenizer->ctypes_next = 0;

  GRN_TEXT_INIT(&(tokenizer->value), 0);

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
  grn_bool is_vgram = GRN_FALSE;
  grn_tokenizer_status status = 0;

  if (tokenizer->ctypes) {
    token_ctypes = tokenizer->ctypes + tokenizer->ctypes_next;
  } else {
    token_ctypes = NULL;
  }
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
    if (tokenizer->use_vgram) {
      grn_id id;
      id = grn_table_get(ctx, tokenizer->vgram_table,
                         (const char *)token_top, token_tail - token_top);
      if (id) {
        is_vgram = GRN_TRUE;
      }
    }
  }

  if (token_top == token_tail || token_next == string_end) {
    ctypes_skip_size = 0;
  } else {
    if (is_token_grouped) {
      ctypes_skip_size = token_size;
    } else {
      ctypes_skip_size = 1;
    }
  }

  if (tokenizer->use_vgram){
    if (is_vgram) {
      if (token_tail < string_end && !is_group_border(ctx, tokenizer,token_ctypes, token_size)) {
        char_length = grn_plugin_charlen(ctx, (char *)token_tail,
                                         tokenizer->rest_length,
                                         tokenizer->query->encoding);
        token_size++;
        token_tail += char_length;
      } else {
       //2文字のみのクエリの場合で伸ばせない場合は強制的に前方一致検索にするためLAST
       //最後の1文字トークンが残っているがREACH_ENDの場合はもともとGET時のみSKIPされる仕様
        if (token_tail == string_end && tokenizer->query->token_mode == GRN_TOKEN_GET) {
          //status |= GRN_TOKENIZER_TOKEN_UNMATURED;
          //status |= GRN_TOKENIZER_TOKEN_LAST;
          status |= GRN_TOKENIZER_TOKEN_FORCE_PREFIX;
        }
      }
    }
  }

  if (token_top == token_tail || token_next == string_end) {
    status |= GRN_TOKENIZER_TOKEN_LAST;
  }

  if (token_tail == string_end) {
    status |= GRN_TOKENIZER_TOKEN_REACH_END;
  }

  if (!is_token_grouped && token_size < tokenizer->ngram_unit) {
    status |= GRN_TOKENIZER_TOKEN_UNMATURED;
  }

  if (token_size) {
    if (tokenizer->skip_overlap &&
        is_token_all_blank(ctx, tokenizer, token_top, token_size)) {
      status |= GRN_TOKENIZER_TOKEN_SKIP_WITH_POSITION;
    }
  }

  if (tokenizer->pushed_token_tail &&
      token_top < tokenizer->pushed_token_tail) {
    status |= GRN_TOKENIZER_TOKEN_OVERLAP;
    if (tokenizer->skip_overlap &&
        !(status & GRN_TOKENIZER_TOKEN_REACH_END) &&
        !(status & GRN_TOKENIZER_TOKEN_SKIP_WITH_POSITION) &&
      tokenizer->query->token_mode == GRN_TOKEN_GET) {
      if (token_tail <= tokenizer->pushed_token_tail) {
        status |= GRN_TOKENIZER_TOKEN_SKIP;
      } else {
        if (!is_group_border(ctx, tokenizer, token_ctypes, token_size)) {
          status |= GRN_TOKENIZER_TOKEN_SKIP;
        }
      }
    }
  }

  if (!(status & GRN_TOKENIZER_TOKEN_SKIP) &&
      !(status & GRN_TOKENIZER_TOKEN_SKIP_WITH_POSITION)) {
    tokenizer->pushed_token_tail = token_tail;
  }

  tokenizer->next = token_next;
  tokenizer->rest_length = string_end - token_next;
  tokenizer->ctypes_next = tokenizer->ctypes_next + ctypes_skip_size;

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
  grn_obj_unlink(ctx, &(tokenizer->value));
  grn_tokenizer_query_close(ctx, tokenizer->query);
  grn_tokenizer_token_fin(ctx, &(tokenizer->token));
  GRN_PLUGIN_FREE(ctx,tokenizer);
  return NULL;
}

static grn_obj *
yabigram_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 0, 0, 0, 1, 0);
}

static grn_obj *
yabigram_ib_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 1, 0, 0, 0, 1, 0);
}

static grn_obj *
yabigram_sa_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 1, 1, 0, 1, 0);
}

static grn_obj *
yatrigram_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 3, 0, 0, 0, 0, 1, 0);
}

static grn_obj *
yatrigram_ib_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 3, 1, 0, 0, 0, 1, 0);
}

static grn_obj *
yatrigram_sa_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 3, 0, 1, 1, 0, 1, 0);
}

static grn_obj *
yavgram_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 0, 0, 0, 1, 1);
}

static grn_obj *
yavgram_sa_init(grn_ctx * ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  return yangram_init(ctx, nargs, args, user_data, 2, 0, 1, 1, 0, 1, 1);
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

  return ctx->rc;
}

grn_rc
GRN_PLUGIN_FIN(GNUC_UNUSED grn_ctx *ctx)
{

  return GRN_SUCCESS;
}
