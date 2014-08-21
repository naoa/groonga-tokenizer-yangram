# Yet another ngram tokenizer plugin for Groonga

## Tokenizer

* TokenYaBigramOverskip
* TokenYaBigramContinua
* TokenYaBigramCombhiraCombkata
* TokenYaBigramOverskipContinua
* TokenYaBigramOverskipCombhiraCombkata
* TokenYaTrigramOverskip
* TokenYaTrigramContinua
* TokenYaTrigramCombhiraCombkata
* TokenYaTrigramOverskipContinua
* TokenYaTrigramOverskipCombhiraCombkata
* TokenYaBigramSplitSymbolAlphaOverskip
* TokenYaTrigramSplitSymbolAlphaOverskip

TokenYaBigram～等は、原則、通常のTokenBigram等と同じルールでトークナイズされます。

それに加え、以下の機能が実行されるようにカスタマイズしています。

また十分なテスト検証は行えていません。

このほかに英文用にストップワードやstemmingの機能も追加する予定です。
トークナイザの種類が増えすぎるため、何か方法を考えないといけません。

### Overskip

検索時のみNgramのオーバラップをスキップしてトークナイズします。  
Bigramの場合、検索時のトークンの数が半分+αになります。  
Trigramの場合、検索時のトークンの数が1/3+αになります。  
トークンの比較回数が減るため、検索処理の速度向上が見込めるかもしれません。  

通常のNgramのGETモードでは、末尾のNgramに満たないトークンはスキップされていますが、
Overskipの場合はスキップしないようにしています。そのため1文字のトークンが含まれることがあります。  
検索クエリで登場した場合でも文脈で登場した場合でも一致できるように  
末尾のNに満たないトークンは強制的に前方一致検索になります(デフォルトと同じ)。

3文字とか短い検索クエリの場合、1文字トークンと前方一致検索のものが含まれるため、
検索性能の足を引っ張る可能性があるかもしれません。

検索性能と一致数の変化を検証予定。

* ADD mode

```
tokenize TokenYaBigramOverskip "今日は雨だな" NormalizerAuto --mode ADD
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
tokenize TokenYaBigramOverskip "今日は雨だな" NormalizerAuto --mode GET
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

### Continua

検索時、追加時の両方で以下の連結文字から始まるトークンを除去します。
以下のような連結文字は検索クエリとして入力されることは少ないため
転置索引に含めなくても影響が低いという観点によるものです。
転置索引の容量を削減させることができます。

Ngramではオーバラップされているため、検索クエリの途中にでてくるもの
には影響しません(連続しなければ)。

Overskipと併用することができますがトークンを落としすぎて検索のヒット率に悪影響を
与える可能性があります。

転置索引の容量削減率と一致数の変化を検証予定。

* 連結文字

```
    "゛", " ゜",
    "ヽ", "ヾ", "ゝ", "ゞ", "〃", "仝", "々",
    "ー",
    "ぁ", "ぃ", "ぅ", "ぇ", "ぉ",
    "っ",
    "ゃ", "ゅ", "ょ",
    "ゎ",
    "を", "ん",
    "ァ", "ィ", "ゥ", "ェ", "ォ",
    "ッ",
    "ャ", "ュ", "ョ",
    "ヮ",
    "ン",
    "ヵ", "ヶ"
```

* ADD mode / GET mode

```
tokenize TokenYaBigramContinua "今日はぁ雨" NormalizerAuto --mode GET
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
      "value": "はぁ",
      "position": 2
    },
    {
      "value": "雨",
      "position": 3
    }
  ]
]
```

### Combhira/Combkata

検索時、追加時の両方で``ひらがな1文字+ひらがな以外``または``カタカナ1文字+漢字``の組み合わせで
始まるトークンを除去します。
``ひらがな1文字+他の字種``、``カタカナ1文字+漢字``で始まる組み合わせは検索クエリとして入力され
ることは少ないため転置索引に含めなくても影響が低いという観点によるものです。
転置索引の容量を削減させることができます。

ただし、以下の除外文字が2文字目にでてくる場合は除外されません。ひらがなorカタカナ+漢字1字で検索
したいことがありそうなためです。

Overskipと併用することができますがトークンを落としすぎて検索のヒット率に悪影響を
与える可能性があります。

転置索引の容量削減率と一致数の変化を検証予定。

* 除外文字

```
    "段", "行", "列", "組", "号", "回", "連", "式", "系",
    "型", "形", "変", "長", "短", "音", "階", "字"
