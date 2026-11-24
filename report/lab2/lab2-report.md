# Báo cáo Lab 2 – Quản lý bộ nhớ (page table)

Báo cáo này trình bày ba bài tập của đề bài Lab 2 trên nhánh `pgtbl` của bộ `xv6-labs-2024`: (1) mở rộng system call `sysinfo` để trả thêm số byte bộ nhớ vật lý còn trống, (2) viết `vmprint()` in cây bảng trang, và (3) cài đặt system call `pgaccess()` đọc bit truy cập `PTE_A`. Chương 2 trình bày cơ sở lý thuyết chung — phân trang Sv39, `walk()`, các bit trong PTE — vì cả ba bài đều đọc hoặc sửa cùng một cấu trúc.

## Thông tin nhóm

| STT | MSSV | Họ và tên |
| --- | --- | --- |
| 1 | 24120010 | Trần Văn Tấn Khôi |
| 2 | 24120035 | Lê Thành Đạt |
| 3 | 24120361 | Trịnh Tuấn Kiệt |

# 1. Mục tiêu và phạm vi

Đề bài yêu cầu ba việc, mỗi việc tương ứng một điểm số khác nhau và đều thao tác trên bảng trang:

- **Mở rộng `sysinfo`** (2 điểm): thêm số byte bộ nhớ vật lý còn trống.
- **In bảng trang** (4 điểm): `vmprint(pagetable_t)` in cây bảng trang theo đúng định dạng của đề, không làm đổi hành vi `exec()`, kèm giải thích từng trang lá và quyền truy cập.
- **Phát hiện trang được truy cập** (4 điểm): `pgaccess(base, len, mask)` trả về bitmask các trang đã bị truy cập và xoá bit đó cho lần gọi sau.

| Yêu cầu | Nội dung | Mục tương ứng |
| --- | --- | --- |
| 2.1 | Phân trang Sv39, `walk()`, `mappages()`, bố cục `memlayout.h` | 2.1–2.4 |
| 3 | Số byte bộ nhớ trống trong `sysinfo` | 3.1–3.4 |
| 4 | `vmprint()` và mốc gọi trong `exec()` | 4.1–4.3 |
| 4 | In theo yêu cầu bằng system call `vmprint()` | 4.4 |
| 4 | Giải thích từng trang lá và quyền truy cập | 4.6 |
| 5 | `pgaccess()`, bit `PTE_A`, chương trình kiểm thử | 5.1–5.5 |

Phương pháp thực hiện là **đọc mã nguồn tĩnh** trong `xv6-labs-2024` (phiên bản RISC-V chạy trên QEMU), đối chiếu với chương 3 của sách xv6 và chương trình kiểm thử được **thực nghiệm** trên QEMU: build sạch với `LAB=pgtbl`, chạy `pgtbltest`, `sysinfotest` và `usertests -q`.

# 2. Cơ sở lý thuyết: bảng trang trong xv6

## 2.1 Mô hình Sv39

RISC-V dùng phân trang ba cấp Sv39: địa chỉ ảo 39 bit, trang 4096 byte (`PGSIZE`), mỗi bảng trang là một trang chứa 512 PTE 8 byte.

| Bit | Độ rộng | Chỉ số | Ý nghĩa |
| --- | --- | --- | --- |
| 38–30 | 9 | `PX(2)` | Chỉ số trong bảng trang cấp 2 (gốc) |
| 29–21 | 9 | `PX(1)` | Chỉ số trong bảng trang cấp 1 |
| 20–12 | 9 | `PX(0)` | Chỉ số trong bảng trang cấp 0 (trang lá) |
| 11–0 | 12 | offset | Vị trí trong trang |

xv6 giới hạn `MAXVA = 1 << 38` chứ không dùng $2^{39}$, để địa chỉ hợp lệ không bao giờ có bit 38 bật và không phải sign-extend.

## 2.2 Bit trong PTE

