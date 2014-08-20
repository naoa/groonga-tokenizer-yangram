
#CentOS6

```
docker run -v /usr/local/src:/usr/local/src -i -t centos:centos6 /bin/bash
rpm -ivh http://packages.groonga.org/centos/groonga-release-1.1.0-1.noarch.rpm
yum update -y
yum install -y groonga-devel autoconf automake libtool wget tar gcc-c++ make mecab-devel libedit-devel rpm-build rpmdevtools

cd /usr/local/src/groonga-tokenizer-yangram
make clean
rm -rf Makefile.in aclocal.m4 autom4te.cache config.guess config.h.in config.sub configure depcomp install-sh ltmain.sh m4 missing groonga-tokenizer-yangram.pc packages/rpm/centos/groonga-tokenizer-yangram.spec packages/rpm/fedora/groonga-tokenizer-yangram.spec Makefile config.log config.h libtool stamp-h1 packages/Makefile packages/Makefile.in packages/apt/Makefile packages/apt/Makefile.in packages/rpm/Makefile packages/rpm/Makefile.in  packages/rpm/centos/Makefile packages/rpm/centos/Makefile.in packages/rpm/fedora/Makefile packages/rpm/fedora/Makefile.in packages/source/Makefile packages/source/Makefile.in  packages/yum/Makefile packages/yum/Makefile.in test/Makefile.in test/Makefile tokenizers/Makefile tokenizers/Makefile.in  tokenizers/.deps/ tokenizers/.libs/ config.status
sh autogen.sh
./configure && make

mkdir -p ~/rpmbuild/{BUILD,SRPMS,SPECS,SOURCES,RPMS}
cp packages/rpm/centos/groonga-tokenizer-yangram.spec ~/rpmbuild/SPECS/

cd /usr/local/src
rm -rf groonga-tokenizer-yangram-1.0.0
cp -rf groonga-tokenizer-yangram groonga-tokenizer-yangram-1.0.0
tar zcvf groonga-tokenizer-yangram-1.0.0.tar.gz groonga-tokenizer-yangram-1.0.0
mv groonga-tokenizer-yangram-1.0.0.tar.gz ~/rpmbuild/SOURCES/
rpmbuild -ba ~/rpmbuild/SPECS/groonga-tokenizer-yangram.spec
mv ~/rpmbuild/RPMS/x86_64/groonga-tokenizer-yangram-1.0.0-1.el6.x86_64.rpm /usr/local/src/groonga-tokenizer-yangram/public/centos/6/
```

#CentOS7

```
docker run -v /usr/local/src:/usr/local/src -i -t centos:centos7 /bin/bash
…
mv ~/rpmbuild/RPMS/x86_64/groonga-tokenizer-yangram-1.0.0-1.el7.centos.x86_64.rpm /usr/local/src/groonga-tokenizer-yangram/public/centos/7/```
```

#Fefora20

```
docker run -v /usr/local/src:/usr/local/src -i -t fedora:20 /bin/bash
rpm -ivh http://packages.groonga.org/fedora/groonga-release-1.1.0-1.noarch.rpm
yum update -y
yum install -y groonga-devel autoconf automake libtool wget tar gcc-c++ make mecab-devel libedit-devel rpm-build rpmdevtools
…
cp packages/rpm/fedora/groonga-tokenizer-yangram.spec ~/rpmbuild/SPECS/
…
mv ~/rpmbuild/RPMS/x86_64/groonga-tokenizer-yangram-1.0.0-1.fc20.x86_64.rpm /usr/local/src/groonga-tokenizer-yangram/public/fedora/20/
```

#Debian(wheezy)

```
docker run -v /usr/local/src:/usr/local/src -i -t debian:wheezy /bin/bash

echo "deb http://packages.groonga.org/debian/ wheezy main" >> /etc/apt/sources.list.d/groonga.list
echo "deb-src http://packages.groonga.org/debian/ wheezy main" >> /etc/apt/sources.list.d/groonga.list

