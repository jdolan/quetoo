SOURCE=https://github.com/LRN/ntldd/tarball/master

wget --no-check-certificate -c $SOURCE 

tar xzf master
cd *ntldd*
sh makeldd.sh 
cp ntldd.exe /mingw/bin/ldd.exe