| Cờ | Bit | Ý nghĩa |
| --- | --- | --- |
| `PTE_V` | 0 | Hợp lệ; thiếu cờ này thì phần cứng coi như chưa ánh xạ |
| `PTE_R` | 1 | Cho phép đọc; cũng là dấu hiệu PTE lá |
| `PTE_W` | 2 | Cho phép ghi |
| `PTE_X` | 3 | Cho phép thực thi |
| `PTE_U` | 4 | Truy cập được từ chế độ người dùng |
| `PTE_A` | 6 | Đã được truy cập (đọc hoặc ghi) |

Hai phép biến đổi: `PA2PTE(pa) = (pa >> 12) << 10`, `PTE2PA(pte) = (pte >> 10) << 12`, và `PTE_FLAGS(pte) = pte & 0x3FF`.

PTE không có `R`/`W`/`X` là PTE trung gian, trỏ tới bảng trang cấp dưới. xv6 phân biệt bằng macro `PTE_LEAF`:

```c
// A leaf PTE maps a page; a non-leaf PTE points at the next page-table page.
#define PTE_LEAF(pte) (((pte) & (PTE_R | PTE_W | PTE_X)) != 0)
```

Macro này do nhóm bổ sung vào `kernel/riscv.h`: bản xv6 nhận từ nhánh `syscall` đã dùng `PTE_LEAF` trong `walk()` dưới `#ifdef LAB_PGTBL` nhưng chưa định nghĩa, nên chỉ cần đổi `LAB` sang `pgtbl` là build dừng ở `kernel/vm.o`.

## 2.3 Đi xuống bảng trang: `walk()`

`walk(pagetable, va, alloc)` duyệt cấp 2 rồi cấp 1; nếu PTE hợp lệ thì đi xuống theo `PTE2PA`, nếu không và `alloc` bật thì cấp một trang bảng mới; cuối cùng trả về con trỏ tới PTE cấp 0. Hàm panic nếu `va >= MAXVA`.

Điểm quan trọng cho cả ba bài: `walk()` trả về **con trỏ tới PTE**, không phải địa chỉ vật lý. Nhờ vậy người gọi đọc được cờ (`PTE_V`, `PTE_A`) và sửa được bit — đúng những gì `vmprint()` và `pgaccess()` cần.

## 2.4 Bố cục bộ nhớ và các hàm ánh xạ

`memlayout.h` mô tả bố cục địa chỉ ảo của một tiến trình: text ở địa chỉ 0, data/bss ngay sau, stack cố định một trang (`USERSTACK = 1`) kèm một trang chắn phía dưới, vùng heap mọc lên bằng `sbrk()`, và hai trang cuối không gian địa chỉ là `TRAPFRAME` (`MAXVA - 2*PGSIZE`) và `TRAMPOLINE` (`MAXVA - PGSIZE`).

Các hàm liên quan:

- `mappages()` gọi `walk(..., 1)` cho từng trang, cấp bảng trang thiếu bằng `kalloc()`, rồi ghi `PA2PTE(pa) | perm`.
- `uvmalloc()` cấp trang cho vùng heap mọc lên; `uvmdealloc()` thu hồi khi `sbrk()` âm.
- `uvmclear()` xoá `PTE_U` để biến một trang đã ánh xạ thành trang chắn.
- `freewalk()` duyệt đệ quy theo cây để giải phóng bảng trang — đây là mẫu cho `vmprint()`.

xv6 **không** có handler cho lỗi `A`/`D`. Trên QEMU phần cứng tự bật `PTE_A` khi TLB nạp một PTE, nên bit này luôn phản ánh truy cập thật.

# 3. Mở rộng `sysinfo`: số byte bộ nhớ trống

## 3.1 Bộ cấp phát trang

Bộ nhớ vật lý kernel quản lý nằm giữa `end` và `PHYSTOP` ($`KERNBASE` + 128$ MiB). `kinit()` gọi `freerange()` đẩy từng trang vào danh sách liên kết đơn `kmem.freelist`; mỗi trang trống dùng chính nội dung của nó để lưu con trỏ `next`, nên danh sách không tốn thêm bộ nhớ. `kalloc()` lấy phần tử đầu, `kfree()` đẩy trang trở lại đầu — cả hai đều giữ `kmem.lock`. Vì vậy **độ dài danh sách** chính là số trang còn trống.

## 3.2 `freemem()`

