# Yet another ngram tokenizer plugin for Groonga

## Tokenizer

原則、ビルトインのTokenBigramトークナイザー等と同様のルールで文字列をトークナイズします。
それに加え、以下の機能をカスタマイズしています。

### Overlap Skip

* ``TokenYaBigram``
* ``TokenYaBigramIgnoreBlank``
* ``TokenYaBigramSplitSymbolAlpha``
* ``TokenYaTrigram``
* ``TokenYaTrigramIgnoreBlank``
* ``TokenYaTrigramSplitSymbolAlpha``

検索時のみNgramのオーバーラップをスキップしてトークナイズします。  
検索時のトークン数を減らすことができ、検索処理の速度向上が見込めます。  

トークンの末尾が最終まで到達した場合(``GRN_TOKENIZER_TOKEN_REACH_END``)、
アルファベットや記号などトークンがグループ化される字種境界および次のトークンが
フィルターされる場合ではスキップされません。
すなわち、検索クエリ末尾や字種境界では、オーバーラップしているトークンが含まれます。
これはNgramのNに満たないトークンを含めてしまうと検索性能が劣化するため、
あえてスキップしないようにしています。

オーバーラップスキップを有効にし、且つ、``IgnoreBlank``が有効でない場合、通常のTokenBigram等と異なり空白が含まれた状態でトークナイズされます。これはオーバーラップをスキップすると空白の有無をうまく区別することができないためです。このため、通常のTokenBigram等よりも若干インデックスサイズが増えます。なお、空白のみのトークンは除去されます。

* Wikipedia(ja)で1000回検索した場合の検索速度差とヒット件数差

|                       | TokenYaBigram            | TokenBigram |
|:----------------------|-------------------------:|------------:|
| Hits                  | 112378                   | 112378      |
| Searching time (Avg)  | 0.0325 sec               | 0.0508 sec  |
| Offline Indexing time | 1224 sec                 | 1200 sec    |
| Index size            | 7.898GiB                 | 7.580GiB    |


|                       | TokenYaTrigram            | TokenTrigram |
|:----------------------|--------------------------:|-------------:|
| Hits                  | 112378                    | 112378       |
| Searching time (Avg)  | 0.0063 sec                | 0.0146 sec   |
| Offline Indexing time | 2293 sec                  | 2333 sec     |
| Index size            | 9.275GiB                  | 9.009GiB     |

* ADD mode

```
tokenize TokenYaBigram "今日は雨だな" NormalizerAuto --mode ADD
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "今日",
      "position": 0
    },
    {
      "value": "日は",
      "position": 1
    },
    {
      "value": "は雨",
      "position": 2
    },
    {
      "value": "雨だ",
      "position": 3
    },
    {
      "value": "だな",
      "position": 4
    },
    {
      "value": "な",
      "position": 5
    }
  ]
]

```

* GET mode

```
tokenize TokenYaBigram "今日は雨だな" NormalizerAuto --mode GET
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "今日",
      "position": 0
    },
    {
      "value": "は雨",
      "position": 2
    },
    {
      "value": "だな",
      "position": 4
    }
  ]
]
```

### Variable Ngram

* ``TokenYaVgram``
* ``TokenYaVgramSplitSymbolAlpha``

検索時、更新時において``vgram_words``テーブルのキーに含まれるBigramトークンを前に伸ばしてTrigramにします。
出現頻度が高いBigramトークンのみをあらかじめ``vgram_words``テーブルに格納しておくことで、キーの種類、キーサイズの増大を抑えつつ、検索速度向上に効果的なトークンのみをTrigramにすることができます。   
なお、検索クエリの末尾で後ろに伸ばすことができず、Trigramにする対象のBigramトークン単体で検索される場合は強制的に前方一致検索になります。  
Groonga4.0.7時点では、この強制前方一致検索を有効にするには、Groonga本体にパッチを当てる必要があります。  
整合性を保つため、Vgram対象の語句を追加した場合は、インデックス再構築が必要です。  
これらのトークナイザーも上記同様に検索時のみNgramのオーバーラップをできるだけスキップしてトークナイズします。

テーブルは環境変数``GRN_VGRAM_WORD_TABLE_NAME``により変更することができます。

* Wikipedia(ja)で出現頻度上位2000個のbigramトークンを登録して1000回検索

|                       | TokenYaVgram | TokenTrigram | TokenBigram |
|:----------------------|-------------:|-------------:|------------:|
| Hits                  | 112378       | 112378       | 112378      |
| Searching time (Avg)  | 0.0166 sec   | 0.0126 sec   | 0.0444 sec  |
| Offline Indexing time | 1592 sec     | 2150 sec     | 1449 sec    |
| Index size            | 8.474GiB     | 9.009GiB     | 7.580GiB    |
| Key sum               | 7425198      | 28691883     | 5767474     |
| Key size              | 172.047MiB   | 684.047MiB   | 136.047MiB  |

