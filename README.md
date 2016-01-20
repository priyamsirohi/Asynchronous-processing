
* PURPOSE:

To learn about asynchronous and concurrent processing, kernel locking,
efficiency, and callbacks.


* OVERVIEW:

Certain processing of data files, such as encryption or compression,
consumes a lot of CPU.  But it also requires a lot of I/O activity, because
the file's data buffers have to be copied between user and kernel levels,
producing expensive data copies and context switches.

An alternative is to read the files in the kernel, do all the CPU-heavy
processing on them, and then write it back out directly.  This can eliminate
expensive user/kernel data copies and context switches.

Another problem is that such processing takes a long time, no matter whether
you do it in the kernel or user level.  We don't want user programs to wait
too long, and we don't want to wait for system calls that may take a long
time to finish either.

So an additional solution is to perform this processing asynchronously.  The
user program will "submit" a request to the kernel to process a file, say,
encrypt or compress it.  The user program will NOT wait for the request to
be processed, however.  The kernel will accept the request and queue it for
processing, for example using a FCFS policy or any other priority based
policy.  That way, the user program doesn't need to block waiting for the
request to finish.

The last problem we have is caused by the very nature of asynchronous
processing: how does the kernel inform the user program about the status of
such a request?  How do we propagate errors?  How does user-land know when
the request was processed and finished?

* TASK:

Create a Linux kernel module (in vanilla 4.0.y Linux that's in your personal
or group HW3 GIT repository) that, when loaded into Linux, will support a
new multi-mode system call called:

	int sys_submitjob(void *args, int argslen)

With such a void*, you can encode anything you want.  Your system should
support the ability to submit jobs to the kernel for processing.  The job
descriptions could include

- unique code for a job type
- what file name(s) to process
- what to do with them (encrypt, compress, compute checksum, etc.)
- configuration processing (e.g., cipher name, checksum alg, compression, etc.)
- flags that control processing (atomicity, error handling, priority, etc.)
- anything else you can think of that's useful

Your module, when loaded, should instantiate at least one producer-consumer
work queue, as well as one or more kernel threads to process the queue.
Your system call should submit jobs to the queue(s).  The scheduler should
wake up one or more queues as needed to perform the work needed in the
queue.  When there's no work to do, no kthreds should run.

You should support a method to REMOVE queued jobs (maybe you changed your
mind); support a method to LIST all queued jobs in user-land; support a
method to change the priority of a job in the queue, which should reorder it
in the list of pending jobs.  You can be clever and reuse the
sys_submitjob() syscall for these remove and list operations.

Based on what we study in class, you should consider issues of efficiency,
correctness, concurrency, races, deadlocks, and more.

Because this is an open-ended assignment, you have a lot of freedom.  The
more features you support, the better your grade will be.  But your code has
to be first and foremost stable, clean, correct, and efficient.  It is
therefore recommended that you build a simple system first, then add new
features to it gradually.

Linux supports a number of work-queue mechanisms and all sorts of locks
(spinlock, mutex, rwsem, rcu).  Linux also has asynchronous I/O system calls
(AIO) and VFS methods.  Study all of those carefully.  Feel free to utilize
existing mechanism in Linux, write your own, or any mix.

Examples of features that your system call may support are:

1. Encrypt (or decrypt) file F1 with cipher C and key K.  Options include
   overwriting/deleting original file, or renaming file F1 to F2.  Return an
   error code or success code.

2. Compress (or decompress) file F1 with algorithm C.  Options include
   overwriting/deleting original file, or renaming file F1 to F2.  Return an
   error code or success code.  Optionally you may return the size of the
   compressed file.

3. Compute checksum C for file F1, using hashing algorithm H.  Return
   checksum back to userland, or error code.

4. Concatenate 2 or more files (F1, F2, .. Fn) into one output file.

Implement at least one of the above four options, or all four if you can.
You can also be creative and implement additional or other features,
documented well of course.  For all three examples above, you can use the
Linux subsystem called CryptoAPI, which allows you to encrypt, compress, or
checksum data; study how CryptoAPI works.

Returning information asynchronously back to a user application can be
tricky.  You can consider various mechanisms such as signals, shared memory,
polling a descriptor, or any other facility you deem suitable (see how the
AIO syscalls work).  Also consider "netlink" sockets that provide a flexible
user-kernel communication channel: when one side of the channel "writes" to
the channel, the other end automatically gets interrupted to "read" the
data.  The key, however, is that the user program CANNOT busy-wait for the
kernel to finish its work; user programs may submit a job to the kernel,
then go off to do something else while they wait for the kernel to be done.
Note that the user process can also die / exit after sending request to the
kernel.  In this case, the kernel should not panic and handle it gracefully.
You will have to write small user-level programs to demonstrate the utility
of your new system call.  Be sure to thoroughly test your code and document
it.