```c
// Count the bytes of physical memory currently on the free list.
// The walk runs under kmem.lock so the list cannot change underneath us;
// callers must not hold kmem.lock themselves.
uint64
freemem(void)
{
  struct run *r;
  uint64 bytes = 0;

  acquire(&kmem.lock);
  for(r = kmem.freelist; r; r = r->next)
    bytes += PGSIZE;
  release(&kmem.lock);

  return bytes;
}
```

Duyệt danh sách trong `kmem.lock` là bắt buộc: nếu thả lock giữa đường, một `kalloc()` khác có thể lấy mất một trang và con trỏ `next` của trang đó bị ghi đè bởi dữ liệu mới, khiến vòng lặp đi vào rác.

## 3.3 Nối vào `sysinfo`

`struct sysinfo` được thêm trường đầu tiên:

```c
struct sysinfo {
  uint64 freemem;                  // bytes of physical memory on the free list
  int nfile;
  struct sysinfo_file files[SYSINFO_MAX_FILES];
  int nproc;
  struct sysinfo_proc procs[SYSINFO_MAX_PROCS];
};
```

Trường mới nằm ở đầu cấu trúc vì cả kernel lẫn user biên dịch từ cùng `kernel/sysinfo.h`, nên không có chuyện lệch layout. Kích thước tăng từ 32.984 lên **32.992 byte**, đúng bằng dòng `sizeof` mà `sysinfotest` in ra.

Trong `collect()`, `freemem()` được gọi **trước** hai pha chụp ảnh:

```c
  // Free physical memory, in bytes.  freemem() takes kmem.lock, which no
  // other snapshot path holds, so there is no lock-order cycle.
  info->freemem = freemem();
```

## 3.4 Thứ tự lock và chi phí

`sys_sysinfo()` giữ `snapshot_lock` suốt thời gian chụp ảnh. Bên trong, `freemem()` lấy thêm `kmem.lock`, pha 1 lấy `ftable.lock`, pha 2 lấy `p->lock`. Đồ thị lock vẫn là DAG vì `kmem.lock` không bao giờ được giữ đồng thời với `ftable.lock` hay `p->lock` (lời gọi `freemem()` kết thúc trước pha 1), và không đường nào trong kernel lấy `snapshot_lock` khi đang giữ một trong ba lock kia.

Chi phí là $O(\text{số trang trống})$ cho mỗi lời gọi: với 128 MiB và trang 4 KiB, danh sách có khoảng 32.000 phần tử, tất cả được duyệt trong lúc giữ spinlock (tắt interrupt trên CPU hiện tại). Hướng cải thiện là duy trì biến đếm `nfree` cập nhật ngay trong `kalloc()`/`kfree()`.

## 3.5 Kết quả kiểm thử

`user/sysinfotest.c` được bổ sung tám check, chạy **sau** toàn bộ 56 check của đồ án 1 nên đồng thời xác nhận không hồi quy:

| Check | Ý nghĩa |
| --- | --- |
| `freemem: reports a positive number of bytes` | Còn bộ nhớ trống |
| `freemem: value is a whole number of pages` | Giá trị chia hết cho 4096 |
| `freemem: value fits inside physical memory` | Nhỏ hơn `PHYSTOP - KERNBASE` |
| `freemem: sbrk grows the heap` | `sbrk(8*PGSIZE)` thành công |
| `freemem: drops after page allocation` | Giá trị giảm sau khi cấp trang |
| `freemem: drops by at least one page per allocated page` | Mức giảm $\ge 8$ trang |
| `freemem: drop stays within the data and page-table pages` | Mức giảm $\le 10$ trang |
| `freemem: recovers after the pages are freed` | `sbrk` âm trả lại trang |

Cận trên 10 trang là có chủ ý: `sbrk()` cấp 8 trang dữ liệu, cộng tối đa 2 trang bảng trang nếu dải 8 trang vắt qua ranh giới giữa hai bảng cấp 0 liền nhau.