```
table_create vgram_words TABLE_HASH_KEY ShortText
[[0,0.0,0.0],true]
load --table vgram_words
[
{"_key": "画像"}
]
[[0,0.0,0.0],1]
tokenize TokenYaVgram "画像処理" NormalizerAuto --mode ADD
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "画像処",
      "position": 0
    },
    {
      "value": "像処",
      "position": 1
    },
    {
      "value": "処理",
      "position": 2
    },
    {
      "value": "理",
      "position": 3
    }
  ]
]
tokenize TokenYaVgram "画像処理" NormalizerAuto --mode GET
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "画像処",
      "position": 0
    },
    {
      "value": "処理",
      "position": 2
    }
  ]
]
```

出現頻度の高いBigramトークンの抽出は、以下の``token_count``コマンドプラグインを使うと便利です。日本語のBigramトークンのみを出現頻度の多い順でソートして出力してくれます。

https://github.com/naoa/groonga-command-token-count

```
token_count Terms document_index --token_size 2 --ctype ja --threshold 10000000
```

* Groongaへのパッチ

```diff
diff --git a/lib/ii.c b/lib/ii.c
index e342c73..c8bd83d 100644
--- a/lib/ii.c
+++ b/lib/ii.c
@@ -5431,6 +5431,7 @@ token_info_build(grn_ctx *ctx, grn_obj *lexicon, grn_ii *ii, const char *string,
     tis[(*n)++] = ti;
     while (token_cursor->status == GRN_TOKEN_DOING) {
       tid = grn_token_cursor_next(ctx, token_cursor);
+      if (token_cursor->force_prefix) { ef |= EX_PREFIX; }
       switch (token_cursor->status) {
       case GRN_TOKEN_DONE_SKIP :
         continue;
```

```diff
diff --git a/include/groonga/tokenizer.h b/include/groonga/tokenizer.h
index 400166d..7908af6 100644
--- a/include/groonga/tokenizer.h
+++ b/include/groonga/tokenizer.h
@@ -183,6 +183,8 @@ typedef unsigned int grn_tokenizer_status;
 #define GRN_TOKENIZER_TOKEN_SKIP               (0x01L<<4)
 /* GRN_TOKENIZER_TOKEN_SKIP_WITH_POSITION means that the token and postion is skipped */
 #define GRN_TOKENIZER_TOKEN_SKIP_WITH_POSITION (0x01L<<5)
+/* GRN_TOKENIZER_TOKEN_FORCE_PREIX that the token is used common prefix search */
+#define GRN_TOKENIZER_TOKEN_FORCE_PREFIX       (0x01L<<6)

 /*
  * GRN_TOKENIZER_CONTINUE and GRN_TOKENIZER_LAST are deprecated. They
```

```diff
diff --git a/lib/token_cursor.c b/lib/token_cursor.c
index b12217d..1ffa726 100644
--- a/lib/token_cursor.c
+++ b/lib/token_cursor.c
@@ -212,6 +212,7 @@ grn_token_cursor_next(grn_ctx *ctx, grn_token_cursor *token_cursor)
         }
       }
 #undef SKIP_FLAGS
+      if (status & GRN_TOKENIZER_TOKEN_FORCE_PREFIX) { token_cursor->force_prefix = 1; }
       if (token_cursor->curr_size == 0) {
         char tokenizer_name[GRN_TABLE_MAX_KEY_SIZE];
         int tokenizer_name_length;
```

## Install

Install ``groonga-tokenizer-yangram`` package:

以下のパッケージにはまだTokenYaVgramは含まれていません。検証後、問題なければ作ります。

### CentOS

* CentOS6

```
% sudo yum localinstall -y http://packages.createfield.com/centos/6/groonga-tokenizer-yangram-1.0.1-1.el6.x86_64.rpm
```

* CentOS7

```
% sudo yum localinstall -y http://packages.createfield.com/centos/7/groonga-tokenizer-yangram-1.0.1-1.el7.centos.x86_64.rpm
```

### Fedora

* Fedora 20

```
% sudo yum localinstall -y http://packages.createfield.com/fedora/20/groonga-tokenizer-yangram-1.0.1-1.fc20.x86_64.rpm
```

* Fedora 21

```
% sudo yum localinstall -y http://packages.createfield.com/fedora/21/groonga-tokenizer-yangram-1.0.1-1.fc21.x86_64.rpm
```

### Debian GNU/LINUX

* wheezy

```
% wget http://packages.createfield.com/debian/wheezy/groonga-tokenizer-yangram_1.0.1-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.1-1_amd64.deb
```

* jessie

```
% wget http://packages.createfield.com/debian/jessie/groonga-tokenizer-yangram_1.0.1-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.1-1_amd64.deb
```

### Ubuntu

* precise

```
% wget http://packages.createfield.com/ubuntu/precise/groonga-tokenizer-yangram_1.0.1-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.1-1_amd64.deb
```

* trusty

```
% wget http://packages.createfield.com/ubuntu/trusty/groonga-tokenizer-yangram_1.0.1-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.1-1_amd64.deb
```

### Source install

Build this tokenizer.

    % sh autogen.sh
    % ./configure
    % make
    % sudo make install

## Dependencies

* Groonga >= 4.0.7

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
