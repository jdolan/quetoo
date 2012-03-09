SOURCE=https://github.com/LRN/ntldd/tarball/master

wget --no-check-certificate -c $SOURCE 

mv master ntldd-git.tar.gz
tar xzf `ls ntldd-*.tar.gz`
cd *ntldd*
sh makeldd.sh 
cp ntldd.exe /mingw/bin/ldd.exe
