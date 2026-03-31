//
// Support functions for system calls that involve file descriptors.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"

// Global variables for the buffer
static char buffer[BUFFER_SIZE];
static int buffer_used = 0;
static struct spinlock buffer_lock;

// Drain buffered console bytes to the UART. Does not sleep while holding buffer_lock.
static void
flush_console_buffer(void)
{
  int n;
  char tmp[BUFFER_SIZE];

  acquire(&buffer_lock);
  n = buffer_used;
  if(n > 0)
    memmove(tmp, buffer, n);
  buffer_used = 0;
  release(&buffer_lock);

  if(n > 0){
    if(devsw[CONSOLE].write == 0)
      panic("flush_console_buffer");
    int w = devsw[CONSOLE].write(0, (uint64)tmp, n);
    if(w != n)
      panic("flush_console_buffer");
  }
}

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
  initlock(&buffer_lock, "buffer");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, ff.writable);
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    if(ff.type == FD_DEVICE && ff.major == CONSOLE)
      flush_console_buffer();
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  if(f->readable == 0)
    return -1;

  if(f->type == FD_PIPE){
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  if(n == 0) return 0;
  if(n < 0) return -1;


    int r, ret = 0;

    if(f->writable == 0)
      return -1;

       

    if(f->type == FD_PIPE){
      ret = pipewrite(f->pipe, addr, n);
    } else if(f->type == FD_DEVICE){
      if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
        return -1;

      
       // Only write to the buffer if the write size is less than the max buffer write size and the device is the console
      if (n < MAX_BUFFER_WRITE_SIZE && f->major == CONSOLE) {
        acquire(&buffer_lock);
        if(buffer_used + n > BUFFER_SIZE){
          release(&buffer_lock);
          flush_console_buffer();
          acquire(&buffer_lock);
        }
        if(buffer_used + n > BUFFER_SIZE){
          release(&buffer_lock);
          return -1;
        }

        int err = either_copyin(buffer + buffer_used, 1, addr, n);
        if(err < 0){
          release(&buffer_lock);
          return -1;
        }
        buffer_used += n;
        release(&buffer_lock);
        // Quick path: flush after each small write so the system stays usable until batch policy lands.
        flush_console_buffer();
        return n;
      }

      if(f->major == CONSOLE)
        flush_console_buffer();
      ret = devsw[f->major].write(1, addr, n);
    } else if(f->type == FD_INODE){
      // write a few blocks at a time to avoid exceeding
      // the maximum log transaction size, including
      // i-node, indirect block, allocation blocks,
      // and 2 blocks of slop for non-aligned writes.
      int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
      int i = 0;
      while(i < n){
        int n1 = n - i;
        if(n1 > max)
          n1 = max;

        begin_op();
        ilock(f->ip);
        if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
          f->off += r;
        iunlock(f->ip);
        end_op();

        if(r != n1){
          // error from writei
          break;
        }
        i += r;
      }
      ret = (i == n ? n : -1);
    } else {
      panic("filewrite");
    }
   

    return ret;
  

}



