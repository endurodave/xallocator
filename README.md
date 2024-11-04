# Replace malloc/free with a Fast Fixed Block Memory Allocator
Replace malloc/free with xmalloc/xfree is faster than the global heap and prevents heap fragmentation faults.

# Table of Contents

- [Replace malloc/free with a Fast Fixed Block Memory Allocator](#replace-mallocfree-with-a-fast-fixed-block-memory-allocator)
- [Table of Contents](#table-of-contents)
- [Preface](#preface)
- [Introduction](#introduction)
- [Storage Recycling](#storage-recycling)
- [Heap vs. Pool](#heap-vs-pool)
- [xallocator](#xallocator)
- [Overload new and delete](#overload-new-and-delete)
- [Code Implementation](#code-implementation)
- [Hiding the Allocator Pointer](#hiding-the-allocator-pointer)
- [Porting Issues](#porting-issues)
- [Reducing Slack](#reducing-slack)
- [Allocator vs. xallocator](#allocator-vs-xallocator)
- [Benchmarking](#benchmarking)
- [Reference articles](#reference-articles)
- [Conclusion](#conclusion)


# Preface

Originally published on CodeProject at: <a href="https://www.codeproject.com/Articles/1084801/Replace-malloc-free-with-a-Fast-Fixed-Block-Memory"><strong>Replace malloc/free with a Fast Fixed Block Memory Allocator</strong></a>

<p><a href="https://www.cmake.org/">CMake</a>&nbsp;is used to create the build files. CMake is free and open-source software. Windows, Linux and other toolchains are supported. See the <strong>CMakeLists.txt </strong>file for more information.</p>

# Introduction

<p>Custom fixed block allocators are specialized memory managers used to solve performance problems with the global heap. In the article &quot;<b><a href="http://www.codeproject.com/Articles/1083210/An-efficient-Cplusplus-fixed-block-memory-allocato">An Efficient C++ Fixed Block Memory Allocator</a></b>&quot;, I implemented an allocator class to improve speed and eliminate the possibility of a fragmented heap memory fault. In this latest article, the <code>Allocator</code> class is used as a basis for the <code>xallocator</code> implementation to replace <code>malloc()</code> and <code>free()</code>.</p>

<p>Unlike most fixed block allocators, the <code>xallocator</code> implementation is capable of running in a completely dynamic fashion without advanced knowledge of block sizes or block quantity. The allocator takes care of all the fixed block management for you. It is completely portable to any PC-based or embedded system. In addition, it offers insight into your dynamic usage with memory statistics.</p>

<p>In this article, I replace the C library <code>malloc</code>/<code>free</code> with alternative fixed memory block versions <code>xmalloc()</code> and <code>xfree()</code>. First, I&#39;ll briefly explain the underlying <code>Allocator</code> storage recycling method, then present how <code>xallocator</code> works.</p>

# Storage Recycling

<p>The basic philosophy of the memory management scheme is to recycle memory obtained during object allocations. Once storage for an object has been created, it&#39;s never returned to the heap. Instead, the memory is recycled, allowing another object of the same type to reuse the space. I&#39;ve implemented a class called <code>Allocator</code> that expresses the technique.</p>

<p>When the application deletes using <code>Allocator</code>, the memory block for a single object is freed for use again but is not actually released back to the memory manager. Freed blocks are retained in a linked list, called the free-list, to be doled out again for another object of the same type. On every allocation request, <code>Allocator</code> first checks the free-list for an existing memory block. Only if none are available is a new one created. Depending on the desired behavior of <code>Allocator</code>, storage comes from either the global heap or a static memory pool with one of three operating modes:</p>

<ol>
	<li>Heap blocks</li>
	<li>Heap pool</li>
	<li>Static pool</li>
</ol>

# Heap vs. Pool

<p>The <code>Allocator</code> class is capable of creating new blocks from the heap or a memory pool whenever the free-list cannot provide an existing one. If the pool is used, you must specify the number of objects up front. Using the total objects, a pool large enough to handle the maximum number of instances is created. Obtaining block memory from the heap, on the other hand, has no such quantity limitations &ndash; construct as many new objects as storage permits.</p>

<p>The <em>heap blocks</em> mode allocates from the global heap a new memory block for a single object as necessary to fulfill memory requests. A deallocation puts the block into a free-list for later reuse. Creating fresh new blocks off the heap when the free-list is empty frees you from having to set an object limit. This approach offers dynamic-like operation since the number of blocks can expand at run-time. The disadvantage is a loss of deterministic execution during block creation.</p>

<p>The <em>heap pool</em> mode creates a single pool from the global heap to hold all blocks. The pool is created using operator new when the <code>Allocator</code> object is constructed. <code>Allocator</code> then provides blocks of memory from the pool during allocations.</p>

<p>The <em>static pool</em> mode uses a single memory pool, typically located in static memory, to hold all blocks. The static memory pool is not created by <code>Allocator</code> but instead is provided by the user of the class.</p>

<p>The heap pool and static pool modes offers consistent allocation execution times because the memory manager is never involved with obtaining individual blocks. This makes a new operation very fast and deterministic.</p>

<p>The <code>Allocator</code> constructor controls the mode of operation.</p>

<pre lang="C++">
class Allocator
{
public:
    Allocator(size_t size, UINT objects=0, CHAR* memory=NULL, const CHAR* name=NULL);
...</pre>

<p>Refer to &quot;<b><a href="http://www.codeproject.com/Articles/1083210/An-efficient-Cplusplus-fixed-block-memory-allocato">An Efficient C++ Fixed Block Memory Allocator</a></b>&quot; for more information on <code>Allocator</code>.</p>

# xallocator

<p>The <code>xallocator</code> module has six main APIs:</p>

<ul>
	<li><code>xmalloc</code></li>
	<li><code>xfree</code></li>
	<li><code>xrealloc</code></li>
	<li><code>xalloc_stats</code></li>
	<li><code>xalloc_init</code></li>
	<li><code>xalloc_destroy</code></li>
</ul>

<p><code>xmalloc()</code> is equivalent to <code>malloc()</code> and used in exactly the same way. Given a number of bytes, the function returns a pointer to a block of memory the size requested.</p>

<pre lang="C++">
void* memory1 = xmalloc(100);</pre>

<p>The memory block is at least as large as the user request, but could actually be more due to the fixed block allocator implementation. The additional over allocated memory is called slack but with fine-tuning block size, the waste is minimized, as I&#39;ll explain later in the article.</p>

<p><code>xfree()</code> is the CRT equivalent of <code>free()</code>. Just pass <code>xfree()</code> a pointer to a previously allocated <code>xmalloc()</code> block to free the memory for reuse.</p>

<pre lang="C++">
xfree(memory1);</pre>

<p><code>xrealloc()</code> behaves the same as <code>realloc()</code> in that it expands or contracts the memory block while preserving the memory block contents.</p>

<pre lang="C++">
char* memory2 = (char*)xmalloc(24);    
strcpy(memory2, &quot;TEST STRING&quot;);
memory2 = (char*)xrealloc(memory2, 124);
xfree(memory2);</pre>

<p><code>xalloc_stats()</code> outputs allocator usage statistics to the standard output stream. The output provides insight into how many <code>Allocator</code> instances are being used, blocks in use, block sizes, and more.</p>

<p><code>xalloc_init()</code> must be called one time before any worker threads start, or in the case of an embedded system, before the OS starts. On a C++ application, this function is called automatically for you. However, it is desirable to call <code>xalloc_init()</code> manually is some cases, typically on an embedded system to avoid the small memory overhead involved with the automatic <code>xalloc_init()</code>/<code>xalloc_destroy()</code> call mechanism.</p>

<p><code>xalloc_destroy()</code> is called when the application exits to clean up any dynamically allocated resources. On a C++ application, this function is called automatically when the application terminates. You must never call <code>xalloc_destroy()</code> manually except in programs that use <code>xallocator</code> only within C files.</p>

<p>Now, when to call <code>xalloc_init()</code> and <code>xalloc_destroy()</code> within a C++ application is not so easy. The problem arises with <code>static</code> objects. If <code>xalloc_destroy()</code> is called too early, <code>xallocator</code> may still be needed when a <code>static</code> object destructor get called at program exit. Take for instance this class:</p>

<pre lang="C++">
class MyClassStatic
{
public:
    MyClassStatic() 
    { 
        memory = xmalloc(100); 
    }
    ~MyClassStatic() 
    { 
        xfree(memory); 
    }
private:
    void* memory;
};</pre>

<p>Now create a <code>static</code> instance of this class at file scope.</p>

<pre lang="C++">
static MyClassStatic myClassStatic;</pre>

<p>Since the object is <code>static</code>, the <code>MyClassStatic</code> constructor will be called before <code>main()</code>, which is okay as I&rsquo;ll explain in the &ldquo;Porting issues&rdquo; section below. However, the destructor is called after <code>main()</code> exits which is not okay if not handled correctly. The problem becomes how to determine when to destroy the <code>xallocator</code> dynamically allocated resources. If <code>xalloc_destroy()</code> is called before <code>main()</code> exits, <code>xallocator</code> will already be destroyed when <code>~MyClassStatic()</code> tries to call <code>xfree()</code> causing a bug.</p>

<p>The key to the solution comes from a guarantee in the C++ Standard:</p>

<blockquote class="quote">
<div class="op">Quote:</div>

<p>&ldquo;Objects with static storage duration defined in namespace scope in the same translation unit and dynamically initialized shall be initialized in the order in which their definition appears in the translation unit.&rdquo;</p>
</blockquote>

<p>In other words, <code>static</code> object constructors are called in the same order as defined within the file (translation unit). The destruction will reverse that order. Therefore, <em>xallocator.h</em> defines a <code>XallocInitDestroy</code> class and creates a <code>static</code> instance of it.</p>

<pre lang="C++">
class XallocInitDestroy
{
public:
    XallocInitDestroy();
    ~XallocInitDestroy();
private:
    static INT refCount;
};
static XallocInitDestroy xallocInitDestroy;</pre>

<p>The constructor keeps track of the total number of <code>static</code> instances created and calls <code>xalloc_init()</code> on the first construction.</p>

<pre lang="C++">
INT XallocInitDestroy::refCount = 0;
XallocInitDestroy::XallocInitDestroy() 
{ 
    // Track how many static instances of XallocInitDestroy are created
    if (refCount++ == 0)
        xalloc_init();
}</pre>

<p>The destructor calls <code>xalloc_destroy()</code> automatically when the last instance is destroyed.</p>

<pre lang="C++">
XallocDestroy::~XallocDestroy()
{
    // Last static instance to have destructor called?
    if (--refCount == 0)
        xalloc_destroy();
}</pre>

<p>When including <em>xallocator.h</em> in a translation unit, <code>xallocInitDestroy</code> will be declared first since the <code>#include</code> comes before user code. Meaning any other <code>static</code> user classes relying on <code>xallocator</code> will be declared after <code>#include &ldquo;xallocator.h</code>&rdquo;. This guarantees that <code>~XallocInitDestroy()</code> is called after all user <code>static</code> classes destructors are executed. Using this technique, <code>xalloc_destroy()</code> is safely called when the program exits without danger of having <code>xallocator</code> destroyed prematurely.</p>

<p><code>XallocInitDestroy</code> is an empty class and therefore is 1-byte in size. The cost of this feature is then 1-byte for every translation unit that includes <em>xallocator.h</em> with the following exceptions.</p>

<ol>
	<li>On an embedded system where the application never exits, the technique is not required <em>except</em> if <code>STATIC_POOLS</code> mode is used. All references to <code>XallocInitDestroy</code> can be safety removed and <code>xalloc_destroy()</code> need never be called. However, you must now call <code>xalloc_init()</code> manually in <code>main()</code> before the <code>xallocator</code> API is used.&nbsp;</li>
	<li>When <code>xallocator</code> is included within a C translation unit, a <code>static</code> instance of <code>XallocInitDestroy</code> is not created. In this case, you must call <code>xalloc_init()</code> in main() and<code> xalloc_destroy() </code>before main() exits.&nbsp;</li>
</ol>

<p>To enable or disable automatic <code>xallocator</code> initialization and destruction, use the <code>#define</code> below:</p>

<pre lang="C++">
#define AUTOMATIC_XALLOCATOR_INIT_DESTROY </pre>

<p>On a PC or similarly equipped high RAM system, this 1-byte is insignificant and in return ensures safe <code>xallocator</code> operation in <code>static</code> class instances during program exit. It also frees you from having to call <code>xalloc_init()</code> and<code> xalloc_destroy()</code> as this is handled automatically.&nbsp;</p>

# Overload new and delete

<p>To make the <code>xallocator</code> really easy to use, I&#39;ve created a macro to overload the <code>new</code>/<code>delete</code> within a class and route the memory request to <code>xmalloc()</code>/<code>xfree()</code>. Just add the macro <code>XALLOCATOR</code> anywhere in your class definition.</p>

<pre lang="C++">
class MyClass 
{
    XALLOCATOR
    // remaining class definition
};</pre>

<p>Using the macro, a <code>new</code>/<code>delete</code> of your class routes the request to <code>xallocator</code> by way of the overloaded <code>new</code>/<code>delete</code>.</p>

<pre lang="C++">
// Allocate MyClass using fixed block allocator
MyClass* myClass = new MyClass();
delete myClass;</pre>

<p>A neat trick is to place&nbsp;<code>XALLOCATOR</code> within the&nbsp;base class of an inheritance&nbsp;hierarchy so that all derived classes&nbsp;allocate/deallocate&nbsp;using <code>xallocator</code>. For instance, say you had a GUI library with a base class.</p>

<pre lang="c++">
class GuiBase 
{
    XALLOCATOR
    // remaining class definition
};</pre>

<p>Any <code>GuiBase</code> derived class (buttons, widgets, etc...)&nbsp;now uses&nbsp;<code>xallocator</code> when <code>new</code>/<code>delete</code> is called without having to add <code>XALLOCATOR</code>&nbsp;to every derived class. This is a powerful means to enable fixed block allocations for an entire hierarchy with a single macro statement.</p>

# Code Implementation

<p><code>xallocator</code> relies upon multiple <code>Allocator</code> instances to manage the fixed blocks; each <code>Allocator</code> instance handles one block size. Like <code>Allocator</code>, <code>xallocator</code> is designed to operate in heap blocks or static pool modes. The mode is controlled by the <code>STATIC_POOLS</code> define within <code>xallocator.cpp</code>.</p>

<pre lang="C++">
#define STATIC_POOLS    // Static pools mode enabled</pre>

<p>In heap blocks mode, <code>xallocator</code> creates both <code>Allocator</code> instances and new blocks dynamically at runtime based upon the requested block sizes. By default, <code>xallocator</code> uses powers of two block sizes. 8, 16, 32, 64, 128, etc... This way, <code>xallocator</code> doesn&#39;t need to know the block sizes in advance and offers the utmost flexibility.</p>

<p>The maximum number of <code>Allocator</code> instances dynamically created by <code>xallocator</code> is controlled by <code>MAX_ALLOCATORS</code>. Increase or decrease this number as necessary for your target application.</p>

<pre lang="C++">
#define MAX_ALLOCATORS  15</pre>

<p>In static pools&nbsp;mode, <code>xallocator</code> relies upon&nbsp;<code>Allocator</code> instances created during dynamic initialization (before entering <code>main()</code>)&nbsp;and static memory pools to satisfy memory requests. This eliminates all heap access with the tradeoff being the block sizes and pools are of fixed size and cannot expand at runtime.</p>

<p>Using <code>Allocator</code>&nbsp;initialization in <font color="#990000" face="Consolas, Courier New, Courier, mono"><span style="font-size: 14.66px">STATIC_POOLS</span></font>&nbsp;mode is tricky. The problem again lies with user class static constructors which might call into the <code>xallocator</code> API during construction/destruction. The C++ standard does not guarantee the order of static constructor calls between translation units&nbsp;during dynamic initialization. Yet,&nbsp;<code>xallocator</code> must be initialized before any APIs are executed. Therefore, the first part of the solution is to preallocate enough static memory for each <code>Allocator</code> instance.&nbsp; Of course, each allocator can use a different <code>MAX_BLOCKS</code> value as required. Using this mode, the global heap is never called.</p>

<pre lang="c++">
// Update this section as necessary if you want to use static memory pools.
// See also xalloc_init() and xalloc_destroy() for additional updates required.
#define MAX_ALLOCATORS    12
#define MAX_BLOCKS        32

// Create static storage for each static allocator instance
CHAR* _allocator8 [sizeof(AllocatorPool&lt;CHAR[8], MAX_BLOCKS&gt;)];
CHAR* _allocator16 [sizeof(AllocatorPool&lt;CHAR[16], MAX_BLOCKS&gt;)];
CHAR* _allocator32 [sizeof(AllocatorPool&lt;CHAR[32], MAX_BLOCKS&gt;)];
CHAR* _allocator64 [sizeof(AllocatorPool&lt;CHAR[64], MAX_BLOCKS&gt;)];
CHAR* _allocator128 [sizeof(AllocatorPool&lt;CHAR[128], MAX_BLOCKS&gt;)];
CHAR* _allocator256 [sizeof(AllocatorPool&lt;CHAR[256], MAX_BLOCKS&gt;)];
CHAR* _allocator396 [sizeof(AllocatorPool&lt;CHAR[396], MAX_BLOCKS&gt;)];
CHAR* _allocator512 [sizeof(AllocatorPool&lt;CHAR[512], MAX_BLOCKS&gt;)];
CHAR* _allocator768 [sizeof(AllocatorPool&lt;CHAR[768], MAX_BLOCKS&gt;)];
CHAR* _allocator1024 [sizeof(AllocatorPool&lt;CHAR[1024], MAX_BLOCKS&gt;)];
CHAR* _allocator2048 [sizeof(AllocatorPool&lt;CHAR[2048], MAX_BLOCKS&gt;)];    
CHAR* _allocator4096 [sizeof(AllocatorPool&lt;CHAR[4096], MAX_BLOCKS&gt;)];

// Array of pointers to all allocator instances
static Allocator* _allocators[MAX_ALLOCATORS];</pre>

<p>Then when <code>xalloc_init()</code> is called during dynamic initalization (via <code>XallocInitDestroy()</code>), placement <code>new</code> is used to initialize each <code>Allocator</code> instance into the static memory previously reserved.</p>

<pre lang="c++">
extern &quot;C&quot; void xalloc_init()
{
    lock_init();

#ifdef STATIC_POOLS
    // For STATIC_POOLS mode, the allocators must be initialized before any other
    // static user class constructor is run. Therefore, use placement new to initialize
    // each allocator into the previously reserved static memory locations.
    new (&amp;_allocator8) AllocatorPool&lt;CHAR[8], MAX_BLOCKS&gt;();
    new (&amp;_allocator16) AllocatorPool&lt;CHAR[16], MAX_BLOCKS&gt;();
    new (&amp;_allocator32) AllocatorPool&lt;CHAR[32], MAX_BLOCKS&gt;();
    new (&amp;_allocator64) AllocatorPool&lt;CHAR[64], MAX_BLOCKS&gt;();
    new (&amp;_allocator128) AllocatorPool&lt;CHAR[128], MAX_BLOCKS&gt;();
    new (&amp;_allocator256) AllocatorPool&lt;CHAR[256], MAX_BLOCKS&gt;();
    new (&amp;_allocator396) AllocatorPool&lt;CHAR[396], MAX_BLOCKS&gt;();
    new (&amp;_allocator512) AllocatorPool&lt;CHAR[512], MAX_BLOCKS&gt;();
    new (&amp;_allocator768) AllocatorPool&lt;CHAR[768], MAX_BLOCKS&gt;();
    new (&amp;_allocator1024) AllocatorPool&lt;CHAR[1024], MAX_BLOCKS&gt;();
    new (&amp;_allocator2048) AllocatorPool&lt;CHAR[2048], MAX_BLOCKS&gt;();
    new (&amp;_allocator4096) AllocatorPool&lt;CHAR[4096], MAX_BLOCKS&gt;();

    // Populate allocator array with all instances 
    _allocators[0] = (Allocator*)&amp;_allocator8;
    _allocators[1] = (Allocator*)&amp;_allocator16;
    _allocators[2] = (Allocator*)&amp;_allocator32;
    _allocators[3] = (Allocator*)&amp;_allocator64;
    _allocators[4] = (Allocator*)&amp;_allocator128;
    _allocators[5] = (Allocator*)&amp;_allocator256;
    _allocators[6] = (Allocator*)&amp;_allocator396;
    _allocators[7] = (Allocator*)&amp;_allocator512;
    _allocators[8] = (Allocator*)&amp;_allocator768;
    _allocators[9] = (Allocator*)&amp;_allocator1024;
    _allocators[10] = (Allocator*)&amp;_allocator2048;
    _allocators[11] = (Allocator*)&amp;_allocator4096;
#endif
}</pre>

<p>At application exit, the destructor for each <code>Allocator</code> is called manually.&nbsp;</p>

<pre lang="c++">
extern &quot;C&quot; void xalloc_destroy()
{
    lock_get();

#ifdef STATIC_POOLS
    for (INT i=0; i&lt;MAX_ALLOCATORS; i++)
    {
        _allocators[i]-&gt;~Allocator();
        _allocators[i] = 0;
    }
#else
    for (INT i=0; i&lt;MAX_ALLOCATORS; i++)
    {
        if (_allocators[i] == 0)
            break;
        delete _allocators[i];
        _allocators[i] = 0;
    }
#endif

    lock_release();
    lock_destroy();
}</pre>

# Hiding the Allocator Pointer

<p>When deleting memory, <code>xallocator</code> needs the original <code>Allocator</code> instance&nbsp;so the deallocation request can be routed to the correct <code>Allocator</code> instance for processing. Unlike <code>xmalloc()</code>, <code>xfree()</code> does not take a size and only uses a <code>void*</code> argument. Therefore, <code>xmalloc()</code> actually hides a pointer to the allocator&nbsp;within an unused portion of the memory block by adding an additional 4-bytes (typical <code>sizeof(Allocator*)</code>) to the request. The caller gets a pointer to the block&rsquo;s client region where the hidden allocator pointer&nbsp;is not overwritten.</p>

<pre lang="C++">
extern &quot;C&quot; void *xmalloc(size_t size)
{
&nbsp;&nbsp; &nbsp;lock_get();

&nbsp;&nbsp; &nbsp;// Allocate a raw memory block&nbsp;
&nbsp;&nbsp; &nbsp;Allocator* allocator = xallocator_get_allocator(size);
&nbsp;&nbsp; &nbsp;void* blockMemoryPtr = allocator-&gt;Allocate(size);

&nbsp;&nbsp; &nbsp;lock_release();

&nbsp;&nbsp; &nbsp;// Set the block Allocator* within the raw memory block region
&nbsp;&nbsp; &nbsp;void* clientsMemoryPtr = set_block_allocator(blockMemoryPtr, allocator);
&nbsp;&nbsp; &nbsp;return clientsMemoryPtr;
}</pre>

<p>When <code>xfree()</code> is called, the allocator pointer is extracted from the memory block so the correct <code>Allocator</code> instance can be called to deallocate the block.</p>

<pre lang="C++">
extern &quot;C&quot; void xfree(void* ptr)
{
&nbsp;&nbsp; &nbsp;if (ptr == 0)
&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;return;

&nbsp;&nbsp; &nbsp;// Extract the original allocator instance from the caller&#39;s block pointer
&nbsp;&nbsp; &nbsp;Allocator* allocator = get_block_allocator(ptr);

&nbsp;&nbsp; &nbsp;// Convert the client pointer into the original raw block pointer
&nbsp;&nbsp; &nbsp;void* blockPtr = get_block_ptr(ptr);

&nbsp;&nbsp; &nbsp;lock_get();

&nbsp;&nbsp; &nbsp;// Deallocate the block&nbsp;
&nbsp;&nbsp; &nbsp;allocator-&gt;Deallocate(blockPtr);

&nbsp;&nbsp; &nbsp;lock_release();
}</pre>

# Porting Issues

<p>The <code>xallocator</code> is thread-safe when the locks are implemented for your target platform. The code provided has Windows locks. For other platforms, you&#39;ll need to provide lock implementations for the four functions within <em>xallocator.cpp</em>:</p>

<ul>
	<li><code>lock_init()</code></li>
	<li><code>lock_get()</code></li>
	<li><code>lock_release()</code></li>
	<li><code>lock_destroy()</code></li>
</ul>

<p>When selecting a lock, use the fastest OS lock available to ensure <code>xallocator</code> operates as efficiently as possible within a multi-threaded environment. If your system is single threaded, then leave the implementation for each of the above functions empty.</p>

<p>Depending on how <code>xallocator</code> is used, it may be called before <code>main()</code>. This means <code>lock_get()</code>/<code>lock_release()</code> can be called before <code>lock_init()</code>. Since the system is single threaded at this point, the locks aren&rsquo;t necessary until the OS kicks off. However, just make sure <code>lock_get()</code>/<code>lock_release()</code> behaves correctly if <code>lock_init()</code> isn&rsquo;t called first. For instance, the check for <code>_xallocInitialized</code> below ensures the correct behavior by skipping the lock until <code>lock_init()</code> is called.</p>

<pre lang="C++">
static void lock_get()
{
    if (_xallocInitialized == FALSE)
        return;

    EnterCriticalSection(&amp;_criticalSection); 
}</pre>

# Reducing Slack

<p><code>xallocator</code> may return block sizes larger than the requested amount and the additional unused memory is called slack. For instance, for a request of 33 bytes, <code>xallocator</code> returns a block of 64 bytes. The additional memory (64 &ndash; (33 + 4) = 27 bytes) is slack and goes unused. Remember, if 33 bytes is requested an additional 4-bytes are required to hold the block size. So if a client requests 64-bytes, really the 128-byte allocator is used because 68-bytes are needed.</p>

<p>Adding additional allocators to handle block sizes other than powers of two offers more block sizes to minimize waste. Run your application and profile your <code>xmalloc()</code> requested sizes with a bit of temporary debug code. Then add allocator block sizes for specifically handling those cases where a large number of blocks are being used.</p>

<p>In the code below, an <code>Allocator</code> instance is created with a block size of 396 when a block between 257 and 396 is requested. Similarly, a block request of between 513 and 768 results in an <code>Allocator</code> to handle 768-byte blocks.</p>

<pre lang="C++">
// Based on the size, find the next higher powers of two value.
// Add sizeof(size_t) to the requested block size to hold the size
// within the block memory region. Most blocks are powers of two,
// however some common allocator block sizes can be explicitly defined
// to minimize wasted storage. This offers application specific tuning.
size_t blockSize = size + sizeof(Allocator*);
if (blockSize &gt; 256 &amp;&amp; blockSize &lt;= 396)
&nbsp;&nbsp; &nbsp;blockSize = 396;
else if (blockSize &gt; 512 &amp;&amp; blockSize &lt;= 768)
&nbsp;&nbsp; &nbsp;blockSize = 768;
else
&nbsp;&nbsp; &nbsp;blockSize = nexthigher&lt;size_t&gt;(blockSize);</pre>

<p>With a minor amount of fine-tuning, you can reduce wasted storage due to slack based on your application&#39;s memory usage patterns. If no tuning is required and using blocks solely based on powers of two is acceptable, the only lines of code required from the snippet above are:</p>

<pre lang="C++">
size_t blockSize;
blockSize = nexthigher&lt;size_t&gt;(size + sizeof(Allocator*));</pre>

<p>Using <code>xalloc_stats()</code>, it&rsquo;s easy to find which allocators are being used the most.</p>

<pre lang="text">
xallocator Block Size: 128 Block Count: 10001 Blocks In Use: 1
xallocator Block Size: 16 Block Count: 2 Blocks In Use: 2
xallocator Block Size: 8 Block Count: 1 Blocks In Use: 0
xallocator Block Size: 32 Block Count: 1 Blocks In Use: 0</pre>

# Allocator vs. xallocator

<p>The advantage of using <code>Allocator</code> is that the allocator block size exactly the size of the object and the minimum block size is only 4-bytes. The disadvantage is that the <code>Allocator</code> instance is <code>private</code> and only usable by that class. This means that the fixed block memory pool can&#39;t easily be shared with other instances of similarly sized blocks. This can waste storage due to the lack of sharing between memory pools.</p>

<p><code>xallocator</code>, on the other hand, uses a range of different block sizes to satisfy requests and is thread-safe. The advantage is that the various sized memory pools are shared via the <code>xmalloc</code>/<code>xfree</code> interface, which can save storage, especially if you tune the block sizes for your specific application. The disadvantage is that even with block size tuning, there will always be some wasted storage due to slack. For small objects, the minimum block size is 8-bytes, 4-bytes for the free-list pointer and 4-bytes to hold the block size. This can become a problem with a large number of small objects.</p>

<p>An application can mix <code>Allocator</code> and <code>xallocator</code> usage in the same program to maximize efficient memory utilization as the designer sees fit.</p>

# Benchmarking

<p>Benchmarking the <code>xallocator</code> performance vs. the global heap on a Windows PC shows just how fast it is. An basic test of allocating and deallocating 20000 4096 and 2048 sized blocks in a somewhat interleaved fashion&nbsp;tests the speed improvement. All tests run with maximum compiler speed optimizations. See the attached source code for the exact algorithm.</p>

<h4>Allocation Times in Milliseconds</h4>

<table class="ArticleTable">
	<thead>
		<tr>
			<td>Allocator</td>
			<td>Mode</td>
			<td>Run</td>
			<td>Benchmark Time (mS)</td>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>Global Heap</td>
			<td>Debug Heap</td>
			<td>1</td>
			<td>1247</td>
		</tr>
		<tr>
			<td>Gobal Heap</td>
			<td>Debug Heap</td>
			<td>2</td>
			<td>1640</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Debug Heap</td>
			<td>3</td>
			<td>1650</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release Heap</td>
			<td>1</td>
			<td>32.9</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release Heap</td>
			<td>2</td>
			<td>33.0</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release Heap</td>
			<td>3</td>
			<td>27.8</td>
		</tr>
		<tr>
			<td>xallocator</td>
			<td>Heap Blocks</td>
			<td>1</td>
			<td>17.5</td>
		</tr>
		<tr>
			<td>xallocator</td>
			<td>Heap Blocks</td>
			<td>2</td>
			<td>5.0</td>
		</tr>
		<tr>
			<td>xallocator</td>
			<td>Heap Blocks</td>
			<td>3</td>
			<td>5.9</td>
		</tr>
	</tbody>
</table>

<p>Windows uses a debug heap when executing within the debugger. The debug heap adds extra safety checks slowing its performance. The release heap is much faster as the checks are disabled. The debug heap can be disabled within&nbsp;Visual Studio by setting&nbsp;<code><strong>_NO_DEBUG_HEAP=1</strong></code>&nbsp;in the&nbsp;<strong>Debugging &gt; Environment&nbsp;</strong>project option.&nbsp;</p>

<p>The debug global heap is predictably the slowest at about 1.6&nbsp;seconds. The release heap is much faster at ~30mS. This benchmark&nbsp;test is very simplistic and a more realistic scenario with varying blocks sizes and random new/delete intervals might produce different results. However, the basic point is illustrated nicely; the memory manager is slower than allocator and highly dependent on the platform&#39;s implementation.</p>

<p>The <code>xallocator</code>&nbsp;running heap blocks mode very fast once the free-list is populated with blocks obtained from the heap. Recall that the heap blocks mode relies upon the global heap to get new blocks, but then recycles them into the free-list for later use. Run 1 shows the allocation hit creating the memory blocks at 17mS. Subsequent benchmarks clock in a very fast 5mS since the free-list is fully populated.&nbsp;</p>

<p>As the benchmarking shows, the <code>xallocator</code> is highly efficient and about five times&nbsp;faster than the global heap on a Windows PC. On an&nbsp;ARM STM32F4 CPU built using a Keil compiler I&#39;ve see well over a 10x speed increase.&nbsp;</p>

# Reference articles

<ul>
	<li><a href="http://www.codeproject.com/Articles/1083210/An-Efficient-Cplusplus-Fixed-Block-Memory-Allocato"><strong>An Efficient C++ Fixed Block Memory Allocator</strong></a>&nbsp;by David Lafreniere</li>
	<li><a href="http://www.codeproject.com/Articles/1089905/A-Custom-STL-std-allocator-Replacement-Improves-Pe"><strong>A Custom STL std::allocator Replacement Improves Performance</strong></a> by David Lafreniere</li>
</ul>

# Conclusion

<p>A medical device I worked on had a commercial GUI library that utilized the heap extensively. The size and frequency of the memory requests couldn&rsquo;t be predicted or controlled. Using the heap is such an uncontrolled fashion is a no-no on a medical device, so a solution was needed. Luckily, the GUI library had a means to replace <code>malloc()</code> and <code>free()</code> with our own custom implementation. <code>xallocator</code> solved the heap speed and fragmentation problem making the GUI framework a viable solution on that product.</p>

<p>If you have an application that really hammers the heap and is causing slow performance, or if you&rsquo;re worried about a fragmented heap fault, integrating <code>Allocator</code>/<code>xallocator</code> may help solve those problems.</p>



