# kvzRTP

kvzRTP is a thread-safe, easy-to-use RTP library written for next-generation video conferencing solutions. In ideal conditions it's able to achieve up to 650 MB/s goodput for HEVC stream, with an average processing time of 1.49 µs per 1000 bytes.

There's no global state between the threads which results in no locking, it features a very easy-to-use and intuitive API and it's licensed under the permissive BSD 2-Clause License.

Based on Marko Viitanen's [fRTPlib](https://github.com/fador/fRTPlib)

## Usage

We provide several simple and thoroughly commented examples on how to use kvzRTP, please see:

[How to create a simple RTP sender](examples/simple/rtp/sending.cc)

[How to create a simple RTP receiver (hooking)](examples/simple/rtp/receiving_hook.cc)

[How to create a simple RTP receiver (polling)](examples/simple/rtp/receiving_poll.cc)

[How to create an RTCP instance (polling)](examples/simple/rtcp/rtcp_poll.cc)

[How to create an RTCP instance (hoooking)](examples/simple/rtcp/rtcp_hook.cc)

### Memory ownership/deallocation

If you have not enabled the system call dispatcher, you don't need to worry about these

[Method 1, unique_ptr](examples/simple/rtp/deallocation_1.cc)

[Method 2, copying](examples/simple/rtp/deallocation_2.cc)

[Method 3, deallocation hook](examples/simple/rtp/deallocation_3.cc)

## Building

Linux
```
make -j8
sudo make install
```

You can also use QtCreator to build the library. The library must be built using a 64-bit compiler!

## Linking

The library should be linked as a static library to your program.

Linux

`-L<path to library folder> -lkvzrtp -lpthread`

Windows

`-L<path to library folder> -lkvzrtp -lpthread -lwsock32 -lws2_32`

## How is sending optimized?

For kvzRTP we've used 3 methods to improve the send throughput:
* Vectored I/O
   * This makes our stack zero copy. Both Linux and Windows network stacks allow scatter/gather type I/O where we can construct one complete UDP packet from smaller buffers
* System call batching (Linux only)
   * kvzRTP utilizes system call batching to reduce the overhead caused by system calls. It gathers all packets into a special data structure and flushes these packets all at once reducing the amount of system calls to one per _N_ packets. There's one-to-one correlation between number of `push_frame()`'s and number of system calls perfomed (regarless of the payload size supplied to `push_frame()`)
* System call dispatching
   * To improve the responsiveness of the library, application code only needs to call `push_frame()` which does the packetization and pushes the packets to a writer-specific frame queue. The actual sending is done by a separate thread called system call dispatcher (SCD). Its sole purpose is to take the packets from the frame queue and send them. This minimizes the delay experienced by the calling application and makes sending faster

System call batching and dispatching are a very tightly-knit combination. Frame queue is implemented as a vector of media transactions. For every `push_frame()`, a new transaction entry is created. Transaction entries contain pointers to media frame, RTP headers and the outgoing address.
This model makes the sending very clean because for every input there's one discrete output record which can be processed immediately or given to SCD. It is, however, not mandatory to enable SCD if frame queue want to be used. SCD brings benefits (smaller delay experienced by the application, better throughput) but the downside is that there's yet another thread running in the background. The goal is the dedicate one physical core for SCDs to minimize cache thrashing.

Because of these optimizations, we're the [fastest at sending HEVC](https://google.com) (and AVC [not tested]). Basically we're fast at sending any media stream with frame sizes big and small that require packetization to MTU-sized chunks.

Some of these optimizations, however, have some penalties and they must be enabled explicitly, see "Defines" for more details.

## Deallocation and memory ownership

Enabling SCD makes the lifetime of a transaction unknown. It might be processed immediately, it might take 2ms before it's processed. This is why no memory can be stored on the caller's stack unless the calling application knows that its stack frame has a longer lifetime than the transaction (kvzRTP doesn't provide any tools to figure this out so it's very risky). This also means that once `push_frame()` is called with some memory chunk, kvzRTP owns that memory (meaning it cannot be deallocated by the calling application).

Another problem is that kvzRTP doesn't know what **kind of** memory is given to it: does the pointer point to stack or heap, or perhaps to a memory-mapped file? This "opaqueness" of the pointer makes deallocation a dangerous task which is why kvzRTP needs help from the application.

There are few ways to deal with this memory ownership/deallocation issue:
* Provide deallocation callback function for the SCD:
   * When SCD has processed the transaction, it will call the deallocation callback function which deallocates the memory properly
* Use `unique_ptr`/`shared_ptr`
   * This is the advised way of using `push_frame()` because it's the easiest both from application's and kvzRTP's viewpoint
* Give `RTP_COPY` flag for `push_frame()`
   * This will cause kvzRTP to make a copy of the memory chunk and deallocate it when it's done. The calling application can also deallocate the memory it gave to kvzRTP without worries. This is also a very simple way of dealing with the issue but it degrades performance.
