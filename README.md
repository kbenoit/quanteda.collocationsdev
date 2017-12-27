
# quanteda.collocationsdev

[![CRAN
Version](https://www.r-pkg.org/badges/version/quanteda)](https://CRAN.R-project.org/package=quanteda)
[![Travis-CI Build
Status](https://travis-ci.org/kbenoit/quanteda.svg?branch=master)](https://travis-ci.org/kbenoit/quanteda.collocationsdev)
[![Appveyor Build
status](https://ci.appveyor.com/api/projects/status/e3tf2h1ff0nlv249/branch/master?svg=true)](https://ci.appveyor.com/project/kbenoit/quanteda.collocationsdev/branch/master)
[![codecov](https://codecov.io/gh/kbenoit/quanteda/branch/master/graph/badge.svg)](https://codecov.io/gh/kbenoit/quanteda.collocationsdev)

## About

An extension package with additional functionality for computing
collocations statistics. For developmental work.

## How to Install

``` r
# devtools package required to install quanteda from Github 
devtools::install_github("kbenoit/quanteda.collocationsdev") 
```

Because this compiles some C++ source code, you will need a compiler
installed. **If you are using a Windows platform**, this means you will
need also to install the
[Rtools](https://CRAN.R-project.org/bin/windows/Rtools/) software
available from CRAN. **If you are using macOS**, you will need to to
install XCode, available for free from the App Store, or if you prefer a
lighter footprint set of tools, [just the Xcode command line
tools](http://osxdaily.com/2014/02/12/install-command-line-tools-mac-os-x/),
using the command `xcode-select --install` from the Terminal.

## How to Use

See `?collocationsdev`.
