# Yet another ngram tokenizer plugin for Groonga

## Tokenizer

原則、ビルトインのTokenBigramトークナイザー等と同様のルールで文字列をトークナイズします。
それに加え、以下の機能をカスタマイズしています。

### ``Skip Overlap``

* ``TokenYaBigram``
* ``TokenYaBigramIgnoreBlank``
* ``TokenYaBigramSplitSymbolAlpha``
* ``TokenYaBigramSplitDigit``
* ``TokenYaTrigram``
* ``TokenYaTrigramIgnoreBlank``
* ``TokenYaTrigramSplitSymbolAlpha``
* ``TokenYaTrigramSplitSymbolAlphaDigit``
* ``TokenYaHexgramSplitSymbolAlphaDigit``

検索時のみNgramのオーバーラップをスキップしてトークナイズします。  
検索時のトークン数を減らすことができ、検索処理の速度向上が見込めます。  

検索クエリに空白がなくても文書中の空白を1つまたぐフレーズがヒットすることがあります。

Groonga6.0.3で追加された環境変数``GRN_II_OVERLAP_TOKEN_SKIP_ENABLE``を利用した場合、Groonga本体側でトークンのレア度を考慮してオーバラップがスキップされます。トークナイザー内ではスキップされません。

### ``Variable Ngram``

* ``TokenYaVgram``
* ``TokenYaVgramSplitSymbolAlpha``

``vgram_words``テーブルのキーに含まれるBigramトークンを後ろに伸ばしてTrigramにします。
出現頻度が高いBigramトークンのみをあらかじめ``vgram_words``テーブルに格納しておくことで、キーの種類、キーサイズの増大を抑えつつ、検索速度向上に効果的なトークンのみをTrigramにすることができます。   
なお、検索クエリの末尾で後ろに伸ばすことができず、Trigramにする対象のBigramトークン単体で検索される場合は強制的に前方一致検索になります。  
この強制前方一致検索を有効にするためには、Groonga4.0.8以降が必要です。  
整合性を保つため、Vgram対象の語句を追加した場合は、インデックス再構築が必要です。  
これらのトークナイザーも上記同様に検索時のみNgramのオーバーラップをできるだけスキップしてトークナイズします。

テーブルは環境変数``GRN_VGRAM_WORD_TABLE_NAME``により変更することができます。

検索クエリに非アスキーのあとに空白やアスキーがでてきた場合、検索できないことがあります。

例えば、検索クエリが``"搬送 シート"``の空白を含むフレーズで、``搬送``がvgram対象の場合、前方一致で``搬送``から始まるトークンを探す必要がありますが、現状、末尾のトークン以外は正常に強制前方一致検索フラグが動きません。

これを動くようにするには今のところGroonga本体に以下のパッチを適用する必要があります。

```diff
diff --git a/lib/ii.c b/lib/ii.c
index c88c2e9..49c7640 100644
--- a/lib/ii.c
+++ b/lib/ii.c
@@ -6754,7 +6754,7 @@ token_candidate_build(grn_ctx *ctx, grn_obj *lexicon, grn_ii *ii,
       case GRN_TOKEN_CURSOR_DOING :
         key = _grn_table_key(ctx, lexicon, node->tid, &size);
         ti = token_info_open(ctx, lexicon, ii, key, size, node->pos,
-                             EX_NONE, NULL);
+                             node->ef, NULL);
         break;
       case GRN_TOKEN_CURSOR_DONE :
         if (node->tid) {
@@ -6862,8 +6862,13 @@ token_info_build(grn_ctx *ctx, grn_obj *lexicon, grn_ii *ii, const char *string,
     switch (token_cursor->status) {
     case GRN_TOKEN_CURSOR_DOING :
       key = _grn_table_key(ctx, lexicon, tid, &size);
-      ti = token_info_open(ctx, lexicon, ii, key, size, token_cursor->pos,
-                           ef & EX_SUFFIX, NULL);
+      if (token_cursor->force_prefix) {
+        ti = token_info_open(ctx, lexicon, ii, key, size, token_cursor->pos,
+                             ef, NULL);
+      } else {
+        ti = token_info_open(ctx, lexicon, ii, key, size, token_cursor->pos,
+                             ef & EX_SUFFIX, NULL);
+      }
       break;
     case GRN_TOKEN_CURSOR_DONE :
       ti = token_info_open(ctx, lexicon, ii, (const char *)token_cursor->curr,
@@ -6902,7 +6907,7 @@ token_info_build(grn_ctx *ctx, grn_obj *lexicon, grn_ii *ii, const char *string,
       case GRN_TOKEN_CURSOR_DOING :
         key = _grn_table_key(ctx, lexicon, tid, &size);
         ti = token_info_open(ctx, lexicon, ii, key, size, token_cursor->pos,
-                             EX_NONE, NULL);
+                             ef, NULL);
         break;
       case GRN_TOKEN_CURSOR_DONE :

```

