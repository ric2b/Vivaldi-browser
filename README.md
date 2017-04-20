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
