# Bài tập Lab 0 — System Call: `trace`

**Mã sinh viên:** StudentID1 / StudentID2 / StudentID3
**Nhánh:** `feat/syscall-trace`

---

## Các file được sửa và mục đích

**1. `kernel/syscall.h`**
- **Sửa:** khai báo `#define SYS_trace 22`.
- **Tại sao:** là số định danh duy nhất ánh xạ lời gọi syscall từ user-space vào hàm thực thi trong kernel.

**2. `kernel/syscall.c`**
- **Sửa:** thêm `extern uint64 sys_trace(void);`, mục `[SYS_trace] sys_trace` vào bảng `syscalls[]`, thêm mảng `syscall_names[]`, sửa `syscall()` để in `syscall <name> -> <retval>`.
- **Tại sao:** đây là điểm trung tâm mà mọi syscall đều đi qua, nên in ở đây tránh sửa từng hàm. Giá trị trả về được lưu vào biến cục bộ `ret` trước khi gán vào `trapframe->a0` để tránh gọi hàm hai lần (tránh đọc/ghi hai lần).

**3. `kernel/proc.h`**
- **Sửa:** thêm `int trace_mask;` vào `struct proc`.
- **Tại sao:** lưu mask trên từng process để trace không ảnh hưởng tới process khác.

**4. `kernel/proc.c`**
- **Sửa:** gán `p->trace_mask = 0` trong `allocproc()`; copy `trace_mask` trong `fork()`.
- **Tại sao:** `allocproc()` không zero-toàn bộ struct, nên phải reset để process được cấp lại không kế thừa mask lẫn lộn. Copy trong `fork()` giữ trace cho process con.

**5. `kernel/sysproc.c`**
- **Sửa:** cài `sys_trace()` — `argint(0, &mask)` rồi gán `myproc()->trace_mask = mask`.
- **Tại sao:** cài phần thực thi syscall; đọc đối số mask từ user. (`argint` ở bản này trả về `void`, nên không kiểm tra lỗi.)

**6. `user/user.h`**
- **Sửa:** thêm `int trace(int);`.
- **Tại sao:** khai báo cho code user gọi.

**7. `user/usys.pl`**
- **Sửa:** thêm `entry("trace");`.
- **Tại sao:** sinh stub `usys.S` dùng `ecall` chuyển xuống kernel.

**8. `Makefile`**
- **Sửa:** thêm `$U/_trace\` vào `UPROGS`.
- **Tại sao:** biên dịch chương trình `trace` và đưa vào `fs.img`.

---

## Kiểm tra

Chạy trong QEMU, đúng với đầu ra trong đề:

```
$ trace 32 grep hello README
syscall read -> 1023
syscall read -> 971
syscall read -> 298
syscall read -> 0
```

`32 = 1 << SYS_read`, chỉ trace lệnh `read`.

```
$ trace 2147483647 grep hello README
syscall trace -> 0
syscall exec -> 3
syscall open -> 3
syscall read -> 1023
...
syscall close -> 0
```

`2147483647` bật hết 31 bit → trace tất cả syscall.

```
$ grep hello README
$      (không có dòng nào)
```

Process gọi `grep` không được trace → không in gì, chứng minh việc trace được cô lập trên từng process.

---

## Lưu ý môi trường

Bản xv6 gốc không biên dịch/boot được trên máy này vì **GCC 15 + QEMU 10** (compiler sinh lệnh vector/`c.mul` mà CPU mặc định trong QEMU chưa hỗ trợ), cũng xuất hiện lỗi `rwsbrk()` ở `user/usertests.c` vì kiểu khai báo không khớp với bảng `quicktests[]`. Hai vấn đề này đã được sửa ở hai commit riêng (`fix(usertests)` và `chore(build)`) và **không nằm trong file patch** nộp, vì chúng là lỗi của công cụ, không liên quan tới cài đặt `trace`.