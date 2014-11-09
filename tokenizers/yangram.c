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

#define VGRAM_WORD_TABLE_NAME "#vgram_words"
#define VGRAM_WORD_TABLE_NAME_MRN "@0023vgram_words"

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
  grn_bool use_vgram;
  grn_obj *vgram_table;
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
execute_token_filter(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                     GNUC_UNUSED const unsigned char *ctypes,
                     const unsigned char *token_top,
                     GNUC_UNUSED const unsigned char *token_tail,
                     int token_size)
{
  if (tokenizer->skip_overlap &&
      is_token_all_blank(ctx, tokenizer, token_top, token_size)) {
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
is_next_token_group(GNUC_UNUSED grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
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

static grn_bool
ignore_token_skip_overlap(grn_ctx *ctx, grn_yangram_tokenizer *tokenizer,
                          const unsigned char *ctypes,
                          GNUC_UNUSED unsigned int ctypes_skip_size,
                          GNUC_UNUSED const unsigned char *token_top,
                          const unsigned char *token_tail,
                          GNUC_UNUSED const unsigned char *token_next,
                          int token_size)
{
  if (token_tail > tokenizer->pushed_token_tail) {
    if (is_next_token_group(ctx, tokenizer, ctypes, token_size)) {
      return GRN_TRUE;
    }
  }

  return GRN_FALSE;
}

static grn_obj *
yangram_init(grn_ctx *ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  grn_tokenizer_query *query;
  unsigned int normalize_flags =
    GRN_STRING_WITH_TYPES |
    GRN_STRING_REMOVE_TOKENIZED_DELIMITER;

  const char *normalized;
  unsigned int normalized_length_in_bytes;
  grn_yangram_tokenizer *tokenizer;
  grn_obj *var;
  grn_bool skip_overlap = GRN_FALSE;
  grn_bool ignore_blank = GRN_FALSE;

  var = grn_plugin_proc_get_var(ctx, user_data, "skip_overlap", -1);
  if (GRN_TEXT_LEN(var) != 0) {
    skip_overlap = GRN_INT32_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "ignore_blank", -1);
  if (GRN_TEXT_LEN(var) != 0) {
    ignore_blank = GRN_INT32_VALUE(var);
  }

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

  var = grn_plugin_proc_get_var(ctx, user_data, "ngram_unit", -1);
  if (GRN_TEXT_LEN(var) != 0) {
    tokenizer->ngram_unit = GRN_INT32_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "split_symbol", -1);
  if (GRN_TEXT_LEN(var) != 0) {
    tokenizer->split_symbol = GRN_INT32_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "split_alpha", -1);
  if (GRN_TEXT_LEN(var) != 0) {
    tokenizer->split_alpha = GRN_INT32_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "split_digit", -1);
  if (GRN_TEXT_LEN(var) != 0) {
    tokenizer->split_digit = GRN_INT32_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "use_vgram", -1);
  if (GRN_TEXT_LEN(var) != 0) {
    tokenizer->use_vgram = GRN_INT32_VALUE(var);
    if (tokenizer->use_vgram) {
      tokenizer->vgram_table = grn_ctx_get(ctx,
                                           VGRAM_WORD_TABLE_NAME,
                                           strlen(VGRAM_WORD_TABLE_NAME));
      if (!tokenizer->vgram_table) {
        tokenizer->vgram_table = grn_ctx_get(ctx,
                                             VGRAM_WORD_TABLE_NAME_MRN,
                                             strlen(VGRAM_WORD_TABLE_NAME_MRN));
      }
      if (!tokenizer->vgram_table) {
         GRN_PLUGIN_ERROR(ctx, GRN_NO_MEMORY_AVAILABLE,
                          "[tokenizer][yangram] "
                          "couldn't open a vgram table");
         return NULL;
      }
    }
  }

  grn_string_get_normalized(ctx, tokenizer->query->normalized_query,
                            &normalized, &normalized_length_in_bytes,
                            NULL);
  tokenizer->next = (const unsigned char *)normalized;
  tokenizer->end = tokenizer->next + normalized_length_in_bytes;
  tokenizer->rest_length = tokenizer->end - tokenizer->next;
  tokenizer->ctypes =
    grn_string_get_types(ctx, tokenizer->query->normalized_query);

  tokenizer->pushed_token_tail = NULL;
  tokenizer->ctypes_next = 0;

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
  grn_tokenizer_status status = 0;
  grn_bool is_token_grouped = GRN_FALSE;
  const unsigned char *ctypes;
  unsigned int ctypes_skip_size;
  int char_length = 0;
  grn_bool is_vgram = GRN_TRUE;

  if (tokenizer->ctypes) {
    ctypes = tokenizer->ctypes + tokenizer->ctypes_next;
  } else {
    ctypes = NULL;
  }
  is_token_grouped = is_token_group(tokenizer, ctypes);

  if (is_token_grouped) {
    token_size = forward_grouped_token_tail(ctx, tokenizer, ctypes, &token_tail);
    token_next = token_tail;
  } else {
    token_size = forward_ngram_token_tail(ctx, tokenizer, ctypes, &token_tail);
    char_length = grn_plugin_charlen(ctx, (char *)token_next,
                                     tokenizer->rest_length,
                                     tokenizer->query->encoding);
    token_next += char_length;

    {
      grn_id id;
      id = grn_table_get(ctx, tokenizer->vgram_table,
                         (const char *)token_top, token_tail - token_top);

      if (id != GRN_ID_NIL) {
        const unsigned char *ctypes_vgram = ctypes + 1;
        char_length = grn_plugin_charlen(ctx, (char *)token_tail,
                                         tokenizer->rest_length,
                                         tokenizer->query->encoding);
        if (ctypes_vgram) {
          if (!tokenizer->ignore_blank && GRN_STR_ISBLANK(*ctypes_vgram)) {
            is_vgram = GRN_FALSE;
          }
          ctypes_vgram++;
          if ((tokenizer->split_alpha == GRN_FALSE &&
              GRN_STR_CTYPE(*ctypes_vgram) == GRN_CHAR_ALPHA) ||
              (tokenizer->split_digit == GRN_FALSE &&
              GRN_STR_CTYPE(*ctypes_vgram) == GRN_CHAR_DIGIT) ||
              (tokenizer->split_symbol == GRN_FALSE &&
              GRN_STR_CTYPE(*ctypes_vgram) == GRN_CHAR_SYMBOL)) {
            is_vgram = GRN_FALSE;
          }
        }
        if (is_vgram) {
          if (token_tail < string_end) {
            token_size++;
            token_tail += char_length;
          } else {
            //Vgram対象なのに文末で伸ばせなかったら文中では3文字になっている可能性があるため強制前方一致
            //この方法だとstatusにLASTがついてしまうので、最後の1文字トークンが追加されなくなってしまう。
            //force_prefixをつけるための特別なステータスが欲しい。
            status |= GRN_TOKENIZER_TOKEN_UNMATURED;
            status |= GRN_TOKENIZER_TOKEN_LAST;
          }
        }
      }

    }
  }

  if (token_top == token_tail || token_next == string_end) {
    ctypes_skip_size = 0;
    status |= GRN_TOKENIZER_TOKEN_LAST;
  } else {
    if (is_token_grouped) {
      ctypes_skip_size = token_size;
    } else {
      ctypes_skip_size = 1;
    }
  }

  if (token_tail == string_end) {
    status |= GRN_TOKENIZER_TOKEN_REACH_END;
  }

  if (!is_token_grouped && token_size < tokenizer->ngram_unit) {
    status |= GRN_TOKENIZER_TOKEN_UNMATURED;
  }

  if (token_size) {
    if (execute_token_filter(ctx, tokenizer, ctypes,
                             token_top, token_tail, token_size)) {
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
      if (!ignore_token_skip_overlap(ctx, tokenizer, ctypes, ctypes_skip_size,
                                     token_top, token_tail, token_next, token_size)) {
        status |= GRN_TOKENIZER_TOKEN_SKIP;
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
  grn_expr_var vars[10];

  grn_plugin_expr_var_init(ctx, &vars[0], NULL, -1);
  grn_plugin_expr_var_init(ctx, &vars[1], NULL, -1);
  grn_plugin_expr_var_init(ctx, &vars[2], NULL, -1);
  grn_plugin_expr_var_init(ctx, &vars[3], "ngram_unit", -1);
  grn_plugin_expr_var_init(ctx, &vars[4], "ignore_blank", -1);
  grn_plugin_expr_var_init(ctx, &vars[5], "split_symbol", -1);
  grn_plugin_expr_var_init(ctx, &vars[6], "split_alpha", -1);
  grn_plugin_expr_var_init(ctx, &vars[7], "split_digit", -1);
  grn_plugin_expr_var_init(ctx, &vars[8], "skip_overlap", -1);
  grn_plugin_expr_var_init(ctx, &vars[9], "use_vgram", -1);

  GRN_INT32_SET(ctx, &vars[3].value, 2);
  GRN_INT32_SET(ctx, &vars[4].value, 0);
  GRN_INT32_SET(ctx, &vars[5].value, 0);
  GRN_INT32_SET(ctx, &vars[6].value, 0);
  GRN_INT32_SET(ctx, &vars[7].value, 0);
  GRN_INT32_SET(ctx, &vars[8].value, 1);
  GRN_INT32_SET(ctx, &vars[9].value, 0);

  grn_proc_create(ctx, "TokenYaBigram", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  GRN_INT32_SET(ctx, &vars[4].value, 1);
  grn_proc_create(ctx, "TokenYaBigramIgnoreBlank", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  GRN_INT32_SET(ctx, &vars[4].value, 0);
  GRN_INT32_SET(ctx, &vars[5].value, 1);
  GRN_INT32_SET(ctx, &vars[6].value, 1);
  grn_proc_create(ctx, "TokenYaBigramSplitSymbolAlpha", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  GRN_INT32_SET(ctx, &vars[3].value, 3);
  GRN_INT32_SET(ctx, &vars[4].value, 0);
  GRN_INT32_SET(ctx, &vars[5].value, 0);
  GRN_INT32_SET(ctx, &vars[6].value, 0);
  grn_proc_create(ctx, "TokenYaTrigram", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  GRN_INT32_SET(ctx, &vars[4].value, 1);
  grn_proc_create(ctx, "TokenYaTrigramIgnoreBlank", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  GRN_INT32_SET(ctx, &vars[4].value, 0);
  GRN_INT32_SET(ctx, &vars[5].value, 1);
  GRN_INT32_SET(ctx, &vars[6].value, 1);
  grn_proc_create(ctx, "TokenYaTrigramSplitSymbolAlpha", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  GRN_INT32_SET(ctx, &vars[3].value, 2);
  GRN_INT32_SET(ctx, &vars[4].value, 0);
  GRN_INT32_SET(ctx, &vars[5].value, 0);
  GRN_INT32_SET(ctx, &vars[6].value, 0);
  GRN_INT32_SET(ctx, &vars[7].value, 0);
  GRN_INT32_SET(ctx, &vars[8].value, 1);
  GRN_INT32_SET(ctx, &vars[9].value, 1);

  grn_proc_create(ctx, "TokenYaVgram", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  GRN_INT32_SET(ctx, &vars[5].value, 1);
  GRN_INT32_SET(ctx, &vars[6].value, 1);
  grn_proc_create(ctx, "TokenYaVgramSplitSymbolAlpha", -1,
                  GRN_PROC_TOKENIZER,
                  yangram_init, yangram_next, yangram_fin, 10, vars);

  return ctx->rc;
}

grn_rc
GRN_PLUGIN_FIN(GNUC_UNUSED grn_ctx *ctx)
{

  return GRN_SUCCESS;
}