* Disable SCD
   * This is not advised, but it completely eliminates the ownership/deallocation problem

If SCD is used, it's **very important** to use one of the aforementioned tactics when calling `push_frame()`. Below is listed three possible error conditions that can happen if application doesn't follow these rules.

#### Problem 1
If application doesn't provide a deallocation callback, doesn't use `unique_ptr`/`shared_ptr` and doesn't pass `RTP_COPY` to `push_frame()`, kvzRTP assumes that the pointer points to stack and **won't do anything** about it. If the memory is on the heap, this will leak a lot of memory.

#### Problem 2
If deallocation callback is provided, kvzRTP assumes that application won't deallocate the memory by itself. If the application decides to deallocate the memory without kvzRTP's permission anyway, kvzRTP's deallocation call will results in a double-free.

#### Problem 3
If the memory becomes invalid while the transaction is waiting/being processed, SCD will sent garbage. Application must not assume anything about the lifetime of the memory given to `push_frame()` and rather use "persistent" memory like heap or somehow ensure that the lifetime of memory won't expire while the transaction is being processed.

By adhering to the memory ownership/deallocation scheme presented above, the application can be sure that there will be no memory issues stemming from kvzRTP.

## Optimistic fragment receiver (OFR)

kvzRTP features a special kind of fragment receiver that tries to minimize the number of copies. The number of copies is minimized by assuming that UDP packets come in order even though the protocol itself doesn't guarantee that (hence optimistic).

During very rudimentary tests using two computers in the same private network, only **1 %** of fragments needed relocations (ie. **99 %** of the packets were read to correct place using the system call) when `__RTP_N_PACKETS_PER_SYSCALL__` was set to `1`. TODO
This is a significant optimization, especially when transferring large amounts of data per second (such 4K video call).

NOTE: Using the OFR requires some extra defines so it can work optimally, please see "Configuring the OFR" below

The process starts by allocating a large amount of memory (kvzRTP doesn't know the frame size when it's started). The amount of memory allocated can be much more than what is needed but kvzRTP learns from "its mistakes" and adjusts the default allocation size when it notices a pattern in the sizes of received frames. Predicting the frame size incorrectly incurs a large penalty (new allocation + copy all fragments from active frame to newly allocated frame), so kvzRTP favors larger internal fragmentation over memory preservation.

When the actual fragments are received, they're read to the allocated frame with offsets so that first assumed fragment is stored at offset `0 * MAX_PAYLOAD`, second fragment at offset `1 * MAX_PAYLOAD`, third fragment at offset `2 * MAX_PAYLOAD` and so on.

When the fragments are read into place, the OFR will check if they all are part of this active frame and that they're all in correct position. If OFR notices that a fragment is not in correct place it will do a relocation.

#### Relocations
There are three types of relocations:

1) Relocation within read:
	This relocation can be done efficiently as we only need to
	shuffle memory around and the shuffled objects are spatially
	very close (at most `MAX_DATAGRAMS` * `MAX_PAYLOAD` bytes apart)

	Relocation within read is also called shifting (defined below).

	Relocation within read can happen if during larger frame reception we receive f.ex. VPS or SPS packet.
	It can also happen if a duplicate or invalid packet is received.

2) Relocation to other frame:
	This relocation must be done because the received fragment is not part of this frame

	Relocation to other frame will try to copy the fragment to correct place and if it
	cannot do that, it will copy the fragment to the frame's probation zone if that's enabled.
	Otherwise a new temporary frame is created.

3) Relocation within frame:
	This is the most complex type of relocation. Previous two relocations can be resolved immediately
	(except the rare case of relocation to other frame where the start sequence of the other frame is not known).

	If the correct place of the fragment can be determined safely (start sequence is known and the offset for the fragment 
	is less than the offset pointer [meaning there will be no conflicts/complex book keeping of free slots]), the fragment
	can be copied to its correct place immediately

	If on the other hand the place of the fragment is determined to be past next_off pointer or start sequence is missing,
	the fragment will copied to probation zone.

	In any case, relocation to other frame will leave a mark in the relocation table and when the frame becomes active, 
	all these relocations are solved to the best of ability.

	Relocation table can have three different relocations types: `RT_PAYLOAD`, `RT_PROBATION` and `RT_FRAME`

	1) `RT_PAYLOAD` means that the fragment has been copied to frame but its place must be verified (it may be out of place)
	2) `RT_PROBATION` means that the fragment was not copied to frame and it resides in the probation zone and should be copied to the frame 
	3) `RT_FRAME` means that the fragment was not copied to frame (just like `RT_PROBATION`) but either probation zone has run out of memory or it was not enabled at all meaning this fragment was copied to temporary RTP frame. Frame should be relocated to the actual frame.
	
