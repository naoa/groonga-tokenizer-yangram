# Yet another ngram tokenizer plugin for Groonga

## Tokenizer

### TokenYaBigram

not implemented yet.

* ADD mode
```
> tokenize TokenYaBigram "This is a pen." --mode ADD

```

* GET mode

```
> tokenize TokenYaBigram "This is a pen." --mode GET

```

## Install

Install ``groonga-tokenizer-yangram`` package:

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
    > table_create Terms TABLE_PAT_KEY ShortText --default_tokenizer TokenYaBigram
    > column_create Terms diaries_body COLUMN_INDEX|WITH_POSITION Diaries body

Mroonga:

    mysql> use db;
    mysql> select mroonga_command("register tokenizers/yangram");
    mysql> CREATE TABLE `Diaries` (
        -> id INT NOT NULL,
        -> body TEXT NOT NULL,
        -> PRIMARY KEY id(id) USING HASH,
        -> FULLTEXT INDEX body(body) COMMENT 'parser "TokenYaBigram"'
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
    ?>                              :default_tokenizer => "TokenYaBigram") do |table|
    ?>   table.index("Diaries.body")
    >> end
    
## Author

* Naoya Murakami <naoya@createfield.com>

## License

LGPL 2.1. See COPYING for details.

This program includes the Groonga ngrame tokenizer.  
https://github.com/groonga/groonga/blob/master/lib/token.c

This program is the same license as Groonga.

The Groonga ngrame tokenizer is povided by ``Copyright(C) 2009-2012 Brazil``
