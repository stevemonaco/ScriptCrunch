# ScriptCrunch

A text analysis utility that applies combinations of DTE, Dictionary, and Substring text compression to text scripts. Aimed towards retro videogame scripts and supports Atlas scripts. Intended for picking optimal character sequences for best compression, benchmarking included algorithms against scripts, and outputting table encoding files.

### Archive Notice

I created ScriptCrunch over a few days back in 2007. It is being archived in Github for preservation purposes and because it is still, to my knowledge, the most comprehensive utility for the intended task.

### Getting Started

See the ScriptCrunch.txt manual and included example .ini files to learn how to configure ScriptCrunch to analyze your script.

### Running

ScriptCrunch is a commandline program and must be ran with your specified configuration .ini as follows:

```ScriptCrunch Config.ini```

### Benchmark

The following are the results of a script benchmarked against several choices in compression algorithm, table sizes, and string lengths. Such benchmarks are insightful in determining whether implementing a simple compression approach is good enough for the target videogame or if a more complex, better compression ratio algorithm such as Huffman or LZ is required.

```main.txt - 259235 bytes. Actual uncompressed insertion size: 228149 bytes.```

## Compression
```
DTE(X) - Dual Tile Encoding with X entries
Dictionary(a, b, X) - Dictionary-encoding (whole word) with string length in the interval [a, b] with X entries
Substring(a, b, X) - Substring-encoding with string length in the interval [a, b] with X entries
YYYYY [XX.X%] - The difference between the uncompressed and compressed scripts is YYYYY bytes and the space-saved compression ratio is XX.X% (higher ratio is better).
```

## Results
```
DTE(64) - 69536 [30.5%]
DTE(128) - 84301 [36.9%]

Dictionary(3,14,256) - 43377 [19.0%]
Dictionary(3,14,512) - 55676 [24.4%]
Dictionary(4,14,512) - 48912 [21.4%]

Substring(3,14,256) - 61064 [26.8%]
Substring(4,14,256) - 61415 [26.9%]
Substring(5,14,256) - 58401 [26.9%]
Substring(3,14,512) - 71801 [31.5%]

Dictionary(3,14,256)+DTE(64) - 81933 [35.9%]
Dictionary(3,14,256)+DTE(128) -  92423 [40.5%]
Dictionary(3,14,512)+DTE(64) - 87054 [38.2%]
Dictionary(3,14,512)+DTE(128) - 94735 [41.5%]

Dictionary(4,14,256)+DTE(64) - 83682 [36.7%]
Dictionary(4,14,256)+DTE(128) - 94598 [41.5%]
Dictionary(4,14,512)+DTE(64) - 88987 [39.0%]
Dictionary(4,14,512)+DTE(128) - 98202 [43.0%]

Substring(3,14,256)+DTE(128) - 88884 [39.0%] 
Substring(4,14,256)+DTE(128) - 97907 [42.9%]
Substring(5,14,256)+DTE(128) - 100101 [43.9%]
Substring(6,14,256)+DTE(128) - 99867 [43.8%]
Substring(5,14,512)+DTE(128) - 105502 [46.2%]
```

## Thanks

Brodie Thiesfield - Author of the SimpleIni source code used for reading .ini configuration files.
