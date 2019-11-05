# project2-DrewBarrett
## Approach
I simply load the filesystem into memory, and when a request is made to fuse I translate it into vmufs data and return.
## Compiling
You need GNU Extensions enabled in your compiler. The one that comes with the vm, and any gcc should have this.
```make```