apt-get update
apt-get install -y --allow-unauthenticated groonga-keyring
apt-get update
apt-get install -y groonga libgroonga-dev wget tar build-essential zlib1g-dev liblzo2-dev libmsgpack-dev libzmq-dev libevent-dev libmecab-dev autoconf automake libtool lsb-release aptitude devscripts

cd /usr/local/src/groonga-tokenizer-yangram
make clean                                                                                                             rm -rf Makefile.in aclocal.m4 autom4te.cache config.guess config.h.in config.sub configure depcomp install-sh ltmain.sh m4 missing groonga-tokenizer-yangram.pc packages/rpm/centos/groonga-tokenizer-yangram.spec packages/rpm/fedora/groonga-tokenizer-yangram.spec Makefile config.log config.h libtool stamp-h1 packages/Makefile packages/Makefile.in packages/apt/Makefile packages/apt/Makefile.in packages/rpm/Makefile packages/rpm/Makefile.in  packages/rpm/centos/Makefile packages/rpm/centos/Makefile.in packages/rpm/fedora/Makefile packages/rpm/fedora/Makefile.in packages/source/Makefile packages/source/Makefile.in  packages/yum/Makefile packages/yum/Makefile.in test/Makefile.in test/Makefile tokenizers/Makefile tokenizers/Makefile.in  tokenizers/.deps/ tokenizers/.libs/ config.status
sh autogen.sh
./configure && make

rm -rf ~/build
mkdir -p ~/build

cd /usr/local/src

cp -rf groonga-tokenizer-yadelirm -rf groonga-tokenizer-yangram-1.0.0mit groonga-tokenizer-yangram-1.0.0
tar zcvf groonga-tokenizer-yangram-1.0.0.tar.gz groonga-tokenizer-yangram-1.0.0
mv groonga-tokenizer-yangram-1.0.0.tar.gz ~/build/groonga-tokenizer-yangram_1.0.0.orig.tar.gz
cd ~/build
tar xfz groonga-tokenizer-yangram_1.0.0.orig.tar.gz
cd groonga-tokenizer-yangram-1.0.0/
cp -rf packages/debian .
debuild -us -uc
cd ..
mv ~/build/groonga-tokenizer-yangram_1.0.0-1_amd64.deb /usr/local/src/groonga-tokenizer-yangram/public/debian/wheezy/
```

#Debian(jessie)

```
docker run -v /usr/local/src:/usr/local/src -i -t debian:jessie /bin/bash

echo "deb http://packages.groonga.org/debian/ jessie main" >> /etc/apt/sources.list.d/groonga.list
echo "deb-src http://packages.groonga.org/debian/ jessie main" >> /etc/apt/sources.list.d/groonga.list
…
mv ~/build/groonga-tokenizer-yangram_1.0.0-1_amd64.deb /usr/local/src/groonga-tokenizer-yangram/public/debian/jessie/
```

#Ubuntu(precise)

```
docker run -v /usr/local/src:/usr/local/src -i -t ubuntu:precise /bin/bash

apt-get update
apt-get -y install software-properties-common python-software-properties
apt-get update
add-apt-repository -y universe
add-apt-repository -y ppa:groonga/ppa
apt-get update
…
mv ~/build/groonga-tokenizer-yangram_1.0.0-1_amd64.deb /usr/local/src/groonga-tokenizer-yangram/public/ubuntu/precise/
```

#Ubuntu(trusty)

```
docker run -v /usr/local/src:/usr/local/src -i -t ubuntu:trusty /bin/bash

apt-get update
apt-get -y install software-properties-common python-software-properties
apt-get update
add-apt-repository -y universe
add-apt-repository -y ppa:groonga/ppa
apt-get update
…
mv ~/build/groonga-tokenizer-yangram_1.0.0-1_amd64.deb /usr/local/src/groonga-tokenizer-yangram/public/ubuntu/trusty/
```

