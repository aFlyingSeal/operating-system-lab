# Lab 1 — LaTeX source

Mã nguồn báo cáo Lab 1.

## Cấu trúc

- `report.tex` — file chính.
- `reportDoCS2025.cls` — class của bộ môn.
- `biblio.tex` — tài liệu tham khảo.
- `sections/` — nội dung báo cáo:
  - `credits.tex` — bảng phân chia công việc.
  - `part1.tex` — mục tiêu và phạm vi.
  - `part2.tex` — quản lý file.
  - `part3.tex` — quản lý tiến trình.
  - `part4.tex` — cài đặt và kiểm thử `sysinfo`.
  - `conclusion.tex` — kết luận.
- `utilities.tex` — mẫu bảng và hộp code, hiện chưa dùng.
- `latexmkrc` — cấu hình build bằng XeLaTeX.
- `archive/` — README và bảng cũ từ template, không dùng.

## Build

Chạy `latexmk` trong thư mục này.
