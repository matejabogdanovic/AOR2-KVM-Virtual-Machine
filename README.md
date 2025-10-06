# Mini Hypervisor Project

This project implements a simplified hypervisor using the Kernel-based Virtual Machine (KVM) API in C.  
The goal is to design and implement a minimalistic hypervisor capable of loading and running guest virtual machines (VMs) in 64-bit long mode with configurable memory and page size settings.

The project is divided into three parts: Version A, Version B, and Version C — each extending the functionality of the previous one.

---

## Version A — Basic Hypervisor Features 

The first version implements the fundamental features required for running a single virtual machine.

### Requirements
<ul>
<li>Guest physical memory size can be 2MB, 4MB, or 8MB, set via the <code>-m</code> or <code>--memory</code> command-line option.</li>
<li>The virtual machine operates in 64-bit long mode.</li>
<li>Supported page sizes: 4KB or 2MB, defined with <code>-p</code> or <code>--page</code>.</li>
<li>Only one virtual CPU is supported.</li>
<li>Serial output and input are handled via I/O port 0xE9, supporting 1-byte read/write operations.</li>
<li>The guest VM terminates upon executing the <code>hlt</code> instruction.</li>
<li>The guest executable is specified with the <code>-g</code> or <code>--guest</code> option.</li>
</ul>

### Example Usage
``./mini_hypervisor --memory 4 --page 2 --guest guest.img ``


---

## Version B — Multiple Virtual Machines 

This version extends Version A to support multiple guest VMs running concurrently using POSIX threads.

### Requirements
<ul>
<li>Multiple guest images can be specified through the <code>-g</code> or <code>--guest</code> option. Each image represents one VM instance.</li>
<li>Each VM runs in its own POSIX thread to enable concurrent execution.</li>
</ul>

### Example Usage
``./mini_hypervisor --memory 4 --page 2 --guest guest1.img guest2.img``


---

## Version C — File I/O Support

This version extends Version B by enabling guest programs to perform file operations through I/O ports.

### Requirements
<ul>
<li>File operations (open, close, read, write) are implemented using IN/OUT instructions via the I/O parallel port 0x0278.</li>
<li>Opening a non-existent file for writing creates a new file automatically.</li>
<li>Files accessible to guests are specified with the <code>-f</code> or <code>--file</code> command-line option.</li>
<li>Files defined through <code>-f</code> are shared among all VMs.</li>
<li>If a VM attempts to write to a shared file, the hypervisor must create a local copy of that file for that VM.</li>
<li>The hypervisor must ensure that each guest can only access its own files or files it has read permissions for.</li>
</ul>

### Example Usage
``./mini_hypervisor -m 4 -p 2 -g guest1.img guest2.img -f ./flowers.png``
