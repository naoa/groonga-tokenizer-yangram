# Yet another ngram tokenizer plugin for Groonga

このトークナイザープラグインは、原則、ビルトインのTokenBigramトークナイザー等と同様のルールで文字列をトークナイズします。
それに加えストップワードやオーバーラップスキップ、トークンフィルターなどいくつかの機能を追加しています。  
トークナイザープラグインの登録には、``register``コマンドを利用します。

```
register tokenizer/yangram
```

その後、``yangram_register``コマンドを使って必要な機能のオプションを指定してトークナイザーを登録します。

## yangram_register

* Summary

``yangram_register``コマンドは、TokenYaNgram系トークナイザーを登録します。

``TokenYa``の後に以下のオプションに応じた名前が追加されたトークナイザーが1つ登録されます。
``Split``、``Skip``、``Filter``系を複数指定した場合は、先頭にのみ、それらの名前が追加されます。

``Skip``は、検索時のみトークンをスキップします。``Filter``は、検索時、更新時の両方でトークンを除去します。

* Example

```
yangram_register --ngram_unit 2 --ignore_blank 0 \
  --split_symbol 1 --split_alpha 1 --split_digit 0 \
  --skip_overlap 1 --skip_stopword 1 \
  --filter_combhira 1 --filter_combkata 1 \
  --filter_length 0 --filter_stoptable "" \
  --stem_snowball en
[
  [
    0,
    0.0,
    0.0
  ],
  "TokenYaBigramSplitSymbolAlphaSkipOverlapStopwordFilterCombhiraCombkataStemSnowball"
]
```

* Parameters

|Args|Input|Added_name|Description|
|:---|-----|----------|-----------|
|ngram_unit|[1-10]|[Uni/Bi/Tri/Quad/Pent/<BR>Hex/Hept/Oct/Non/Dec+gram]|NgramのNの値|
|ignore_blank|[0-1]|IgnoreBlank|空白区切りを無視|
|split_symbol|[0-1]|SplitSymbol|ノーマライザー使用時でも<BR>記号を分割|
|split_alpha|[0-1]|SplitAlpha|ノーマライザー使用時でも<BR>アルファベットを分割|
|split_digit|[0-1]|SplitDigit|ノーマライザー使用時でも<BR>数値を分割|
|skip_overlap|[0-1]|SkipOverlap|検索時のみ<BR>オーバーラップしている<BR>トークンをスキップ|
|skip_stopword|[0-1]|SkipStopword|検索時のみ<BR>ストップワードカラムが<BR>trueのトークンをスキップ|
|filter_combhira|[0-1]|FilterCombhira|検索時、更新時において<BR>[ひらがな+その他の字種]の<BR>組み合わせのトークンを除去|
|filter_combkata|[0-1]|FilterCombkata|検索時、更新時において<BR>[カタカナ+漢字]の<BR>組み合わせのトークンを除去|
|filter_length|[0-]|FilterLength|検索時、更新時において<BR>指定バイトを超える<BR>トークンを除去|
|filter_stoptable|[table_name]|FilterStoptable|検索時、更新時において<BR>指定したテーブルのキーに<BR>一致するトークンを除去|
|stem_snowball|[en/etc]|StemSnowball|検索時、更新時において<BR>Snowball stemmerを使って<BR>トークンをステミング|

* Return value

```
[HEADER, tokenizer_name]
```

``tokenizer_name``

登録されたトークナイザ名が出力されます。

### SkipOverlap

検索時のみNgramのオーバーラップをスキップしてトークナイズします。  
検索時のトークン数を減らすことができ、検索処理の速度向上が見込めます。  

トークンの末尾が最終まで到達した場合(``GRN_TOKENIZER_TOKEN_REACH_END``)、
アルファベットや記号などトークンがグループ化される字種境界および次のトークンが
フィルターされる場合ではスキップされません。
すなわち、検索クエリ末尾や字種境界では、オーバーラップしているトークンが含まれます。
これはNgramのNに満たないトークンを含めてしまうと検索性能が劣化するため、
あえてスキップしないようにしています。

オーバーラップスキップを有効にすると、通常のTokenBigram等と異なり空白が含まれた状態でトークナイズされます。これはオーバーラップをスキップすると空白の有無をうまく区別することができないためです。このため、通常のTokenBigram等よりも若干インデックスサイズが増えます。なお、空白のみのトークンは除去されます。

* Wikipedia(ja)で1000回検索した場合の検索速度差とヒット件数差

|                       | TokenYaBigramSkipOverlap | TokenBigram |
|:----------------------|-------------------------:|------------:|
| Hits                  | 112378                   | 112378      |
| Searching time (Avg)  | 0.0325 sec               | 0.0508 sec  |
| Offline Indexing time | 1224 sec                 | 1200 sec    |
| Index size            | 7.898GiB                 | 7.580GiB    |


