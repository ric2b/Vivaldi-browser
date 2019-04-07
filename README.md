### I am in no way afiliated with Vivaldi-browser, except as a user. 
### This is just a mirror from https://vivaldi.com/source

The file `README` has the original readme from the developpers.

# Building

(Thanks to [RAZR_96](reddit.com/user/RAZR_96) for providing these instructions in [this comment](https://www.reddit.com/r/vivaldibrowser/comments/639q1p/ive_uploaded_the_provided_vivaldi_source_code_to/dfsgc79/))

> 1. The build instructions (from the `README` file) is missing a command to make the file (or you could make it manually):
>`vivaldi-source\testdata\stp.viv` and add to the file: `// Vivaldi Standalone`
>
> 1. Build according to the instructions on the `README` file
>
>1. The build will use a standalone profile as a result of this file. To use your normal profile you need to delete stp.viv from:  
>`vivaldi-source\out\Release`. Be aware that using different versions of Vivaldi on the same profile will give an error on start up. It'll probably still work but I wouldn't advise it.
>
>1. Once you've built it you need to find where Vivaldi is installed and copy this folder: 
`Vivaldi\Application\1.8.770.50\resources\vivaldi`
>
>1. to this directory: 
>`vivaldi-source\out\Release\resources\`
>
>If anyone wants to build on windows, there's a build.bat [here](https://gist.github.com/justdanpo/c0d41b4173533324aba95bc1f58d063f). You also need [WinSparkle](https://github.com/vslavik/winsparkle). Extract the contents of the zip to:
>
>    vivaldi-source\third_party\_winsparkle_lib
>
>And lastly the 'Release' folder will be about 30GB once built. Not all of the files/folders in it are needed to run the browser. So [here's](https://gist.github.com/Sporif/89e9584ef2370079756700e7f2aecf4e) a Powershell script I made that copies only the stuff you need.

# Note for the maintainer - Update the repository

```
# ssh $USER@$FAST_MACHINE
# cd /var/tmp/
# mkdir -p ${USER:0:2}
# cd ${USER:0:2}
# wget Miniconda.sh
# bash Miniconda.sh -b -p /var/tmp/...
# source /var/tmp/.../bin/activate
# conda install git
# git config --global user.name  "..."
# git config --global user.email "..."
git clone --depth=1 https://github.com/ric2b/Vivaldi-browser w

wget https://vivaldi.com/source/vivaldi-source-2.0...tar.xz
tar --xz -xf ...
mv vivaldi-source v0
ls v0/.git/*/*/*
rm -rf v0/.git

cp w/.git README.md v0
cd v0
git status | grep -v chromium
git add .
git status | grep -v chromium
git commit -m 'Added version 2.0...' --author '... <...>' | grep -v chromium
git status | grep -v chromium
cd ..

wget https://vivaldi.com/source/vivaldi-source-2.1...tar.xz
tar --xz -xf ...
mv vivaldi-source v1
ls v0/.git/*/*/*
rm -rf v1/.git

cp w/.git README.md v1
...

git status | grep -v chromium
git push
cd ..

cd ..

ls -l

rm -rf
```