* ``TokenYaVgramBoth``
* ``TokenYaVgramBothSplitSymbolAlpha``

１つ後のBigramトークンが``vgram_words``テーブルのキーに含まれるBigramトークンであった場合もBigramトークンを後ろに伸ばしてTrigramにします。こうすると、検索クエリ末尾の場合も末尾から1つ前に伸ばしたトークンが採用されるため、検索速度のさらなる高速化が望めます。なお、このトークナイザーでは、検索クエリ末尾の場合に次のBigramトークンを判定することができないため、``vgram_words``テーブルのキーに含まれないトークンであっても全て強制前方一致検索になります。ただ、前方一致検索になったとしても、トライのキー探索にかかる時間 << ポスティングリスト探索にかかる時間のため、こちらのほうが速度的メリットが大きいことがあります。

なお、Vgramトークナイザーは、文書サイズが大きくBigramトークンのポスティングリストが非常に長くなっている場合に、検索クエリが3文字以上の場合に高速化できるということであって、検索クエリが2文字以下の場合の高速化には繋がりません。キーがばらけたとしても、2文字で前方一致検索して全部のリストを辿るのであれば結局同じかむしろ若干遅くなります。

* Wikipedia(ja)で出現頻度上位2000個のbigramトークンを登録して1000回検索

|                       | TokenYaVgramBoth | TokenYaVgram | TokenTrigram | TokenBigram |
|:----------------------|-----------------:|-------------:|-------------:|------------:|
| Hits                  | 112378           | 112378       | 112378       | 112378      |
| Searching time (Avg)  | 0.0065 sec       | 0.0166 sec   | 0.0126 sec   | 0.0444 sec  |
| Offline Indexing time | 1818 sec         | 1592 sec     | 2150 sec     | 1449 sec    |
| Index size            | 8.566GiB         | 8.474GiB     | 9.009GiB     | 7.580GiB    |
| Key sum               | 8560779          | 7425198      | 28691883     | 5767474     |
| Key size              | 200.047MiB       | 172.047MiB   | 684.047MiB   | 136.047MiB  |

```bash
register tokenizers/yangram
[[0,0.0,0.0],true]
table_create vgram_words TABLE_HASH_KEY ShortText
[[0,0.0,0.0],true]
load --table vgram_words
[
{"_key": "処理"}
]
[[0,0.0,0.0],1]
tokenize TokenYaVgramBoth "画像処理装置" NormalizerAuto --mode ADD
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "画像",
      "position": 0
    },
    {
      "value": "像処理",
      "position": 1
    },
    {
      "value": "処理装",
      "position": 2
    },
    {
      "value": "理装",
      "position": 3
    },
    {
      "value": "装置",
      "position": 4
    },
    {
      "value": "置",
      "position": 5
    }
  ]
]
tokenize TokenYaVgramBoth "画像処理装置" NormalizerAuto --mode GET
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "画像",
      "position": 0
    },
    {
      "value": "処理装",
      "position": 2
    },
    {
      "value": "装置",
      "position": 4
    }
  ]
]
```

出現頻度の高いBigramトークンの抽出は、以下の``token_count``コマンドプラグインを使うと便利です。日本語のBigramトークンのみを出現頻度の多い順でソートして出力してくれます。

https://github.com/naoa/groonga-command-token-count

```
token_count Terms document_index --token_size 2 --ctype ja --threshold 10000000
```

### ``Known phrase``