```
sysinfotest: sizeof(struct sysinfo) = 32992 bytes
freemem: 133124096 bytes (about 126 MB) free
PASS: freemem: reports a positive number of bytes
PASS: freemem: value is a whole number of pages
PASS: freemem: value fits inside physical memory
PASS: freemem: sbrk grows the heap
PASS: freemem: drops after page allocation
PASS: freemem: drops by at least one page per allocated page
PASS: freemem: drop stays within the data and page-table pages
PASS: freemem: recovers after the pages are freed
ALL TESTS PASSED
```

133.124.096 byte xấp xỉ 126,9 MiB trên tổng 128 MiB. Chênh lệch 1.093.632 byte (đúng 267 trang) là text/data của kernel cùng các bảng trang và kernel stack đã cấp trước khi `init` chạy.

# 4. In bảng trang: `vmprint()`

## 4.1 Yêu cầu

Đề yêu cầu in cây bảng trang theo định dạng cố định: dòng đầu là địa chỉ bảng trang gốc, mỗi PTE hợp lệ một dòng, mỗi cấp thêm một tiền tố `" .."`, kèm chỉ số PTE, giá trị PTE và địa chỉ vật lý. PTE không hợp lệ không in. Quan trọng nhất: **không làm đổi hành vi `exec()` hiện có**.

## 4.2 Cài đặt đệ quy

```c
// Recursively print every valid PTE in a page table.  The depth
// controls the ".." prefix: one for the top level, two for the
// next level, and three for leaf entries.
static void
vmprintwalk(pagetable_t pagetable, int depth)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];

    if((pte & PTE_V) == 0)
      continue;

    // One " .." per level: the spec prints " ..0:" at the top level and
    // " .. ..0:" one level down.
    for(int j = 0; j < depth; j++)
      printf(" ..");
    printf("%d: pte %p pa %p\n", i, (void*)pte, (void*)PTE2PA(pte));

    // A PTE without R/W/X points to another page-table page.
    if((pte & (PTE_R | PTE_W | PTE_X)) == 0)
      vmprintwalk((pagetable_t)PTE2PA(pte), depth + 1);
  }
}

void
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", (void*)pagetable);
  vmprintwalk(pagetable, 1);
}
```

Ba điểm cần lưu ý:

- **Điều kiện dừng** là "không có bit nào trong `R`/`W`/`X`". Chỉ PTE trung gian mới trỏ tới bảng trang cấp dưới.
- **Địa chỉ vật lý** lấy bằng `PTE2PA(pte)`, tức dịch phải 10 rồi dịch trái 12 để bỏ 10 bit cờ.
- **Định dạng** dùng `%p` và ép kiểu `(void*)` cho cả PTE lẫn địa chỉ, nhờ vậy in đủ 64 bit có tiền tố `0x`.

## 4.3 Mốc gọi trong `exec()`

```c
  // Print the initial user page table once, when init execs.
  if(p->pid == 1)
    vmprint(p->pagetable);

  return argc; // this ends up in a0, the first argument to main(argc, argv)
```

Điều kiện `p->pid == 1` khiến bảng trang chỉ được in **một lần**, khi tiến trình `init` nạp chương trình đầu tiên. Mọi lần `exec()` khác (`sh`, `ls`, `usertests`) chạy đúng như cũ và không sinh thêm dòng nào. Nửa còn lại của yêu cầu — in khi người dùng muốn — được giải quyết bằng system call ở mục 4.4.

## 4.4 In theo yêu cầu: system call `vmprint()`

Đề yêu cầu bảng trang được in "khi người dùng muốn", nên nhóm thêm một system call không tham số, in bảng trang của **chính tiến trình gọi**:

```c
// Print the calling process's page table.  exec() already dumps it once for
// init, but this lets a user program ask for it at any point instead of
// waiting for that one moment.
uint64
sys_vmprint(void)
{
  vmprint(myproc()->pagetable);
  return 0;
}
```

Hàm nhận `myproc()->pagetable` thay vì một tham số địa chỉ, nên người dùng **không thể** yêu cầu in bảng trang của tiến trình khác (tránh một kênh rò rỉ thông tin về bố cục bộ nhớ), không cần kiểm tra địa chỉ do người dùng truyền vào, và mỗi tiến trình tự in bảng trang của mình đúng ngữ nghĩa "bảng trang của tiến trình đang chạy".

