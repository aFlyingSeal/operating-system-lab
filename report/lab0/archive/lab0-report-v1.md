# Báo cáo Lab 0 - System Call: `trace`

Thông tin nhóm:

| STT | MSSV | Họ và tên |
| --- | --- | --- |
| 1 | 24120010 | Trần Văn Tấn Khôi |
| 2 | 24120035 | Lê Thành Đạt |
| 3 | 24120361 | Trịnh Tuấn Kiệt |

# 1. Giới thiệu và Mục tiêu

Mục tiêu của Lab 0 nhằm mở rộng thêm nhân của hệ điều hành xv6 với system call mới là `trace`. Syscall trên có tính năng in ra các syscall khác được gọi bởi một chương trình người dùng dựa trên một bitmask để lọc, nhằm hỗ trợ cho quá trình debugging tại các bài Lab tiếp theo. 

# 2. Tóm tắt các thay đổi về mã nguồn

## 2.1. Khai báo giao diện System Call

- `kernel/syscall.h`
    - Mỗi system call cần có mã định danh duy nhất để kernelP có thể ánh xạ lời gọi syscall từ user-space vào hàm thực thi trong kernel.
    - Nội dung: Định nghĩa macro `#define SYS_trace 22`.
- `user/user.h`
    - Thêm nguyên mẫu hàm vào user-space để khai báo cho code mà user gọi.
    - Nội dung: Thêm prototype `int trace(int)`.
- `user/usys.pl`
    - xv6 sử dụng ngôn ngữ Perl để tự động sinh ra mã stub hợp ngữ, thực thi lệnh `ecall` của RISC-V chuyển đổi vào kernel.
    - Nội dung: Thêm entry `entry("trace")`.

## 2.2. Logic định tuyến và xử lý Kernel

- `kernel/syscall.c`
    - Kernel cần ánh xạ số hiệu system call tới hàm thực tế và in ra log trace khi được bật.
    - Nội dung:
        - Khai báo prototype `extern uint_64 sys_trace(void);` và ánh xạ `[SYS_trace] sys_trace` trong bảng con trỏ hàm `syscalls[]`.
        - Tạo bảng ánh xạ các syscall sang tên tương ứng `syscall_names[]` theo yêu cầu.
        - Cập nhật hàm `syscall()` để kiểm tra `p->trace_mask` với `(1 << num)` và in ra kết quả gồm PID, tên syscall và giá trị trả về.
- `kernel/proc.h` & `kernel/proc.c`
    - Biến trạng thái `trace_mask` cần được duy trì ở mỗi tiến trình và kế thừa chính xác khi một tiến trình sinh ra tiến trình con qua hàm `fork()`.
    - Nội dung: Thêm biến `int trace_mask;` vào struct `proc` trong `kernel/proc.h` và đảm bảo sao chép `np->trace_mask` = p->trace_mask;` bên trong `fork()` ở `kernel/proc.c`.
- `kernel/sysproc.c`
    - Xử lý logic phía kernel khi người dùng gọi hàm `trace(mask)`.
    - Nội dung: Cài đặt hàm `sys_trace()` sử dụng `argint(0, &p->trace_mask)` để lấy tham số từ user-space và gán cho tiến trình hiện tại.

## 2.3. Chương trình người dùng & Cấu hình biên dịch

- `user/trace.c`
    - Cài đặt tiện ích dòng lệnh để nhận mask va người dung thực thi chương trình mục tiêu kèm với tính năng `trace`.
- `Makefile`
    - Đảm bảo hệ thống biên dịch tự động tạo ra file thực thi mới sau mỗi lần build.
    - Nội dung: Thêm entry `$U/_trace\` vào trong `Makefile`.

# 3. Kiểm thử và Xác thực

Quá trình cài đặt được kiểm chứng trên môi trường QEMU. Kết quả thử nghiệm trả về đúng các syscall được lọc và các giá trị tương ứng:

```bash
$ trace 32 grep hello README
syscall read -> 1023
syscall read -> 971
syscall read -> 298
syscall read -> 0
```

```bash
$ trace 2147483647 grep hello README
syscall trace -> 0
syscall exec -> 3
syscall open -> 3
syscall read -> 1023
syscall read -> 971
syscall read -> 298
syscall read -> 0
syscall close -> 0
```