#### Shifting

When a relocation-within-read condition is detected (and after proper handling if the packet was valid packet), the memory is considered garbage and it must be overwritten.
This process of overwriting is called shifting and there's types of it: overwriting and appending.

_Overwriting shifts_, as the name suggests, overwrite the previous content so the shift offset is not updated between shifts. Non-fragments and fragments that belong to other frames cause overwriting shifts. 

Overwriting shifts also cause _appending shifts_ because when a fragment is removed, and a valid fragment is shifted on its place, this valid fragment must not be overwritten and all subsequent valid fragments must be appended.


#### Frame Size and Allocation Heuristics (FSAH)

Trying to mitigate both reallocations and internal fragmentation, OFR uses a "tool" called FSAH.

FSAH keeps track of allocations and frame sizes and tries to build a model that resembles the frame sizes to the best of its ability. Usually a stream consist of inter and intra frames and the intra period is user-configurable. OFR tries to guess the intra period from the frame sizes using FSAH.

Using FSAH, the OFR can efficiently update its allocation sizes when f.ex. encoding parameters or resolution of the video is changed.

### Configuring the OFR

NOTE 1: If you wish to use OFR, you need to supply the maximum payload size used by the sender. When using kvzRTP for both sending and receiving you don't need to worry about that but if you're using f.ex. FFmpeg for sending, you need to set the `MAX_PAYLOAD` to 1506.

`__RTP_USE_OPTIMISTIC_RECEIVER__`
* Enable the receiver, if not given the then default receiver is used

`__RTP_USE_PROBATION_ZONE__`
* Linux-only optimization, small area of free-to-use memory for fragments that cannot be relocated at the moment

`__RTP_PROBATION_ZONE_SIZE__`
* How many **packets** (ie. `N * MAX_PAYLOAD` bytes) can the probation zone hold

`__RTP_N_PACKETS_PER_SYSCALL__`
* How many packets should the OFR read per system call
* This is a double-edged sword because on the other you can reduce the overhead caused by system calls but it will in turn increase the amount of processing done by system call and may cause more complex relocations
* Setting this to 1, you can get rid of	shifting completely

`__RTP_MAX_PAYLOAD__`
* This defines the maximum payload used by sender (so only the actual HEVC data, all headers excluded).
* NOTE: it's important that most of the fragments are of size `__RTP_MAX_PAYLOAD__`, otherwise the OFR will spent a lot of time shifting the memory

One additional way of improving the performance of OFR is decreasing the VPS period. VPS/SPS/PPS packets are read into the active frame just like fragments which means that they must be copied out and all remaining fragments of current read must be shifted.

For VPS period of 1, that's 3 copies (assuming all three are received) and `MAX_DATAGRAMS - 3` memory shifts.

## Defines

Use `__RTP_SILENT__` to disable all prints

Use `__RTP_USE_OPTIMISTIC_RECEIVER__` to enable the optimistic fragment receiver
* See src/formats/hevc.cc for more details

Use `__RTP_USE_PROBATION_ZONE__` to enable the probation zone allocation for RTP frames
* See src/frame.hh for more details
* NOTE: Probation zone is enabled only if optimistic fragment receiver is enabled

Use `__RTP_MAX_PAYLOAD__` to determine the maximum UDP **payload** size
* If you're unsure about this, don't define it. kvzRTP will default to 1441 bytes
* NOTE: If you're not using kvzRTP for both sending and receiving, it is very much advised to set this (to some default value??)

`__RTP_N_PACKETS_PER_SYSCALL__` (Linux only)
* How many packets should the OFR read per system call
* See "Configuring the OFR" for more details

Use `__RTP_PROBATION_ZONE_SIZE__` to configure the probation zone is
* This should define the number of **packets** that fit into probation zone

Use `__RTP_MAX_QUEUED_MSGS__` to configure how many transaction entries the free queue can hold
* This can be rather low number for small amount of clients
* Default is 20

Use `__RTP_FQUEUE_MAX_SIZE__` to configure how many UDP packets one transaction entry holds
* Default is 500

Use `__RTP_FQUEUE_BUFFERS_PER_PACKET__`
* How many buffers does it take to construct one UDP packet
* Default is 4 (RTP, NAL, FU and payload)

Use `NDEBUG` to disable `LOG_DEBUG` which is the most verbose level of logging

# Adding support for new media types

Adding support for new media types quite straight-forward:
* add the payload to util.hh's `RTP_FORMAT` list
* create files to src/formats/`format_name`.{cc, hh}
* create `namespace format_name` inside `namespace kvz_rtp`
* Add functions `push_frame()` and `frame_receiver()`
   * You need to implement all (de)fragmentation required by the media type

See src/formats/hevc.cc and src/formats/hevc.hh for help when in doubt.

