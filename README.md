# Yet another ngram tokenizer plugin for Groonga

## Tokenizer

原則、ビルトインのTokenBigramトークナイザー等と同様のルールで文字列をトークナイズします。
それに加え、以下の機能をカスタマイズしています。

### ``Skip Overlap``

* ``TokenYaBigram``
* ``TokenYaBigramIgnoreBlank``
* ``TokenYaBigramSplitSymbolAlpha``
* ``TokenYaTrigram``
* ``TokenYaTrigramIgnoreBlank``
* ``TokenYaTrigramSplitSymbolAlpha``

検索時のみNgramのオーバーラップをスキップしてトークナイズします。  
検索時のトークン数を減らすことができ、検索処理の速度向上が見込めます。  

トークンの末尾が最終まで到達した場合、アルファベットや記号などトークンがグループ化される字種境界ではスキップされません。
すなわち、検索クエリ末尾や字種境界では、オーバーラップしているトークンが含まれます。
これはNgramのNに満たないトークンを含めてしまうと検索性能が劣化するため、あえてスキップしないようにしています。

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

```bash
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

```bash
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

## Install

Install ``groonga-tokenizer-yangram`` package:

以下のパッケージにはまだTokenYaVgramとKnown Phraseは含まれていません。Groongaにパッチが取り入れられればリリース後に作ります。

### CentOS

* CentOS6

```
% sudo yum localinstall -y http://packages.createfield.com/centos/6/groonga-tokenizer-yangram-1.0.2-1.el6.x86_64.rpm
```

* CentOS7

```
% sudo yum localinstall -y http://packages.createfield.com/centos/7/groonga-tokenizer-yangram-1.0.2-1.el7.centos.x86_64.rpm
```

### Fedora

* Fedora 20

```
% sudo yum localinstall -y http://packages.createfield.com/fedora/20/groonga-tokenizer-yangram-1.0.2-1.fc20.x86_64.rpm
```

* Fedora 21

```
% sudo yum localinstall -y http://packages.createfield.com/fedora/21/groonga-tokenizer-yangram-1.0.2-1.fc21.x86_64.rpm
```

### Debian GNU/LINUX

* wheezy

```
% wget http://packages.createfield.com/debian/wheezy/groonga-tokenizer-yangram_1.0.2-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.2-1_amd64.deb
```

* jessie

```
% wget http://packages.createfield.com/debian/jessie/groonga-tokenizer-yangram_1.0.2-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.2-1_amd64.deb
```

* sid

```
% wget http://packages.createfield.com/debian/sid/groonga-tokenizer-yangram_1.0.2-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.2-1_amd64.deb
```

### Ubuntu

* precise

```
% wget http://packages.createfield.com/ubuntu/precise/groonga-tokenizer-yangram_1.0.2-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.2-1_amd64.deb
```

* trusty

```
% wget http://packages.createfield.com/ubuntu/trusty/groonga-tokenizer-yangram_1.0.2-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.2-1_amd64.deb
```

* utopic

```
% wget http://packages.createfield.com/ubuntu/utopic/groonga-tokenizer-yangram_1.0.2-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.2-1_amd64.deb
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
