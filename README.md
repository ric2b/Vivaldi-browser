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

'

## Note for the maintainer - How to update the repository

#### Setup

```bash
ssh $USER@$FAST_MACHINE
cd /var/tmp/
mkdir -p ${USER:0:2}
cd ${USER:0:2}
curl https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh > Miniconda.sh
bash Miniconda.sh -b -p /var/tmp/${USER:0:2}/co
source /var/tmp/${USER:0:2}/co/bin/activate
conda install git
git config --global user.name  "..."
git config --global user.email "..."
```

#### Committing a version

```bash
git clone --depth=1 https://github.com/ric2b/Vivaldi-browser w

wget https://vivaldi.com/source/vivaldi-source-2.0...tar.xz
tar --xz -xf ... && mv vivaldi-source v0 && ls -la
ls -ld {w,v0}/.git
# rm -rf v0/.git

cp -r w/{.git,README.md} v0
cd v0
git status | grep -v chromium
git add .
git status | grep -v chromium
git commit -m 'Added version 2.0...' | grep -v chromium
git status | grep -v chromium
git log
cd ..

# git add . && git commit -m 'Added version 2.0...' | grep -v chromium
```

#### Committing a second version

```
wget https://vivaldi.com/source/vivaldi-source-2.1...tar.xz
tar --xz -xf ... && mv vivaldi-source v1 && ls -la
ls -ld v{0,1}/.git
# rm -rf v1/.git

cp -r v0/{.git,README.md} v1
...
```

#### Pushing and cleaning up

```
git status | grep -v chromium
git push
cd ..

cd ..

ls -l

rm -rf
```

### `process.sh` -- automate version addition from a list of urls

```
extract_version() {
    local v="${1##*vivaldi-source_}"
    echo "${v%.tar.xz}"
}

add() {
    ###
    previous_directory="$1"
    file_url="$2"
    file_name="${file_url##*/}"
    version="$(extract_version "$file_url")"
    echo "(PREVIOUS_DIRECTORY $previous_directory) (URL $file_url) (FILE $file_name) (VERSION $version)"
    ###

    (
        set -xe
        wget "$file_url"
        tar --xz -xf "$file_name"
        # echo $(($(ls vivaldi-source -la | awk '{ print "+"$2 }')))
        # while echo $(date '+%H:%M:%S-') $(($(ls vivaldi-source -la | awk '{ print "+"$2 }'))); do sleep 1; done
        mv vivaldi-source "v$version"
        rm "$file_name"
        
        mv "$previous_directory"/{.git,README.md} "v$version" && rm -r "$previous_directory"
        cd "v$version"
        git add .
        git commit -m "Added version $version" | grep -v chromium
        git tag "$version"
        cd ..
    )
}

previous=w
while IFS= read -r file_url; do
    add "$previous" "$file_url"
    previous="v$(extract_version "$file_url")"
done
cd "$previous"
git push && git push --tags

# Usage:
#
# git clone --depth=1 "https://${PERSONAL_ACCESS_TOKEN}@github.com/${ORGANISATION_NAME}/${REPOSITORY_NAME}" w
# git clone --depth=1 "https://${PERSONAL_ACCESS_TOKEN}@github.com/ric2b/Vivaldi-browser" w
#
# git config --global user.name  "..."
# git config --global user.email "..."
#
# vim list.txt
# ...
#
# cat list.txt | bash process.sh
#
# git push
# git push --tags
#
## process.sh expect the name of the cloned repo to be "w"

