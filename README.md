# Yet another ngram tokenizer plugin for Groonga

## Tokenizer

原則、ビルトインのTokenBigramトークナイザー等と同様のルールで文字列をトークナイズします。
それに加え、以下の機能をカスタマイズしています。

### 検索時のオーバラップスキップ

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

### 可変Ngram

* ``TokenYaVgram``
* ``TokenYaVgramSplitSymbolAlpha``

検索時、更新時において``vgram_words``テーブルのキーに含まれるBigramトークンを後に伸ばしてTrigramにします。Trigramになるため、対象のBigramトークンを含む3文字以上での検索性能の向上が見込めます。
出現頻度が高いBigramトークンのみをあらかじめ``vgram_words``テーブルに格納しておくことで、キーサイズの増大を抑えつつ、検索速度向上に効果的なトークンのみをTrigramにすることができます。   
``vgram_words``テーブルには、原則、Trigramにしたい2文字のトークンを格納します。ただし、格納した2文字のトークンのうち、検索クエリの末尾に来ることがあるトークンについては、先頭の1文字のトークンも格納する必要があります。これは、検索クエリで末尾に来るものは、文中では必ずTrigramに伸ばせるとは限らないためです。  
なお、Trigramにする対象のBigramトークン単体で検索される場合は自動的に前方一致検索になります。
整合性を保つため、Vgram対象の語句を追加した場合は、インデックス再構築が必要です。  
これらのトークナイザーも上記同様に検索時のみNgramのオーバーラップをできるだけスキップしてトークナイズします。

テーブルは環境変数``GRN_YANGRAM_VGRAM_WORD_TABLE_NAME``により変更することができます。

検証中。かなり複雑な処理になっているので、まだ想定できていないケースがあるかもしれません。

```
table_create vgram_words TABLE_HASH_KEY ShortText
[[0,0.0,0.0],true]
load --table vgram_words
[
{"_key": "雨だ"},
{"_key": "雨"}
]
[[0,0.0,0.0],2]
tokenize TokenYaVgram "今日は雨だ" NormalizerAuto --mode ADD
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
      "value": "日は雨",
      "position": 1
    },
    {
      "value": "は雨だ",
      "position": 2
    },
    {
      "value": "雨だ",
      "position": 3
    },
    {
      "value": "だ",
      "position": 4
    }
  ]
]
tokenize TokenYaVgram "今日は雨だ" NormalizerAuto --mode GET
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
      "value": "は雨だ",
      "position": 2
    }
  ]
]
tokenize TokenYaVgram "今日は雨" NormalizerAuto --mode GET
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
      "value": "日は雨",
      "position": 1
    }
  ]
]
```

出現頻度の高いBigramトークンの抽出は、以下の``token_count``コマンドプラグインを使うと便利です。日本語のBigramトークンのみを出現頻度の多い順でソートして出力してくれます。

https://github.com/naoa/groonga-command-token-count

```
token_count Terms document_index --token_size 2 --ctype ja --threshold 10000000 --output_prefix 1
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
        -> ) ENGINE=mroonga DEFAULT CHARSET=utf8;

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
