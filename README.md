# Yet another ngram tokenizer plugin for Groonga

## Tokenizer

* ``TokenYaBigram``
* ``TokenYaBigramSplitSymbolAlpha``
* ``TokenYaTrigram``
* ``TokenYaTrigramSplitSymbolAlpha``
* ``TokenYaBigramSnowball``
* ``TokenYaTrigramSnowball``

原則、ビルトインのTokenBigramトークナイザー等と同様のルールで文字列をトークナイズします。
それに加え、以下の機能をカスタマイズしています。
``Snowball``以外は、全てのトークナイザーで有効になっています。
ストップワードの機能は、あらかじめ所定のテーブルやカラムを作成しなければ動作しません。

### ``SkipOverlap``

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

### ``SkipStopword``

検索時のみ語彙表の``@stopword``のカラムが``true``となっているキーのトークンがスキップされます。検索速度に影響が大きく、
検索精度にはあまり影響のないキーを取捨選択して検索から除外することができます。インデックス更新からは除外されません。  
``SkipStopword``を使うには、語彙表に``@stopword``という名前のカラムを作る必要があります。
検索時のみスキップれるため、運用中に適宜変更することが可能です。

```
column_create <lexicon_name> @stopword COLUMN_SCALAR Bool
load --table <lexicon_name>
[
{"_key": "the", "@stopword": true }
]
```

### ``FilterLength``

検索時、追加時の両方で64バイトを超えるトークンを除去します。
無駄な長いキーによる語彙表のキーサイズの逼迫を防ぐことができます。

### ``FilterStoptable``

検索時、追加時の両方でテーブルのキーと一致するトークンを除去します。
検索速度に影響が大きく検索精度にあまり影響ないキーをあらかじめ設定することにより、検索速度の向上および転置索引のサイズを抑えることができます。  
``FilterStoptable``を使うには、あらかじめ除外対象の語句が格納されたテーブル``@yangram_stopwords``を作る必要があります。  
整合性を保つため、除外対象の語句を追加した場合は、インデックス再構築が必要です。  
除外対象の語句は、ノーマライズ後のワードを登録する必要があります。``NormalizerMySQLGeneralCI``などでは英字は大文字、``NormalizerAuto``では英字は小文字です。

```
table_create @yangram_stopwords TABLE_HASH_KEY ShortText
load --table @yangram_stopwords
[
{"_key": "this"},
{"_key": "a"}
]
tokenize TokenYaBigramFilterStoptable "This is a pen" NormalizerAuto
[[0,0.0,0.0],[{"value":"is","position":0},{"value":"pen","position":1}]]
```

### ``Snowball``

検索時、追加時の両方で[Snowball](http://snowball.tartarus.org/)を使ってステミングします。

英語のトークンの語幹を抽出します。
複数形や過去形などの活用形の語尾を所定の規則に沿って切除します。


* ADD mode / GET mode

```
tokenize TokenYaBigramSnowball "There are cars" NormalizerAuto
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
    > table_create Diaries TABLE_HASH_KEY INT32
    > column_create Diaries body COLUMN_SCALAR TEXT
    > table_create Terms TABLE_PAT_KEY ShortText \
    >   --default_tokenizer TokenYaBigram
    > column_create Terms diaries_body COLUMN_INDEX|WITH_POSITION Diaries body
    > column_create Terms @stopword COLUMN_SCALAR Bool

Mroonga:

    mysql> use db;
    mysql> CREATE TABLE `temp` (id INT NOT NULL) ENGINE=mroonga DEFAULT CHARSET=utf8;
    mysql> DROP TABLE `temp`;
    mysql> select mroonga_command("register tokenizers/yangram");
    mysql> CREATE TABLE `Diaries` (
        -> id INT NOT NULL,
        -> body TEXT NOT NULL,
        -> PRIMARY KEY (id) USING HASH,
        -> FULLTEXT INDEX (body) COMMENT 'parser "TokenYaBigram"'
        -> ) ENGINE=mroonga DEFAULT CHARSET=utf8;
    mysql> select mroonga_command("column_create Diaries-body @stopword COLUMN_SCALAR Bool");
    mysql> CREATE TABLE `@yangram_stopwords` (
        -> stopword VARCHAR(64) NOT NULL,
        -> PRIMARY KEY (stopword) USING HASH
        -> ) ENGINE=mroonga DEFAULT CHARSET=utf8;
    mysql> INSERT `@yangram_stopwords` VALUES("THAT");

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
