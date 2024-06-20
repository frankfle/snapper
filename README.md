# Snapper Suite

Command line tools for investigating and managing file structures.

## snapper

Tool that can take a snapshot of a directory. Intended to be part of a workflow that supports packaging filesystem changes (such as software installations).

The general idea is to take a snapshot, then perform the changes, then take another snapshot, then compare the snapshots for a filesystem-level understanding of the changes.

### Usage
`snapper -C snapper.conf`

## clop

Tool that allows you to clone the ownership/permissions from one folder to another folder.

### Usage
`clop -s </path/to/source> -d </path/to/destination>`