``known_phrases``テーブルのキーはひとまとまりにしてトークナイズします。上記のトークナイザーすべてで``known_phrases``テーブルがあるときのみ有効になります。  
見出しタグや必ず切り出したい既知のフレーズを登録しておくことにより、検索ノイズの低減や頻出トークン数の低減を図ることができます。
パトリシアトライのLCPサーチを利用しているため、``known_phrases``テーブルのキーの種類は``TABLE_PAT_KEY``である必要があります。  
整合性を保つため、フレーズ対象の語句を追加した場合は、インデックス再構築が必要です。

テーブルは環境変数``GRN_KNOWN_PHRASE_TABLE_NAME``により変更することができます。

* Wikipedia(ja)で検索対象のカテゴリ1000件をフレーズ登録して検索

|                       | TokenYaBigram (Phraseあり) | TokenYaBigram (Phraseなし) |
|:----------------------|---------------------------:|---------------------------:|
| Hits                  | 112265                     | 112378                     |
| Searching time (Avg)  | 0.0014 sec                 | 0.0325 sec                 |
| Offline Indexing time | 2180 sec                   | 1378 sec                   |

登録したフレーズはBigramトークンに比べて出現頻度が極端に小さいので、そのフレーズで検索するときは非常に高速になっています。少しずれている理由は、"日本のコーラスグループ"と"コーラスグループ"など、包含関係にあるフレーズが一方のフレーズで切り出されており他方のフレーズではヒットしないからです。

```bash
table_create known_phrases TABLE_PAT_KEY ShortText --normalizer NormalizerAuto
[[0,0.0,0.0],true]
load --table known_phrases
[
{"_key": "【請求項１】"}
]
[[0,0.0,0.0],1]
tokenize TokenYaBigram "【請求項１】検索装置" NormalizerAuto --mode ADD
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "【請求項1】",
      "position": 0
    },
    {
      "value": "検索",
      "position": 1
    },
    {
      "value": "索装",
      "position": 2
    },
    {
      "value": "装置",
      "position": 3
    },
    {
      "value": "置",
      "position": 4
    }
  ]
]
```

### Source install

Build this tokenizer.

    % sh autogen.sh
    % ./configure
    % make
    % sudo make install

## Dependencies

* Groonga >= 4.0.8

Install ``groonga-devel`` in CentOS/Fedora. Install ``libgroonga-dev`` in Debian/Ubuntu.

See http://groonga.org/docs/install.html

## Usage

Firstly, register `tokenizers/yangram`

Groonga:

    % groonga db
    > register tokenizers/yangram
    > table_create Diaries TABLE_HASH_KEY INT32
    > column_create Diaries body COLUMN_SCALAR TEXT
    > table_create Terms TABLE_PAT_KEY ShortText \
    >   --default_tokenizer TokenYaBigram
    > column_create Terms diaries_body COLUMN_INDEX|WITH_POSITION Diaries body

Mroonga:

    mysql> use db;
    mysql> select mroonga_command("register tokenizers/yangram");
    mysql> CREATE TABLE `Diaries` (
        -> id INT NOT NULL,
        -> body TEXT NOT NULL,
        -> PRIMARY KEY (id) USING HASH,
        -> FULLTEXT INDEX (body) COMMENT 'parser "TokenYaBigram"'
        -> ) ENGINE=Mroonga DEFAULT CHARSET=utf8;

Rroonga:

    irb --simple-prompt -rubygems -rgroonga
    >> Groonga::Context.default_options = {:encoding => :utf8}   
    >> Groonga::Database.create(:path => "/tmp/db")
    >> Groonga::Plugin.register(:path => "tokenizers/yangram.so")
    >> Groonga::Schema.create_table("Diaries",
    ?>                              :type => :hash,
    ?>                              :key_type => :integer32) do |table|
    ?>   table.text("body")
    >> end
    >> Groonga::Schema.create_table("Terms",
    ?>                              :type => :patricia_trie,
    ?>                              :normalizer => :NormalizerAuto,
    ?>                              :default_tokenizer => "TokenYaBigram") do |table|
    ?>   table.index("Diaries.body")
    >> end
    
## Author

* Naoya Murakami <naoya@createfield.com>

## License

LGPL 2.1. See COPYING for details.

This is programmed based on the Groonga ngram tokenizer.  
https://github.com/groonga/groonga/blob/master/lib/token.c

This program is the same license as Groonga.

The Groonga ngram tokenizer is povided by ``Copyright(C) 2009-2012 Brazil``
