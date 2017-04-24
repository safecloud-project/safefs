# SafeFS
(c) 2016 2016 INESC TEC. Written by J. Paulo, R. Pontes and D. Burihabwa.

SafeFS is a software-defined file system based on a modular architecture featuring stackable layers that can be combined to construct a secure distributed file system. SafeFS allows users to specialize their data store to their specific needs by choosing the combination of layers that provide the best safety and performance tradeoffs. The prototype is implemented in user space using FUSE The provided layers include mechanisms based on encryption, replication, and coding. 

For each layer, a set of drivers that provide different approaches for encryption, replication and coding are provided and allow having an even more flexible and modular file system approach.

For more information regarding SafeFS you may read the paper published in SYSTOR'17:

- "SafeFS: A Modular Architecture for Secure User-Space File Systems"

## SafeFS Layers and Drivers:

In folder conf/ there are several .ini files with examples on how layers and drivers can be stacked in SafeFS. As an example, the aligned_aes.ini file has the following configuration:

A multiple backend layer (multi_loop) is at the end of the stack (value 0), while an encryption layer (sfuse) is stacked on top of it (value 1). Then, a block virtualization layer (block_align) is stacked on top (value 2) of the encryption layer. If one wants to use just an encryption layer, then only this layer must be defined in the configuration file. In other words, defining all these layers is not a requirement of SafeFS. 

After defining the order by which layers are stacked, we must define, for each layer, the specific settings. 

The multiple_backend layer is writing data to a single storage backend that, in this example is a local machine folder (path_1 = /var/tmp/safefs/dev1). The FUSE filesystem is mounted on another machine folder (root = /var/tmp/safefs). In this configuration data is being written to a single backend (ndevs = 1).

The encryption layer is using AES (mode = 2). The key, iv and key-size values are also specified in the configuration file. Finally, since FUSE can issue read and write requests for a dynamic number of bytes and AES is being used as a block cipher, the block virtualization layer is translating Fuse requests into blocks (mode = 1) with a size of 4096 bytes. Note that bottom layers will only require this abstaction when these layers use block-oriented techniques (erasure codes, standard/deterministic block encryption, etc).


Next we detail all the parameters for the configuration file. For more information about the layers/drivers functionality/implementation details please check the SYSTOR paper pointed previously.

Layers ([layers])

- multi_loop (position)
- sfuse (position)
- block_align (position)

Multiple backend configuration ([multi_loop]):

- mode: replication (0), XOR (1), erasure codes (2)
- root: Path to the folder where the filesystem will be mounted
- ndevs: number of devices where data will be stored. E.g., if a size two is chosen for mode 0 (replication), then data will be replicated in two devices.
- path_*: path for devices where data will be stored. E.g., if ndevs has value two then a path_1 and path_2 must be assigned.

Encryption layer configuration ([sfuse]):

- mode: without encryption (0), without encryption with padding (1), standard encryption (2), deterministic encryption (3). Option 1 is used for testing the overhead of having a layer that adds a padding to data without encrypting it.
- key: cipher key for standard and deterministic encryption schemes.
- key_size: key size for standard and deterministic encryption schemes.
- iv: initialization vector for deterministic encryption.

Block virtualization layer ([block_align]):

- mode: do not create virtual blocks (0), create a block abstractio layer for subsequent layers (1).
- block_size: size of the block created with the abstraction.



## Deploying SafeFS


## Running SafeFS


For more information please contact:
Joao Paulo jtpaulo at di.uminho.pt