|                       | TokenYaTrigramSkipOverlap | TokenTrigram |
|:----------------------|--------------------------:|-------------:|
| Hits                  | 112378                    | 112378       |
| Searching time (Avg)  | 0.0063 sec                | 0.0146 sec   |
| Offline Indexing time | 2293 sec                  | 2333 sec     |
| Index size            | 9.275GiB                  | 9.009GiB     |

* ADD mode

```
tokenize TokenYaBigramSkipOverlap "今日は雨だな" NormalizerAuto --mode ADD
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
tokenize TokenYaBigramSkipOverlap "今日は雨だな" NormalizerAuto --mode GET
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

### SkipStopword

``SkipStopword``の機能を有効にするには、語彙表に``@stopword``という名前のカラムを作る必要があります。

```
column_create <lexicon_name> @stopword COLUMN_SCALAR Bool
```

検索時のみ``@stopword``のカラムが``true``となっているキーのトークンがスキップされます。検索速度に影響が大きく、
検索精度にはあまり影響のないキーを取捨選択して検索から除外することができます。インデックス更新からは除外されません。

```
load --table <lexicon_name>
[
{"_key": "the", "@stopword": true }
]
```

### FilterCombhira/FilterCombkata

検索時、追加時の両方で``ひらがな1文字+他の字種``または``カタカナ1文字+漢字``の組み合わせで
始まるトークンを除去します。
``ひらがな1文字+他の字種``、``カタカナ1文字+漢字``で始まる組み合わせは検索クエリとして入力され
ることは少ないため転置索引に含めなくても影響が低いという観点によるものです。
転置索引の容量を削減させることができます。

ただし、以下の除外文字が2文字目にでてくる場合は除去されません。ひらがなorカタカナ+漢字1字で検索
したいことがありそうなためです。

* 除外文字

```
    "段", "行", "列", "組", "号", "回", "連", "式", "系",
    "型", "形", "変", "長", "短", "音", "階", "字"