```

* ADD mode / GET mode

```
tokenize TokenYaBigramCombhiraCombkata "今日は雨だ" NormalizerAuto --mode ADD
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
    },
    {
      "value": "だ",
      "position": 3
    }
  ]
]
```

## Install

Install ``groonga-tokenizer-yangram`` package:

準備中です。4.0.5がリリースされたら各パッケージを用意する予定です。

### CentOS

* CentOS6

```
% sudo yum localinstall -y http://github.com/naoa/groonga-tokenizer-yangram/public/centos/6/groonga-tokenizer-yangram-1.0.0-1.el6.x86_64.rpm
```

* CentOS7

```
% sudo yum localinstall -y http://github.com/naoa/groonga-tokenizer-yangram/public/centos/7/groonga-tokenizer-yangram-1.0.0-1.el7.x86_64.rpm
```

### Fedora

* Fedora 20

```
% sudo yum localinstall -y http://github.com/naoa/groonga-tokenizer-yangram/public/fedora/20/groonga-tokenizer-yangram-1.0.0-1.fc20.x86_64.rpm
```

### Debian GNU/LINUX

* wheezy

```
% wget http://github.com/naoa/groonga-tokenizer-yangram/public/debian/wheezy/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.0-1_amd64.deb
```

* jessie

```
% wget http://github.com/naoa/groonga-tokenizer-yangram/public/debian/jessie/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.0-1_amd64.deb
```


### Ubuntu

* precise

```
% wget http://github.com/naoa/groonga-tokenizer-yangram/public/ubuntu/precise/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-tokenizer-yangram_1.0.0-1_amd64.deb
```

* trusty

```
% wget http://github.com/naoa/groonga-tokenizer-yangram/public/ubuntu/trusty/groonga-tokenizer-yangram_1.0.0-1_amd64.deb
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
    > table_create Terms TABLE_PAT_KEY ShortText --default_tokenizer TokenYaBigramOverskip
    > column_create Terms diaries_body COLUMN_INDEX|WITH_POSITION Diaries body

Mroonga:

    mysql> use db;
    mysql> select mroonga_command("register tokenizers/yangram");
    mysql> CREATE TABLE `Diaries` (
        -> id INT NOT NULL,
        -> body TEXT NOT NULL,
        -> PRIMARY KEY id(id) USING HASH,
        -> FULLTEXT INDEX body(body) COMMENT 'parser "TokenYaBigramOverskip"'
        -> ) ENGINE=mroonga DEFAULT CHARSET=utf8;

Rroonga:

    irb --simple-prompt -rubygems -rgroonga
    >> Groonga::Context.default_options = {:encoding => :utf8}   
    >> Groonga::Database.create(:path => "/tmp/db")
    >> Groonga::Plugin.register(:path => "/usr/lib/groonga/plugins/tokenizers/yangram.so")
    >> Groonga::Schema.create_table("Diaries",
    ?>                              :type => :hash,
    ?>                              :key_type => :integer32) do |table|
    ?>   table.text("body")
    >> end
    >> Groonga::Schema.create_table("Terms",
    ?>                              :type => :patricia_trie,
    ?>                              :normalizer => :NormalizerAuto,
    ?>                              :default_tokenizer => "TokenYaBigramOverskip") do |table|
    ?>   table.index("Diaries.body")
    >> end
    
## Author

* Naoya Murakami <naoya@createfield.com>

## License

LGPL 2.1. See COPYING for details.

This program includes the Groonga ngram tokenizer.  
https://github.com/groonga/groonga/blob/master/lib/token.c

This program is the same license as Groonga.

The Groonga ngram tokenizer is povided by ``Copyright(C) 2009-2012 Brazil``
