# Modern LSM-Tree
This repository contains a modern C++ implementation of the LSM-Tree KVStore, one of the assignment for the <i>Advanced Data Structures (2021Spring) </i> course at the School of Software, Shanghai Jiao Tong University(SE-SJTU).
## This implementation attempt to:
 - Achieve a high degree of scalability through template methods and polymorphism methods;
 - Prevent memory leaks and access to null pointers through smart pointers;
 - Maximize the use of modern C++ features.
## Unlike the assignment requirements, this implementation:
- Currently bases whether to write to an SSTable on the number of keys rather than the size of the SSTable after persistence;
- Compact, persistence, etc. have not yet been fully implemented.
## TODO
 - Compact functionality is implemented
 - Implement persistence functionality
 - Realize high availability
 - ...