```

* Wikipedia(ja)で1000回検索した場合の検索速度差とヒット件数差

|                       | TokenYaBigram<BR>FilterCombhiraCombkata | TokenYaBigramSkipOverlap<BR>FilterCombhiraCombkata |
|:----------------------|------------------------------:|------------------------------------:|
| Hits                  | 110549                        | 110748                              |
| Searching time (Avg)  | 0.0471 sec                    | 0.0376 sec                          |
| Offline Indexing time | 1312 sec                      | 1200 sec                            |
| Index size            | 6.940GiB                      | 6.940GiB                            |


* ADD mode / GET mode

```
tokenize TokenYaBigramFilterCombhiraCombkata "今日は雨だ" NormalizerAuto --mode GET
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
      "value": "雨だ",
      "position": 2
    }
  ]
]
```
### FilterLength

検索時、追加時の両方でトークナイザー登録時に指定したバイト数を超えるトークンを除去します。
無駄な長いキーによる語彙表のキーサイズの逼迫を防ぐことができます。

```
yangram_register --filter_length 5
[[0,0.0,0.0],"TokenYaBigramFilterLength"]
tokenize TokenYaBigramFilterLength "123456 digit" NormalizerAuto
[[0,0.0,0.0],[{"value":"digit","position":0}]]
```

### FilterStoptable

``FilterStoptable``を使うには、あらかじめ除外対象の語句が格納されたテーブルを作る必要があります。

```
yangram_register --filter_stoptable stopwords
[[0,0.0,0.0],"TokenYaBigramFilterStoptable"]
table_create stopwords TABLE_HASH_KEY ShortText
[[0,0.0,0.0],true]
load --table stopwords
[
{"_key": "this"},
{"_key": "a"}
]
[[0,0.0,0.0],2]
```

検索時、追加時の両方でテーブルのキーと一致するトークンを除去します。
検索速度に影響が大きく検索精度にあまり影響ないキーをあらかじめ設定することにより、検索速度の向上および転置索引のサイズを抑えることができます。

```
tokenize TokenYaBigramFilterStoptable "This is a pen" NormalizerAuto
[[0,0.0,0.0],[{"value":"is","position":0},{"value":"pen","position":1}]]
```

### StemSnowball

検索時、追加時の両方で[Snowball stemmer](http://snowball.tartarus.org/)を使ってステミングします。

英語などのトークンの語幹を抽出します。
複数形や過去形などの活用形の語尾を所定の規則に沿って切除します。
Snowball stemmerが対応している以下の言語を使うことができます。

| Desctiption           | Arg                   |
|:----------------------|:----------------------|
| 英語                  | english, en, eng      |
| 英語(porter)          | porter, por           |
| デンマーク語          | danish, da, dan       |
| オランダ語            | dutch, nl, nld, dut   |
| フィンランド語        | finnish, fi, fin      |
| フランス語            | french, fr, fra, fre  |
| ドイツ語              | german, de, deu, ger  |
| イタリア語            | italian, it, ita      |
| ノルウェー語          | norweigian, no, nor   |
| ポルトガル語          | potuguese, pt         |
| スペイン語            | spanish, es, esl, spa |
| スウェーデン語        | swedish, sv, swe      |

* ADD mode / GET mode

```
tokenize TokenYaBigramStemSnowball "There are cars" NormalizerAuto
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "there",
      "position": 0
    },
    {
      "value": "are",
      "position": 1
    },
    {
      "value": "car",
      "position": 2
    }
  ]
]
```

## Install

Install ``groonga-tokenizer-yangram`` package:

### CentOS

* CentOS6

```
% sudo yum localinstall -y http://packages.createfield.com/centos/6/groonga-tokenizer-yangram-1.0.0-1.el6.x86_64.rpm
```

* CentOS7

```
% sudo yum localinstall -y http://packages.createfield.com/centos/7/groonga-tokenizer-yangram-1.0.0-1.el7.centos.x86_64.rpm
```

### Fedora

* Fedora 20

```
% sudo yum localinstall -y http://packages.createfield.com/fedora/20/groonga-tokenizer-yangram-1.0.0-1.fc20.x86_64.rpm
```

### Debian GNU/LINUX

* wheezy

```
% wget http://packages.createfield.com/debian/wheezy/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.0-1_amd64.deb
```

* jessie

```
% wget http://packages.createfield.com/debian/jessie/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.0-1_amd64.deb
```

### Ubuntu

* precise

```
% wget http://packages.createfield.com/ubuntu/precise/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.0-1_amd64.deb
```

* trusty

```
% wget http://packages.createfield.com/ubuntu/trusty/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.0-1_amd64.deb
```

### Source install

Build this tokenizer.

    % sh autogen.sh
    % ./configure
    % make
    % sudo make install

## Dependencies

* Groonga >= 4.0.5

Install ``groonga-devel`` in CentOS/Fedora. Install ``libgroonga-dev`` in Debian/Ubutu.

See http://groonga.org/docs/install.html

## Usage

Firstly, register `tokenizers/yangram`

Groonga:

    % groonga db
    > register tokenizers/yangram
    > yangram_register 2 --skip_overlap 1 --skip_stopword 1
    > table_create Diaries TABLE_HASH_KEY INT32
    > column_create Diaries body COLUMN_SCALAR TEXT
    > table_create Terms TABLE_PAT_KEY ShortText \
    >   --default_tokenizer TokenYaBigramSkipOverlapStopword
    > column_create Terms diaries_body COLUMN_INDEX|WITH_POSITION Diaries body
    > column_create Terms @stopword COLUMN_SCALAR Bool

Mroonga:

    mysql> use db;
    mysql> select mroonga_command("register tokenizers/yangram");
    mysql> select mroonga_command("yangram_register 2 --skip_overlap 1 --skip_stopword 1");
    mysql> CREATE TABLE `Diaries` (
        -> id INT NOT NULL,
        -> body TEXT NOT NULL,
        -> PRIMARY KEY (id) USING HASH,
        -> FULLTEXT INDEX (body) COMMENT 'parser "TokenYaBigramSkipOverlapStopword"'
        -> ) ENGINE=mroonga DEFAULT CHARSET=utf8;
    mysql> select mroonga_command("column_create Diaries-body @stopword COLUMN_SCALAR Bool");

Rroonga:

    irb --simple-prompt -rubygems -rgroonga
    >> Groonga::Context.default_options = {:encoding => :utf8}   
    >> context = Groonga::Context.new
    >> Groonga::Database.create(:path => "/tmp/db", :context => context)
    >> context.register_plugin(:path => "tokenizers/yangram.so")
    >> context.execute_command("yangram_register 2 --skip_overlap 1 --skip_stopword 1")
    >> Groonga::Schema.create_table("Diaries",
    ?>                              :type => :hash,
    ?>                              :key_type => :integer32,
    ?>                              :context => context) do |table|
    ?>   table.text("body")
    >> end
    >> Groonga::Schema.create_table("Terms",
    ?>                              :type => :patricia_trie,
    ?>                              :normalizer => :NormalizerAuto,
    ?>                              :default_tokenizer => "TokenYaBigramSkipOverlapStopword",
    ?>                              :context => context) do |table|
    ?>   table.index("Diaries.body")
    ?>   table.bool("@stopword")
    >> end
    
## Author

* Naoya Murakami <naoya@createfield.com>

## License

LGPL 2.1. See COPYING for details.

This is programmed based on the Groonga ngram tokenizer.  
https://github.com/groonga/groonga/blob/master/lib/token.c

This program is the same license as Groonga.

The Groonga ngram tokenizer is povided by ``Copyright(C) 2009-2012 Brazil``
