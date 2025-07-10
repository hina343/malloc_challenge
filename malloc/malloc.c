//
// >>>> malloc challenge! <<<<
//
// Your task is to improve utilization and speed of the following malloc
// implementation.
// Initial implementation is the same as the one implemented in simple_malloc.c.
// For the detailed explanation, please refer to simple_malloc.c.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Interfaces to get memory pages from OS
//

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

//
// Struct definitions
//

typedef struct my_metadata_t {
  size_t size;
  struct my_metadata_t *next;
} my_metadata_t;

typedef struct my_heap_t {
  my_metadata_t *free_head;
  my_metadata_t dummy;
} my_heap_t;

//
// Static variables (DO NOT ADD ANOTHER STATIC VARIABLES!)
//
my_heap_t my_heap;

//
// Helper functions (feel free to add/remove/edit!)
//

void my_add_to_free_list(my_metadata_t *metadata) {
  assert(!metadata->next);
  metadata->next = my_heap.free_head;
  my_heap.free_head = metadata;
}

void my_remove_from_free_list(my_metadata_t *metadata, my_metadata_t *prev) {
  if (prev) {
    prev->next = metadata->next;
  } else {
    my_heap.free_head = metadata->next;
  }
  metadata->next = NULL;
}

//
// Interfaces of malloc (DO NOT RENAME FOLLOWING FUNCTIONS!)
//

// This is called at the beginning of each challenge.
void my_initialize() {
  my_heap.free_head = &my_heap.dummy;
  my_heap.dummy.size = 0;
  my_heap.dummy.next = NULL;
}

// my_malloc() is called every time an object is allocated.
// |size| is guaranteed to be a multiple of 8 bytes and meets 8 <= |size| <=
// 4000. You are not allowed to use any library functions other than
// mmap_from_system() / munmap_to_system().
void *my_malloc(size_t size) {
  // free list（空き領域のリスト）の中で、最適な（＝一番サイズが小さいけど足りる）ブロックを探します
  my_metadata_t *best = NULL;        // 最適な空きブロック
  my_metadata_t *best_prev = NULL;   // そのひとつ前のブロック（リストから削除するために必要）

  // リストをたどるためのポインタ
  my_metadata_t *cur = my_heap.free_head;
  my_metadata_t *prev = NULL;

  // free list を先頭から最後まで走査
  while (cur != NULL) {
    // 今のブロックのサイズが要求サイズ以上であれば候補
    if (cur->size >= size) {
      // まだ候補がない、または今のブロックがこれまでで最小なら更新
      if (best == NULL || cur->size < best->size) {
        best = cur;
        best_prev = prev;
      }
    }
    // 次のブロックに進むため、現在を前のポインタとして記録し、次へ
    prev = cur;
    cur = cur->next;
  }

  // 最適なブロックが見つからなかった場合（＝free listに十分な空き領域がない）
  if (best == NULL) {
    // 新しいページ（4096バイト）をOSから取得
    size_t buffer_size = 4096;
    my_metadata_t *new_block = (my_metadata_t *)mmap_from_system(buffer_size);

    // 新しく確保したメモリ領域にサイズ情報を記録
    new_block->size = buffer_size - sizeof(my_metadata_t); // メタデータを除いたサイズ
    new_block->next = NULL;

    // free list に追加
    my_add_to_free_list(new_block);

    // 再び my_malloc を呼び出してやり直す（再帰）
    return my_malloc(size);
  }

  // ブロックが見つかったので、ユーザーに返すポインタを計算
  // メタデータのすぐ後ろが使えるメモリ領域
  void *ptr = best + 1;

  // 残りサイズを計算（今のブロックサイズから使う分を引いた残り）
  size_t remaining = best->size - size;

  // free list からこのブロックを削除（もう使うので）
  my_remove_from_free_list(best, best_prev);

  // もし残りサイズがメタデータ1個分より大きいなら、分割して再利用可能にする
  if (remaining > sizeof(my_metadata_t)) {
    // 今のブロックはちょうど要求サイズに縮める
    best->size = size;

    // 残り部分のメモリを新しい free ブロックとして扱う
    my_metadata_t *rest = (my_metadata_t *)((char *)ptr + size); // ptr を size バイト進める
    rest->size = remaining - sizeof(my_metadata_t); // メタデータを引いた残りが使えるサイズ
    rest->next = NULL;

    // 残り部分を free list に戻す
    my_add_to_free_list(rest);
  }

  // 要求されたメモリ領域を返す
  return ptr;
}

  // |ptr| is the beginning of the allocated object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  void *ptr = metadata + 1;
  size_t remaining_size = metadata->size - size;
  // Remove the free slot from the free list.
  my_remove_from_free_list(metadata, prev);

  if (remaining_size > sizeof(my_metadata_t)) {
    // Shrink the metadata for the allocated object
    // to separate the rest of the region corresponding to remaining_size.
    // If the remaining_size is not large enough to make a new metadata,
    // this code path will not be taken and the region will be managed
    // as a part of the allocated object.
    metadata->size = size;
    // Create a new metadata for the remaining free slot.
    //
    // ... | metadata | object | metadata | free slot | ...
    //     ^          ^        ^
    //     metadata   ptr      new_metadata
    //                 <------><---------------------->
    //                   size       remaining size
    my_metadata_t *new_metadata = (my_metadata_t *)((char *)ptr + size);
    new_metadata->size = remaining_size - sizeof(my_metadata_t);
    new_metadata->next = NULL;
    // Add the remaining free slot to the free list.
    my_add_to_free_list(new_metadata);
  }
  return ptr;
}

// This is called every time an object is freed.  You are not allowed to
// use any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  // Look up the metadata. The metadata is placed just prior to the object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  my_metadata_t *metadata = (my_metadata_t *)ptr - 1;
  // Add the free slot to the free list.
  my_add_to_free_list(metadata);
}

// This is called at the end of each challenge.
void my_finalize() {
  // Nothing is here for now.
  // feel free to add something if you want!
}

void test() {
  // Implement here!
  assert(1 == 1); /* 1 is 1. That's always true! (You can remove this.) */
}
