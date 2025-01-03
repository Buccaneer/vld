////////////////////////////////////////////////////////////////////////////////
//
//  Visual Leak Detector - Internal C++ Heap Management
//  Copyright (c) 2005-2014 VLD Team
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
//  See COPYING.txt for the full terms of the GNU Lesser General Public License.
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#define VLDBUILD     // Declares that we are building Visual Leak Detector.
#include "ntapi.h"   // Provides access to NT APIs.
#include "vldheap.h" // Provides access to VLD's internal heap data structures.
#undef new           // Do not map "new" to VLD's new operator in this file

// Global variables.
vldblockheader_t *g_vldBlockList = nullptr; // List of internally allocated blocks on VLD's private heap.
HANDLE            g_vldHeap;             // VLD's private heap.
bool g_vldHeapActive = false;

// Local helper functions.
static inline void* vldnew (size_t size, const char *file, int line);
static inline void vlddelete (void *block);

// scalar new operator - New operator used to allocate a scalar memory block
//   from VLD's private heap.
//
//  - size (IN): Size of the memory block to be allocated.
//
//  - file (IN): The name of the file from which this function is being
//      called.
//
//  - line (IN): The line number, in the above file, at which this function is
//      being called.
//
//  Return Value:
//
//    If the allocation succeeds, a pointer to the allocated memory block is
//    returned. If the allocation fails, nullptr is returned.
//
void* operator new (size_t size, const char *file, int line)
{
    return vldnew(size, file, line);
}

// vector new operator - New operator used to allocate a vector memory block
//   from VLD's private heap.
//
//  - size (IN): Size of the memory block to be allocated.
//
//  - file (IN): The name of the file from which this function is being
//      called.
//
//  - line (IN): The line number, in the above file, at which this function is
//      being called.
//
//  Return Value:
//
//    If the allocation succeeds, a pointer to the allocated memory block is
//    returned. If the allocation fails, nullptr is returned.
//
void* operator new [] (size_t size, const char *file, int line)
{
    return vldnew(size, file, line);
}

// scalar delete operator - Delete operator used to free internally used memory
//   back to VLD's private heap.
//
//  - block (IN): Pointer to the scalar memory block to free.
//
//  Return Value:
//
//    None.
//
void operator delete (void *block)
{
    vlddelete(block);
}

// vector delete operator - Delete operator used to free internally used memory
//   back to VLD's private heap.
//
//  - block (IN): Pointer to the vector memory block to free.
//
//  Return Value:
//
//    None.
//
void operator delete [] (void *block)
{
    vlddelete(block);
}

// scalar delete operator - Delete operator used to free memory partially
//   allocated by new in the event that the corresponding new operator throws
//   an exception.
//
//  Note: This version of the delete operator should never be called directly.
//    The compiler automatically generates calls to this function as needed.
//
void operator delete (void *block, const char *, int)
{
    vlddelete(block);
}

// vector delete operator - Delete operator used to free memory partially
//   allocated by new in the event that the corresponding new operator throws
//   an exception.
//
//  Note: This version of the delete operator should never be called directly.
//    The compiler automatically generates calls to this function as needed.
//
void operator delete [] (void *block, const char *, int)
{
    vlddelete(block);
}

// get_heap_mutex - Returns a reference to the mutex used to serialize access to
//  the VLD's private heap. Wrapping this inside a function ensures that the mutex
//  is initialized before it is used, but not during static initialization of the   
//  entire program.
vld::criticalsection& get_heap_mutex()
{
    static vld::criticalsection heap_mutex;
    return heap_mutex;
}

// vldnew - Local helper function that actually allocates memory from VLD's
//   private heap. Prepends a header, which is used for bookkeeping information
//   that allows VLD to detect and report internal memory leaks, to the returned
//   block, but the header is transparent to the caller because the returned
//   pointer points to the usable section of memory requested by the caller, it
//   does not point to the block header.
//
//  - size (IN): Size of the memory block to be allocated.
//
//  - file (IN): Name of the file that called the new operator.
//
//  - line (IN): Line, in the above file, at which the new operator was called.
//
//  Return Value:
//
//    If the memory allocation succeeds, a pointer to the allocated memory
//    block is returned. If the allocation fails, nullptr is returned.
//
void* vldnew (size_t size, const char *file, int line)
{
    assert(size > 0);
    assert(file != nullptr);
    assert(line > 0);

    auto *header = reinterpret_cast<vldblockheader_t*>(RtlAllocateHeap(g_vldHeap, 0x0, size + sizeof(vldblockheader_t)));
    static size_t serialnumber = 0;

    if (header == nullptr) {
        // Out of memory.
        return nullptr;
    }

    // Fill in the block's header information.
    header->file         = file;
    header->line         = line;
    header->serialNumber = serialnumber++;
    header->size         = size;

    // Link the block into the block list.
	std::scoped_lock lock(get_heap_mutex());
    header->next         = g_vldBlockList;
    if (header->next != nullptr) {
        header->next->prev = header;
    }
    header->prev         = nullptr;
    g_vldBlockList       = header;

    // Return a pointer to the beginning of the data section of the block.
    return static_cast<void*>(VLDBLOCKDATA(header));
}

// vlddelete - Local helper function that actually frees memory back to VLD's
//   private heap.
//
//  - block (IN): Pointer to a memory block being freed.
//
//  Return Value:
//
//    None.
//
void vlddelete (void *block)
{
    if (block == nullptr)
        return;

    vldblockheader_t *header = VLDBLOCKHEADER((LPVOID)block);

    // Unlink the block from the block list.
    std::scoped_lock lock(get_heap_mutex());
    if (header->prev) {
        header->prev->next = header->next;
    }
    else {
        g_vldBlockList = header->next;
    }

    if (header->next) {
        header->next->prev = header->prev;
    }

    // Free the block.
    bool freed = RtlFreeHeap(g_vldHeap, 0x0, header);
    assert(freed);
    UNREFERENCED_PARAMETER(freed);
}
