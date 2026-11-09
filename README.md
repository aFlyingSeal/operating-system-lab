# Operating System - xv6 Labs

Repo lưu trữ mã nguồn `xv6-labs-2024` đã được chỉnh sửa, patch nộp bài, báo cáo và tài liệu các lab môn Hệ Điều Hành.

## Cấu trúc repo

```text
operating-system/
├── docs/                 # Đề bài Lab 0, Lab 1, Lab 2
├── patches/              # Patch nộp bài, sinh từ thư mục xv6-labs-2024
├── report/               # Báo cáo theo từng lab
│   ├── lab0/             # Lab 0: system call trace
│   └── lab1/             # Lab 1: file/process management + sysinfo
│       ├── lab1-report.md
│       └── latex/        # Mã nguồn LaTeX của báo cáo Lab 1
├── xv6-labs-2024/        # Mã nguồn xv6 RISC-V đã chỉnh sửa
└── README.md
```

## Tóm tắt nội dung các lab

| Lab # | Nội dung | Chi tiết |
| --- | --- | --- |
| 0 | System Call | Khảo sát cơ chế system call trong xv6, cách chương trình user space gọi vào kernel và trace các system call. |
| 1 | File & Memory Management | Khảo sát cơ chế quản lý file (file descriptor, open file, inode), quản lý tiến trình (pid, state, context, fork, scheduler) và cài đặt system call `sysinfo` cùng chương trình kiểm thử `sysinfotest`. |
| 2 | Memory | Khảo sát quản lý bộ nhớ trong xv6, bao gồm cấp phát, ánh xạ địa chỉ và page table. |

## Build và chạy xv6

```bash
cd xv6-labs-2024
make clean
make qemu
```