Số hiệu là `SYS_vmprint = 25` trong `kernel/syscall.h`, được thêm vào bảng dispatch và bảng tên nên `trace(1)` cũng in được tên `vmprint`. Phía người dùng có `int vmprint(void)` (`user/user.h`) và `entry("vmprint")` (`user/usys.pl`).

Hai mốc in cùng tồn tại vì trả lời hai nửa khác nhau của yêu cầu: mốc `pid == 1` ứng với "khi tiến trình được thực thi", system call ứng với "khi người dùng muốn".

## 4.5 Định dạng: một sai lệch đã sửa

Bản đầu tiên in `".."` một lần rồi `" .."` cho các cấp còn lại, nên dòng đầu ra là `..0:` thay vì ` ..0:`. Đề in **một tiền tố `" .."` cho mỗi cấp**, kể cả cấp trên cùng. Sửa thành vòng lặp `for(j = 0; j < depth; j++) printf(" ..")` thì đầu ra khớp đề:

```
page table 0x0000000087f4e000
 ..0: pte 0x0000000021fd2801 pa 0x0000000087f4a000
 .. ..0: pte 0x0000000021fd2401 pa 0x0000000087f49000
 .. .. ..0: pte 0x0000000021fd2c1b pa 0x0000000087f4b000
 .. .. ..1: pte 0x0000000021fd201b pa 0x0000000087f48000
 .. .. ..2: pte 0x0000000021fd1c17 pa 0x0000000087f47000
 .. .. ..3: pte 0x0000000021fd1807 pa 0x0000000087f46000
 .. .. ..4: pte 0x0000000021fd1417 pa 0x0000000087f45000
 ..255: pte 0x0000000021fd3401 pa 0x0000000087f4d000
 .. ..511: pte 0x0000000021fd3001 pa 0x0000000087f4c000
 .. .. ..510: pte 0x0000000021fd5807 pa 0x0000000087f56000
 .. .. ..511: pte 0x000000002000240b pa 0x0000000080009000
init: starting sh
```

## 4.5 Giải thích từng trang lá

Cây in ra có 11 dòng: 4 PTE trung gian và 7 PTE lá. Địa chỉ ảo tính từ ba chỉ số, cờ đọc bằng `PTE_FLAGS(pte) = pte & 0x3FF`.

| Cấp 2/1/0 | Cờ | Địa chỉ ảo | Nội dung và quyền |
| --- | --- | --- | --- |
| 0/0/0 | `0x01B` | `0x0000` | Trang text đầu tiên của `init`. `V R X U` — đọc và thực thi, không ghi |
| 0/0/1 | `0x01B` | `0x1000` | Trang text thứ hai. `V R X U` |
| 0/0/2 | `0x017` | `0x2000` | Data/bss đã khởi tạo của `init`. `V R W U` — đọc ghi, không thực thi |
| 0/0/3 | `0x007` | `0x3000` | Trang chắn dưới stack, đã bị `uvmclear()` xoá `PTE_U`. `V R W` — user không truy cập được |
| 0/0/4 | `0x017` | `0x4000` | Stack của `init` (`USERSTACK = 1` trang). `V R W U` |
| 255/511/510 | `0x007` | `0x3FFFFFE000` | `TRAPFRAME`. `V R W` — chỉ supervisor |
| 255/511/511 | `0x00B` | `0x3FFFFFF000` | `TRAMPOLINE`. `V R X` — đọc, thực thi, chỉ supervisor |

Bốn PTE trung gian còn lại có cờ `0x001` (chỉ `V`): chúng trỏ tới bảng trang chứ không ánh xạ trang nào. `..0` và `.. ..0` dẫn tới bảng trang chứa các trang 0–4; `..255` và `.. ..511` dẫn tới bảng trang của vùng địa chỉ cao.

Đối chiếu với mã nguồn tạo ra từng trang:

