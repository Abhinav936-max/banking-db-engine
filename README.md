# High-Concurrency Banking & E-Commerce Database Engine

A small C++17 project that shows the main ideas behind a database engine:

- B+ Tree indexing for fast key lookup
- row-level locking with strict 2PL
- write-ahead logging
- simple transaction commit / abort flow
- range scans over the leaf chain

## Build

```bash
make
```

## Run

```bash
./minidb
```

## What to explain in interview

- Why B+ Tree is used instead of a plain tree or hash map
- What strict 2PL means
- Why write-ahead logging is needed before modifying data
- What a commit record does
- How range queries walk the linked leaf nodes
