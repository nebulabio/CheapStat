# CheapStat v2

An open-source, “do-it-yourself” potentiostat for analytical and educational applications.

## Original code

The designs and code from the [CheapStat](http://web.chem.ucsb.edu/~kwp/cheapstat/) were originally created by:

* Aaron A Rowe
* Andrew J Bonham
* Ryan J White
* Michael P Zimmer
* Ramsin J Yadgar
* Tony M Hobza
* Jim W Honea
* Ilan Ben-Yaacov
* Kevin W Plaxco

at UC Santa Barbara, in the Department of Chemistry and Biochemistry. The original code was committed on [March 15, 2015](https://github.com/nebulabiotech/CheapStat/commit/23728bba3c2cc7285828582dd80bdf4469ad5679) and marked with the [v2.0 tag](https://github.com/nebulabiotech/CheapStat/releases/tag/v2.0). You can get the original code with:

```sh
$ git clone --branch v2.0 git@github.com:nebulabiotech/CheapStat
```


## Changes to the original code

In analyzing and working with the CheapStat, we decided that some iprovements could be made to the design of the board and the code. Incremental changes will be made in this repo, while a from-scratch design is being created in the [Protochip](https://github.com/nebulabiotech/protochip).

- - -

## Original Readme

```
schematic/ contains the EagleCAD files for schematic and board layout, parts list, and Gerber files for PCB manufacturing.

gui/ contains the Java program for receiving results from the CheapStat device via USB.  Source code is located inside .jar file.

mcu/ contains the firmware for the MCU as well as the source files.  AVRstudio 4 and WinAVR are required for compiling source code, and well as AVRISP mkII or equivalent programmer for programming the MCU.
```