- Trang 0 và 1 do `exec()` cấp qua `uvmalloc()` với `flags2perm(ph.flags)`; `uvmalloc()` luôn thêm `PTE_R|PTE_U`. ELF của `init` đánh dấu text là `PF_X` nên kết quả là `R X U`.
- Trang 2 là data: ELF đánh dấu `PF_W` nên ra `R W U`.
- Trang 3 và 4 do `exec()` cấp thêm `(USERSTACK+1)*PGSIZE` với `PTE_W`; `uvmclear()` xoá `PTE_U` của trang thấp hơn để làm trang chắn.
- `TRAMPOLINE` và `TRAPFRAME` do `proc_pagetable()` ánh xạ cố định với `PTE_R|PTE_X` và `PTE_R|PTE_W`, cố ý **không** có `PTE_U`.

Một chi tiết đáng chú ý: bảng trang của `init` in lúc khởi động **không** có bit `A`/`D`, vì các trang vừa được kernel ghi qua direct map chứ tiến trình chưa truy cập. Khi dùng system call ở mục 4.4 để in bảng trang của một tiến trình đang chạy, cờ đổi thành `0x05B` cho text và `0x0D7` cho data/stack — bit `A` (64) và `D` (128) đã được phần cứng bật. Đây chính là bit mà `pgaccess()` ở chương 5 đọc.

## 4.7 Hạn chế

Sau khi thêm system call `vmprint()`, cả hai nửa của yêu cầu đều được đáp ứng. Hai hạn chế còn lại đều nhỏ và có chủ ý:

- **Vẫn in một lần lúc khởi động.** Mốc trong `exec()` làm đầu ra dài thêm 11 dòng mỗi lần boot; nếu cần im lặng hoàn toàn thì chỉ cần bỏ ba dòng trong `exec()` mà không ảnh hưởng system call.
- **Chưa có cách in bảng trang của tiến trình khác.** System call luôn in `myproc()->pagetable`; muốn in của tiến trình khác thì phải thêm tham số pid và một lock để đọc `p->pagetable` an toàn.

# 5. Phát hiện trang đã được truy cập: `pgaccess()`

## 5.1 Ý tưởng

RISC-V ghi nhận truy cập ở cấp trang bằng bit `PTE_A` (bit 6): mỗi khi bộ dịch địa chỉ nạp một PTE vào TLB, phần cứng bật bit này. Chỉ cần đọc và **xoá** `PTE_A` là biết được, trong khoảng giữa hai lần gọi, những trang nào đã bị đọc hoặc ghi. System call có ba tham số:

```
int pgaccess(void *base, int len, void *mask)
```

`base` là địa chỉ ảo của trang đầu, `len` là số trang cần kiểm tra, `mask` là vùng nhớ người dùng nhận bitmask: bit $i$ ứng với trang `base + i*PGSIZE`.

## 5.2 Đường đi của system call

| Tầng | Tệp | Nội dung |
| --- | --- | --- |
| Số hiệu | `kernel/syscall.h` | `SYS_pgaccess = 24` |
| Bảng dispatch | `kernel/syscall.c` | prototype và `sys_pgaccess` |
| Bảng tên | `kernel/syscall.c` | `"pgaccess"` cho `trace` |
| Cài đặt | `kernel/sysproc.c` | `sys_pgaccess()` |
| Stub user | `user/usys.pl` | `entry("pgaccess")` |

Bảng tên được cập nhật vì đồ án 1 đã thêm system call `trace`; nếu thiếu, `trace(1)` sẽ in tên rỗng.

## 5.3 Cài đặt

```c
// The bitmask is a single uint64, so one call can report at most 64 pages.
#define PGACCESS_MAX 64

uint64
sys_pgaccess(void)
{
  struct proc *p = myproc();
  uint64 base, maskaddr, bits = 0;
  pte_t *pte;
  int len;

  argaddr(0, &base);
  argint(1, &len);
  argaddr(2, &maskaddr);

  if(len <= 0 || len > PGACCESS_MAX)
    return -1;
  if(base >= MAXVA || base + (uint64)len * PGSIZE > MAXVA)
    return -1;

  // Pass 1: every page must be a valid user mapping.
  for(int i = 0; i < len; i++){
    if((pte = walk(p->pagetable, base + i * PGSIZE, 0)) == 0)
      return -1;
    if((*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
      return -1;
  }

  // Pass 2: collect the access bits and clear them for the next call.
  for(int i = 0; i < len; i++){
    pte = walk(p->pagetable, base + i * PGSIZE, 0);
    if(*pte & PTE_A)
      bits |= 1UL << i;
    *pte &= ~PTE_A;
  }

  if(copyout(p->pagetable, maskaddr, (char *)&bits, sizeof(bits)) < 0)
    return -1;
  return 0;
}
```

