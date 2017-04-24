# ec_fuse

A fuse based file system that applies erasure coding to files for better dependability.

## Requirements

### Libraries

The project relies on the following libraries:

* glib
* liberasurecode

### Tools

In order to compile to code, you will need:

* gcc
* make
* pkg-config

## Compile

From the root of the project, run:
```bash
make
```

## Run
Compiling the code should produce a binary named `ec_fuse`.
To start the program, run:

```bash
./main <transparent> -d -omodules=subdir,subdir=<actual> -oallow_other -ononempty
```

Where:
* `<transparent>` is the directory to interact with
* `<actual>` is the directory where the actual data is stored