Bốn quyết định thiết kế:

1. **Bitmask nằm trong kernel.** Kernel điền xong mới `copyout()` một lần, thay vì ghi từng bit vào con trỏ người dùng.
2. **Giới hạn 64 trang.** Một `uint64` chỉ có 64 bit, nên `len > 64` bị từ chối thay vì âm thầm cắt bớt.
3. **Kiểm tra trước, sửa sau.** Toàn bộ dải được kiểm tra ở pha 1; chỉ khi mọi trang hợp lệ mới sang pha 2 để đọc và xoá `PTE_A`. Nhờ vậy một lời gọi bị từ chối **không** để lại tác dụng phụ.
4. **Kiểm tra biên trước khi `walk()`.** `walk()` panic khi `va >= MAXVA`, nên `base + len*PGSIZE` được kiểm tra trước, tránh cả trường hợp tràn số.

## 5.4 Chương trình kiểm thử

`user/pgtbltest.c` gồm **23 check**, chia năm nhóm:

| Nhóm | Số check | Nội dung |
| --- | --- | --- |
| Trạng thái ban đầu | 3 | Cấp 32 trang bằng `sbrk()`; lần gọi đầu trả 0 vì kernel vừa xoá trang qua direct map |
| Ghi và đọc | 7 | Ghi trang 0, đọc trang 5: mask đúng hai bit; gọi lần ba trả 0 vì bit đã bị xoá; trang cuối dải cũng được báo |
| Dải ngắn | 2 | `len = 4` chỉ xét 4 trang đầu, bỏ qua các trang đã truy cập phía sau |
| Đầu vào sai | 7 | Dọn bit còn lại, rồi dải đã `sbrk` âm, `len = 0`, `len` âm, `len > 64`, `base = MAXVA`, con trỏ mask xấu — tất cả trả $-1$ |
| In theo yêu cầu | 4 | `vmprint()` trả 0, heap vẫn dùng được, bảng trang vẫn `walk` được, trang mới vẫn ghi nhận bit `A` sau khi in |

Hai điểm đáng chú ý:

- Test **không** giả định trạng thái ban đầu của `PTE_A`: lời gọi đầu tiên vừa kiểm tra vừa xoá mọi bit còn sót, nên kết quả các lần sau là tất định.
- Trường hợp "dải chưa ánh xạ" không dùng một địa chỉ cố định, mà `sbrk(-32*PGSIZE)` để thu hồi đúng 32 trang vừa cấp, nên phép thử không phụ thuộc bố cục địa chỉ cụ thể.

## 5.5 Kết quả và hạn chế

```
pgtbltest: page-table tests
PASS: pgaccess: heap grows by NPAGES pages
PASS: pgaccess: first call succeeds
PASS: pgaccess: freshly mapped pages are not reported
PASS: pgaccess: untouched page reads as zero
PASS: pgaccess: second call succeeds
PASS: pgaccess: exactly the written and read pages are reported
PASS: pgaccess: third call succeeds
PASS: pgaccess: the previous call cleared the access bits
PASS: pgaccess: last page call succeeds
PASS: pgaccess: last page of the range is reported
PASS: pgaccess: short range call succeeds
PASS: pgaccess: short range ignores pages past len
PASS: pgaccess: range cleared
PASS: pgaccess: unmapped range is rejected
PASS: pgaccess: len 0 is rejected
PASS: pgaccess: negative len is rejected
PASS: pgaccess: more than MAXPAGES pages is rejected
PASS: pgaccess: virtual address beyond MAXVA is rejected
PASS: pgaccess: bad mask pointer is rejected
PASS: vmprint: dumps the page table on demand
PASS: vmprint: the heap is still usable afterwards
PASS: vmprint: the page table is still walkable
PASS: vmprint: the new page still records accesses
ALL TESTS PASSED
```

Bốn dòng cuối thuộc `vmprint_test()` (mục 4.4); phần bảng trang mà `vmprint()` in ra được lược bớt cho gọn.

`usertests -q` trên cùng bản build cũng cho `ALL TESTS PASSED`, nên việc thêm syscall 24–25 và đụng vào PTE không làm hỏng các bài kiểm thử có sẵn.

Ba hạn chế đã biết:

- **Giới hạn 64 trang và một `uint64`.** Muốn quét nhiều hơn phải đổi ABI thành mảng bitmask.
- **Chỉ xoá `PTE_A`, không đụng `PTE_D`** (dirty). `pgaccess` trả lời "đã truy cập", không phân biệt đọc với ghi.
- **Phụ thuộc phần cứng đặt `PTE_A` tự động.** QEMU bật bit này khi đi bộ bảng trang; trên CPU chỉ hỗ trợ `Svade` thì xv6 không có handler và sẽ panic.

# 6. Kết luận

Cả ba bài đều xoay quanh một cấu trúc duy nhất: mảng 512 PTE của mỗi trang bảng trang. Chỉ cần ba macro (`PX`, `PA2PTE`, `PTE2PA`) và một hàm đi xuống `walk()` là đủ để đọc và sửa toàn bộ cây.

Bài 1 tận dụng danh sách trang trống của `kalloc`: `freemem()` đếm số trang dưới `kmem.lock` rồi nhân với `PGSIZE` — đúng và không cần thêm trạng thái, nhưng tốn $O(\text{số trang trống})$ mỗi lần gọi. Kết quả: 133.124.096 byte trống trên 128 MiB, giảm đúng khi `sbrk()` cấp trang và trở lại khi thu hồi.

Bài 2 cho thấy `vmprint()` là một biến thể của `freewalk()`: duyệt 512 PTE, bỏ qua PTE không hợp lệ, in tiền tố theo độ sâu, chỉ đệ quy khi PTE không phải lá. Đầu ra đã được đọc ngược về mã nguồn: `R X U` là text, `R W U` là data và stack, `R W` không `U` là trang chắn và `TRAPFRAME`, `R X` không `U` là `TRAMPOLINE`. Mốc in đặt sau `proc_freepagetable()` và trước `return argc`, kèm điều kiện `p->pid == 1`, giữ cho `exec()` không đổi hành vi; system call `vmprint()` bổ sung đường in theo yêu cầu cho chính tiến trình gọi.

Bài 3 là bài duy nhất phải **ghi** vào bảng trang: `pgaccess()` đọc rồi xoá `PTE_A`. Hai bài học từ kiểm thử: phải kiểm tra toàn bộ dải trước khi xoá bit, và phải kiểm tra biên trước khi gọi `walk()` vì hàm này panic với địa chỉ $\ge$ `MAXVA`.

Hai lỗi đáng ghi lại vì đều thuộc về môi trường chứ không phải thuật toán: `PTE_LEAF` được `walk()` dùng nhưng chưa định nghĩa trong `riscv.h` (build dừng khi `LAB=pgtbl`), và tiền tố `".."` của `vmprint()` thiếu dấu cách đầu dòng so với định dạng của đề.

Hạn chế còn lại: mốc `pid == 1` vẫn in bảng trang `init` một lần lúc khởi động (chỉ cần bỏ ba dòng trong `exec()` nếu muốn im lặng), system call `vmprint()` chưa cho phép in bảng trang của tiến trình khác, và `pgaccess()` giới hạn 64 trang và chỉ quan tâm bit `A`, chưa phân biệt đọc với ghi bằng bit `D`.

# Tài liệu tham khảo

1. MIT PDOS, *xv6-riscv: Xv6 for RISC-V*, GitHub repository. https://github.com/mit-pdos/xv6-riscv
2. R. Cox, F. Kaashoek, and R. Morris, *xv6: A Simple, Unix-like Teaching Operating System*, Revision 4, Aug. 2024. https://pdos.csail.mit.edu/6.828/2024/xv6/book-riscv-rev4.pdf
3. A. Waterman and K. Asanović, Eds., *The RISC-V Instruction Set Manual, Volume II: Privileged Architecture*, Version 20240411, RISC-V International, 2024. https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-privileged.pdf
